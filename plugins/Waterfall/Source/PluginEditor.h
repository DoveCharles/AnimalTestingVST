#pragma once

#include "PluginProcessor.h"
#include "SpectrogramVisual.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void resized() override;

private:
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    SpectrogramVisual visual;
    GlassPanel controlsPanel;
    at::Knob key, chord, brightness, decay, mix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
