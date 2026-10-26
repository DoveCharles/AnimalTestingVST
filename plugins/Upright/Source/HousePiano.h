#pragma once

#include "PianoModel.h"

namespace piano
{
    /** A late-80s ROM piano made from the model: a handful of strikes recorded from the Studio
        voicing when the plugin loads, each cut to its attack plus a short loop and stretched over
        several keys. Velocity opens a lowpass rather than changing the strike, an octave-down copy
        thickens it, and a chorus widens it. None of it is meant to sound like a real piano. */
    class HousePiano
    {
    public:
        void prepare (double sampleRate);

        void setControls (const Controls& c) noexcept { controls = c; }
        void noteOn (int midiNote, float velocity01);
        void noteOff (int midiNote);
        void setSustain (bool on);
        void allNotesOff();

        /** Adds the stereo output to `left` and `right`. */
        void render (float* left, float* right, int numSamples);

    private:
        struct Multisample
        {
            int root = 60;
            int loopStart = 0, loopEnd = 0; // playback wraps from loopEnd back to loopStart
            std::vector<float> data;
        };

        struct Voice
        {
            const Multisample* sample = nullptr;
            int note = -1;
            double position = 0.0, increment = 1.0;
            float gain = 0.0f, envelope = 0.0f, decay = 1.0f, release = 1.0f;
            float cutoff = 1000.0f, cutoffFall = 1.0f;
            float pan = 0.5f;
            float s1 = 0.0f, s2 = 0.0f; // lowpass state
            bool held = false, releasing = false;
            unsigned age = 0;

            bool active() const noexcept { return sample != nullptr; }
        };

        void startVoice (int playedNote, int pitchNote, float velocity01, float level);
        const Multisample& sampleFor (int midiNote) const;

        static constexpr int maxVoices = 48;

        std::vector<Multisample> samples;
        std::array<Voice, maxVoices> voices;
        std::array<bool, 128> keyHeld {};
        Controls controls;
        double rate = 44100.0;
        bool sustain = false;
        unsigned voiceCounter = 0;

        // Stereo chorus: one delay line read at two modulated taps.
        std::vector<float> chorusLine;
        int chorusWrite = 0;
        double chorusPhase = 0.0;
    };
}
