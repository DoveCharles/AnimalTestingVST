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
    /** Gain reduction per band and at the limiter, hanging from the top. */
    struct Meters : juce::Component
    {
        void paint (juce::Graphics&) override;

        std::array<float, Maximizer::numBands + 1> reduction {};
    };

    void timerCallback() override;
    void showMode (int index);

    PluginProcessor& goodBoy;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    at::Knob amount, output;
    std::array<juce::TextButton, 4> modeButtons;
    juce::ParameterAttachment modeAttachment;
    Meters meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
