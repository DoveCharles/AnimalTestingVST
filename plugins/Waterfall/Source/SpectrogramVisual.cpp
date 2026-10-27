#include "SpectrogramVisual.h"
#include "PluginProcessor.h"

namespace
{
    struct Rgb { float r, g, b; };

    Rgb mix (Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
    Rgb toRgb (juce::Colour c) { return { (float) c.getRed(), (float) c.getGreen(), (float) c.getBlue() }; }

    constexpr Rgb skyTop    {  92.0f, 176.0f, 242.0f };
    constexpr Rgb skyBottom {  52.0f, 138.0f, 224.0f };
    constexpr Rgb haze      { 250.0f, 252.0f, 255.0f };

    constexpr Rgb cloudDark  { 34.0f, 92.0f, 196.0f };    // deeper indigo-blue patches
    constexpr Rgb cloudLight { 120.0f, 206.0f, 238.0f };  // paler teal patches

    const juce::Colour waterShadow { 0xff04182e };        // shadows on the surface, as the knobs'

    /** Large, very faint colour blurs drifting left to right. */
    struct Blur { float y, radius, speed, phase, alpha; juce::uint32 colour; };

    constexpr Blur blurs[] {
        { 0.30f, 0.45f, 0.012f, 0.00f, 0.24f, 0xffd9b8ff },   // lavender
        { 0.70f, 0.55f, 0.008f, 0.40f, 0.20f, 0xffffc9d6 },   // blush
        { 0.50f, 0.40f, 0.015f, 0.75f, 0.18f, 0xffbff5dc },   // mint
        { 0.20f, 0.35f, 0.010f, 0.55f, 0.16f, 0xfffff1c2 },   // pale gold
        { 0.85f, 0.50f, 0.006f, 0.20f, 0.14f, 0xffc4e4ff },   // ice
    };

    float hash (int x, int y) noexcept
    {
        auto n = (juce::uint32) (x * 374761393 + y * 668265263);
        n = (n ^ (n >> 13)) * 1274126177u;
        return (float) ((n ^ (n >> 16)) & 0xffffu) / 65535.0f;
    }

    /** Smooth 2D value noise in 0..1. */
    float valueNoise (float x, float y) noexcept
    {
        const auto ix = (int) std::floor (x), iy = (int) std::floor (y);
        auto fx = x - (float) ix, fy = y - (float) iy;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const auto top    = hash (ix, iy)     + (hash (ix + 1, iy)     - hash (ix, iy))     * fx;
        const auto bottom = hash (ix, iy + 1) + (hash (ix + 1, iy + 1) - hash (ix, iy + 1)) * fx;
        return top + (bottom - top) * fy;
    }

    /** A few octaves of value noise for a cloudy, painterly texture, roughly 0..1. */
    float clouds (float x, float y) noexcept
    {
        return 0.55f * valueNoise (x, y) + 0.3f * valueNoise (x * 2.1f + 5.2f, y * 2.1f + 1.3f)
             + 0.15f * valueNoise (x * 4.3f + 9.7f, y * 4.3f + 3.1f);
    }

    float smoothstep (float edge0, float edge1, float x) noexcept
    {
        const auto t = juce::jlimit (0.0f, 1.0f, (x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    /** River current in component pixels per second: fastest mid-stream, slower at the banks. */
    float currentSpeed (float normalisedY) noexcept
    {
        const auto d = 2.0f * normalisedY - 1.0f;
        return 70.0f + 90.0f * (1.0f - d * d);
    }

    /** Each pitch has its own hue: red for lows up to violet for highs, drifting slightly along the trail. */
    juce::Colour trailColour (float normalisedY, float normalisedX)
    {
        return juce::Colour::fromHSV (0.8f * (1.0f - normalisedY) + 0.06f * normalisedX, 0.6f, 1.0f, 1.0f);
    }

    /** Water strider, traced from a line sketch with its head up: the left half as polylines in sketch
        units, mirrored about x = 237 for the right. Scaled to 1 unit = the sketch's full height. */
    struct SketchLine { std::vector<juce::Point<float>> points; };

    constexpr float sketchMirrorX = 237.07f, sketchCentreY = 290.0f, sketchHeight = 575.0f;

    const SketchLine striderSketch[] {
        { { { 177.4f, 82.3f }, { 179.4f, 105.7f }, { 216.9f, 120.4f }, { 225.6f, 164.1f }, { 215.3f, 234.8f } } },   // antenna, head, thorax
        { { { 215.3f, 234.8f }, { 50.4f, 192.4f }, { 10.2f, 10.2f } } },                                            // front leg
        { { { 215.3f, 234.8f }, { 215.3f, 307.7f }, { 226.4f, 396.3f } } },                                         // abdomen
        { { { 215.3f, 307.7f }, { 46.3f, 400.7f }, { 46.3f, 564.9f } } },                                           // hind leg
    };

    // The left edge of the head, thorax and abdomen, filled in so the body has some weight.
    const std::vector<juce::Point<float>> sketchBody {
        { 216.9f, 120.4f }, { 225.6f, 164.1f }, { 215.3f, 234.8f }, { 215.3f, 307.7f }, { 226.4f, 396.3f }
    };

    // Where the left feet touch the water, in sketch units.
    constexpr juce::Point<float> sketchFeet[] { { 10.2f, 10.2f }, { 46.3f, 564.9f } };

    juce::Point<float> fromSketch (juce::Point<float> p, bool mirrored)
    {
        const auto x = mirrored ? 2.0f * sketchMirrorX - p.x : p.x;
        return { (x - sketchMirrorX) / sketchHeight, (p.y - sketchCentreY) / sketchHeight };
    }

    const juce::Path& striderPath()
    {
        static const auto path = []
        {
            juce::Path p;
            for (auto mirrored : { false, true })
                for (const auto& line : striderSketch)
                {
                    auto first = true;
                    for (auto point : line.points)
                    {
                        const auto q = fromSketch (point, mirrored);
                        if (std::exchange (first, false)) p.startNewSubPath (q);
                        else                              p.lineTo (q);
                    }
                }
            return p;
        }();
        return path;
    }

    const juce::Path& striderBody()
    {
        static const auto path = []
        {
            juce::Path p;
            p.startNewSubPath (fromSketch (sketchBody.front(), false));
            for (auto point : sketchBody)
                p.lineTo (fromSketch (point, false));
            for (auto it = sketchBody.rbegin(); it != sketchBody.rend(); ++it)
                p.lineTo (fromSketch (*it, true));
            p.closeSubPath();
            return p;
        }();
        return path;
    }

    constexpr float striderSize = 42.0f;   // pixels from feet to feet, front to back
    constexpr float rippleLife = 1.8f;     // seconds for a ring to spread and fade
    constexpr float crestGap = 0.16f;      // seconds between the rings of one splash

    /** How fast things on the surface drift: a little under the current, as if paddling against it.
        Striders and their ripples share it, so the rings stay centred on a strider until it hops. */
    float surfaceDrift (float normalisedY) noexcept
    {
        return 0.55f * currentSpeed (normalisedY);
    }

    constexpr float flowCycle = 6.0f;      // seconds per flow-map cycle (two offset layers cross-fade)

    constexpr float minHz = 40.0f, maxHz = 10000.0f;
    constexpr float floorDb = -78.0f;
}

SpectrogramVisual::SpectrogramVisual (PluginProcessor& p)
    : processor (p),
      vblank (this, [this] (double t) { tick (t); })
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
    particles.reserve (maxParticles);
    striders.reserve (numStriders + 1);
    ripples.reserve (maxRipples);
}

void SpectrogramVisual::resized()
{
    const auto w = juce::jmax (1, getWidth()  / pixelSize);
    const auto h = juce::jmax (1, getHeight() / pixelSize);
    frame = juce::Image (juce::Image::ARGB, w, h, true);
    history.assign ((size_t) (w * h), 0.0f);
    columnValues.assign ((size_t) h, 0.0f);
    head = 0;

    // Start with the striders already spread along the river, so it's never empty.
    striders.clear();
    ripples.clear();
    for (size_t i = 0; i < numStriders; ++i)
        spawnStrider (((float) i + 0.3f + 0.4f * random.nextFloat()) / (float) numStriders * (float) getWidth());

    if (! grain.isValid())
    {
        grain = juce::Image (juce::Image::ARGB, 256, 256, true);
        juce::Image::BitmapData data (grain, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < grain.getHeight(); ++y)
            for (int x = 0; x < grain.getWidth(); ++x)
            {
                const auto v = random.nextFloat();
                const auto light = v > 0.5f;
                const auto a = (juce::uint8) (std::abs (v - 0.5f) * 2.0f * 255.0f);
                const auto grey = (juce::uint8) (light ? 255 : 10);
                data.setPixelColour (x, y, juce::Colour (grey, grey, grey, a));
            }
    }
}

float SpectrogramVisual::historyAt (int column, int row) const noexcept
{
    const auto w = frame.getWidth();
    const auto c = (((head + column) % w) + w) % w;
    return history[(size_t) (c * frame.getHeight() + row)];
}

void SpectrogramVisual::tick (double timestampSec)
{
    if (startTime < 0.0)
    {
        startTime = timestampSec;
        columnsEmitted = 0.0;
    }

    time = (float) (timestampSec - startTime);
    const auto dt = juce::jlimit (0.0f, 0.1f, time - lastTickTime);
    lastTickTime = time;
    readAudio();

    if (frame.isValid())
    {
        // Columns advance with wall-clock time so the scroll speed is steady whatever the host buffer size.
        const auto due = std::floor (time * columnsPerSecond);
        for (int i = 0; columnsEmitted < due && i < 4; ++i, columnsEmitted += 1.0)
            pushColumn();
        columnsEmitted = juce::jmax (columnsEmitted, due - 1.0);
    }

    updateParticles (dt);
    updateStriders (dt);
    repaint();

    if (onTick)
        onTick (time);
}

void SpectrogramVisual::readAudio()
{
    int total = 0;

    for (;;)
    {
        const auto n = processor.pullScopeSamples (pullBuffer.data(), (int) pullBuffer.size());
        if (n == 0)
            break;

        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = pullBuffer[(size_t) i];
            ringPos = (ringPos + 1) % fftSize;
        }
        total += n;
    }

    if (total > 0)
        lastAudioTime = time;
    else if (time - lastAudioTime > 0.25f)
        std::fill (ring.begin(), ring.end(), 0.0f);   // host stopped calling us: let the picture fall silent
}

void SpectrogramVisual::pushColumn()
{
    const auto w = frame.getWidth();
    const auto h = frame.getHeight();

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const auto sampleRate = processor.getSampleRate() > 0.0 ? (float) processor.getSampleRate() : 44100.0f;
    const auto binHz = sampleRate / (float) fftSize;
    const auto normalise = 2.0f / (float) fftSize * 2.0f;   // full-scale sine ≈ 0 dB (Hann loses half)
    const auto ratio = maxHz / minHz;

    const auto* previous = history.data() + (size_t) (head * h);
    head = (head - 1 + w) % w;
    auto* column = history.data() + (size_t) (head * h);

    for (int y = 0; y < h; ++y)
    {
        // Log frequency axis, high frequencies at the top.
        const auto t0 = 1.0f - (float) (y + 1) / (float) h;
        const auto t1 = 1.0f - (float) y / (float) h;
        const auto lo = juce::jlimit (1, fftSize / 2 - 1, (int) (minHz * std::pow (ratio, t0) / binHz));
        const auto hi = juce::jlimit (lo, fftSize / 2 - 1, (int) (minHz * std::pow (ratio, t1) / binHz));

        float mag = 0.0f;
        for (int b = lo; b <= hi; ++b)
            mag = juce::jmax (mag, fftData[(size_t) b]);

        // Tilt around 1 kHz, +3 dB per octave below and +9 dB per octave above: real sound falls off
        // steeply with pitch, so without this the upper partials barely show.
        const auto centreHz = minHz * std::pow (ratio, 0.5f * (t0 + t1));
        const auto octaves = std::log2 (centreHz / 1000.0f);
        const auto tilt = octaves * (octaves > 0.0f ? 9.0f : 3.0f);
        const auto db = juce::jmax (floorDb, juce::Decibels::gainToDecibels (mag * normalise, -200.0f) + tilt);
        columnValues[(size_t) y] = juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
    }

    for (int y = 0; y < h; ++y)
    {
        // Soften across neighbouring frequencies, then blend with the previous column, so hits smear into
        // the flow rather than showing as hard blocks.
        const auto above = columnValues[(size_t) juce::jmax (0, y - 1)];
        const auto below = columnValues[(size_t) juce::jmin (h - 1, y + 1)];
        const auto value = 0.25f * above + 0.5f * columnValues[(size_t) y] + 0.25f * below;
        column[y] = 0.4f * value + 0.6f * previous[y];

        // Loud partials shed glowing particles of their own colour into the current.
        const auto s = column[y];
        if (s > 0.3f && random.nextFloat() < 0.06f * s * s)
            spawnParticle (random.nextFloat() * 12.0f, ((float) y + random.nextFloat()) * (float) pixelSize,
                           trailColour ((float) y / (float) h, 0.0f).interpolatedWith (juce::Colours::white, 0.35f));
    }
}

void SpectrogramVisual::spawnParticle (float x, float y, juce::Colour colour)
{
    if (particles.size() >= maxParticles || getHeight() <= 0)
        return;

    const auto vx = currentSpeed (y / (float) getHeight()) * (0.8f + 0.4f * random.nextFloat());
    const auto life = ((float) getWidth() - x) / vx * (0.7f + 0.3f * random.nextFloat());
    particles.push_back ({ x, y, vx, random.nextFloat() * juce::MathConstants<float>::twoPi,
                           1.2f + 2.3f * random.nextFloat(), 0.0f, life, colour });
}

void SpectrogramVisual::updateParticles (float dt)
{
    // A slow trickle of pale motes even in silence, so the river never quite stops.
    if (random.nextFloat() < dt * 3.0f)
        spawnParticle (-4.0f, random.nextFloat() * (float) getHeight(),
                       juce::Colour (0xffe8f6ff).interpolatedWith (trailColour (random.nextFloat(), 0.0f), 0.2f));

    for (auto& p : particles)
    {
        p.age += dt;
        p.x += p.vx * dt;
        p.y += std::sin (time * 1.3f + p.phase) * 6.0f * dt;
    }

    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [this] (const Particle& p) { return p.age >= p.life || p.x > (float) getWidth() + 8.0f; }),
                     particles.end());
}

