#pragma once

#include <at_shared/at_shared.h>

namespace ParamIDs
{
    inline constexpr auto gain = "gain";
}

class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

private:
    std::atomic<float>& gainParam;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
