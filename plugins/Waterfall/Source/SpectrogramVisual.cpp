#include "SpectrogramVisual.h"
#include "PluginProcessor.h"

namespace
{
    float hash (int x, int y)
    {
        auto h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) (h ^ (h >> 16)) * (1.0f / 4294967295.0f);
    }

    float valueNoise (float x, float y)
    {
        const auto xi = (int) std::floor (x), yi = (int) std::floor (y);
        auto fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);

        const auto top    = juce::jmap (fx, hash (xi, yi),     hash (xi + 1, yi));
        const auto bottom = juce::jmap (fx, hash (xi, yi + 1), hash (xi + 1, yi + 1));
        return juce::jmap (fy, top, bottom);
    }

    float fbm (float x, float y)
    {
        return 0.55f * valueNoise (x, y)
             + 0.30f * valueNoise (x * 2.03f + 17.0f, y * 2.03f)
             + 0.15f * valueNoise (x * 4.11f, y * 4.11f + 31.0f);
    }

    struct Rgb { float r, g, b; };

    Rgb mix (Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

    // Blue normal-map-ish ramp from silence to loudest, plus a magenta accent.
    constexpr Rgb ramp[] {
        {  22.0f,  30.0f,  92.0f },   // navy
        {  58.0f,  78.0f, 170.0f },   // deep blue
        {  92.0f, 112.0f, 202.0f },   // periwinkle
        {  82.0f, 196.0f, 186.0f },   // teal
        { 196.0f, 242.0f, 236.0f },   // pale cyan
    };
    constexpr Rgb magenta { 214.0f, 68.0f, 142.0f };

    Rgb rampColour (float t)
    {
        const auto scaled = juce::jlimit (0.0f, 0.9999f, t) * (float) (std::size (ramp) - 1);
        const auto i = (size_t) scaled;
        return mix (ramp[i], ramp[i + 1], scaled - (float) i);
    }

    constexpr int posterizeLevels = 9;
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
    const auto w = juce::jmax (1, getWidth()  / pixelWidth);
    const auto h = juce::jmax (1, getHeight() / pixelHeight);
    frame = juce::Image (juce::Image::ARGB, w, h, true);
    rowScratch.resize ((size_t) w);
    history.assign ((size_t) (w * h), 0.0f);
    head = 0;
    glitches.clear();
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
    float sumSquares = 0.0f;
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
            sumSquares += pullBuffer[(size_t) i] * pullBuffer[(size_t) i];
        }
        total += n;
    }

    if (total > 0)
    {
        lastAudioTime = time;
    }
    else if (time - lastAudioTime > 0.25f)
    {
        // Host stopped calling us (transport stopped / bypassed): let the picture fall silent.
        std::fill (ring.begin(), ring.end(), 0.0f);
    }

    // Fast attack, slow release, so the glitching swells with the sound.
    const auto target = total > 0 ? juce::jlimit (0.0f, 1.0f, std::sqrt (sumSquares / (float) total) * 4.0f) : 0.0f;
    level = target > level ? target : level * 0.95f + target * 0.05f;
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
        column[y] = juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
    }
}

void SpectrogramVisual::paint (juce::Graphics& g)
{
    if (! frame.isValid())
        return;

    renderScene();
    spawnGlitches();
    applyGlitches();

    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat());

    // Scanlines, one per scaled-up pixel row.
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (int y = 0; y < getHeight(); y += pixelHeight)
        g.fillRect (0, y, getWidth(), 1);
}

