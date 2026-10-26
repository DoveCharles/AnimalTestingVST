#pragma once

#include "PitchDetector.h"
#include "PsolaShifter.h"

namespace ParamIDs
{
    inline constexpr auto mode     = "mode";
    inline constexpr auto pitch    = "pitch";
    inline constexpr auto formant  = "formant";
    inline constexpr auto key      = "key";
    inline constexpr auto scale    = "scale";
    inline constexpr auto retune   = "retune";
    inline constexpr auto gender   = "gender";
    inline constexpr auto chest    = "chest";
    inline constexpr auto air      = "air";
    inline constexpr auto breath   = "breath";
    inline constexpr auto vibRate  = "vibRate";
    inline constexpr auto vibDepth = "vibDepth";
    inline constexpr auto vibDelay = "vibDelay";
    inline constexpr auto drive    = "drive";
    inline constexpr auto mix      = "mix";
    inline constexpr auto output   = "output";
}

namespace Modes
{
    enum Mode { transpose, quantize, robot };
    inline const juce::StringArray names { "Transpose", "Quantize", "Robot" };
}

namespace Scales
{
    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray names { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic" };

    /** Bit n set when the scale contains n semitones above the root. */
    inline constexpr std::array<int, 5> masks { 0xfff, 0xab5, 0x5ad, 0x295, 0x4a9 };
}

/** Vocal pitch and formant shifter in the spirit of the little blue box, plus a Character
    section for changing how big, chesty, airy or breathy a voice sounds:

      Transpose  shifts by Pitch, formants kept unless Formant moves them.
      Quantize   snaps the shifted voice to Key/Scale at the Retune speed.
      Robot      holds the voice on the key's root (plus Pitch), for monotone effects.

    Gender is a macro that moves pitch, formant, chest, air and breath together. */
class PluginProcessor final : public at::ProcessorBase
{
public:
    PluginProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    /** For the editor's readout: detected input pitch and the pitch going out, 0 when unvoiced. */
    std::atomic<float> inputHz { 0.0f }, outputHz { 0.0f };

private:
    static constexpr int maxChannels = PsolaShifter::maxChannels;

    struct Settings
    {
        float semitones, formant, chest, air, breath, drive, mix, gain;
    };

    Settings readSettings() const noexcept;
    void updateFilters (const Settings&);
    float correctionTarget (int mode, float midiIn, float semitones) const noexcept;

    std::atomic<float>& modeParam;
    std::atomic<float>& pitchParam;
    std::atomic<float>& formantParam;
    std::atomic<float>& keyParam;
    std::atomic<float>& scaleParam;
    std::atomic<float>& retuneParam;
    std::atomic<float>& genderParam;
    std::atomic<float>& chestParam;
    std::atomic<float>& airParam;
    std::atomic<float>& breathParam;
    std::atomic<float>& vibRateParam;
    std::atomic<float>& vibDepthParam;
    std::atomic<float>& vibDelayParam;
    std::atomic<float>& driveParam;
    std::atomic<float>& mixParam;
    std::atomic<float>& outputParam;

    double sampleRate = 44100.0;

    PitchDetector detector;
    PsolaShifter shifter;

    // The dry path is delayed by the shifter's latency so Mix doesn't comb.
    std::array<std::vector<float>, maxChannels> dryDelay;
    int dryPos = 0;

    juce::SmoothedValue<float> semitones, formant, mix, gain, drive, breath, air;
    float correction = 0.0f;
    float vibPhase = 0.0f, voicedSeconds = 0.0f;

    // Character: chest cut (high-pass + low-mid dip), air shelf and exciter, shaped breath noise.
    std::array<juce::dsp::IIR::Filter<float>, maxChannels> chestHighPass, chestDip, airShelf, exciterBand,
                                                           breathLow, breathHigh;
    std::array<float, maxChannels> envelope {};
    float voicedLevel = 0.0f;
    juce::Random noise;
    float lastChest = -1.0f, lastAir = -1.0f, lastFormant = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
