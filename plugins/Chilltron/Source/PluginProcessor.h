#pragma once

#include <at_shared/at_shared.h>
#include "TapeEngine.h"

namespace ParamIDs
{
    inline constexpr auto tapeA    = "tapeA";
    inline constexpr auto tapeB    = "tapeB";
    inline constexpr auto blend    = "blend";
    inline constexpr auto pitch    = "pitch";
    inline constexpr auto wow      = "wow";
    inline constexpr auto click    = "click";
    inline constexpr auto loop     = "loop";
    inline constexpr auto attack   = "attack";
    inline constexpr auto release  = "release";
    inline constexpr auto velocity = "velocity";
    inline constexpr auto tone     = "tone";
    inline constexpr auto spread   = "spread";
    inline constexpr auto reverb   = "reverb";
    inline constexpr auto volume   = "volume";
}

class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 3.0; }

    juce::AudioProcessorEditor* createEditor() override;

    /** On-screen keyboard; its notes are merged into the MIDI input. */
    juce::MidiKeyboardState keyboardState;

    juce::SharedResourcePointer<chilltron::TapeLibrary> library;
    const chilltron::Engine& getEngine() const noexcept { return engine; }

private:
    void handleMidi (const juce::MidiMessage&);

    std::atomic<float>& tapeAParam;
    std::atomic<float>& tapeBParam;
    std::atomic<float>& blendParam;
    std::atomic<float>& pitchParam;
    std::atomic<float>& wowParam;
    std::atomic<float>& clickParam;
    std::atomic<float>& loopParam;
    std::atomic<float>& attackParam;
    std::atomic<float>& releaseParam;
    std::atomic<float>& velocityParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& spreadParam;
    std::atomic<float>& reverbParam;
    std::atomic<float>& volumeParam;

    chilltron::Engine engine;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    juce::dsp::Reverb room;
    juce::SmoothedValue<float> cutoff, volume, expression;
    juce::AudioBuffer<float> monoScratch;   // the right channel, when the host gives us mono

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
