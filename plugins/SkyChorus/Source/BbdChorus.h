#pragma once

#include <at_shared/at_shared.h>

/** The wet path of a mid-80s compact analog chorus (MN3207 bucket brigade clocked by an MN3102,
    swept by a triangle LFO):

      input filter -> soft clip + hiss -> BBD (modulated delay) -> reconstruction filters

    The LFO sweeps the BBD clock, not the delay time. Delay is stages / (2 * clock), so the sweep
    is lopsided: the delay stretches further on the slow half of the clock swing than it shrinks
    on the fast half. That is part of what gives these pedals their slightly seasick wobble.
    The filters stand in for the anti-aliasing and reconstruction filters around the chip. */
class BbdChorus
{
public:
    static constexpr float maxDelayMs = 40.0f;

    /** Width 1 swings the clock by this fraction either side of its centre. */
    static constexpr float maxClockSwing = 0.45f;

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;
        const auto needed = (int) std::ceil (sampleRate * maxDelayMs * 0.001f) + 8;
        buffer.assign ((size_t) juce::nextPowerOfTwo (needed), 0.0f);
        mask = (int) buffer.size() - 1;

        inputFilter.prepare (sampleRate, 7500.0f, 0.75f);
        outputFilter1.prepare (sampleRate, 6800.0f, 0.6f);
        outputFilter2.prepare (sampleRate, 6800.0f, 1.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        lfoPhase = 0.0f;
        inputFilter.reset();
        outputFilter1.reset();
        outputFilter2.reset();
    }

    /** Returns the wet signal only. delayMs is the centre delay (Delay Time knob), width 0 to 1. */
    float process (float x, float rateHz, float width, float delayMs) noexcept
    {
        // The chip clips softly well before the rails, and its clock leaks a little hiss.
        const auto driven = std::tanh (inputFilter.lowPass (x) * bbdDrive) / bbdDrive;
        buffer[(size_t) writeIndex] = driven + hissLevel * (random.nextFloat() * 2.0f - 1.0f);
        writeIndex = (writeIndex + 1) & mask;

        lfoPhase += rateHz / sampleRate;
        lfoPhase -= std::floor (lfoPhase);
        const auto triangle = 4.0f * std::abs (lfoPhase - 0.5f) - 1.0f;

        const auto clock = 1.0f + maxClockSwing * width * triangle;
        const auto delaySamples = juce::jlimit (minTapDelay, (float) mask - 4.0f,
                                                delayMs * 0.001f * sampleRate / clock);

        return outputFilter2.lowPass (outputFilter1.lowPass (read (delaySamples)));
    }

    /** Current LFO position, 0 to 1, for the editor's rate LED. */
    float getLfoPhase() const noexcept { return lfoPhase; }

private:
    static constexpr float minTapDelay = 4.0f;   // keeps the cubic interpolator behind the write head
    static constexpr float bbdDrive = 1.6f;
    static constexpr float hissLevel = 0.00012f;   // about -78 dBFS

    /** Cubic Hermite read, delaySamples behind the most recent write. */
    float read (float delaySamples) const noexcept
    {
        const auto position = (float) (writeIndex - 1) - delaySamples;
        const auto base = (int) std::floor (position);
        const auto t = position - (float) base;

        const auto xm1 = buffer[(size_t) ((base - 1) & mask)];
        const auto x0  = buffer[(size_t) (base & mask)];
        const auto x1  = buffer[(size_t) ((base + 1) & mask)];
        const auto x2  = buffer[(size_t) ((base + 2) & mask)];

        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
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
    std::vector<float> buffer;
    int mask = 0, writeIndex = 0;
    float lfoPhase = 0.0f;
    TwoPole inputFilter, outputFilter1, outputFilter2;
    juce::Random random;
};
