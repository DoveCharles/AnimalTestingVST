#pragma once

#include <at_shared/at_shared.h>
#include "ReverseEngine.h"

namespace ParamIDs
{
    inline constexpr auto mode     = "mode";
    inline constexpr auto time     = "time";
    inline constexpr auto sync     = "sync";
    inline constexpr auto division = "division";
    inline constexpr auto repeats  = "repeats";
    inline constexpr auto tone     = "tone";
    inline constexpr auto mix      = "mix";
    inline constexpr auto tails    = "tails";
    inline constexpr auto bypass   = "bypass";
}

namespace Modes
{
    inline const juce::StringArray names { "Reverb", "Delay", "Wash" };
}

namespace Divisions
{
    inline const juce::StringArray names { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T",
                                           "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1" };

    /** Length of each division in quarter notes. */
    inline constexpr std::array<double, 13> beats { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                                    0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 4.0 };
}

/** Reverse reverb and reverse delay pedal, grown out of the RV-7's Reverse setting: Mode (Reverb,
    Delay or Wash), Time (free or synced to the host tempo), Repeats, Tone and Mix, with a Tails
    switch so whatever is playing backwards finishes after the pedal is bypassed. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    double getTailLengthSeconds() const override { return 10.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;

    /** The window length Time or the synced Division gives, clamped to what the engine holds. */
    static float windowSeconds (bool synced, float timeSeconds, int division, double bpm) noexcept;

private:
    void snapSmoothersToTargets() noexcept;
    float targetWindow() const noexcept;

    std::atomic<float>& modeParam;
    std::atomic<float>& timeParam;
    std::atomic<float>& syncParam;
    std::atomic<float>& divisionParam;
    std::atomic<float>& repeatsParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& mixParam;
    std::atomic<float>& tailsParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::SmoothedValue<float> window, repeats, tone, wetGain, dryGain, engagedAmount, wetEnabled;
    double bpm = 120.0;

    // Switching modes fades the wet path out, clears the engine, then fades back in.
    int activeMode = ReverseModes::reverb;
    float modeFade = 1.0f, modeFadeStep = 0.0f;
    bool switchingMode = false;

    ReverseEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
