#include "Vibraphone.h"

namespace piano
{
    namespace
    {
        /** Output level against the modelled voicings at the same velocity. */
        constexpr float outputLevel = 0.08f;

        constexpr double motorHz = 5.0;

        float radiusFor (double t60, double rate) { return ModalResonator::radiusFor (t60, rate); }

        /** 0 at the bottom key, 1 at the top. */
        double place (int midiNote) { return (midiNote - lowestNote) / (double) (numKeys - 1); }
    }

    void Vibraphone::prepare (double sampleRate)
    {
        rate = sampleRate;
        allNotesOff();
    }

    void Vibraphone::noteOn (int midiNote, float velocity01)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        auto& v = voices[(size_t) (midiNote - lowestNote)];
        const auto t = place (midiNote);
        const auto f1 = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
        const auto w1 = juce::MathConstants<double>::twoPi * f1 / rate;

        // The bar rings for half a minute at the bottom of the real instrument (F3), less at the
        // top; the tube draws energy out of the fundamental as it radiates it.
        const auto barT60 = juce::jlimit (1.0, 40.0, controls.decay * 30.0 * std::pow (2.0, -(midiNote - 53) / 18.0))
                            * (1.0 - 0.4 * controls.resonance);
        const auto dampedT60 = 0.25 - 0.1 * t;

        struct ModeSpec { double ratio, t60, weight; };
        const std::array<ModeSpec, numModes> specs {{
            { 1.0,  barT60,                            1.0 },
            { 3.98, juce::jmin (6.0, 0.25 * barT60),  1.0 },
            { 9.85, juce::jmin (1.5, 0.08 * barT60),  0.6 },
            { 6.4,  0.3,                               0.06 }, // torsional, untuned
        }};

        if (! v.sounding)
            v = {};

        v.numModes = 0;
        for (const auto& s : specs)
        {
            const auto w = w1 * s.ratio;
            if (w >= juce::MathConstants<double>::twoPi * 0.45)
                continue;

            auto& m = v.modes[(size_t) v.numModes++];
            m.cosTheta = (float) std::cos (w);
            m.sinTheta = (float) std::sin (w);
            m.heldRadius = radiusFor (s.t60, rate);
            m.dampedRadius = radiusFor (juce::jmin (s.t60, dampedT60), rate);
            m.setRadius (m.heldRadius);
            // Displacement per unit impulse falls as 1/omega; radiating the bar's velocity brings it back.
            m.gain = (float) (std::sin (w) * (w1 / w) * s.weight);
        }

        // The tube: a bandpass at the fundamental with a Q of about 30, unity gain at its peak.
        if (w1 < juce::MathConstants<double>::twoPi * 0.45)
        {
            auto& tube = v.tube;
            const auto r = radiusFor (2.2 * 30.0 / (juce::MathConstants<double>::pi * f1), rate);
            tube.cosTheta = (float) std::cos (w1);
            tube.sinTheta = (float) std::sin (w1);
            tube.heldRadius = tube.dampedRadius = r;
            tube.setRadius (r);
            tube.gain = 2.0f * (1.0f - r) * tube.sinTheta;
        }

        // Yarn over a rubber core: under a millisecond at middling hardness, never longer than
        // about half a period.
        const auto contact = juce::jmin (0.0007 * (1.5 - 1.2 * controls.hardness) * (1.15 - 0.3 * velocity01), 0.45 / f1);
        v.pulseLength = juce::jmax (1, (int) std::round (contact * rate));
        v.pulseLeft = v.pulseLength;

        const auto amplitude = 0.05 + 0.95 * std::pow (velocity01, 1.4);
        v.pulseAmplitude = (float) (amplitude * juce::MathConstants<double>::pi / (2.0 * v.pulseLength));
        v.velocityScale = (float) (1.0 / w1);

        const auto pan = (float) juce::jlimit (0.2, 0.8, 0.5 + 0.25 * (midiNote - 71) / 24.0);
        v.gainLeft = std::cos (pan * juce::MathConstants<float>::halfPi);
        v.gainRight = std::sin (pan * juce::MathConstants<float>::halfPi);

        v.clickLevel = 0.01f * controls.noise * velocity01 * (0.3f + controls.hardness);
        v.clickDecay = radiusFor (0.0008, rate);
        v.sounding = v.held = true;

        if (v.damped)
        {
            v.damped = false;
            for (int i = 0; i < v.numModes; ++i)
                v.modes[(size_t) i].setRadius (v.modes[(size_t) i].heldRadius);
        }
    }

    void Vibraphone::damp (Voice& v)
    {
        if (v.damped)
            return;

        v.damped = true;
        for (int i = 0; i < v.numModes; ++i)
            v.modes[(size_t) i].setRadius (v.modes[(size_t) i].dampedRadius);

        v.clickLevel = juce::jmax (v.clickLevel, 0.003f * controls.noise);
        v.clickDecay = radiusFor (0.005, rate);
    }

    void Vibraphone::noteOff (int midiNote)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        auto& v = voices[(size_t) (midiNote - lowestNote)];
        v.held = false;
        if (v.sounding && ! sustain)
            damp (v);
    }

    void Vibraphone::setSustain (bool on)
    {
        sustain = on;
        if (! on)
            for (auto& v : voices)
                if (v.sounding && ! v.held)
                    damp (v);
    }

    void Vibraphone::allNotesOff()
    {
        for (auto& v : voices)
            v = {};
        sustain = false;
    }

    void Vibraphone::render (float* left, float* right, int numSamples)
    {
        constexpr int chunk = 64;
        std::array<float, chunk> motor {};

        const auto depth = controls.unison;
        const auto tubeLevel = 3.0f * controls.resonance;
        const auto phaseStep = juce::MathConstants<double>::twoPi * motorHz / rate;

        for (int start = 0; start < numSamples; start += chunk)
        {
            const auto n = juce::jmin (chunk, numSamples - start);

            // The discs, all on one shaft: fully open, the tube speaks; closed, it barely does.
            for (int i = 0; i < n; ++i)
            {
                motor[(size_t) i] = tubeLevel * (1.0f - depth * 0.5f * (1.0f - (float) std::cos (motorPhase)));
                motorPhase += phaseStep;
                if (motorPhase > juce::MathConstants<double>::twoPi)
                    motorPhase -= juce::MathConstants<double>::twoPi;
            }

            for (auto& v : voices)
            {
                if (! v.sounding)
                    continue;

                const auto gl = v.gainLeft * outputLevel, gr = v.gainRight * outputLevel;

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

                    // The bar radiates its velocity; the tube is driven by the fundamental's.
                    const auto fundamental = v.modes[0].y1;
                    auto out = (displacement - v.lastDisplacement) * v.velocityScale;
                    out += motor[(size_t) i] * v.tube.process ((fundamental - v.lastFundamental) * v.velocityScale);
                    v.lastDisplacement = displacement;
                    v.lastFundamental = fundamental;

                    if (v.clickLevel > 1.0e-6f)
                    {
                        out += v.clickLevel * (random.nextFloat() * 2.0f - 1.0f);
                        v.clickLevel *= v.clickDecay;
                    }

                    left[start + i] += out * gl;
                    right[start + i] += out * gr;
                }

                if (v.pulseLeft == 0 && v.clickLevel <= 1.0e-6f)
                {
                    auto energy = v.tube.energy();
                    for (int m = 0; m < v.numModes; ++m)
                        energy += v.modes[(size_t) m].energy();
                    if (energy < 1.0e-9f)
                        v = {};
                }
            }
        }
    }
}