void SpectrogramVisual::spawnStrider (float x)
{
    if (getHeight() <= 0)
        return;

    striders.push_back ({ x, (0.18f + 0.64f * random.nextFloat()) * (float) getHeight(),
                          striderSize, 0.3f * (random.nextFloat() - 0.5f),
                          1.0f + 4.0f * random.nextFloat(), false });
}

void SpectrogramVisual::spawnRipple (float x, float y, float size, int crests)
{
    if (ripples.size() < maxRipples)
        ripples.push_back ({ x, y, size, 0.0f, crests });
}

void SpectrogramVisual::updateStriders (float dt)
{
    const auto height = (float) juce::jmax (1, getHeight());

    // Keep a few on the river: a new one paddles in from the left now and then.
    if (striders.size() < numStriders && random.nextFloat() < dt * 0.5f)
        spawnStrider (-40.0f);

    for (auto& s : striders)
    {
        s.x += surfaceDrift (s.y / height) * dt;

        s.untilHop -= dt;
        if (s.untilHop > 0.0f)
            continue;

        // First turn to face the way it will hop, and pause a moment.
        if (! s.aimed)
        {
            s.heading = juce::jlimit (-1.1f, 1.1f, s.heading + 1.4f * (random.nextFloat() - 0.5f));
            s.aimed = true;
            s.untilHop = 0.08f + 0.07f * random.nextFloat();
            continue;
        }

        // Then jump straight there in one go, landing in a splash of rings centred on it.
        s.aimed = false;
        s.untilHop = 1.5f + 5.0f * random.nextFloat();
        const auto distance = 45.0f + 35.0f * random.nextFloat();
        s.x += std::cos (s.heading) * distance;
        s.y = juce::jlimit (0.1f * height, 0.9f * height, s.y + std::sin (s.heading) * distance);
        spawnRipple (s.x, s.y, 1.0f, 3);
    }

    striders.erase (std::remove_if (striders.begin(), striders.end(),
                                    [this] (const Strider& s) { return s.x > (float) getWidth() + 50.0f; }),
                    striders.end());

    for (auto& r : ripples)
    {
        r.age += dt;
        r.x += surfaceDrift (r.y / height) * dt;   // the rings ride the current with the striders
    }

    ripples.erase (std::remove_if (ripples.begin(), ripples.end(),
                                   [] (const Ripple& r) { return r.age >= rippleLife + crestGap * (float) (r.crests - 1); }),
                   ripples.end());
}

