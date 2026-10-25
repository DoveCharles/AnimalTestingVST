#pragma once

#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

/** The lower half of the pedal, bound to the bypass parameter: the nameplate and a chrome
    stomp switch. Clicking anywhere on it stomps; its toggle state is "bypassed". */
class Footswitch final : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

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
    juce::String decayText (double decayPercent);

    PedalLookAndFeel lookAndFeel;

    at::Knob level, liveliness, decay, type;
    juce::TextButton tails { "TAILS" };
    juce::AudioProcessorValueTreeState::ButtonAttachment tailsAttachment;
    Footswitch footswitch;
    juce::AudioProcessorValueTreeState::ButtonAttachment footswitchAttachment;

    juce::Rectangle<int> ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
