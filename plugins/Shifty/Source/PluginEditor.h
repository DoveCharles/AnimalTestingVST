#pragma once

#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

/** The big hinged pedal plate that covers the lower half, bound to the bypass parameter.
    It carries the nameplate and the rubber pad; its toggle state is "bypassed". */
class Treadle final : public juce::Button
{
public:
    Treadle() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String fineText (double finePercent);

    PedalLookAndFeel lookAndFeel;

    at::Knob balance, feedback, fine, mode;
    Treadle treadle;
    juce::AudioProcessorValueTreeState::ButtonAttachment treadleAttachment;

    juce::Rectangle<int> ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
