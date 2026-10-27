#pragma once

#include "PianoModel.h"

namespace piano
{
    /** A tine electric piano, modelled rather than sampled.

        - Each key is a tine and tonebar: an asymmetric tuning fork. The tine's tip is a few damped
          modes (the fundamental and the cantilever's inharmonic bell partials, far above it), and
          the tonebar a second mode at the fundamental, a hair off, that the tine feeds as it rings.
        - The neoprene hammer tip is a half-sine force pulse: harder or louder strikes make it
          shorter, so it excites more of the bell partials. A restruck tine is hit while it is
          still moving, so every key keeps one voice.
        - The electromagnetic pickup senses the rate of change of flux through its coil, and the
          flux falls off with the tip's distance from the pole piece. The tip sits a little off
          the pole's axis, so small swings give a clean tone with some second harmonic and big
          ones swing past the axis and "bark".
        - Felt dampers take a key's tines quickly on release.
        - The keys' sum goes through a Suitcase-style preamp that rounds off peaks, a speaker's
          band limit, and its stereo tremolo, which pans the sound between the two sides.

        Hardness moves the tines towards their pickups (more bark), Unison is the tremolo depth,
        Resonance the tonebar's share, and Noise the hammer and damper clicks. */
    class RhodesPiano
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
        static constexpr int numModes = 4; // fundamental, tonebar, and two bell partials

        /** A two-pole resonator: y = a1 y1 - a2 y2 + gain x, with a1 = 2 r cos theta, a2 = r^2. */
        struct Mode
        {
            float cosTheta = 1.0f, sinTheta = 0.0f;
            float heldRadius = 0.0f, dampedRadius = 0.0f; // per-sample decay while ringing and once damped
            float a1 = 0.0f, a2 = 0.0f, gain = 0.0f;
            float y1 = 0.0f, y2 = 0.0f;

            void setRadius (float r) noexcept { a1 = 2.0f * r * cosTheta; a2 = r * r; }

            /** The squared amplitude of the free oscillation. */
            float energy() const noexcept
            {
                return (y1 * y1 + y2 * y2 - 2.0f * cosTheta * y1 * y2) / (sinTheta * sinTheta);
            }
        };

        struct Voice
        {
            std::array<Mode, numModes> modes;
            int numModes = 0;

            // The hammer pulse still to be delivered.
            int pulseLeft = 0, pulseLength = 1;
            float pulseAmplitude = 0.0f;

            float offset = 0.35f;       // the tip's rest position off the pole axis, in pole distances
            float normalise = 1.0f;     // pickup output to a level that is even across the keys
            float lastFlux = 0.0f;
            float clickLevel = 0.0f, clickDecay = 0.0f;

            bool sounding = false, held = false, damped = false;
        };

        void damp (Voice&);
        static float fluxAt (float y) noexcept { return 1.0f / std::sqrt (y * y + 1.0f); }

        std::array<Voice, numKeys> voices;
        Controls controls;
        double rate = 44100.0;
        bool sustain = false;
        juce::Random random;

        // The Suitcase after the keys.
        float highPassState = 0.0f, highPassCoef = 0.0f;
        float lowPass1 = 0.0f, lowPass2 = 0.0f, lowPassCoef = 0.0f;
        double tremoloPhase = 0.0;
    };
}
