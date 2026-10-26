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
    - Loud notes stretch their strings, raising the tension: the pitch starts a little sharp and
      settles, and the stretching adds "phantom" partials at sums of the string's frequencies,
      strongest near the string's longitudinal resonance. This is the bite in a hard bass note.
    - Every key whose damper is off (held, pedalled, or above the dampers) has a resonator that
      the bridge drives: the open strings ring along with what is played.
    - The action makes noise: the key hits the keybed, it knocks back up, dampers land, and the
      pedal lifts and drops the whole damper rail (setting the open strings faintly ringing).
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
        double tension = 0.0;       // pitch rise, as a fraction, per unit of normalised string energy
        double phantom = 0.0;       // level of the phantom partials relative to the string at mf
        double longitudinalFrequency = 1000.0;
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
        float hardness = 0.5f;  // 0..1
        float unison = 0.3f;    // 0..1
        float decay = 1.0f;     // T60 scale
        float resonance = 0.5f; // 0..1, sympathetic resonance
        float noise = 0.5f;     // 0..1, action noise
    };

    /** Bridge velocity times the key's output gain, for a key struck at velocity 0.7: about the
        peak each key reaches. The tension and phantom partials are scaled from it. */
    constexpr float referenceLevel = 0.04f;

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

        /** Adds this key's bridge velocity to `bridgeOut`, and what the board radiates from it
            (levelled, with the phantom partials) to `radiatedOut`. */
        void render (float* bridgeOut, float* radiatedOut, int numSamples);

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

        // Tension modulation and phantom partials.
        float energy = 0.0f, energyCoef = 0.0f;
        float tensionSamples = 0.0f, maxShift = 0.0f;
        float phantomLow = 0.0f, phantomLowCoef = 0.0f;
        float bandA1 = 0.0f, bandA2 = 0.0f, bandA3 = 0.0f, bandK = 1.0f, bandIc1 = 0.0f, bandIc2 = 0.0f;
        juce::Random random;
    };

    //==============================================================================
    /** A key's strings as the other keys hear them: one loop, without dispersion, that the bridge
        drives while the key's damper is off. */
    class Resonator
    {
    public:
        void prepare (const KeySpec&, const LoopDesign&, double sampleRate);
        void reset();

        /** Drives the loop with `input` (bridge velocity) and adds what it radiates to `radiatedOut`. */
        void render (const float* input, float* radiatedOut, int numSamples, bool undamped, float drive, float decay);

        bool isQuiet() const noexcept { return quietSamples > loopLength; }

    private:
        std::vector<float> line;
        int mask = 0, pos = 0, delay = 1, loopLength = 0, quietSamples = 0;
        float tuneCoef = 0.0f, tuneState = 0.0f;
        float lossPole = 0.0f, lossState = 0.0f, lossGain = 1.0f, lossScale = 1.0f;
        float damper = 1.0f, damperCoef = 0.0f, damperLoopGain = 1.0f;
        float outputGain = 0.0f;
        double frequency = 440.0, sustainT60 = 10.0, rate = 48000.0, tunedDecay = 0.0;
    };

    //==============================================================================
    /** Knocks and thumps from the action: short filtered noise bursts and low thumps. */
    class ActionNoise
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void setAmount (float newAmount) noexcept { amount = newAmount; }

        void keyDown (int midiNote, float velocity01);
        void keyUp (int midiNote, bool damperLands);
        void pedal (bool down);

        /** Adds the noises to `radiatedOut`, and the rush of the damper rail lifting off every
            string to `stringsOut`, which drives the resonators. */
        void render (float* radiatedOut, float* stringsOut, int numSamples);
        bool isActive() const noexcept;

    private:
        struct Sound
        {
            double waitMs = 0.0;
            double noiseHz = 1000.0, noiseQ = 1.0, noiseMs = 5.0, noiseLevel = 0.0;
            double thumpHz = 100.0, thumpMs = 20.0, thumpLevel = 0.0;
            bool toStrings = false;
        };

        struct Event
        {
            int wait = 0, remaining = 0;
            bool toStrings = false;
            float noise = 0.0f, noiseDecay = 0.0f;
            float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
            float thump = 0.0f, thumpDecay = 0.0f, cosW = 1.0f, sinW = 0.0f, re = 1.0f, im = 0.0f;
        };

        void add (const Sound&);

        std::array<Event, 24> events {};
        juce::Random random { 1729 };
        double rate = 48000.0;
        float amount = 1.0f;
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
        /** `audible` is false for an engine that only follows the pedal for notes already ringing. */
        void setSustain (bool on, bool audible = true);
        void setSoftPedal (bool on) noexcept { soft = on; }
        void allNotesOff();

        /** Adds the summed bridge velocity of all keys (mono) to `out`. */
        void render (float* out, int numSamples);

        static constexpr int maxActiveKeys = 48;

    private:
        void updateDampers();

        /** Keys and resonators couple through the bridge in steps this long. */
        static constexpr int subBlock = 32;

        std::array<Key, numKeys> keys;
        std::array<Resonator, numKeys> resonators;
        std::array<bool, numKeys> down {}; // the key is held, whether or not its strings still sound
        std::array<std::array<float, subBlock>, numKeys> keyBridge {};
        ActionNoise noise;
        Hammer hammer;
        Controls controls;
        Voicing voicing = Voicing::mills;
        double rate = 0.0;
        bool sustain = false, soft = false;
    };
}
