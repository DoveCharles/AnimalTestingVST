#pragma once

#include <at_shared/at_shared.h>
#include "ReverbEngine.h"

namespace ParamIDs
{
    inline constexpr auto level      = "level";
    inline constexpr auto liveliness = "liveliness";
    inline constexpr auto decay      = "decay";
    inline constexpr auto type       = "type";
    inline constexpr auto tails      = "tails";
    inline constexpr auto bypass     = "bypass";
}

namespace ReverbTypes
{
    inline const juce::StringArray names { "Room", "Plate", "Reverse", "Modulated", "Gated", "Hall", "Spring" };

    /** Short labels printed around the Reverb Type selector. */
    inline const juce::StringArray dialLabels { "ROOM", "PLATE", "REVERSE", "MOD", "GATED", "HALL", "SPRING" };
}

/** Stereo digital reverb modelled on the DigiTech HardWire RV-7: Level, Liveliness, Decay, a
    seven-way Reverb Type selector, a Tails switch and a footswitch. As on the original, Reverse
    plays the reversed reverb alone, with no dry signal, and with Tails on the reverb keeps
    ringing out after the pedal is bypassed. */
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

private:
    void snapSmoothersToTargets() noexcept;

    std::atomic<float>& levelParam;
    std::atomic<float>& livelinessParam;
    std::atomic<float>& decayParam;
    std::atomic<float>& typeParam;
    std::atomic<float>& tailsParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    juce::SmoothedValue<float> decay, liveliness, wetGain, dryGain, engagedAmount, wetEnabled;

    // Switching types fades the wet path out, clears the tanks, then fades back in.
    int activeType = ReverbTypes::hall;
    float typeFade = 1.0f, typeFadeStep = 0.0f;
    bool switchingType = false;

    ReverbEngine reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
