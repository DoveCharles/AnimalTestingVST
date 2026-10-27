#pragma once

#include "DriverCircuit.h"

namespace ParamIDs
{
    inline constexpr auto level  = "level";
    inline constexpr auto tone   = "tone";
    inline constexpr auto gain   = "gain";
    inline constexpr auto custom = "custom";
    inline constexpr auto bypass = "bypass";
}

/** Overdrive modelled on the BD-2W Blues Driver: Level, Tone, Gain, a Standard / Custom mode
    switch and a footswitch. Stereo, each side through its own circuit. */
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

    void prepareCircuits (bool custom);

    std::atomic<float>& levelParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& gainParam;
    std::atomic<float>& customParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> stageGain, trebleGain, levelGain;

    std::array<DriverCircuit, maxChannels> circuits;
    double oversampledRate = 0.0;
    bool customMode = false;

    juce::dsp::Oversampling<float> oversampling { (size_t) maxChannels, oversamplingStages,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                  true, true };

    // Click-free footswitch: the dry path is delayed to match the oversampling latency.
    juce::dsp::DryWetMixer<float> bypassMix { 256 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
