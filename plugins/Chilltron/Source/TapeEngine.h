#pragma once

#include "TapeLibrary.h"

namespace chilltron
{
    /** The M400's keyboard: 35 notes, G2 to F5, each with its own eight-second strip of tape. */
    inline constexpr int lowestKey = 43, highestKey = 77;
    inline constexpr double tapeSeconds = 8.0;

    struct Controls
    {
        float blend = 0.0f;        // 0 = tape A only, 1 = tape B only
        float pitchCents = 0.0f;   // capstan speed
        float wow = 0.35f;         // wow & flutter depth, 0..1
        float attack = 0.012f, release = 0.12f;   // seconds
        float click = 0.5f;        // 0..1
        bool velocity = false;     // level follows note velocity
        float spread = 0.3f;       // low keys left, high keys right, 0..1
        bool loop = false;         // loop the tape instead of running out
    };

    /** Plays tapes the way the machine does: every key pulls its own strip past the head at
        capstan speed from where it was left, and a spring rewinds it once the key comes up.
        Repeating a note before its tape is home starts part-way through. All keys share the one
        capstan, so wow and flutter move them together; each pinch roller adds its own slight
        offset. */
    class Engine
    {
    public:
        Engine();

        void prepare (double sampleRate);

        /** Called once per block, before render. Tapes may be nullptr (not loaded). */
        void setTapes (const Tape* a, const Tape* b, const Tape* click) noexcept;
        void setControls (const Controls&) noexcept;

        void noteOn (int key, float velocity);
        void noteOff (int key);
        void setSustain (bool down);
        void setPitchBend (float semitones) noexcept { bend = semitones; }
        void setModWheel (float amount) noexcept      { modWheel = amount; }
        void allNotesOff();

        /** Adds n samples of stereo output. */
        void render (float* left, float* right, int n);

        /** For the editor: how far each key's tape has run (seconds), and whether the key is down. */
        float getTapePosition (int key) const noexcept { return uiPosition[(size_t) key].load (std::memory_order_relaxed); }
        bool isSounding (int key) const noexcept       { return uiSounding[(size_t) key].load (std::memory_order_relaxed); }

    private:
        struct Track
        {
            const Tape* tape = nullptr;
            const Tape::Zone* zone = nullptr;
            double position = 0.0;   // in zone samples
            double increment = 0.0;  // zone samples per output sample, at nominal speed
            bool finished = true;
        };

        enum class Stage { idle, attack, hold, release, quickFade };

        struct Voice
        {
            Stage stage = Stage::idle;
            int key = 0;
            float gain = 1.0f;
            float envelope = 0.0f;
            float attackStep = 1.0f, releaseCoef = 0.0f, quickStep = 1.0f;
            float detune = 1.0f;     // this key's pinch roller
            float panLeft = 1.0f, panRight = 1.0f;
            bool sustained = false;
            std::array<Track, 2> tracks;
            juce::uint32 age = 0;
        };

        struct Click
        {
            const Tape::Zone* zone = nullptr;
            const Tape* tape = nullptr;
            double position = 0.0, increment = 1.0;
            float gain = 0.0f;
            float panLeft = 1.0f, panRight = 1.0f;
            bool active = false;
        };

        void startTrack (Track&, const Tape*, int key, double tapeTime) const;
        float readTrack (Track&, double speed) const;
        float voiceTapeTime (const Voice&) const;
        void release (Voice&);
        void triggerClick (int key, float gain);
        void computeSpeed (int n);

        double sampleRate = 48000.0;
        Controls controls;
        const Tape* tapeA = nullptr;
        const Tape* tapeB = nullptr;
        const Tape* clickTape = nullptr;

        std::array<Voice, 48> voices;
        std::array<Click, 16> clicks;
        Tape fallbackTape;   // a synthesised click, for when TapeKlik.sf2 is missing

        // Per key: how much of its tape has been pulled past the head, in seconds at nominal speed.
        std::array<double, 128> keyTime {};
        std::array<bool, 128> keyHeld {};
        bool sustainDown = false;
        juce::uint32 voiceCounter = 0;

        // Capstan: shared speed modulation.
        std::vector<float> speed;
        double wowPhase = 0.0, flutterPhase1 = 0.0, flutterPhase2 = 0.0;
        float drift = 0.0f, driftTarget = 0.0f;
        int driftCountdown = 0;
        float bend = 0.0f, modWheel = 0.0f;
        juce::SmoothedValue<float> blendA, blendB;
        juce::Random random;

        std::array<std::atomic<float>, 128> uiPosition {};
        std::array<std::atomic<bool>, 128> uiSounding {};
    };
}
