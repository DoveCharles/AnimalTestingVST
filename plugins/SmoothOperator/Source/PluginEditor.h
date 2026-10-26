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
    /** Input spectrum with the live cut hanging under it. */
    struct Display : juce::Component
    {
        void paint (juce::Graphics&) override;

        std::array<float, ResonanceSuppressor::displayBands> spectrum {}, reduction {};
    };

    void timerCallback() override;
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    PluginProcessor& smoothOperator;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    Display display;
    at::Knob depth, sharpness, selectivity, attack, release;
    std::array<std::unique_ptr<at::Knob>, ResonanceSuppressor::numWeights> weights;
    at::Knob mix, output;

    juce::ComboBox stereo;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment stereoAttachment;
    juce::TextButton delta { "DELTA" };
    juce::AudioProcessorValueTreeState::ButtonAttachment deltaAttachment;

    juce::Rectangle<int> detectArea, weightArea, outputArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
