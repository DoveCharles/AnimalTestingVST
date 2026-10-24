#pragma once

#include <at_shared/at_shared.h>

namespace ParamIDs
{
    inline constexpr auto drive = "drive";
    inline constexpr auto tone  = "tone";
    inline constexpr auto mix   = "mix";
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
    std::atomic<float>& driveParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& mixParam;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain;
    juce::SmoothedValue<float> toneCutoff;

    juce::dsp::FirstOrderTPTFilter<float> toneFilter;
    juce::dsp::DryWetMixer<float> dryWet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
