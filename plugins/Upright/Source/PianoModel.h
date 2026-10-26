#pragma once

#include <at_shared/at_shared.h>

/** Physically modelled upright piano: no samples.

    Each key has one to three strings, each a digital waveguide loop:

        delay line -> loss filter (one-pole) -> dispersion (cascade of first-order allpasses)
                   -> tuning allpass -> bridge

    - The dispersion allpasses are fitted per key so the partials go sharp the way a stiff string's
      do (f_k = k f1 sqrt ((1 + B k^2) / (1 + B))).
    - The hammer is a mass on a nonlinear felt spring (F = K x^p) pressing on an ideal string, run
      once per strike at 4x the sample rate. Its force pulse, combed by the reflection from the
      near end of the string, excites the loops. Harder hits compress the felt further, which is
      stiffer, so the pulse gets shorter and the tone brighter.
    - The strings of one key meet at a resistive bridge. The in-phase motion drives the bridge and
      dies fast (the prompt sound); out-of-phase motion barely moves it and rings on (the
      aftersound), so slightly mistuned unisons give the piano's two-stage decay and slow beating.
    - The bridge velocity of all keys goes through a soundboard: a synthetic modal impulse response.

    Two voicings: a clean, freshly tuned studio upright, and "Mills", after the Abbey Road
    Studio Two upright: hard, worn hammers, wide honky-tonk unisons, keys a little off from each
    other, shorter sustain and a boxy, forward cabinet. */
namespace piano
{
    constexpr int lowestNote = 21;
    constexpr int highestNote = 108;
    constexpr int numKeys = highestNote - lowestNote + 1;
    constexpr int maxStrings = 3;
    constexpr int maxDispersionStages = 16;

    enum class Voicing { studio, mills };

    /** One key's strings and hammer, interpolated from published measurements of real pianos. */
    struct KeySpec
    {
        int numStrings = 3;
        double frequency = 440.0;   // first partial in Hz, stretch-tuned
        double inharmonicity = 0.0; // B
        double impedance = 2.0;     // string wave impedance sqrt (T mu), kg/s
        double hammerMass = 0.003;  // kg
        double feltStiffness = 1e9; // K in N/m^p
        double feltExponent = 2.5;  // p
        double strikePosition = 0.12; // from the agraffe, as a fraction of the speaking length
        double sustainT60 = 10.0;   // aftersound decay of the fundamental, s
        double promptT60 = 2.0;     // decay of the in-phase mode, s
        double highT60 = 1.0;       // decay of a partial at highFrequency, s
        double highFrequency = 3000.0;
        std::array<double, maxStrings> unisonCents {}; // fixed per-key detune, before the Unison control
        double outputGain = 1.0;    // evens out loudness across the keyboard (what the board radiates)
        bool hasDamper = true;
    };

    KeySpec specFor (int midiNote, Voicing);

    /** The parts of a key's loop that depend only on the key and sample rate. */
    struct LoopDesign
    {
        int stages = 0;
        double dispersionCoef = 0.0;
        double lossPole = 0.0;
    };

    LoopDesign designLoop (const KeySpec&, double sampleRate);

    /** A stereo soundboard + cabinet impulse response, built from damped modes. */
    juce::AudioBuffer<float> makeSoundboard (double sampleRate, Voicing);

    struct Controls
    {
        float hardness = 0.5f; // 0..1
        float unison = 0.3f;   // 0..1
        float decay = 1.0f;    // T60 scale
    };

    //==============================================================================
    /** Runs the hammer against an ideal string and records the contact force. */
    class Hammer
    {
    public:
        void prepare (double sampleRate);

        /** Writes the force pulse (N) at the sample rate into `force`; returns its length. */
        int strike (const KeySpec&, double periodSamples, double velocity, float hardness,
                    float* force, int maxLength);

        static constexpr int oversampling = 4;

    private:
        double rate = 48000.0;
        std::vector<double> left, right;
        int mask = 0;
    };

    //==============================================================================
    class Key
    {
    public:
        void prepare (const KeySpec&, const LoopDesign&, double sampleRate);
        void strike (Hammer&, double velocity, const Controls&);
        void reset();

        /** Adds this key's bridge velocity to `out`. */
        void render (float* out, int numSamples);

        bool active = false;
        bool held = false;
        bool dampersOn = false; // set by the engine from key, pedal and stealing
        bool fastRelease = false;
        float level = 0.0f;
        int age = 0;

        const KeySpec& getSpec() const noexcept { return spec; }

        /** True once nothing audible has gone round the loop for a whole period. A single quiet
            block isn't enough: just after a strike the wave is still travelling down the line. */
        bool isSilent() const noexcept { return quietSamples > loopLength; }

    private:
        struct String
        {
            std::vector<float> line;
            int mask = 0, pos = 0, delay = 1;
            float lossGain = 1.0f, lossPole = 0.0f, lossState = 0.0f;
            float apCoef = 0.0f;
            std::array<float, maxDispersionStages> apState {};
            float tuneCoef = 0.0f, tuneState = 0.0f;
            float exciteGain = 1.0f;
            float damperLoopGain = 1.0f;
        };

        void tuneString (String&, double frequency, double t60);

        KeySpec spec;
        LoopDesign design;
        double rate = 48000.0;
        std::array<String, maxStrings> strings;
        int numStrings = 1;
        float coupling = 0.0f;
        float damper = 0.0f;
        float damperCoef = 0.0f;

        std::vector<float> force;
        int forceLength = 0, forcePos = 0, combDelay = 1;
        int quietSamples = 0, loopLength = 0;
        juce::Random random;
    };

    //==============================================================================
    class Engine
    {
    public:
        void prepare (double sampleRate, Voicing);
        Voicing getVoicing() const noexcept { return voicing; }

        void setControls (const Controls& c) noexcept { controls = c; }
        void noteOn (int midiNote, float velocity01);
        void noteOff (int midiNote);
        void setSustain (bool down) noexcept { sustain = down; }
        void setSoftPedal (bool down) noexcept { soft = down; }
        void allNotesOff();

        /** Adds the summed bridge velocity of all keys (mono) to `out`. */
        void render (float* out, int numSamples);

        static constexpr int maxActiveKeys = 48;

    private:
        void updateDampers();

        std::array<Key, numKeys> keys;
        Hammer hammer;
        Controls controls;
        Voicing voicing = Voicing::mills;
        double rate = 0.0;
        bool sustain = false, soft = false;
    };
}
