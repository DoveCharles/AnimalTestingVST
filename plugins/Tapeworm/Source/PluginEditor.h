#pragma once

#include "PluginProcessor.h"
#include "DeckLookAndFeel.h"
#include "ReelDeck.h"

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

    DeckLookAndFeel lookAndFeel;
    juce::Image plate;

    at::PresetBar presetBar;
    ReelDeck reels;
    at::Knob speed, drive, tone, filter, output, wow, flutter, wear, noise, mechanical;

    juce::TextButton boost { "BOOST" }, toneEnable { "TONE" }, stereoWidth { "WIDE" }, tapeStop { "STOP" };
    ButtonAttachment boostAttachment, toneEnableAttachment, stereoWidthAttachment, tapeStopAttachment;

    juce::ComboBox stopSpeed, catchUp;
    ComboBoxAttachment stopSpeedAttachment, catchUpAttachment;

    juce::Rectangle<int> faceplate, header, transportArea, driveArea, toneArea, outputArea, motionArea, noiseArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
