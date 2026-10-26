#pragma once

#include "TapeMachine.h"

namespace ParamIDs
{
    inline constexpr auto drive       = "drive";
    inline constexpr auto boost       = "boost";
    inline constexpr auto tone        = "tone";
    inline constexpr auto toneEnable  = "toneEnable";
    inline constexpr auto filter      = "filter";
    inline constexpr auto wow         = "wow";
    inline constexpr auto flutter     = "flutter";
    inline constexpr auto wear        = "wear";
    inline constexpr auto noise       = "noise";
    inline constexpr auto mechanical  = "mechanical";
    inline constexpr auto stereoWidth = "stereoWidth";
    inline constexpr auto output      = "output";
    inline constexpr auto tapeStop    = "tapeStop";
    inline constexpr auto stopSpeed   = "stopSpeed";
    inline constexpr auto catchUp     = "catchUp";
}

namespace StopSpeeds
{
    inline const juce::StringArray names { "8 Bar", "4 Bar", "2 Bar", "1 Bar", "1/2 Bar", "1/4 Bar" };
    inline constexpr std::array<double, 6> bars { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
}

namespace CatchUps
{
    enum Mode { instant, fastForward };
    inline const juce::StringArray names { "Instant", "Fast-Forward" };
}

/** Lo-fi tape: drive, a tape voicing, wow, flutter, wear, hiss and motor noise, a bipolar
    filter and a tempo-synced tape stop (see TapeMachine). */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

private:
    TapeMachine tape;

    std::atomic<float>& driveParam;
    std::atomic<float>& boostParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& toneEnableParam;
    std::atomic<float>& filterParam;
    std::atomic<float>& wowParam;
    std::atomic<float>& flutterParam;
    std::atomic<float>& wearParam;
    std::atomic<float>& noiseParam;
    std::atomic<float>& mechanicalParam;
    std::atomic<float>& stereoWidthParam;
    std::atomic<float>& outputParam;
    std::atomic<float>& tapeStopParam;
    std::atomic<float>& stopSpeedParam;
    std::atomic<float>& catchUpParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
