#pragma once

#include <at_shared/at_shared.h>
#include "Chords.h"

namespace ParamIDs
{
    inline constexpr auto key        = "key";
    inline constexpr auto chord      = "chord";
    inline constexpr auto brightness = "brightness";
    inline constexpr auto decay      = "decay";
    inline constexpr auto mix        = "mix";
}

/** Chord resonator in the spirit of the SP-404 / SP-404MKII Resonator: a bank of tuned comb
    filters, one per chord tone, that the input "rings". */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 10.0; }

    juce::AudioProcessorEditor* createEditor() override;

    /** Copies up to maxSamples of the latest mono output into dest for the editor's spectrogram.
        Call from the message thread only. Returns how many samples were copied. */
    int pullScopeSamples (float* dest, int maxSamples);

private:
    void updateTuning();

    std::atomic<float>& keyParam;
    std::atomic<float>& chordParam;
    std::atomic<float>& brightnessParam;
    std::atomic<float>& decayParam;
    std::atomic<float>& mixParam;

    std::array<at::dsp::CombResonator, Chords::numVoices> voices;
    std::array<float, Chords::numVoices> panLeft {}, panRight {};
    int currentKey = -1, currentChord = -1;

    juce::dsp::DryWetMixer<float> dryWet;

    // Single-producer (audio) / single-consumer (GUI) FIFO feeding the spectrogram.
    static constexpr int scopeFifoSize = 1 << 15;
    juce::AbstractFifo scopeFifo { scopeFifoSize };
    std::vector<float> scopeBuffer = std::vector<float> ((size_t) scopeFifoSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
