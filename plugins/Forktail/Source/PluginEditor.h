#pragma once

#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

/** A bat-lever toggle for a choice parameter: one position per choice, labelled above.
    Clicking a position moves the lever there. */
class LeverSwitch final : public juce::Component
{
public:
    LeverSwitch (juce::RangedAudioParameter& parameter, const juce::StringArray& positionLabels);

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray labels;
    int position = 0;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeverSwitch)
};

/** The chrome footswitch, bound to the bypass parameter. In Latch mode a press toggles it;
    in Momentary mode the effect is on only while the switch is held. */
class Stompswitch final : public juce::Component
{
public:
    Stompswitch (juce::RangedAudioParameter& bypass, std::atomic<float>& switchMode);

    std::function<void()> onChange;

    bool isEngaged() const noexcept { return engaged; }
    void setEngaged (bool shouldBeEngaged);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool isMomentary() const noexcept { return (int) switchModeParam.load() == SwitchModes::momentary; }

    std::atomic<float>& switchModeParam;
    bool engaged = true, pressed = false;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stompswitch)
};

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void paintShiftLabels (juce::Graphics&);
    void paintDisplay (juce::Graphics&);

    PedalLookAndFeel lookAndFeel;
    std::atomic<float>& directionParam;

    at::Knob shift, blend, expression, glide;
    LeverSwitch direction, switchMode;
    Stompswitch stompswitch;

    juce::Rectangle<int> displayArea, ledArea, nameArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
