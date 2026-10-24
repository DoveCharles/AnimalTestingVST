#pragma once

#include <at_shared/at_shared.h>

class PluginProcessor;

/** Scrolling spectrogram of the plugin's output, styled like a datamoshed VHS frame: wide
    pixels, posterized teal / periwinkle / navy with magenta streaks on the hottest partials,
    row bands that tear sideways, and scanlines. Newest audio enters on the left and flows right.
    When it's quiet a faint drifting noise field keeps the picture alive. */
class SpectrogramVisual final : public juce::Component
{
public:
    explicit SpectrogramVisual (PluginProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Glitch
    {
        int y = 0, height = 1, shift = 0, framesLeft = 0;
        bool rgbSplit = false;
    };

    void tick (double timestampSec);
    void readAudio();
    void pushColumn();
    void renderScene();
    void spawnGlitches();
    void applyGlitches();

    float historyAt (int column, int row) const noexcept;

    PluginProcessor& processor;
    juce::VBlankAttachment vblank;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float columnsPerSecond = 30.0f;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> ((size_t) fftSize);   // latest fftSize samples
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2);
    std::vector<float> pullBuffer = std::vector<float> (8192);

    // Spectrogram history, column-major, values 0..1. Column `head` is the newest.
    std::vector<float> history;
    int head = 0;
    double columnsEmitted = 0.0;

    juce::Image frame;
    juce::Random random { 0x404 };
    std::vector<Glitch> glitches;
    std::vector<juce::PixelARGB> rowScratch;

    double startTime = -1.0;
    float time = 0.0f;
    float lastAudioTime = 0.0f;
    float level = 0.0f;

    static constexpr int pixelWidth = 5, pixelHeight = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramVisual)
};

/** Translucent smoked-glass panel to sit controls on top of the visual. */
class GlassPanel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff040a1c).withAlpha (0.62f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xff8fd8ff).withAlpha (0.25f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
};
