#pragma once

#include "GrainShifter.h"

namespace ParamIDs
{
    inline constexpr auto shift      = "shift";
    inline constexpr auto direction  = "direction";
    inline constexpr auto blend      = "blend";
    inline constexpr auto expression = "expression";
    inline constexpr auto glide      = "glide";
    inline constexpr auto switchMode = "switchMode";
    inline constexpr auto bypass     = "bypass";
}

namespace SwitchModes
{
    enum SwitchMode { latch, momentary };

    inline const juce::StringArray names { "Latch", "Momentary" };
}

/** Polyphonic pitch-shifter pedal: Shift (detune, intervals and up to three octaves), Up / Dual /
    Down, Blend, an Expression sweep from unison to the full shift, and a footswitch that latches or
    works only while held, with an optional Glide that bends the pitch in and out as it engages. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override {}

    // The shift delays the wet path by a few to about 60 ms depending on the interval. Like the
    // pedal, it isn't reported to the host: the dry path stays in time and Blend mixes the two live.
    double getTailLengthSeconds() const override { return 0.2; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;

private:
    static constexpr int maxChannels = 2;

    float targetSemitones() const noexcept;
    void snapSmoothersToTargets() noexcept;

    std::atomic<float>& shiftParam;
    std::atomic<float>& directionParam;
    std::atomic<float>& blendParam;
    std::atomic<float>& expressionParam;
    std::atomic<float>& glideParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> semitones, upGain, downGain, blendAmount;

    // 0 = bypassed, 1 = engaged. Ramps over the Glide time, or a short click-free fade without it.
    float engaged = 1.0f;

    std::array<GrainShifter, maxChannels> upShifters, downShifters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