void SpectrogramVisual::drawStriders (juce::Graphics& g) const
{
    for (const auto& r : ripples)
    {
        for (int i = 0; i < r.crests; ++i)
        {
            const auto age = r.age - crestGap * (float) i;   // each inner ring follows a moment later
            if (age <= 0.0f || age >= rippleLife)
                continue;

            // Spreads quickly then slows, fading as it goes: a bright crest with a faint trough just inside.
            const auto t = age / rippleLife;
            const auto radius = 20.0f + 48.0f * r.size * (1.0f - std::pow (1.0f - t, 2.5f));
            const auto alpha = std::pow (1.0f - t, 1.5f) * juce::jmin (1.0f, age * 12.0f) * (0.5f + 0.5f * r.size)
                             * (1.0f - 0.25f * (float) i);
            const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ r.x, r.y });

            g.setColour (waterShadow.withAlpha (0.18f * alpha));
            g.drawEllipse (circle.reduced (1.6f), 1.6f);
            g.setColour (juce::Colours::white.withAlpha (0.7f * alpha));
            g.drawEllipse (circle, 1.2f);
        }
    }

    const auto& path = striderPath();
    const auto& body = striderBody();

    for (const auto& s : striders)
    {
        const auto angle = s.heading + juce::MathConstants<float>::halfPi;

        auto place = [&] (float size, float x, float y)
        {
            return juce::AffineTransform::scale (size).rotated (angle).translated (x, y);
        };

        // Its shadow on the water, with the dimples under its feet.
        const auto shadowAt = juce::Point<float> (s.x + 2.0f, s.y + 3.0f);
        g.setColour (waterShadow.withAlpha (0.3f));
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      place (s.size, shadowAt.x, shadowAt.y));
        g.fillPath (body, place (s.size, shadowAt.x, shadowAt.y));

        // Each foot presses a soft dimple into the surface.
        for (auto mirrored : { false, true })
            for (auto foot : sketchFeet)
            {
                const auto p = fromSketch (foot, mirrored).transformedBy (place (s.size, s.x, s.y));
                g.setGradientFill ({ waterShadow.withAlpha (0.35f), p, waterShadow.withAlpha (0.0f), p.translated (4.0f, 0.0f), true });
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));
            }

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      place (s.size, s.x, s.y));
        g.fillPath (body, place (s.size, s.x, s.y));
    }
}

