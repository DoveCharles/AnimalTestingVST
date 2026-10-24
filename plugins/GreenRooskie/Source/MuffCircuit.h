#pragma once

#include <at_shared/at_shared.h>

/** One channel of a 90s "green Russian" four-transistor fuzz, stage by stage:

      input booster -> Sustain -> clipping stage 1 -> clipping stage 2 -> passive tone stack -> recovery

    Each clipping stage is a common-emitter amp with a pair of silicon diodes in its feedback loop,
    which gives the soft, compressed clipping (modelled with asinh) and a high-frequency roll-off from
    the feedback cap. The tone stack is the classic passive low-pass / high-pass blend; the green
    Russian values put the two corners close together, so the mid scoop is shallower and the low end
    fatter than on the American versions. Runs at the oversampled rate. */
class MuffCircuit
{
public:
    void prepare (double sampleRate) noexcept
    {
        inputCoupling.prepare (sampleRate, 25.0f);
        booster.prepare (sampleRate, 30.0f, 12000.0f);
        clip1.prepare (sampleRate, 45.0f, 4500.0f);
        clip2.prepare (sampleRate, 45.0f, 3400.0f);
        toneLowPass.prepare (sampleRate, 450.0f);
        toneHighPass.prepare (sampleRate, 1300.0f);
        recovery.prepare (sampleRate, 20.0f, 9000.0f);
        outputCoupling.prepare (sampleRate, 10.0f);
        reset();
    }

    void reset() noexcept
    {
        inputCoupling.reset();
        booster.reset();
        clip1.reset();
        clip2.reset();
        toneLowPass.reset();
        toneHighPass.reset();
        recovery.reset();
        outputCoupling.reset();
    }

    /** sustainGain is the linear gain of the Sustain pot; tone is 0 (bass) to 1 (treble). */
    float process (float x, float sustainGain, float tone) noexcept
    {
        x = inputCoupling.highPass (x);

        // Input booster: a clean ~12 dB transistor stage that only saturates on very hot signals.
        x = booster.run (x, 4.0f, [] (float v) { return 3.0f * transistor (v / 3.0f); });

        x *= sustainGain;

        // Two cascaded diode-feedback clipping stages: this is where the fuzz and sustain come from.
        x = clip1.run (x, 9.0f, [] (float v) { return 0.55f * diodePair (v); });
        x = clip2.run (x, 9.0f, [] (float v) { return 0.55f * diodePair (v); });

        // Passive tone stack: one pot blends a low-pass and a high-pass of the same signal.
        const auto low  = toneLowPass.lowPass (x);
        const auto high = toneHighPass.highPass (x);
        x = low + tone * (high - low);

        // Recovery stage makes up the tone stack's loss, with a little transistor colour of its own.
        x = recovery.run (x, 2.5f, [] (float v) { return 1.5f * transistor (v / 1.5f); });

        // Output cap: strips the DC the asymmetric clipping leaves behind.
        return outputCoupling.highPass (x);
    }

private:
    /** Zero-delay-feedback one-pole filter; gives both outputs from one state. */
    struct OnePole
    {
        void prepare (double sampleRate, float cutoffHz) noexcept
        {
            const auto g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
            coefficient = g / (1.0f + g);
        }

        void reset() noexcept { state = 0.0f; }

        float lowPass (float x) noexcept
        {
            const auto v = coefficient * (x - state);
            const auto y = v + state;
            state = y + v;
            return y;
        }

        float highPass (float x) noexcept { return x - lowPass (x); }

        float coefficient = 0.0f, state = 0.0f;
    };

    /** A transistor gain stage: coupling cap in, gain + nonlinearity, then the stage's treble roll-off. */
    struct Stage
    {
        void prepare (double sampleRate, float couplingHz, float rollOffHz) noexcept
        {
            coupling.prepare (sampleRate, couplingHz);
            rollOff.prepare (sampleRate, rollOffHz);
        }

        void reset() noexcept
        {
            coupling.reset();
            rollOff.reset();
        }

        template <typename Shaper>
        float run (float x, float gain, Shaper&& shape) noexcept
        {
            return rollOff.lowPass (shape (gain * coupling.highPass (x)));
        }

        OnePole coupling, rollOff;
    };

    /** Anti-parallel silicon diodes in the feedback loop: logarithmic, never a hard ceiling.
        The small offset is the transistor's bias, which adds a touch of even-harmonic warmth. */
    static float diodePair (float x) noexcept
    {
        constexpr auto bias = 0.12f;
        return std::asinh (x + bias) - std::asinh (bias);
    }

    /** Soft, slightly asymmetric transistor saturation with unity-ish slope at zero. */
    static float transistor (float x) noexcept
    {
        constexpr auto bias = 0.2f;
        return std::tanh (x + bias) - std::tanh (bias);
    }

    OnePole inputCoupling, toneLowPass, toneHighPass, outputCoupling;
    Stage booster, clip1, clip2, recovery;
};
