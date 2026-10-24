#pragma once

#include "PluginProcessor.h"
#include "SpectrogramVisual.h"
#include "WaterfallLookAndFeel.h"
#include "Overlays.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void resized() override;

private:
    /** Moves the chord read-out and lets each knob glow with its part of the sound. */
    void animate (float time);

    PluginProcessor& waterfall;
    WaterfallLookAndFeel lookAndFeel;

    SpectrogramVisual visual;
    KnobShadows shadows;
    ChordReadout chordReadout;
    at::Knob key, chord, brightness, decay, mix;

    std::array<float, 5> glow {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