void SpectrogramVisual::paint (juce::Graphics& g)
{
    if (! frame.isValid())
        return;

    renderScene();

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat());

    drawParticles (g);
    drawStriders (g);

    // Fine film grain at full resolution, shifted every frame so it lives rather than sits.
    g.setTiledImageFill (grain, -random.nextInt (256), -random.nextInt (256), 0.07f);
    g.fillAll();
}

void SpectrogramVisual::renderScene()
{
    const auto w = frame.getWidth();
    const auto h = frame.getHeight();

    // Flow map: two layers of noise slide with the current and cross-fade, so the streaks keep
    // flowing without the faster middle shearing them apart over time.
    const auto phase1 = time / flowCycle - std::floor (time / flowCycle);
    const auto phase2 = phase1 + 0.5f - std::floor (phase1 + 0.5f);
    const auto layerBlend = std::abs (2.0f * phase1 - 1.0f);

    {
        juce::Image::BitmapData data (frame, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < h; ++y)
        {
            const auto ny = (float) y / (float) h;
            const auto skyBase = mix (skyTop, skyBottom, ny);
            const auto flow = currentSpeed (ny) / (float) pixelSize * flowCycle * 0.02f;
            auto* row = reinterpret_cast<juce::PixelARGB*> (data.getLinePointer (y));

            for (int x = 0; x < w; ++x)
            {
                const auto fx = (float) x, fy = (float) y;

                // Current: noise stretched along the flow into streaks, breaking the blue into soft patches.
                const auto a = clouds (fx * 0.02f - phase1 * flow, fy * 0.09f + time * 0.01f);
                const auto b = clouds (fx * 0.02f - phase2 * flow + 31.7f, fy * 0.09f + time * 0.01f + 12.9f);
                const auto n = a + (b - a) * layerBlend;
                auto c = n < 0.5f ? mix (skyBase, cloudDark,  juce::jmin (1.0f, (0.5f - n) * 1.6f))
                                  : mix (skyBase, cloudLight, juce::jmin (1.0f, (n - 0.5f) * 1.4f));

                // Random colour variation: wandering tints from teal through violet to pink.
                const auto hueNoise  = valueNoise (fx * 0.014f - time * 0.04f + 40.0f, fy * 0.035f + 17.0f);
                const auto tintNoise = valueNoise (fx * 0.022f - time * 0.03f + 3.0f,  fy * 0.05f - time * 0.01f);
                const auto tint = juce::Colour::fromHSV (std::fmod (0.42f + 0.55f * hueNoise, 1.0f), 0.6f, 0.95f, 1.0f);
                c = mix (c, toRgb (tint), 0.6f * smoothstep (0.3f, 0.8f, tintNoise));

                // Caustics: thin wandering lines of light where two noise fields cross their midpoint.
                const auto r1 = 1.0f - std::abs (2.0f * valueNoise (fx * 0.09f - time * 0.6f, fy * 0.14f + time * 0.15f) - 1.0f);
                const auto r2 = 1.0f - std::abs (2.0f * valueNoise (fx * 0.11f - time * 0.45f + 7.0f, fy * 0.12f - time * 0.1f) - 1.0f);
                const auto caustic = 0.5f * (std::pow (r1, 8.0f) + std::pow (r2, 8.0f));
                c = mix (c, haze, 0.16f * caustic * (1.0f - 0.5f * ny));

                const auto s = historyAt (x, y);

                // The curve keeps quiet partials barely visible; the loudest glow white. Higher trails use a
                // gentler curve and extra light, since their cyan-to-violet hues would otherwise sink into the sky.
                const auto up = 1.0f - ny;
                const auto strength = std::pow (s, 2.0f - 0.7f * up);
                c = mix (c, toRgb (trailColour (ny, fx / (float) w)), juce::jmin (1.0f, 1.1f * strength));
                c = mix (c, haze, 0.5f * s * s * s * s + 0.45f * up * strength);
                row[x].setARGB (0xff, (juce::uint8) c.r, (juce::uint8) c.g, (juce::uint8) c.b);
            }
        }
    }

    juce::Graphics g (frame);
    const auto fw = (float) w, fh = (float) h;

    for (const auto& b : blurs)
    {
        const auto t = b.phase + time * b.speed;
        const auto cx = ((t - std::floor (t)) * 1.8f - 0.4f) * fw;
        const auto cy = (b.y + 0.08f * std::sin (time * 0.11f + b.phase * 6.0f)) * fh;
        const auto r  = b.radius * fw;
        const auto colour = juce::Colour (b.colour);

        g.setGradientFill ({ colour.withAlpha (b.alpha), cx, cy, colour.withAlpha (0.0f), cx + r, cy, true });
        g.fillEllipse (cx - r, cy - r * 0.6f, r * 2.0f, r * 1.2f);
    }
}

void SpectrogramVisual::drawParticles (juce::Graphics& g) const
{
    for (const auto& p : particles)
    {
        // Fade in and out over the particle's life so nothing pops.
        const auto alpha = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, p.age / p.life));
        const auto r = p.size * 3.0f;

        g.setGradientFill ({ p.colour.withAlpha (0.14f * alpha), p.x, p.y, p.colour.withAlpha (0.0f), p.x + r, p.y, true });
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f * alpha));
        g.fillEllipse (p.x - p.size * 0.5f, p.y - p.size * 0.5f, p.size, p.size);
    }
}
