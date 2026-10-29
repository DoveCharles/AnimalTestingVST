#pragma once

#include "SoundFont.h"

namespace magpie
{
    /** What the plugin's own knobs add on top of the SoundFont. */
    struct Controls
    {
        int transpose = 0;           // semitones, applied before choosing a zone
        float tuneCents = 0.0f;
        float bendRange = 2.0f;      // semitones
        float velocity = 1.0f;       // 0 = every note at full level, 1 = the SoundFont's own velocity curve
        float releaseScale = 1.0f;
    };

    /** The SoundFont 2 synthesis model for one MIDI channel: a voice per sounding region, each with
        its own volume and modulation envelopes, two LFOs and a resonant low-pass, and the spec's
        default modulators (velocity, mod wheel, pan, pitch wheel). */
    class Engine
    {
    public:
        static constexpr int maxVoices = 96;

        void prepare (double sampleRate);

        /** Silences everything, since the voices point into the old font. */
        void setFont (const SoundFont*) noexcept;
        const SoundFont* getFont() const noexcept { return font; }

        void setPreset (const Preset*) noexcept;
        const Preset* getPreset() const noexcept { return preset; }

        void setControls (const Controls& c) noexcept { controls = c; }

        void noteOn (int key, int velocity) noexcept;
        void noteOff (int key) noexcept;
        void setSustain (bool down) noexcept;
        void setPitchBend (float semitonesNormalised) noexcept { bend = semitonesNormalised; }
        void setModWheel (float amount) noexcept { modWheel = amount; }
        void setPan (float minusOneToOne) noexcept { channelPan = minusOneToOne; }
        void allNotesOff() noexcept;
        void allSoundOff() noexcept;

        /** Adds into left and right. */
        void render (float* left, float* right, int numSamples) noexcept;

        int getNumActiveVoices() const noexcept;
        /** Bit per key that is sounding, for the keyboard display. */
        std::bitset<128> getSoundingKeys() const noexcept;

    private:
        struct Envelope
        {
            enum class Stage { delay, attack, hold, decay, sustain, release, done };

            // Volume envelope decays and releases in dB (exponentially in amplitude); the
            // modulation envelope does both in straight lines.
            bool exponential = false;
            Stage stage = Stage::done;
            float level = 0.0f;
            int counter = 0;
            int delay = 0, attack = 0, hold = 0;
            float sustain = 0.0f, decayStep = 0.0f, releaseStep = 0.0f;   // multipliers when exponential

            void start() noexcept;
            void release() noexcept;
            void advance (int numSamples) noexcept;
            float next() noexcept;
        };

        struct Lfo
        {
            int delay = 0;
            float phase = 0.0f, increment = 0.0f;

            /** Triangle from 0 rising to 1, as the spec has it. Advances by numSamples. */
            float advance (int numSamples) noexcept;
        };

        struct Voice
        {
            const Region* region = nullptr;
            int key = -1, playedKey = -1, velocity = 0;
            juce::uint32 age = 0;
            bool held = false, sustained = false, fading = false;

            double position = 0.0;
            juce::int64 start = 0, end = 0, loopStart = 0, loopEnd = 0;
            int loopMode = 0;
            bool looping = false;
            double rateRatio = 1.0;     // sample rate over ours
            float pitchCents = 0.0f;    // relative to the root key, before modulation

            float gain = 0.0f, panLeft = 0.0f, panRight = 0.0f, panGen = 0.0f;
            float modGain = 1.0f;       // the mod LFO's volume wobble, ramped across each control block

            Envelope volEnv, modEnv;
            Lfo modLfo, vibLfo;

            bool filtered = false;
            float filterCents = 13500.0f, filterQ = 0.7071f, filterGain = 1.0f;
            float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

            bool isActive() const noexcept { return region != nullptr; }
        };

        void startVoice (const Region&, int key, int playedKey, int velocity) noexcept;
        Voice& findFreeVoice() noexcept;
        void releaseVoice (Voice&) noexcept;
        void renderVoice (Voice&, float* left, float* right, int numSamples) noexcept;
        float readSample (const Voice&, juce::int64 index) const noexcept;

        int toSamples (int timecents) const noexcept;

        double sampleRate = 44100.0;
        const SoundFont* font = nullptr;
        const Preset* preset = nullptr;
        Controls controls;

        std::array<Voice, maxVoices> voices;
        juce::uint32 nextAge = 0;
        bool sustainDown = false;
        float bend = 0.0f, modWheel = 0.0f, channelPan = 0.0f;
    };
}
