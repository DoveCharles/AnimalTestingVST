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

    /** Water strider seen from above, head up, in units of its size. Short front legs held up by the
        head; long middle legs reaching forward and out, which row it along; hind legs trailing back.
        Each leg is listed for the left side, from where it joins the body out to its foot. */
    struct Leg { juce::Point<float> base, knee, foot; };

    constexpr Leg frontLeg  { { -0.02f,  -0.15f }, { -0.075f, -0.17f }, { -0.06f, -0.25f } };
    constexpr Leg middleLeg { { -0.025f, -0.07f }, { -0.2f,   -0.08f }, { -0.46f, -0.38f } };
    constexpr Leg hindLeg   { { -0.02f,  -0.02f }, { -0.13f,   0.12f }, { -0.27f,  0.50f } };

    constexpr float strokeOut = 0.03f;       // seconds for the middle legs to sweep back on a hop
    constexpr float strokeBack = 0.2f;       // seconds to reach forward again
    constexpr float strokeAngle = 1.15f;     // radians the middle legs sweep through
    constexpr float hopDamping = 9.0f;       // how quickly the push-off burst dies away, per second
    constexpr float glideDamping = 2.2f;     // how slowly the coast after it eases to a stop, per second
    constexpr float landAfter = 0.25f;       // seconds after pushing off that it touches down again
    constexpr float glideShare = 0.6f;       // how far it coasts, as a share of the burst's distance

    /** Speed along its heading a given time into a hop of the given distance: a quick burst from the
        push-off, then a long coast that eases out. */
    float hopSpeed (float distance, float sinceHop) noexcept
    {
        return distance * (hopDamping * std::exp (-hopDamping * sinceHop)
                           + glideShare * glideDamping * std::exp (-glideDamping * sinceHop));
    }

    /** How far back the middle legs have swept, 0..1, a given time after pushing off. */
    float strokeAmount (float sinceHop) noexcept
    {
        if (sinceHop < strokeOut)
            return smoothstep (0.0f, 1.0f, sinceHop / strokeOut);
        return 1.0f - smoothstep (0.0f, 1.0f, (sinceHop - strokeOut) / strokeBack);
    }

    /** The strider's legs and body for a stroke amount, in units of its size, with where each foot
        dimples the water (the front legs are held clear of it). */
    struct StriderPose { juce::Path legs, body; std::vector<juce::Point<float>> feet; };

    StriderPose striderPose (float stroke)
    {
        StriderPose pose;

        for (auto side : { 1.0f, -1.0f })
        {
            auto flip = [side] (juce::Point<float> p) { return juce::Point<float> (p.x * side, p.y); };

            auto addLeg = [&] (Leg leg, float sweep, bool onWater)
            {
                // Sweeping back turns the leg about its base, towards the tail.
                const auto turn = juce::AffineTransform::rotation (-sweep, leg.base.x, leg.base.y);
                pose.legs.startNewSubPath (flip (leg.base));
                pose.legs.lineTo (flip (leg.knee.transformedBy (turn)));
                pose.legs.lineTo (flip (leg.foot.transformedBy (turn)));
                if (onWater)
                    pose.feet.push_back (flip (leg.foot.transformedBy (turn)));
            };

            addLeg (frontLeg, 0.0f, false);
            addLeg (middleLeg, strokeAngle * stroke, true);
            addLeg (hindLeg, 0.0f, true);

            pose.legs.startNewSubPath (flip ({ -0.012f, -0.23f }));   // antenna
            pose.legs.lineTo (flip ({ -0.04f, -0.3f }));
            pose.legs.lineTo (flip ({ -0.05f, -0.36f }));
        }

        pose.body.addEllipse (juce::Rectangle<float> (0.06f, 0.07f).withCentre ({ 0.0f, -0.2f }));    // head
        pose.body.addEllipse (juce::Rectangle<float> (0.085f, 0.18f).withCentre ({ 0.0f, -0.08f }));  // thorax
        pose.body.addEllipse (juce::Rectangle<float> (0.065f, 0.25f).withCentre ({ 0.0f, 0.1f }));    // abdomen
        return pose;
    }

    constexpr float striderSize = 48.0f;   // pixels from middle feet to hind feet, front to back
    /** Places a strider's pose (in units of its size, head up) on screen. */
    juce::AffineTransform placeStrider (float x, float y, float size, float heading)
    {
        return juce::AffineTransform::scale (size).rotated (heading + juce::MathConstants<float>::halfPi).translated (x, y);
    }

    constexpr float rippleLife = 0.8f;     // seconds for a packet of ripples to spread and fade
    constexpr float rippleReach = 30.0f;   // pixels the leading crest of a full-size packet travels
    constexpr int rippleCrests = 4;        // fine crests in each packet

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
                          0.0f, 0.0f, 1.0f + 4.0f * random.nextFloat(), 0.0f, 10.0f });
    striders.back().fromHeading = striders.back().toHeading = striders.back().heading;
}

