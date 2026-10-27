#pragma once

#include "PianoModel.h"

namespace piano
{
    /** A late-80s ROM piano.

        - If `sampleFolder()` holds WAV files named for their root key (e.g. "Piano_C4.wav",
          "Piano_F#0.wav", C4 = middle C), they are the multisample: read when the plugin
          prepares, each stretched to the keys nearest it. A file's own loop is used if it has one;
          otherwise it plays once through. None of these files ship with the plugin.
        - Otherwise a handful of strikes are recorded from the Studio voicing, each cut to its
          attack plus a short loop and stretched over several keys.

        Velocity opens a lowpass rather than changing the strike, an octave-down copy thickens it,
        and a chorus widens it. */
    class HousePiano
    {
    public:
        void prepare (double sampleRate);

        /** Where the plugin looks for its samples; subfolders are searched too. */
        static juce::File sampleFolder();

        /** True when the samples came from `sampleFolder()` rather than the model. */
        bool usingSampleFolder() const noexcept { return fromFolder; }

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
            double sampleRate = 44100.0;
            std::vector<float> data;

            /** A one-shot has its loop points at the end of its data. */
            bool looped() const noexcept { return loopEnd > loopStart; }
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

        bool loadFolder (const juce::File&);
        void recordFromModel();
        void startVoice (int playedNote, int pitchNote, float velocity01, float level);
        const Multisample& sampleFor (int midiNote) const;

        static constexpr int maxVoices = 48;

        std::vector<Multisample> samples;
        std::array<Voice, maxVoices> voices;
        std::array<bool, 128> keyHeld {};
        Controls controls;
        double rate = 44100.0;
        bool sustain = false, fromFolder = false;
        unsigned voiceCounter = 0;

        // Stereo chorus: one delay line read at two modulated taps.
        std::vector<float> chorusLine;
        int chorusWrite = 0;
        double chorusPhase = 0.0;
    };
}
