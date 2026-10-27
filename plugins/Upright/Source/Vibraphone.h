#pragma once

#include "PianoModel.h"
#include "ModalResonator.h"

namespace piano
{
    /** A vibraphone, modelled rather than sampled.

        - Each key is an aluminium bar, undercut so its first three bending modes sit at 1 : 4 : 10,
          plus a faint, untuned torsional mode. The bar rings for tens of seconds in the bass.
        - The yarn mallet is a half-sine force pulse: a harder mallet or a harder stroke is a shorter
          contact, which reaches the upper modes. A restruck bar is hit while still moving.
        - Under each bar hangs a tube tuned to its fundamental: a resonant bandpass that the bar's
          fundamental drives. It adds the warm, slightly late body of the note and drains the bar,
          so a louder tube shortens the sustain.
        - A motor turns a disc in the top of every tube, all on one shaft, opening and closing the
          tubes together: the vibraphone's pulse, heard only in what the tubes add.
        - The felt damper bar takes the notes on release, unless the pedal holds it off.
        - The bars are laid out low to high from left to right.

        Hardness is the mallet, Unison the motor's depth, Resonance the tubes, Noise the mallet
        and damper clicks. */
    class Vibraphone
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
        static constexpr int numModes = 4; // three tuned bending modes and a torsional one

        struct Voice
        {
            std::array<ModalResonator, numModes> modes;
            int numModes = 0;
            ModalResonator tube;

            // The mallet pulse still to be delivered.
            int pulseLeft = 0, pulseLength = 1;
            float pulseAmplitude = 0.0f;

            float lastDisplacement = 0.0f, lastFundamental = 0.0f;
            float velocityScale = 1.0f; // bar displacement per sample to output level
            float gainLeft = 0.7f, gainRight = 0.7f;
            float clickLevel = 0.0f, clickDecay = 0.0f;

            bool sounding = false, held = false, damped = false;
        };

        void damp (Voice&);

        std::array<Voice, numKeys> voices;
        Controls controls;
        double rate = 44100.0;
        bool sustain = false;
        juce::Random random;
        double motorPhase = 0.0;
    };
}
