#pragma once

#include "MuffCircuit.h"

namespace ParamIDs
{
    inline constexpr auto volume  = "volume";
    inline constexpr auto tone    = "tone";
    inline constexpr auto sustain = "sustain";
    inline constexpr auto bypass  = "bypass";
}

/** Fuzz modelled on the 90s green Russian four-transistor muff: Volume, Tone, Sustain and a footswitch. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;

private:
    static constexpr int maxChannels = 2;
    static constexpr size_t oversamplingStages = 2;   // 4x, so the clipping stages don't alias

    std::atomic<float>& volumeParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& sustainParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> sustainGain, volumeGain;
    juce::SmoothedValue<float> toneAmount;

    std::array<MuffCircuit, maxChannels> circuits;
    juce::dsp::Oversampling<float> oversampling { (size_t) maxChannels, oversamplingStages,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                  true, true };

    // Click-free footswitch: the dry path is delayed to match the oversampling latency.
    juce::dsp::DryWetMixer<float> bypassMix { 256 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