void SpectrogramVisual::spawnRipple (float x, float y, float size, float heading, float bias, float distance, float sinceHop)
{
    if (ripples.size() < maxRipples)
        ripples.push_back ({ x, y, size, 0.0f, heading, bias, distance, sinceHop });
}

void SpectrogramVisual::updateStriders (float dt)
{
    const auto height = (float) juce::jmax (1, getHeight());

    // Keep a few on the river: a new one paddles in from the left now and then.
    if (striders.size() < numStriders && random.nextFloat() < dt * 0.5f)
        spawnStrider (-40.0f);

    for (auto& s : striders)
    {
        // Turns towards the new heading during the burst, so the hop curves, then coasts on.
        const auto landed = s.sinceHop < landAfter && s.sinceHop + dt >= landAfter;
        s.sinceHop += dt;
        s.heading = s.fromHeading + (s.toHeading - s.fromHeading) * (1.0f - std::exp (-hopDamping * s.sinceHop));
        const auto speed = hopSpeed (s.distance, s.sinceHop);
        s.x += (surfaceDrift (s.y / height) + std::cos (s.heading) * speed) * dt;
        s.y = juce::jlimit (0.1f * height, 0.9f * height, s.y + std::sin (s.heading) * speed * dt);

        // Touching down, each foot on the water sends out a small, even packet of ripples that slides
        // along under it for the rest of the glide, so the rings stay centred on the feet.
        if (landed)
            for (auto foot : striderPose (strokeAmount (s.sinceHop)).feet)
            {
                const auto p = foot.transformedBy (placeStrider (s.x, s.y, s.size, s.heading));
                spawnRipple (p.x, p.y, 0.55f, s.toHeading, 0.0f, s.distance, s.sinceHop);
            }

        s.untilHop -= dt;
        if (s.untilHop > 0.0f)
            continue;

        // Push off with a quick stroke of the middle legs and glide away. Ripples spread from the middle
        // feet, strongest behind it where the legs shoved the water.
        s.untilHop = 1.5f + 5.0f * random.nextFloat();
        s.fromHeading = s.heading;
        s.toHeading = juce::jlimit (-1.1f, 1.1f, s.heading + 1.4f * (random.nextFloat() - 0.5f));
        s.distance = 45.0f + 35.0f * random.nextFloat();
        s.sinceHop = 0.0f;

        const auto feet = striderPose (0.0f).feet;   // middle, hind for each side
        for (auto i : { 0, 2 })
        {
            const auto p = feet[(size_t) i].transformedBy (placeStrider (s.x, s.y, s.size, s.heading));
            spawnRipple (p.x, p.y, 1.0f, s.heading, 0.75f, 0.0f, 0.0f);
        }
    }

    striders.erase (std::remove_if (striders.begin(), striders.end(),
                                    [this] (const Strider& s) { return s.x > (float) getWidth() + 50.0f; }),
                    striders.end());

    for (auto& r : ripples)
    {
        r.age += dt;
        r.sinceHop += dt;
        const auto speed = hopSpeed (r.distance, r.sinceHop);
        r.x += (surfaceDrift (r.y / height) + std::cos (r.heading) * speed) * dt;   // the rings ride the current with the striders
        r.y += std::sin (r.heading) * speed * dt;
    }

    ripples.erase (std::remove_if (ripples.begin(), ripples.end(),
                                   [] (const Ripple& r) { return r.age >= rippleLife; }),
                   ripples.end());
}

