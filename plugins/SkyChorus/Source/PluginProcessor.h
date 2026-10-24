#pragma once

#include "BbdChorus.h"

namespace ParamIDs
{
    inline constexpr auto speed     = "speed";
    inline constexpr auto width     = "width";
    inline constexpr auto delayTime = "delayTime";
    inline constexpr auto mix       = "mix";
    inline constexpr auto stereo    = "stereo";
    inline constexpr auto bypass    = "bypass";
}

/** How the dry and wet signals are spread across the two outputs. */
namespace StereoModes
{
    enum Mode { biPhase, split, monoSafe };

    inline const juce::StringArray names { "Bi-Phase", "Dry/Wet Split", "Mono-Safe" };

    /** Short labels printed on the switch. */
    inline const juce::StringArray switchLabels { "BI-PHASE", "SPLIT", "MONO" };
}

/** Analog stereo chorus modelled on the mid-80s Ibanez SC10: Speed, Width, Delay Time and a
    footswitch, plus a Mix knob and a stereo mode switch the original didn't have. Bi-Phase is the
    original's output: A carries dry + wet, B dry with the wet inverted. Split puts dry on A and wet
    on B; Mono-Safe sends dry + wet to both, so nothing cancels when summed. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    double getTailLengthSeconds() const override { return 0.05; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;

    /** LFO position, 0 to 1, for the editor's rate LED. */
    float getLfoPhase() const noexcept { return lfoPhase.load (std::memory_order_relaxed); }

private:
    void snapSmoothersToTargets() noexcept;

    std::atomic<float>& speedParam;
    std::atomic<float>& widthParam;
    std::atomic<float>& delayTimeParam;
    std::atomic<float>& mixParam;
    std::atomic<float>& stereoParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::SmoothedValue<float> rate, width, engagedAmount;

    // Dry and wet gain on each output, from Mix and the stereo mode; smoothed so switching doesn't click.
    juce::SmoothedValue<float> dryLeft, dryRight, wetLeft, wetRight;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> delayMs;

    BbdChorus chorus;
    std::atomic<float> lfoPhase { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
