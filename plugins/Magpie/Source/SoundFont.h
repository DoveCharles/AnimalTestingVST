#pragma once

#include <at_shared/at_shared.h>

namespace magpie
{
    /** SoundFont 2.04 generator operators (section 8.1.2 of the spec). */
    enum Gen : int
    {
        startAddrsOffset           = 0,
        endAddrsOffset             = 1,
        startloopAddrsOffset       = 2,
        endloopAddrsOffset         = 3,
        startAddrsCoarseOffset     = 4,
        modLfoToPitch              = 5,
        vibLfoToPitch              = 6,
        modEnvToPitch              = 7,
        initialFilterFc            = 8,
        initialFilterQ             = 9,
        modLfoToFilterFc           = 10,
        modEnvToFilterFc           = 11,
        endAddrsCoarseOffset       = 12,
        modLfoToVolume             = 13,
        chorusEffectsSend          = 15,
        reverbEffectsSend          = 16,
        pan                        = 17,
        delayModLFO                = 21,
        freqModLFO                 = 22,
        delayVibLFO                = 23,
        freqVibLFO                 = 24,
        delayModEnv                = 25,
        attackModEnv               = 26,
        holdModEnv                 = 27,
        decayModEnv                = 28,
        sustainModEnv              = 29,
        releaseModEnv              = 30,
        keynumToModEnvHold         = 31,
        keynumToModEnvDecay        = 32,
        delayVolEnv                = 33,
        attackVolEnv               = 34,
        holdVolEnv                 = 35,
        decayVolEnv                = 36,
        sustainVolEnv              = 37,
        releaseVolEnv              = 38,
        keynumToVolEnvHold         = 39,
        keynumToVolEnvDecay        = 40,
        instrument                 = 41,
        keyRange                   = 43,
        velRange                   = 44,
        startloopAddrsCoarseOffset = 45,
        keynum                     = 46,
        velocity                   = 47,
        initialAttenuation         = 48,
        endloopAddrsCoarseOffset   = 50,
        coarseTune                 = 51,
        fineTune                   = 52,
        sampleID                   = 53,
        sampleModes                = 54,
        scaleTuning                = 56,
        exclusiveClass             = 57,
        overridingRootKey          = 58,
        numGens                    = 61
    };

    struct Sample
    {
        juce::int64 start = 0, end = 0, loopStart = 0, loopEnd = 0;   // into SoundFont::pcm
        double sampleRate = 44100.0;
        int originalPitch = 60;
        int correction = 0;   // cents
    };

    /** One instrument zone as heard through one preset zone: the ranges they share, and every
        generator with the instrument's value plus the preset's offset already summed. */
    struct Region
    {
        int keyLo = 0, keyHi = 127, velLo = 0, velHi = 127;
        int sample = 0;
        std::array<int, numGens> gens {};

        int operator[] (Gen g) const noexcept { return gens[(size_t) g]; }
        bool matches (int key, int vel) const noexcept { return key >= keyLo && key <= keyHi && vel >= velLo && vel <= velHi; }
    };

    struct Preset
    {
        juce::String name;
        int bank = 0, program = 0;
        std::vector<Region> regions;
        int keyLo = 127, keyHi = 0;   // the keys anything sounds on
    };

    /** A parsed .sf2. Never changed after loading, so the audio thread reads it without locks. */
    struct SoundFont
    {
        juce::File file;
        juce::String name;
        std::vector<juce::int16> pcm;
        std::vector<Sample> samples;
        std::vector<Preset> presets;   // sorted by bank, then program

        /** The exact bank/program, else the same program in another bank, else the first preset. */
        const Preset* find (int bank, int program) const noexcept;
    };

    /** Reads a SoundFont 2 file. On failure returns nullptr and says why in `error`. */
    std::unique_ptr<SoundFont> readSoundFont (const juce::File&, juce::String& error);
}
