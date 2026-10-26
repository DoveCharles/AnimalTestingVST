#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;

    juce::Label voicingLabel;
    juce::ComboBox voicing;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment voicingAttachment;

    at::Knob hardness, unison, decay, body, volume;

    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
