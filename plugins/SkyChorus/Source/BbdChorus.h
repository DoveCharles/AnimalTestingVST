#pragma once

#include <at_shared/at_shared.h>

/** The wet path of the Ibanez SC10 (MN3207 bucket brigade clocked by an MN3102, swept by a
    triangle LFO):

      input filter -> soft clip + hiss -> BBD at its own clock -> reconstruction filters -> coupling cap

    (all of it 4x oversampled except the coupling cap)

    The bucket brigade is run as the chip runs it, not as a delay line read at a moving tap. Each
    clock tick samples the input into the first bucket and passes every charge one step along;
    a sample comes out 512 ticks later (1024 stages, two per tick) and is held until the next
    tick. The LFO sweeps the clock, so the delay is 512 / clock and the pitch wobble falls out of
    the buckets being filled at one rate and emptied at another, the same way it does in the pedal.
    It also keeps the chip's sampling: at long delays the clock drops into the mid 20 kHz range
    and whatever the input filter lets through above half of it folds back down as faint grit.

    The sweep is lopsided: the delay stretches further on the slow half of the clock swing than it
    shrinks on the fast half. That is part of what gives these pedals their slightly seasick wobble. */
class BbdChorus
{
public:
    /** MN3207: 1024 stages, so a charge takes 512 clock periods to get through. */
    static constexpr int ticksThrough = 512;

    /** Width 1 swings the clock by this fraction either side of its centre. */
    static constexpr float maxClockSwing = 0.45f;

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;
        inputFilter.prepare (sampleRate * (float) oversampling, 7500.0f, 0.75f);
        outputFilter1.prepare (sampleRate * (float) oversampling, 6800.0f, 0.6f);
        outputFilter2.prepare (sampleRate * (float) oversampling, 6800.0f, 1.0f);
        couplingCoefficient = std::exp (-juce::MathConstants<float>::twoPi * couplingHz / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        buckets.fill (0.0f);
        bucketIndex = 0;
        clockPhase = 0.0f;
        previousX = previousInput = held = lastOut = 0.0f;
        couplingIn = couplingOut = 0.0f;
        lfoPhase = 0.0f;
        inputFilter.reset();
        outputFilter1.reset();
        outputFilter2.reset();
    }

    /** Returns the wet signal only. delayMs is the centre delay (Delay Time knob), width 0 to 1. */
    float process (float x, float rateHz, float width, float delayMs) noexcept
    {
        lfoPhase += rateHz / sampleRate;
        lfoPhase -= std::floor (lfoPhase);
        const auto triangle = 4.0f * std::abs (lfoPhase - 0.5f) - 1.0f;

        const auto clockHz = (float) ticksThrough / (delayMs * 0.001f) * (1.0f + maxClockSwing * width * triangle);
        const auto ticksPerStep = clockHz / (sampleRate * (float) oversampling);

        // Everything from the input filter to the reconstruction filters runs at the oversampled
        // rate, so the input filter clears our own interpolation images before the chip samples
        // them, and the reconstruction filters clear the chip's before they could fold at ours.
        auto output = 0.0f;
        for (int step = 0; step < oversampling; ++step)
        {
            // The chip clips softly, a little lopsided, well before the rails, and leaks a little hiss.
            const auto upsampled = previousX + (float) (step + 1) / (float) oversampling * (x - previousX);
            const auto input = saturate (inputFilter.lowPass (upsampled)) + hissLevel * (random.nextFloat() * 2.0f - 1.0f);

            // Ticks that land inside this step, at fractions t of it. Each one samples the input
            // (interpolated to the tick's time) into the first bucket and releases the oldest charge.
            // The step's output is the held staircase averaged over the step.
            auto staircase = 0.0f;
            auto heldSince = 0.0f;
            for (auto t = (1.0f - clockPhase) / ticksPerStep; t <= 1.0f; t += 1.0f / ticksPerStep)
            {
                staircase += held * (t - heldSince);
                heldSince = t;

                // The two output taps are summed to cancel clock leakage, which averages adjacent charges.
                auto& bucket = buckets[(size_t) bucketIndex];
                held = 0.5f * (bucket + lastOut);
                lastOut = bucket;
                bucket = previousInput + t * (input - previousInput);
                bucketIndex = (bucketIndex + 1) & (ticksThrough - 1);
            }
            staircase += held * (1.0f - heldSince);

            clockPhase += ticksPerStep;
            clockPhase -= std::floor (clockPhase);
            previousInput = input;
            output += outputFilter2.lowPass (outputFilter1.lowPass (staircase));
        }
        previousX = x;

        return removeDc (output / (float) oversampling);
    }

    /** Current LFO position, 0 to 1, for the editor's rate LED. */
    float getLfoPhase() const noexcept { return lfoPhase; }

private:
    static constexpr int oversampling = 4;        // for everything from the input filter on
    static constexpr float bbdDrive = 1.6f;
    static constexpr float bbdBias = 0.06f;       // off-centre bias: a touch of second harmonic
    static constexpr float hissLevel = 0.00024f;  // about -72 dBFS, spread over the oversampled band
    static constexpr float couplingHz = 25.0f;    // output coupling cap; also takes off the bias's DC

    static float saturate (float x) noexcept
    {
        return (std::tanh (x * bbdDrive + bbdBias) - std::tanh (bbdBias)) / bbdDrive;
    }

    float removeDc (float x) noexcept
    {
        couplingOut = couplingCoefficient * (couplingOut + x - couplingIn);
        couplingIn = x;
        return couplingOut;
    }

    /** Topology-preserving state-variable low-pass, standing in for the op-amp Sallen-Key stages. */
    struct TwoPole
    {
        void prepare (float rate, float cutoffHz, float q) noexcept
        {
            const auto g = std::tan (juce::MathConstants<float>::pi
                                     * juce::jmin (cutoffHz, 0.45f * rate) / rate);
            const auto k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        float lowPass (float x) noexcept
        {
            const auto v3 = x - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return v2;
        }

        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    };

    float sampleRate = 44100.0f;
    std::array<float, ticksThrough> buckets {};
    int bucketIndex = 0;
    float clockPhase = 0.0f, previousX = 0.0f, previousInput = 0.0f, held = 0.0f, lastOut = 0.0f;
    float couplingCoefficient = 0.0f, couplingIn = 0.0f, couplingOut = 0.0f;
    float lfoPhase = 0.0f;
    TwoPole inputFilter, outputFilter1, outputFilter2;
    juce::Random random;
};
