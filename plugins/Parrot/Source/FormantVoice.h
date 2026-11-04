#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include "Phonemes.h"

namespace parrot
{
    struct VoiceSettings
    {
        float size = 1.15f;           // vocal tract scale: formants multiply by this
        float tone = 0.55f;           // 0 soft and breathy .. 1 bright and pressed
        float breath = 0.15f;         // aspiration mixed into the voice
        float diction = 1.0f;         // consonant level, 0..2
        float vibratoCents = 25.0f;
        float vibratoRate = 5.5f;
        float modWheel = 0.0f;        // 0..1, adds vibrato
        float glideSeconds = 0.06f;
        float releaseSeconds = 0.12f;
        float pitchBend = 0.0f;       // semitones
    };

    /** One singing voice: a glottal pulse through a Klatt-style cascade of formant resonators,
        with a parallel noise branch for fricatives and bursts.

        Each syllable becomes a queue of segments (consonant targets, then the vowel, held until
        the note ends); parameters glide between targets so the transitions do the work that
        makes consonants recognisable. */
    class FormantVoice
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void setSettings (const VoiceSettings& s) noexcept { settings = s; }

        /** Seconds of consonant before the vowel of this syllable starts. */
        float onsetSeconds (const Syllable&) const;
        /** Seconds the syllable takes to close (diphthong glide and final consonants) once released. */
        float codaSeconds (const Syllable&) const;

        /** Sings a syllable. If a note is already held, it closes that syllable and moves on legato. */
        void noteOn (const Syllable&, int note, float velocity);
        /** Closes the syllable and fades out. */
        void noteOff();
        /** Moves the held vowel to another pitch without a new syllable. */
        void glideTo (int note);
        /** Quick fade, for all-notes-off. */
        void stop();

        bool isHolding() const noexcept    { return holding; }
        int getNote() const noexcept       { return currentNote; }
        const Syllable& getSyllable() const noexcept { return syllable; }
        bool isActive() const noexcept;

        /** Writes (not adds) mono samples. */
        void render (float* out, int numSamples) noexcept;

        struct Target
        {
            std::array<float, 5> f { 750.0f, 1200.0f, 2500.0f, 3500.0f, 4500.0f };
            std::array<float, 5> bw { 70.0f, 90.0f, 150.0f, 200.0f, 250.0f };
            float av = 0.0f;          // voicing
            float ah = 0.0f;          // aspiration (noise through the formants)
            float af = 0.0f;          // frication (noise through the parallel bands)
            float nasalZero = 270.0f; // equal to the nasal pole: no nasal colour
            std::array<float, 2> fricF { 4000.0f, 4000.0f }, fricQ { 1.0f, 1.0f }, fricGain { 0.0f, 0.0f };
        };

        struct Segment
        {
            Target target;
            float ms = -1.0f;         // < 0: held until the next noteOn/noteOff
            float formTau = 12.0f;    // formant glide time constant, ms
            float ampTau = 6.0f;      // amplitude time constant, ms
            int pitchNote = -1;       // set the pitch when this segment begins
            bool glide = false;       // glide there rather than jump
            bool startsVowel = false; // restarts the vibrato delay
        };

        struct SegmentList
        {
            static constexpr int capacity = 40;
            std::array<Segment, capacity> items;
            int size = 0;
            void push (const Segment& s) noexcept { if (size < capacity) items[(size_t) size++] = s; }
            float totalMs() const noexcept
            {
                float t = 0.0f;
                for (int i = 0; i < size; ++i)
                    t += juce::jmax (0.0f, items[(size_t) i].ms);
                return t;
            }
        };

    private:
        Target vowelTarget (Ph, bool secondPart) const;
        Target heldTarget (Ph) const;
        void appendConsonant (SegmentList&, Ph, const Target& vowel, bool inCoda, bool first) const;
        void appendOnset (SegmentList&, const Syllable&, bool fromSilence) const;
        void appendCoda (SegmentList&, const Syllable&, bool release) const;

        void startQueue (const SegmentList&);
        void beginSegment (const Segment&);
        void updateFilters() noexcept;
        float nextNoise() noexcept;

        struct Resonator
        {
            float a = 1.0f, b = 0.0f, c = 0.0f, y1 = 0.0f, y2 = 0.0f;
            void set (float f, float bw, float fs) noexcept;
            float process (float x) noexcept
            {
                const auto y = a * x + b * y1 + c * y2;
                y2 = y1;
                y1 = y;
                return y;
            }
            void clear() noexcept { y1 = y2 = 0.0f; }
        };

        struct AntiResonator
        {
            float a = 1.0f, b = 0.0f, c = 0.0f, x1 = 0.0f, x2 = 0.0f;
            void set (float f, float bw, float fs) noexcept;
            float process (float x) noexcept
            {
                const auto y = a * x + b * x1 + c * x2;
                x2 = x1;
                x1 = x;
                return y;
            }
            void clear() noexcept { x1 = x2 = 0.0f; }
        };

        struct BandPass
        {
            float b0 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, s1 = 0.0f, s2 = 0.0f;
            void set (float f, float q, float fs) noexcept;
            float process (float x) noexcept
            {
                const auto y = b0 * x + s1;
                s1 = -a1 * y + s2;
                s2 = b2 * x - a2 * y;
                return y;
            }
            void clear() noexcept { s1 = s2 = 0.0f; }
        };

        VoiceSettings settings;
        float fs = 44100.0f;

        std::array<Segment, SegmentList::capacity> queue;
        int queueHead = 0, queueSize = 0;
        int segmentSamplesLeft = -1;

        Target current, target;
        float formCoef = 0.1f, ampCoef = 0.01f;
        int controlCountdown = 0;

        Resonator nasalPole;
        AntiResonator nasalZero;
        std::array<Resonator, 5> formants;
        std::array<BandPass, 2> fricBands;

        // Source
        float phase = 0.0f;
        float sungHz = 220.0f;
        float tilt = 0.0f;
        float dcIn = 0.0f, dcOut = 0.0f;
        std::uint32_t noiseState = 0x12345678u;
        float gain = 0.0f, gainTarget = 0.0f;

        // Pitch, in MIDI note units
        float pitch = 60.0f, pitchTarget = 60.0f;
        float vibratoPhase = 0.0f;
        int vowelAge = 0;
        float drift = 0.0f, driftTarget = 0.0f;
        int driftCountdown = 0;

        bool holding = false;
        int currentNote = -1;
        Syllable syllable;

        JUCE_LEAK_DETECTOR (FormantVoice)
    };
}
