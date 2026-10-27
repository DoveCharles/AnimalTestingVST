#include "RhodesPiano.h"

namespace piano
{
    namespace
    {
        /** Output level against the modelled voicings at the same velocity, and the preamp's drive. */
        constexpr float outputLevel = 0.2f;
        constexpr float preampDrive = 2.5f;

        /** The keys' acoustic noise against the amplified sound. */
        constexpr float keyNoiseLevel = 2.5f;

        constexpr double speakerLowHz = 70.0, speakerHighHz = 6000.0;
        constexpr double tremoloHz = 4.2;

        float radiusFor (double t60, double rate) { return ModalResonator::radiusFor (t60, rate); }

        /** 0 at the bottom key, 1 at the top. */
        double place (int midiNote) { return (midiNote - lowestNote) / (double) (numKeys - 1); }
    }

    void RhodesPiano::prepare (double sampleRate)
    {
        rate = sampleRate;
        highPassCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * speakerLowHz / sampleRate));
        lowPassCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (speakerHighHz, 0.4 * sampleRate) / sampleRate));
        keyNoise.prepare (sampleRate);
        allNotesOff();
    }

    void RhodesPiano::noteOn (int midiNote, float velocity01)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        keyNoise.setAmount (2.0f * controls.noise);
        keyNoise.keyDown (midiNote, velocity01);

        auto& v = voices[(size_t) (midiNote - lowestNote)];
        const auto t = place (midiNote);
        const auto f1 = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
        const auto w1 = juce::MathConstants<double>::twoPi * f1 / rate;

        // The tine and tonebar ring long in the bass and briefly at the top. The bell partials of
        // the cantilever sit well above the fundamental, closer to it on the short treble tines.
        const auto fundamentalT60 = juce::jlimit (0.8, 30.0, controls.decay * 18.0 * std::pow (2.0, -(midiNote - 28) / 20.0));
        const auto bellRatio = 7.0 - 2.2 * t;
        const auto bellT60 = juce::jlimit (0.06, 0.9, 0.9 * std::pow (2.0, -(midiNote - 28) / 24.0));
        const auto dampedT60 = 0.2 - 0.1 * t;

        struct ModeSpec { double ratio, t60, weight; };
        const std::array<ModeSpec, numModes> specs {{
            { 1.0,                   fundamentalT60,       1.0 },
            { 1.0006,                1.5 * fundamentalT60, -0.6 * controls.resonance }, // swells in as the tine's beat with it turns
            { bellRatio,             bellT60,              0.3 },
            { bellRatio * 2.8,       0.25 * bellT60,       0.12 },
        }};

        // A key still ringing keeps its motion: the hammer hits a moving tine.
        if (! v.sounding)
            v = {};

        v.numModes = 0;
        for (const auto& s : specs)
        {
            const auto w = w1 * s.ratio;
            if (w >= juce::MathConstants<double>::twoPi * 0.45)
                break;

            auto& m = v.modes[(size_t) v.numModes++];
            m.cosTheta = (float) std::cos (w);
            m.sinTheta = (float) std::sin (w);
            m.heldRadius = radiusFor (s.t60, rate);
            m.dampedRadius = radiusFor (juce::jmin (s.t60, dampedT60), rate);
            m.setRadius (m.heldRadius);
            // Displacement per unit impulse falls as 1/omega; the pickup's derivative brings it back.
            m.gain = (float) (std::sin (w) * (w1 / w) * s.weight);
        }

        // The neoprene tip: shorter contact when hit harder or voiced harder, and never longer than
        // about half a period, as the treble hammers are made harder to keep the top keys speaking.
        const auto contact = juce::jmin (0.0022 * (1.35 - 0.7 * velocity01) * (1.25 - 0.5 * controls.hardness), 0.45 / f1);
        v.pulseLength = juce::jmax (1, (int) std::round (contact * rate));
        v.pulseLeft = v.pulseLength;

        // The swing in pole distances: bass tines are longer and move further. Hardness brings the
        // tine closer to its pickup, so the same strike swings further relative to it.
        const auto keySwing = 1.15 - 0.55 * t;
        const auto closeness = 0.6 + 0.8 * controls.hardness;
        const auto swing = keySwing * closeness * (0.06 + 1.25 * std::pow (velocity01, 1.4));
        v.pulseAmplitude = (float) (swing * juce::MathConstants<double>::pi / (2.0 * v.pulseLength));

        // Each tine is set a little differently off its pole.
        v.offset = 0.32f + 0.08f * (float) std::sin (midiNote * 2.39996);
        const auto slope = v.offset / std::pow (v.offset * v.offset + 1.0f, 1.5f);
        v.normalise = (float) (1.0 / (w1 * slope * closeness * std::sqrt (keySwing)));
        if (! v.sounding)
            v.lastFlux = fluxAt (v.offset);

        v.clickLevel = 0.02f * controls.noise * velocity01;
        v.clickDecay = radiusFor (0.0015, rate);
        v.sounding = v.held = true;
        v.damped = false;
    }

    void RhodesPiano::damp (Voice& v)
    {
        if (v.damped)
            return;

        v.damped = true;
        for (int i = 0; i < v.numModes; ++i)
            v.modes[(size_t) i].setRadius (v.modes[(size_t) i].dampedRadius);

        v.clickLevel = juce::jmax (v.clickLevel, 0.004f * controls.noise);
        v.clickDecay = radiusFor (0.004, rate);
    }

    void RhodesPiano::noteOff (int midiNote)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        auto& v = voices[(size_t) (midiNote - lowestNote)];
        keyNoise.setAmount (2.0f * controls.noise);
        keyNoise.keyUp (midiNote, v.sounding && ! v.damped && ! sustain);

        v.held = false;
        if (v.sounding && ! sustain)
            damp (v);
    }

    void RhodesPiano::setSustain (bool on)
    {
        if (on != sustain)
        {
            keyNoise.setAmount (2.0f * controls.noise);
            keyNoise.pedal (on);
        }

        sustain = on;
        if (! on)
            for (auto& v : voices)
                if (v.sounding && ! v.held)
                    damp (v);
    }

    void RhodesPiano::allNotesOff()
    {
        for (auto& v : voices)
            v = {};
        sustain = false;
        keyNoise.reset();
        highPassState = lowPass1 = lowPass2 = 0.0f;
    }

    void RhodesPiano::render (float* left, float* right, int numSamples)
    {
        constexpr int chunk = 64;
        std::array<float, chunk> mono {}, room {}, unused {};

        for (int start = 0; start < numSamples; start += chunk)
        {
            const auto n = juce::jmin (chunk, numSamples - start);
            std::fill (mono.begin(), mono.end(), 0.0f);
            std::fill (room.begin(), room.end(), 0.0f);
            std::fill (unused.begin(), unused.end(), 0.0f);
            if (keyNoise.isActive())
                keyNoise.render (room.data(), unused.data(), n);

            for (auto& v : voices)
            {
                if (! v.sounding)
                    continue;

                for (int i = 0; i < n; ++i)
                {
                    float force = 0.0f;
                    if (v.pulseLeft > 0)
                    {
                        const auto k = v.pulseLength - v.pulseLeft--;
                        force = v.pulseAmplitude * std::sin (juce::MathConstants<float>::pi * ((float) k + 0.5f) / (float) v.pulseLength);
                    }

                    float displacement = 0.0f;
                    for (int m = 0; m < v.numModes; ++m)
                        displacement += v.modes[(size_t) m].process (force);

                    // The coil hears the change in flux.
                    const auto flux = fluxAt (v.offset + displacement);
                    auto out = (flux - v.lastFlux) * v.normalise;
                    v.lastFlux = flux;

                    if (v.clickLevel > 1.0e-6f)
                    {
                        out += v.clickLevel * (random.nextFloat() * 2.0f - 1.0f);
                        v.clickLevel *= v.clickDecay;
                    }

                    mono[(size_t) i] += out;
                }

                if (v.pulseLeft == 0 && v.clickLevel <= 1.0e-6f)
                {
                    float energy = 0.0f;
                    for (int m = 0; m < v.numModes; ++m)
                        energy += v.modes[(size_t) m].energy();
                    if (energy < 1.0e-9f)
                        v = {};
                }
            }

            // The Suitcase: preamp, speaker, and the tremolo panning between its two sides.
            const auto depth = controls.unison;
            const auto phaseStep = juce::MathConstants<double>::twoPi * tremoloHz / rate;
            for (int i = 0; i < n; ++i)
            {
                auto x = std::tanh (preampDrive * outputLevel * mono[(size_t) i]) / preampDrive;

                highPassState += highPassCoef * (x - highPassState);
                x -= highPassState;
                lowPass1 += lowPassCoef * (x - lowPass1);
                lowPass2 += lowPassCoef * (lowPass1 - lowPass2);

                const auto pan = (float) std::sin (tremoloPhase);
                const auto keys = keyNoiseLevel * room[(size_t) i];
                left[start + i] += lowPass2 * (1.0f - 0.5f * depth * (1.0f + pan)) + keys;
                right[start + i] += lowPass2 * (1.0f - 0.5f * depth * (1.0f - pan)) + keys;

                tremoloPhase += phaseStep;
                if (tremoloPhase > juce::MathConstants<double>::twoPi)
                    tremoloPhase -= juce::MathConstants<double>::twoPi;
            }
        }
    }
}
