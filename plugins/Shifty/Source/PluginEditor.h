#pragma once

#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

/** The footswitch: a squishy pill bound to the bypass parameter, pink and glowing while the effect
    is on, grey while bypassed. Its toggle state is "bypassed". */
class Blob final : public juce::Button
{
public:
    Blob() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawTitle (juce::Graphics&);
    juce::String fineText (double finePercent);

    PedalLookAndFeel lookAndFeel;

    at::Knob balance, feedback, fine, mode;
    Blob blob;
    juce::AudioProcessorValueTreeState::ButtonAttachment blobAttachment;

    // Drives the sparkles' twinkle; they dim while bypassed.
    float clock = 0.0f, awake = 1.0f;

    juce::Rectangle<int> titleArea, cardArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