void SpectrogramVisual::renderScene()
{
    const auto w = frame.getWidth();
    const auto h = frame.getHeight();
    const auto invW = 1.0f / (float) w;
    const auto invH = 1.0f / (float) h;

    // The ambient field drifts right at the same speed as the spectrogram scrolls.
    const auto scroll = time * columnsPerSecond * invW * 1.6f;
    const auto tearAmount = 3.0f + 18.0f * level;   // in columns
    const auto pinkThreshold = 0.66f - 0.1f * level;
    const auto frameSeed = (int) (time * 24.0f);    // film grain changes at 24 fps

    juce::Image::BitmapData data (frame, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        const auto ny = (float) y * invH;

        // Bands of rows slide sideways together: the stair-stepped datamosh edges.
        const auto band = (float) (y / 3);
        const auto tear = (int) std::round ((valueNoise (band * 0.45f, time * 0.5f) - 0.5f) * tearAmount
                                            + (valueNoise (band * 0.08f + 9.0f, time * 0.15f) - 0.5f) * 8.0f);

        auto* row = reinterpret_cast<juce::PixelARGB*> (data.getLinePointer (y));

        for (int x = 0; x < w; ++x)
        {
            const auto spectrum = historyAt (juce::jmax (0, x + tear), y);

            const auto nx = (float) (x + tear) * invW;
            const auto u = nx * 1.6f - scroll + ny * 0.5f;
            const auto v = ny * 4.0f;
            const auto ambient = fbm (u, v + time * 0.04f);

            // Idle, the field alone reaches periwinkle / teal; sound pushes it up to cyan.
            const auto value = juce::jlimit (0.0f, 1.0f, (ambient - 0.2f) * 1.05f + spectrum * 0.8f);
            const auto stepped = std::floor (value * posterizeLevels) / (float) (posterizeLevels - 1);
            auto c = rampColour (stepped);

            // Magenta streaks through the loud partials.
            const auto pink = fbm (u * 2.2f + 40.0f, v * 2.5f);
            if (pink > pinkThreshold + 0.08f * (1.0f - spectrum))
                c = mix (c, magenta, juce::jlimit (0.0f, 0.85f, std::floor ((pink - pinkThreshold) * 20.0f) / 3.0f));

            const auto grain = (hash (x + frameSeed * 131, y) - 0.5f) * 12.0f;

            row[x].setARGB (0xff,
                            (juce::uint8) juce::jlimit (0.0f, 255.0f, c.r + grain),
                            (juce::uint8) juce::jlimit (0.0f, 255.0f, c.g + grain),
                            (juce::uint8) juce::jlimit (0.0f, 255.0f, c.b + grain));
        }
    }
}

void SpectrogramVisual::spawnGlitches()
{
    const auto h = frame.getHeight();
    const auto chance = 0.06f + 0.45f * level;

    if (random.nextFloat() < chance && glitches.size() < 10)
    {
        Glitch gl;
        gl.height = 1 + random.nextInt (juce::jmax (2, h / 6));
        gl.y = random.nextInt (juce::jmax (1, h - gl.height));
        gl.shift = (random.nextBool() ? 1 : -1) * (2 + random.nextInt (juce::jmax (3, (int) (frame.getWidth() * (0.05f + 0.25f * level)))));
        gl.framesLeft = 2 + random.nextInt (12);
        gl.rgbSplit = random.nextFloat() < 0.4f;
        glitches.push_back (gl);
    }
}

void SpectrogramVisual::applyGlitches()
{
    const auto w = frame.getWidth();
    juce::Image::BitmapData data (frame, juce::Image::BitmapData::readWrite);

    for (auto& gl : glitches)
    {
        const auto split = gl.rgbSplit ? 1 + std::abs (gl.shift) / 6 : 0;

        for (int y = gl.y; y < juce::jmin (gl.y + gl.height, frame.getHeight()); ++y)
        {
            auto* row = reinterpret_cast<juce::PixelARGB*> (data.getLinePointer (y));
            std::copy (row, row + w, rowScratch.begin());

            auto at = [&] (int x) -> const juce::PixelARGB& { return rowScratch[(size_t) (((x % w) + w) % w)]; };

            for (int x = 0; x < w; ++x)
            {
                const auto src = x - gl.shift;
                row[x].setARGB (0xff, at (src + split).getRed(), at (src).getGreen(), at (src - split).getBlue());
            }
        }

        --gl.framesLeft;
    }

    glitches.erase (std::remove_if (glitches.begin(), glitches.end(), [] (const Glitch& gl) { return gl.framesLeft <= 0; }),
                    glitches.end());
}
