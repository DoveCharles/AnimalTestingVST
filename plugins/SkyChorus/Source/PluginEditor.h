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
    void resized() override;

private:
    juce::Image wash;
};

/** Three-way stereo mode switch: one latching button per mode, bound to the stereo choice parameter. */
class StereoSwitch final : public juce::Component
{
public:
    explicit StereoSwitch (juce::AudioProcessorValueTreeState&);

    void resized() override;

private:
    std::array<juce::TextButton, 3> buttons;
    juce::ParameterAttachment attachment;
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

    PluginProcessor& chorusProcessor;
    PedalLookAndFeel lookAndFeel;

    at::Knob speed, width, delayTime, mix;
    StereoSwitch stereoSwitch;
    Treadle treadle;
    juce::AudioProcessorValueTreeState::ButtonAttachment treadleAttachment;

    juce::Image wash;
    juce::Rectangle<int> ledArea;
    float ledPulse = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
