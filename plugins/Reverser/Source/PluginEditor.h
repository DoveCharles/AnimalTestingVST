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
    void styleSwitch (juce::TextButton&);

    PedalLookAndFeel lookAndFeel;

    // Time and Division share a spot; Sync decides which one shows.
    at::Knob time, division, repeats, tone, mix;

    std::array<juce::TextButton, ReverseModes::numModes> modeButtons;
    juce::ParameterAttachment modeAttachment;

    juce::TextButton sync { "SYNC" };
    juce::ParameterAttachment syncAttachment;

    juce::TextButton tails { "TAILS" };
    juce::AudioProcessorValueTreeState::ButtonAttachment tailsAttachment;

    Footswitch footswitch;
    juce::AudioProcessorValueTreeState::ButtonAttachment footswitchAttachment;

    juce::Rectangle<int> ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
