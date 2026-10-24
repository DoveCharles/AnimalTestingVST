#pragma once

#include <at_shared/at_shared.h>

class PluginProcessor;

/** Scrolling spectrogram of the plugin's output on a soft sky-blue field: sound shows as smooth
    rainbow trails coloured by pitch, with faint coloured blurs drifting through. Newest audio enters on the left and
    flows right. Rendered small and scaled up smoothly so everything stays soft. */
class SpectrogramVisual final : public juce::Component
{
public:
    explicit SpectrogramVisual (PluginProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void tick (double timestampSec);
    void readAudio();
    void pushColumn();
    void renderScene();

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

    double startTime = -1.0;
    float time = 0.0f;
    float lastAudioTime = 0.0f;

    static constexpr int pixelSize = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramVisual)
};

/** Translucent frosted-glass panel to sit controls on top of the visual. */
class GlassPanel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff0d3a63).withAlpha (0.28f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
};
