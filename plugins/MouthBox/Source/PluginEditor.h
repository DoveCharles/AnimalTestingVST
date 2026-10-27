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
    /** One drum pad: lights up on its hits, with its note, a Train button and a Clear button. */
    struct Pad : juce::Component
    {
        Pad (PluginProcessor&, int slot);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        PluginProcessor& processor;
        const int slot;
        float flash = 0.0f;
        int velocity = 0, examples = 0;

        juce::ComboBox note;
        juce::AudioProcessorValueTreeState::ComboBoxAttachment noteAttachment;
        juce::TextButton train { "Train" }, clear { "Clear" };
        juce::Rectangle<int> face;
    };

    void timerCallback() override;

    PluginProcessor& mouthBox;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    std::vector<std::unique_ptr<Pad>> pads;
    at::Knob threshold, listen, gap, dynamics, monitor;

    int seenHits = 0;
    float meter = 0.0f;
    juce::Rectangle<int> meterArea, statusArea, padsArea, knobsArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