void SpectrogramVisual::drawStriders (juce::Graphics& g) const
{
    const auto toScreen = juce::AffineTransform::scale ((float) getWidth()  / (float) frame.getWidth(),
                                                         (float) getHeight() / (float) frame.getHeight());

    for (const auto& r : ripples)
    {
        // A packet of fine crests that spreads quickly then slows, drawing apart as it goes, and fades.
        const auto t = r.age / rippleLife;
        const auto front = rippleReach * r.size * (1.0f - (1.0f - t) * (1.0f - t));
        const auto spacing = 2.5f + 2.0f * t;
        const auto fade = std::pow (1.0f - t, 1.5f) * juce::jmin (1.0f, r.age * 20.0f) * (0.6f + 0.4f * r.size);
        const juce::Point<float> centre { r.x, r.y };

        for (int i = 0; i < rippleCrests; ++i)
        {
            const auto radius = front - spacing * (float) i;
            if (radius <= 1.0f)
                continue;

            // Brightest mid-packet, fading to the leading and trailing crests, and to nothing while a crest
            // is still tiny so a fresh packet doesn't look like a bullseye.
            const auto alpha = fade * std::sin (juce::MathConstants<float>::pi * ((float) i + 0.5f) / (float) rippleCrests)
                             * smoothstep (1.0f, 7.0f, radius);

            // Each crest bends the water seen through it, alternately pushing the picture out and pulling it in.
            {
                juce::Graphics::ScopedSaveState state (g);
                juce::Path band;
                band.addEllipse (juce::Rectangle<float> (2.0f * radius + 3.0f, 2.0f * radius + 3.0f).withCentre (centre));
                band.addEllipse (juce::Rectangle<float> (2.0f * radius - 3.0f, 2.0f * radius - 3.0f).withCentre (centre));
                band.setUsingNonZeroWinding (false);
                g.reduceClipRegion (band);
                g.setOpacity (alpha);
                const auto bend = 1.0f + (i % 2 == 0 ? 3.0f : -3.0f) / radius;
                g.drawImageTransformed (frame, toScreen.scaled (bend, bend, centre.x, centre.y));
            }

            // Then the lit crest with its trough just inside, brighter on the side the water was shoved
            // towards: behind the strider for a push-off, all round for a landing. JUCE arc angles run
            // clockwise from 12 o'clock, a quarter turn on from the heading's.
            constexpr int segments = 24;
            for (int j = 0; j < segments; ++j)
            {
                const auto a0 = juce::MathConstants<float>::twoPi * (float) j / (float) segments;
                const auto a1 = a0 + juce::MathConstants<float>::twoPi / (float) segments;
                const auto towards = 0.5f * (a0 + a1) - juce::MathConstants<float>::halfPi;
                const auto weight = 1.0f - r.bias * 0.5f * (1.0f + std::cos (towards - r.heading));

                juce::Path crest, trough;
                crest.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, a0, a1, true);
                trough.addCentredArc (centre.x, centre.y, radius - 1.4f, radius - 1.4f, 0.0f, a0, a1, true);
                g.setColour (waterShadow.withAlpha (0.2f * alpha * weight));
                g.strokePath (trough, juce::PathStrokeType (1.3f));
                g.setColour (juce::Colours::white.withAlpha (0.65f * alpha * weight));
                g.strokePath (crest, juce::PathStrokeType (1.1f));
            }
        }
    }

    for (const auto& s : striders)
    {
        const auto pose = striderPose (strokeAmount (s.sinceHop));
        auto place = [&] (float x, float y) { return placeStrider (x, y, s.size, s.heading); };

        // Each foot on the water presses a dimple into the surface that bends the light like a small lens:
        // the water under it looks shrunk, ringed by a faint glint where the surface flattens out.
        for (auto foot : pose.feet)
        {
            const auto p = foot.transformedBy (place (s.x, s.y));
            const auto r = s.size * 0.1f;
            const auto dimple = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p);

            {
                juce::Graphics::ScopedSaveState state (g);
                juce::Path clip;
                clip.addEllipse (dimple);
                g.reduceClipRegion (clip);
                g.drawImageTransformed (frame, toScreen.scaled (0.55f, 0.55f, p.x, p.y));
            }

            // A soft bright ring where the surface curves back up to flat.
            const auto outer = r + 2.0f;
            juce::ColourGradient rim (juce::Colours::white.withAlpha (0.0f), p, juce::Colours::white.withAlpha (0.0f), p.translated (outer, 0.0f), true);
            rim.addColour (0.6f * r / outer, juce::Colours::white.withAlpha (0.0f));
            rim.addColour (r / outer, juce::Colours::white.withAlpha (0.18f));
            g.setGradientFill (rim);
            g.fillEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (p));
        }

        // Its shadow on the water.
        g.setColour (waterShadow.withAlpha (0.3f));
        g.strokePath (pose.legs, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      place (s.x + 2.0f, s.y + 3.0f));
        g.fillPath (pose.body, place (s.x + 2.0f, s.y + 3.0f));

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (pose.legs, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      place (s.x, s.y));
        g.fillPath (pose.body, place (s.x, s.y));
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
