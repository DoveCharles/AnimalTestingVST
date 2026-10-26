#pragma once

#include "ResonanceSuppressor.h"

namespace ParamIDs
{
    inline constexpr auto depth       = "depth";
    inline constexpr auto sharpness   = "sharpness";
    inline constexpr auto selectivity = "selectivity";
    inline constexpr auto attack      = "attack";
    inline constexpr auto release     = "release";
    inline constexpr auto stereo      = "stereo";
    inline constexpr auto mix         = "mix";
    inline constexpr auto delta       = "delta";
    inline constexpr auto output      = "output";

    /** Frequency weighting of the cut, one per ResonanceSuppressor::weightHz band. */
    inline constexpr std::array<const char*, ResonanceSuppressor::numWeights> weights { "w100", "w300", "w1k", "w3k", "w8k" };
}

namespace StereoModes
{
    enum Mode { stereo, midSide };
    inline const juce::StringArray names { "Stereo", "Mid/Side" };
}

/** Dynamic resonance suppressor: finds the frequencies that stick up out of the sound's own
    spectrum, moment to moment, and pulls them down (see ResonanceSuppressor).

      Depth        most any frequency can be cut, in dB
      Sharpness    how narrow a peak must be to count: low catches broad bumps, high only rings
      Selectivity  how far a peak must stick out before it's touched
      Attack, Release  how quickly the cuts follow
      Weighting    scales the cut at 100 Hz, 300 Hz, 1 kHz, 3 kHz and 8 kHz
      Delta        listen to only what's being removed */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    ResonanceSuppressor suppressor;

private:
    static constexpr int maxChannels = ResonanceSuppressor::maxChannels;

    std::atomic<float>& depthParam;
    std::atomic<float>& sharpnessParam;
    std::atomic<float>& selectivityParam;
    std::atomic<float>& attackParam;
    std::atomic<float>& releaseParam;
    std::atomic<float>& stereoParam;
    std::atomic<float>& mixParam;
    std::atomic<float>& deltaParam;
    std::atomic<float>& outputParam;
    std::array<std::atomic<float>*, ResonanceSuppressor::numWeights> weightParams {};

    juce::SmoothedValue<float> gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
