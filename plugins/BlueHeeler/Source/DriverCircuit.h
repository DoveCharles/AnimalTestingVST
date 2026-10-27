#pragma once

#include <at_shared/at_shared.h>

/** One channel of a Blues Driver-style overdrive (after the BD-2W), stage by stage:

      input buffer -> gain stage 1 -> gain stage 2 -> active treble control -> output buffer

    Both gain stages hang off one dual-gang Gain pot, so they rise together. Stage 1 clips softly
    through diodes in its feedback loop: logarithmic, no hard ceiling, so picking dynamics survive.
    Stage 2 runs into the rails of its discrete op-amp, which rounds the peaks off and gives the
    amp-like compression once the gain is up. Each stage's bias is slightly off centre, which adds
    the even harmonics that make it sound like an amp rather than a pedal. The tone control is a
    treble shelf that cuts or boosts above about 1 kHz, flat at noon.

    The two modes follow the W's switch:
      Standard  the classic voicing: the coupling caps trim the lows before they reach the clippers
      Custom    bigger coupling caps let more bass into the clipping, the stages roll off a little
                lower and clip a little rounder, for a fuller, smoother, longer-sustaining tone

    Runs at the oversampled rate. */
class DriverCircuit
{
public:
    void prepare (double sampleRate, bool custom) noexcept
    {
        const auto& v = custom ? customVoicing : standardVoicing;

        inputCoupling.prepare (sampleRate, 20.0f);
        stage1.prepare (sampleRate, v.stage1LowCutHz, v.stage1RollOffHz);
        stage2.prepare (sampleRate, v.stage2LowCutHz, v.stage2RollOffHz);
        toneSplit.prepare (sampleRate, 1100.0f);
        outputRollOff.prepare (sampleRate, v.outputRollOffHz);
        outputCoupling.prepare (sampleRate, 10.0f);
        stage2Knee = v.stage2Knee;
    }

    void reset() noexcept
    {
        inputCoupling.reset();
        stage1.reset();
        stage2.reset();
        toneSplit.reset();
        outputRollOff.reset();
        outputCoupling.reset();
    }

    /** stageGain is the linear gain of each half of the Gain pot; trebleGain is the shelf's gain. */
    float process (float x, float stageGain, float trebleGain) noexcept
    {
        x = inputCoupling.highPass (x);

        x = stage1.run (x, stageGain, [] (float v) { return diodeFeedback (v); });

        const auto knee = stage2Knee;
        x = stage2.run (x, stageGain, [knee] (float v) { return knee * railClip (v / knee); });

        const auto low = toneSplit.lowPass (x);
        x = low + trebleGain * (x - low);

        x = outputRollOff.lowPass (x);

        // Output cap: strips the DC the off-centre bias leaves behind.
        return outputCoupling.highPass (x);
    }

private:
    struct Voicing
    {
        float stage1LowCutHz, stage1RollOffHz;
        float stage2LowCutHz, stage2RollOffHz;
        float outputRollOffHz;
        float stage2Knee;
    };

    static constexpr Voicing standardVoicing { 110.0f, 7500.0f, 70.0f, 5600.0f, 9000.0f, 1.0f };
    static constexpr Voicing customVoicing   {  45.0f, 6500.0f, 30.0f, 4600.0f, 7500.0f, 1.25f };

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

    /** A gain stage: coupling cap in, gain + nonlinearity, then the feedback cap's treble roll-off. */
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

    /** Diodes in the feedback loop: clean below the knee, then logarithmic, never a hard ceiling. */
    static float diodeFeedback (float x) noexcept
    {
        constexpr auto bias = 0.08f;
        return std::asinh (x + bias) - std::asinh (bias);
    }

    /** The discrete op-amp running into its rails: rounder than the diodes and with a ceiling. */
    static float railClip (float x) noexcept
    {
        constexpr auto bias = 0.12f;
        return std::tanh (x + bias) - std::tanh (bias);
    }

    OnePole inputCoupling, toneSplit, outputRollOff, outputCoupling;
    Stage stage1, stage2;
    float stage2Knee = 1.0f;
};
