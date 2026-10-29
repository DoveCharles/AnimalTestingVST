#pragma once

#include <at_shared/at_shared.h>
#include "Engine.h"
#include "FontLoader.h"

namespace ParamIDs
{
    inline constexpr auto volume    = "volume";
    inline constexpr auto transpose = "transpose";
    inline constexpr auto tune      = "tune";
    inline constexpr auto bendRange = "bendRange";
    inline constexpr auto velocity  = "velocity";
    inline constexpr auto release   = "release";
    inline constexpr auto reverb    = "reverb";
}

/** Saved with the set (and in .atpreset files) alongside the parameters. */
namespace StateKeys
{
    inline constexpr auto soundFont = "soundFont";
    inline constexpr auto bank      = "bank";
    inline constexpr auto program   = "program";
}

class PluginProcessor final : public at::ProcessorBase,
                              private juce::ValueTree::Listener,
                              private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 3.0; }

    juce::AudioProcessorEditor* createEditor() override;

    /** Message thread. Loads a .sf2 and plays its first preset. */
    void loadSoundFont (const juce::File&);
    /** Message thread. */
    void selectPreset (int bank, int program);

    static int packPreset (int bank, int program) noexcept { return (bank << 8) | (program & 0xff); }
    /** The preset playing, packed, or -1 with no font. */
    int getPlayingPreset() const noexcept { return playing.load(); }
    int getActiveVoices() const noexcept { return activeVoices.load(); }

    /** On-screen keyboard; its notes are merged into the MIDI input. */
    juce::MidiKeyboardState keyboardState;

    magpie::FontLoader fonts;

private:
    void valueTreeRedirected (juce::ValueTree&) override { restoreFromState(); }
    void timerCallback() override;
    void restoreFromState();

    void handleMidi (const juce::MidiMessage&);
    void resolvePreset() noexcept;

    std::atomic<float>& volumeParam;
    std::atomic<float>& transposeParam;
    std::atomic<float>& tuneParam;
    std::atomic<float>& bendRangeParam;
    std::atomic<float>& velocityParam;
    std::atomic<float>& releaseParam;
    std::atomic<float>& reverbParam;

    magpie::Engine engine;
    juce::dsp::Reverb room;
    juce::SmoothedValue<float> volume, channelVolume, expression;
    juce::AudioBuffer<float> monoScratch;   // the right channel, when the host gives us mono

    std::atomic<int> selection { -1 };      // what's asked for, packed; -1 = the font's first preset
    std::atomic<int> playing { -1 }, playingSerial { 0 }, activeVoices { 0 };
    int resolvedSelection = -2;             // audio thread: the selection the engine's preset came from
    int bankSelect = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
