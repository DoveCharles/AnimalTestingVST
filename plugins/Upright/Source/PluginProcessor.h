#pragma once

#include <at_shared/at_shared.h>
#include "PianoModel.h"

namespace ParamIDs
{
    inline constexpr auto voicing  = "voicing";
    inline constexpr auto hardness = "hardness";
    inline constexpr auto unison   = "unison";
    inline constexpr auto decay    = "decay";
    inline constexpr auto body     = "body";
    inline constexpr auto volume   = "volume";
}

class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 0.5; }

    juce::AudioProcessorEditor* createEditor() override;

    /** On-screen keyboard; its notes are merged into the MIDI input. */
    juce::MidiKeyboardState keyboardState;

private:
    /** Every engine sees every message; only the selected voicing's engine strikes new notes. */
    static void handleMidi (const juce::MidiMessage&, piano::Engine&, bool strikes);

    std::atomic<float>& voicingParam;
    std::atomic<float>& hardnessParam;
    std::atomic<float>& unisonParam;
    std::atomic<float>& decayParam;
    std::atomic<float>& bodyParam;
    std::atomic<float>& volumeParam;

    // One engine per voicing: switching never rebuilds anything on the audio thread, and notes
    // already ringing finish in the voicing they were struck in.
    std::array<piano::Engine, 2> engines;
    std::array<juce::dsp::Convolution, 2> soundboards;

    std::array<juce::AudioBuffer<float>, 2> boardBuffers;
    std::vector<float> bridge;
    juce::dsp::IIR::Filter<float> dryHighPass;
    juce::SmoothedValue<float> body, volume;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
