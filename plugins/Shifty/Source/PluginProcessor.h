#pragma once

#include "PitchDelay.h"

namespace ParamIDs
{
    inline constexpr auto balance  = "balance";
    inline constexpr auto feedback = "feedback";
    inline constexpr auto fine     = "fine";
    inline constexpr auto mode     = "mode";
    inline constexpr auto bypass   = "bypass";
}

/** Digital pitch shifter/delay modelled on the late-80s compact pedal: Balance, Feedback,
    Fine-Manual, a six-position Mode selector and a footswitch. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override {}

    double getTailLengthSeconds() const override { return 4.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;

private:
    static constexpr int maxChannels = 2;

    void snapSmoothersToTargets() noexcept;

    std::atomic<float>& balanceParam;
    std::atomic<float>& feedbackParam;
    std::atomic<float>& fineParam;
    std::atomic<float>& modeParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    double sampleRate = 44100.0;

    // Turning the time knob glides the read head, so repeats bend in pitch like the original.
    juce::SmoothedValue<float> delaySamples, semitones, feedbackAmount, balanceAmount, engagedAmount;

    // Switching modes fades the wet path out, clears the buffer, then fades back in.
    int activeMode = Modes::delay500;
    float modeFade = 1.0f, modeFadeStep = 0.0f;
    bool switchingMode = false;

    std::array<PitchDelay, maxChannels> engines;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
