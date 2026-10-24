#pragma once

#include <at_shared/at_shared.h>

class PluginProcessor;

/** Scrolling spectrogram of the plugin's output, styled as a digital river. Sound shows as smooth
    rainbow trails coloured by pitch that ripple with loudness, carried left to right by a current of
    streaky, colour-varied noise with faint caustics. Notes shed glowing particles into the current, and
    the trails dissolve into soft pixel squares at the right edge. Faint coloured blurs and a fine film
    grain sit over everything. Rendered small and scaled up smoothly so everything stays soft. */
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
    void updateParticles (float dt);
    void drawParticles (juce::Graphics&) const;
    void drawDigitalEdge (juce::Graphics&) const;
    void spawnParticle (float x, float y, juce::Colour);

    float historyAt (int column, int row) const noexcept;
    float sampleHistory (float column, float row) const noexcept;

    /** Light carried along by the current, in component pixels. */
    struct Particle { float x, y, vx, phase, size, age, life; juce::Colour colour; };

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
    juce::Image grain;   // full-resolution tile, re-positioned every frame
    juce::Random random;
    std::vector<Particle> particles;
    std::vector<float> columnEnergy;   // loudest value per frame column, drives the ripples

    double startTime = -1.0;
    float time = 0.0f;
    float lastAudioTime = 0.0f;
    float lastTickTime = 0.0f;

    static constexpr int pixelSize = 4;
    static constexpr size_t maxParticles = 220;

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
