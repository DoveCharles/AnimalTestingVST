#include "SpectrogramVisual.h"
#include "PluginProcessor.h"

namespace
{
    struct Rgb { float r, g, b; };

    Rgb mix (Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

    constexpr Rgb skyTop    {  92.0f, 176.0f, 242.0f };
    constexpr Rgb skyBottom {  52.0f, 138.0f, 224.0f };
    constexpr Rgb haze      { 250.0f, 252.0f, 255.0f };

    /** Large, very faint colour blurs drifting left to right. */
    struct Blur { float y, radius, speed, phase, alpha; juce::uint32 colour; };

    constexpr Blur blurs[] {
        { 0.30f, 0.45f, 0.012f, 0.00f, 0.24f, 0xffd9b8ff },   // lavender
        { 0.70f, 0.55f, 0.008f, 0.40f, 0.20f, 0xffffc9d6 },   // blush
        { 0.50f, 0.40f, 0.015f, 0.75f, 0.18f, 0xffbff5dc },   // mint
        { 0.20f, 0.35f, 0.010f, 0.55f, 0.16f, 0xfffff1c2 },   // pale gold
        { 0.85f, 0.50f, 0.006f, 0.20f, 0.14f, 0xffc4e4ff },   // ice
    };

    constexpr Rgb cloudDark  { 34.0f, 92.0f, 196.0f };    // deeper indigo-blue patches
    constexpr Rgb cloudLight { 120.0f, 206.0f, 238.0f };  // paler teal patches

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

    constexpr float minHz = 30.0f, maxHz = 16000.0f;
    constexpr float floorDb = -78.0f;
}

SpectrogramVisual::SpectrogramVisual (PluginProcessor& p)
    : processor (p),
      vblank (this, [this] (double t) { tick (t); })
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void SpectrogramVisual::resized()
{
    const auto w = juce::jmax (1, getWidth()  / pixelSize);
    const auto h = juce::jmax (1, getHeight() / pixelSize);
    frame = juce::Image (juce::Image::ARGB, w, h, true);

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
    history.assign ((size_t) (w * h), 0.0f);
    head = 0;
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
    readAudio();

    if (frame.isValid())
    {
        // Columns advance with wall-clock time so the scroll speed is steady whatever the host buffer size.
        const auto due = std::floor (time * columnsPerSecond);
        for (int i = 0; columnsEmitted < due && i < 4; ++i, columnsEmitted += 1.0)
            pushColumn();
        columnsEmitted = juce::jmax (columnsEmitted, due - 1.0);
    }

    repaint();
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

        const auto db = juce::Decibels::gainToDecibels (mag * normalise, floorDb);
        const auto value = juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);

        // Blend with the previous column so the trail is soft rather than flickery.
        column[y] = 0.55f * value + 0.45f * previous[y];
    }
}

void SpectrogramVisual::paint (juce::Graphics& g)
{
    if (! frame.isValid())
        return;

    renderScene();

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat());

    // Fine film grain at full resolution, shifted every frame so it lives rather than sits.
    g.setTiledImageFill (grain, -random.nextInt (256), -random.nextInt (256), 0.07f);
    g.fillAll();
}

void SpectrogramVisual::renderScene()
{
    const auto w = frame.getWidth();
    const auto h = frame.getHeight();

    {
        juce::Image::BitmapData data (frame, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < h; ++y)
        {
            const auto ny = (float) y / (float) h;
            const auto skyBase = mix (skyTop, skyBottom, ny);
            auto* row = reinterpret_cast<juce::PixelARGB*> (data.getLinePointer (y));

            for (int x = 0; x < w; ++x)
            {
                // Each pitch has its own hue: red for lows up to violet for highs, drifting slightly
                // along the trail. The curve keeps quiet partials barely visible; the loudest glow white.
                // Slow cloudy noise drifting left to right breaks the flat blue into soft patches.
                const auto n = clouds ((float) x * 0.045f - time * 0.05f, (float) y * 0.06f + time * 0.012f);
                const auto sky = n < 0.5f ? mix (skyBase, cloudDark,  juce::jmin (1.0f, (0.5f - n) * 1.6f))
                                          : mix (skyBase, cloudLight, juce::jmin (1.0f, (n - 0.5f) * 1.4f));

                const auto s = historyAt (x, y);
                const auto hue = 0.8f * (1.0f - ny) + 0.06f * (float) x / (float) w;
                const auto rainbow = juce::Colour::fromHSV (hue, 0.6f, 1.0f, 1.0f);
                auto c = mix (sky, { (float) rainbow.getRed(), (float) rainbow.getGreen(), (float) rainbow.getBlue() },
                              juce::jmin (1.0f, 1.1f * s * s));
                c = mix (c, haze, 0.5f * s * s * s * s);
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
