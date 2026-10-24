#pragma once

#include <at_shared/at_shared.h>

/** The six positions of the Mode selector, in dial order: three delay ranges, then three pitch modes. */
namespace Modes
{
    enum Mode { delay125, delay500, delay2s, pitchManual, octaveDown, octaveUp };

    inline const juce::StringArray names { "Delay 125ms", "Delay 500ms", "Delay 2s",
                                           "Pitch Manual", "Pitch -1 Oct", "Pitch +1 Oct" };

    /** Short labels printed around the Mode selector. */
    inline const juce::StringArray dialLabels { "125ms", "500ms", "2s", "MAN", "-1 OCT", "+1 OCT" };

    inline bool isPitch (int mode) noexcept { return mode >= pitchManual; }

    /** Delay modes: Fine-Manual sweeps the mode's range on an exponential taper (30 ms to 2 s overall). */
    inline float delayMs (int mode, float fine) noexcept
    {
        const auto lo = mode == delay125 ? 30.0f : (mode == delay500 ? 125.0f : 500.0f);
        const auto hi = lo * 4.0f;
        return lo * std::pow (hi / lo, fine);
    }

    /** Pitch modes: Manual sweeps -1 to +1 octave; in the octave modes Fine trims by up to 50 cents. */
    inline float semitones (int mode, float fine) noexcept
    {
        if (mode == pitchManual)
            return juce::jmap (fine, -12.0f, 12.0f);

        return (mode == octaveUp ? 12.0f : -12.0f) + (fine - 0.5f);
    }
}

/** One channel of a late-80s digital pitch shifter/delay. Both halves share one delay line:

      input filter -> buffer (+ feedback through a dark, soft-clipping loop) -> read taps -> output filter

    Delay modes read a single tap. Pitch modes use the classic two-tap rotating delay of the era:
    two read heads sweep through a short window at a rate set by the pitch ratio, each faded out by a
    raised-cosine window as it jumps back. That gives the grainy, warbly shift of the original, and
    with feedback the shifted signal re-enters the shifter and climbs (or falls) in steps. The
    filters stand in for the converters' anti-aliasing, which is what makes the wet path sound dark. */
class PitchDelay
{
public:
    static constexpr float maxDelaySeconds = 2.0f;

    void prepare (double sampleRate)
    {
        const auto needed = (int) std::ceil (sampleRate * (maxDelaySeconds + 0.1));
        buffer.assign ((size_t) juce::nextPowerOfTwo (needed), 0.0f);
        mask = (int) buffer.size() - 1;

        window = (float) sampleRate * windowSeconds;
        inputRampStep = (float) (1.0 / (inputRampSeconds * sampleRate));

        inputFilter.prepare (sampleRate, 9000.0f);
        loopFilter.prepare (sampleRate, 5500.0f);
        outputFilter1.prepare (sampleRate, 8000.0f);
        outputFilter2.prepare (sampleRate, 8000.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        phase = 0.0f;
        lastTap = 0.0f;
        inputGain = 0.0f;
        inputFilter.reset();
        loopFilter.reset();
        outputFilter1.reset();
        outputFilter2.reset();
    }

    /** Returns the wet signal only. delaySamples must be at least 4 and below the buffer length. */
    float processDelay (float x, float delaySamples, float feedback) noexcept
    {
        write (x, feedback);
        return output (read (delaySamples));
    }

    /** Returns the wet signal only. ratio is the pitch ratio, 0.5 to 2. */
    float processPitch (float x, float ratio, float feedback) noexcept
    {
        write (x, feedback);

        // Both heads move through the window at (1 - ratio) samples per sample, half a window apart.
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        const auto phaseB = phase < 0.5f ? phase + 0.5f : phase - 0.5f;

        // sin^2 + cos^2 = 1, so the crossfade holds level; each head is silent at its jump.
        const auto gainA = juce::square (std::sin (juce::MathConstants<float>::pi * phase));
        const auto gainB = 1.0f - gainA;

        return output (gainA * read (minTapDelay + phase * window)
                     + gainB * read (minTapDelay + phaseB * window));
    }

private:
    static constexpr float windowSeconds = 0.045f;
    static constexpr float minTapDelay = 4.0f;   // keeps the cubic interpolator behind the write head
    static constexpr double inputRampSeconds = 0.01;

    void write (float x, float feedback) noexcept
    {
        // After a reset the input fades in, so the edge between the cleared buffer and live audio
        // doesn't reach the read head as a click.
        inputGain = juce::jmin (1.0f, inputGain + inputRampStep);

        const auto recirculated = std::tanh (loopFilter.lowPass (lastTap) * feedback);
        buffer[(size_t) writeIndex] = inputGain * inputFilter.lowPass (x) + recirculated;
        writeIndex = (writeIndex + 1) & mask;
    }

    float output (float tap) noexcept
    {
        lastTap = tap;
        return outputFilter2.lowPass (outputFilter1.lowPass (tap));
    }

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

    /** Zero-delay-feedback one-pole low-pass. */
    struct OnePole
    {
        void prepare (double sampleRate, float cutoffHz) noexcept
        {
            const auto g = std::tan (juce::MathConstants<float>::pi
                                     * juce::jmin (cutoffHz, 0.45f * (float) sampleRate) / (float) sampleRate);
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

        float coefficient = 0.0f, state = 0.0f;
    };

    std::vector<float> buffer;
    int mask = 0, writeIndex = 0;
    float window = 0.0f, phase = 0.0f, lastTap = 0.0f;
    float inputGain = 0.0f, inputRampStep = 1.0f;
    OnePole inputFilter, loopFilter, outputFilter1, outputFilter2;
};
