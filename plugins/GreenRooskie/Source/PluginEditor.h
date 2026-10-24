#pragma once

#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

/** Chrome stomp switch bound to the bypass parameter. Its toggle state is "bypassed". */
class FootSwitch final : public juce::Button
{
public:
    FootSwitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

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
    PedalLookAndFeel lookAndFeel;

    at::Knob volume, tone, sustain;
    FootSwitch footSwitch;
    juce::AudioProcessorValueTreeState::ButtonAttachment footSwitchAttachment;

    juce::Rectangle<int> ledArea, titleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
