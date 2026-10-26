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
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    at::Knob drive, tone, filter, output, wow, flutter, wear, noise, mechanical;

    juce::TextButton boost { "BOOST" }, toneEnable { "TONE" }, stereoWidth { "WIDE" }, tapeStop { "STOP" };
    ButtonAttachment boostAttachment, toneEnableAttachment, stereoWidthAttachment, tapeStopAttachment;

    juce::ComboBox stopSpeed, catchUp;
    ComboBoxAttachment stopSpeedAttachment, catchUpAttachment;

    juce::Rectangle<int> driveArea, toneArea, outputArea, motionArea, noiseArea, stopArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
