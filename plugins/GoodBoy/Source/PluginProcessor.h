#pragma once

#include "Maximizer.h"

namespace ParamIDs
{
    inline constexpr auto mode   = "mode";
    inline constexpr auto amount = "amount";
    inline constexpr auto output = "output";
}

namespace Modes
{
    inline const juce::StringArray names { "A", "B", "C", "D" };
    inline const juce::StringArray descriptions { "Warm", "Bright", "Loud", "Big" };
}

/** One knob that makes things sound better: a multiband maximizer with four voicings
    (see Maximizer). */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    Maximizer maximizer;

private:
    std::atomic<float>& modeParam;
    std::atomic<float>& amountParam;
    std::atomic<float>& outputParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
