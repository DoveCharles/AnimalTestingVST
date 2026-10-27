#pragma once

#include <at_shared/at_shared.h>

class PluginProcessor;

/** Scrolling spectrogram of the plugin's output, styled as a digital river. Sound shows as smooth
    rainbow trails coloured by pitch, carried left to right by a current of streaky, colour-varied noise
    with faint caustics. Notes shed faint particles into the current. Faint coloured blurs and a fine
    film grain sit over everything. Rendered small and scaled up smoothly so everything stays soft.
    White water striders drift downstream on the surface, now and then hopping and sending out
    rings of ripples that the current carries away. */
class SpectrogramVisual final : public juce::Component
{
public:
    explicit SpectrogramVisual (PluginProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Advances the visual to the given time. Called on every vblank, and by tools/WaterfallPreview
        to render offline. */
    void tick (double timestampSec);

    /** Called at the end of every tick with the visual's time in seconds, so the editor can animate
        its controls in step with the visual (also when rendered offline). */
    std::function<void (float)> onTick;

private:
    void readAudio();
    void pushColumn();
    void renderScene();
    void updateParticles (float dt);
    void drawParticles (juce::Graphics&) const;
    void spawnParticle (float x, float y, juce::Colour);
    void updateStriders (float dt);
    void drawStriders (juce::Graphics&) const;
    void spawnStrider (float x);
    void spawnRipple (float x, float y, float size, int crests);

    float historyAt (int column, int row) const noexcept;

    /** Light carried along by the current, in component pixels. */
    struct Particle { float x, y, vx, phase, size, age, life; juce::Colour colour; };

    /** A water strider on the surface, in component pixels. Heading is in radians, 0 facing downstream.
        Once aimed, it has turned to face its next hop and jumps when untilHop runs out. */
    struct Strider { float x, y, size, heading, untilHop; bool aimed; };

    /** Concentric rings spreading on the surface, their centre drifting downstream with the striders. */
    struct Ripple { float x, y, size, age; int crests; };

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
    std::vector<float> columnValues;   // the column being analysed, before smoothing
    double columnsEmitted = 0.0;

    juce::Image frame;
    juce::Image grain;   // full-resolution tile, re-positioned every frame
    juce::Random random;
    std::vector<Particle> particles;
    std::vector<Strider> striders;
    std::vector<Ripple> ripples;

    double startTime = -1.0;
    float time = 0.0f;
    float lastAudioTime = 0.0f;
    float lastTickTime = 0.0f;

    static constexpr int pixelSize = 4;
    static constexpr size_t maxParticles = 220;
    static constexpr size_t numStriders = 3;
    static constexpr size_t maxRipples = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramVisual)
};
