#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A labelled choice box bound to a parameter. */
    struct Choice : juce::Component
    {
        Choice (PluginProcessor&, const char* paramId, const juce::String& caption);
        void resized() override;

        juce::Label label;
        juce::ComboBox box;
        juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
    };

    void timerCallback() override;
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    PluginProcessor& voiceBox;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    Choice mode, key, scale;
    at::Knob pitch, formant, retune, gender;
    at::Knob chest, air, breath, drive;
    at::Knob vibRate, vibDepth, vibDelay;
    at::Knob mix, output;
    juce::String readout;

    juce::Rectangle<int> readoutArea, shiftArea, characterArea, vibratoArea, outputArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
