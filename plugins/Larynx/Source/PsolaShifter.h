#pragma once

#include <at_shared/at_shared.h>

/** Pitch-synchronous overlap-add (TD-PSOLA) pitch and formant shifter, for monophonic voice.

    Analysis marks walk through the (delayed) input one pitch period apart. At every synthesis
    mark a Hann-windowed grain, two periods long, is cut around the nearest analysis mark and
    added to the output. Spacing the synthesis marks period / ratio apart sets the pitch; the
    grain shape, and so the spectral envelope (the formants), is untouched.

    The formant shift resamples each grain on the way out: reading it formant times faster
    squeezes it in time and stretches its spectrum up by the same factor, which is what a
    shorter vocal tract does. Pitch and formant are therefore fully independent.

    Unvoiced input (breath, consonants) uses short fixed grains with no pitch change, so noise
    stays noise; the formant shift still applies to it. Stereo channels share the marks. */
class PsolaShifter
{
public:
    static constexpr int maxChannels = 2;

    struct Frame
    {
        float period = 0.0f;   // input pitch period in samples, for the sample being shifted
        bool voiced = false;
        float ratio = 1.0f;    // pitch ratio
        float formant = 1.0f;  // formant ratio
    };

    void prepare (double sampleRate)
    {
        maxHalf = (int) (0.02 * sampleRate);
        maxSource = (int) (0.02 * sampleRate);
        latency = maxHalf + maxSource + 4;
        unvoicedPeriod = (float) (sampleRate / 200.0);

        const auto size = juce::nextPowerOfTwo (4 * latency);
        mask = size - 1;
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            input[(size_t) ch].assign ((size_t) size, 0.0f);
            output[(size_t) ch].assign ((size_t) size, 0.0f);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            std::fill (input[(size_t) ch].begin(), input[(size_t) ch].end(), 0.0f);
            std::fill (output[(size_t) ch].begin(), output[(size_t) ch].end(), 0.0f);
        }
        now = 0;
        nextMark = (double) latency;
        analysisMark = 0.0;
    }

    /** Delay from input to output, in samples. */
    int getLatency() const noexcept { return latency; }

    /** Absolute time of the next input sample. Frame values should describe now - latency. */
    juce::int64 getTime() const noexcept { return now; }

    void process (const float* in, float* out, int numChannels, const Frame& frame) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            input[(size_t) ch][(size_t) (now & mask)] = in[ch];

        for (;;)
        {
            const auto shape = grainShape (frame);
            if (nextMark - shape.half > (double) now)
                break;

            emit (shape, frame.formant, numChannels);
            nextMark += shape.spacing;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& slot = output[(size_t) ch][(size_t) (now & mask)];
            out[ch] = slot;
            slot = 0.0f;
        }
        ++now;
    }

private:
    struct Shape
    {
        double period, spacing, half;
    };

    Shape grainShape (const Frame& frame) const noexcept
    {
        const auto formant = juce::jlimit (0.25, 4.0, (double) frame.formant);
        jassert (std::isfinite (frame.period) && std::isfinite (frame.ratio) && std::isfinite (frame.formant));
        const auto period = frame.voiced ? juce::jlimit (16.0, (double) maxSource, (double) frame.period)
                                         : (double) unvoicedPeriod;
        const auto spacing = juce::jlimit (8.0, (double) maxHalf,
                                           frame.voiced ? period / juce::jlimit (0.25, 4.0, (double) frame.ratio) : period);

        // Grains must at least touch their neighbours, or a big downward shift turns into clicks.
        auto half = juce::jmin ((double) maxHalf, juce::jmax (period / formant, spacing));
        half = juce::jmin (half, (double) maxSource / formant);
        return { period, spacing, half };
    }

    void emit (const Shape& shape, float formantRatio, int numChannels) noexcept
    {
        const auto formant = juce::jlimit (0.25, 4.0, (double) formantRatio);
        const auto centre = nextMark;
        const auto target = centre - latency;

        // The latest mark at or before the target: the latency only covers grains reaching back from there.
        if (analysisMark > target || target - analysisMark > 4.0 * shape.period)
            analysisMark = target;
        while (analysisMark + shape.period <= target)
            analysisMark += shape.period;

        // Hann grains spaced S with half-width H sum to H / S. Stretched grains (formant down) no
        // longer line up with their neighbours and partly cancel, so they get some of that back.
        const auto gain = (float) (shape.spacing / shape.half / std::sqrt (juce::jmin (1.0, formant)));
        const auto first = juce::jmax ((juce::int64) std::ceil (centre - shape.half), now);
        const auto last = (juce::int64) std::floor (centre + shape.half);

        for (auto j = first; j <= last; ++j)
        {
            const auto offset = (double) j - centre;
            const auto w = gain * (0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::pi * offset / shape.half));
            const auto source = analysisMark + offset * formant;

            for (int ch = 0; ch < numChannels; ++ch)
                output[(size_t) ch][(size_t) (j & mask)] += w * read (input[(size_t) ch], source);
        }
    }

    /** Cubic (Catmull-Rom) read at an absolute input time. */
    float read (const std::vector<float>& buffer, double time) const noexcept
    {
        const auto base = (juce::int64) std::floor (time);
        const auto t = (float) (time - (double) base);
        const auto at = [&] (juce::int64 i) { return buffer[(size_t) (i & mask)]; };
        const auto y0 = at (base - 1), y1 = at (base), y2 = at (base + 1), y3 = at (base + 2);
        return y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
    }

    std::array<std::vector<float>, maxChannels> input, output;
    juce::int64 mask = 0, now = 0;
    int maxHalf = 960, maxSource = 960, latency = 1924;
    float unvoicedPeriod = 240.0f;
    double nextMark = 0.0, analysisMark = 0.0;
};
