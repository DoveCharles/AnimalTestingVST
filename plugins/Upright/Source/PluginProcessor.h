#pragma once

#include <at_shared/at_shared.h>
#include "PianoModel.h"
#include "HousePiano.h"
#include "RhodesPiano.h"
#include "Vibraphone.h"

namespace ParamIDs
{
    inline constexpr auto voicing  = "voicing";
    inline constexpr auto hardness = "hardness";
    inline constexpr auto unison   = "unison";
    inline constexpr auto decay    = "decay";
    inline constexpr auto resonance = "resonance";
    inline constexpr auto noise    = "noise";
    inline constexpr auto body     = "body";
    inline constexpr auto room     = "room";
    inline constexpr auto volume   = "volume";
}

class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 1.3; }

    juce::AudioProcessorEditor* createEditor() override;

    /** On-screen keyboard; its notes are merged into the MIDI input. */
    juce::MidiKeyboardState keyboardState;

private:
    /** Every engine sees every message; only the selected voicing's engine strikes new notes. */
    static void handleMidi (const juce::MidiMessage&, piano::Engine&, bool strikes);

    /** For the House, Rhodes and Vibraphone voicings, which have no soft pedal or pedal noise. */
    template <typename Instrument>
    static void handleMidi (const juce::MidiMessage&, Instrument&, bool strikes);

    /** Renders one of the board-less voicings into `out`, applying the block's MIDI on the way. */
    template <typename Instrument>
    static void renderWithMidi (Instrument&, const juce::MidiBuffer&, juce::AudioBuffer<float>& out, int numSamples, bool strikes);

    std::atomic<float>& voicingParam;
    std::atomic<float>& hardnessParam;
    std::atomic<float>& unisonParam;
    std::atomic<float>& decayParam;
    std::atomic<float>& resonanceParam;
    std::atomic<float>& noiseParam;
    std::atomic<float>& bodyParam;
    std::atomic<float>& roomParam;
    std::atomic<float>& volumeParam;

    // One engine per voicing: switching never rebuilds anything on the audio thread, and notes
    // already ringing finish in the voicing they were struck in.
    std::array<piano::Engine, 2> engines;

    // The House voicing plays recordings of the model, so it skips the board and goes straight to the mix.
    piano::HousePiano house;

    // The Rhodes is tines and pickups, not strings: it has no board either.
    piano::RhodesPiano rhodes;

    // Nor do the vibraphone's bars: their tubes are their own resonators.
    piano::Vibraphone vibraphone;

    // Per voicing, one board response per driving point.
    std::array<std::array<juce::dsp::Convolution, piano::numDrivingPoints>, 2> soundboards;
    std::array<std::array<juce::AudioBuffer<float>, piano::numDrivingPoints>, 2> boardBuffers;

    juce::AudioBuffer<float> dry, mix, houseBuffer, rhodesBuffer, vibraphoneBuffer;
    std::array<juce::dsp::IIR::Filter<float>, 2> dryHighPass;
    juce::dsp::Convolution room;
    juce::AudioBuffer<float> roomBuffer;
    juce::SmoothedValue<float> body, roomLevel, volume;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
