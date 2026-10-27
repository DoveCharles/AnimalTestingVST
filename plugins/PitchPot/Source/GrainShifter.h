#pragma once

#include <at_shared/at_shared.h>

/** The positions of the Shift selector: a detune, the intervals up to a sixth, then octaves. */
namespace Shifts
{
    enum Shift { detune, minor2nd, major2nd, minor3rd, major3rd, fourth, fifth,
                 minor6th, major6th, octave, octaveFifth, twoOctaves, threeOctaves };

    inline const juce::StringArray names { "Detune", "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd",
                                           "4th", "5th", "Minor 6th", "Major 6th", "1 Octave",
                                           "Octave + 5th", "2 Octaves", "3 Octaves" };

    /** Short labels printed around the selector. */
    inline const juce::StringArray dialLabels { "DET", "m2", "M2", "m3", "M3", "4TH", "5TH",
                                                "m6", "M6", "OCT", "O+5", "2 OCT", "3 OCT" };

    /** Size of the shift in semitones; Direction decides the sign. Detune is 12 cents. */
    inline float semitones (int shift) noexcept
    {
        static constexpr std::array<float, 13> table { 0.12f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f,
                                                       8.0f, 9.0f, 12.0f, 19.0f, 24.0f, 36.0f };
        return table[(size_t) juce::jlimit (0, (int) table.size() - 1, shift)];
    }
}

namespace Directions
{
    enum Direction { down, dual, up };

    inline const juce::StringArray names { "Down", "Dual", "Up" };
}

/** One channel of a polyphonic pitch shifter, in the time domain like the compact pedals:

      anti-alias low-pass -> buffer -> four overlapping Hann grains, each read at the pitch ratio

    A new grain starts every quarter grain, placed so it never overtakes the write head (shifting
    up) or falls out of the buffer (shifting down). Where it starts is nudged, within a few
    milliseconds, to the spot whose waveform best matches what the newest playing grain is reading
    (normalised cross-correlation, as in WSOLA), so the overlapping grains add up in phase instead
    of beating. Chords have no single period to lock to, so they keep a little of the warble that
    is part of this kind of pedal's sound.

    Grains are about 30 ms long near unison. They get shorter for big upward shifts, so each still
    covers about 60 ms of input, and longer for downward ones, so each reads at least 30 ms of
    input; that keeps low notes recognisable two or three octaves down. The cost is delay that
    grows with the interval: a few ms for a detune, about 15 ms an octave up, up to about 60 ms
    three octaves down. */
class GrainShifter
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        const auto needed = (int) std::ceil (newSampleRate * maxDelaySeconds) + 8;
        buffer.assign ((size_t) juce::nextPowerOfTwo (needed), 0.0f);
        mask = (int) buffer.size() - 1;
        maxDelay = (double) needed - 8.0;

        searchRadius = juce::roundToInt (searchSeconds * sampleRate);
        matchLength = juce::roundToInt (matchSeconds * sampleRate);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        written = 0;
        untilNextGrain = 0;
        newestGrain = -1;
        filterRatio = 0.0f;
        for (auto& grain : grains)
            grain.active = false;
        for (auto& stage : antiAlias)
            stage.reset();
    }

    /** Returns the shifted signal only. ratio is the pitch ratio, 1/8 to 8. */
    float process (float x, float ratio) noexcept
    {
        if (std::abs (ratio - filterRatio) > 0.002f * filterRatio)
            updateAntiAlias (ratio);

        buffer[(size_t) (written & mask)] = antiAlias[1].lowPass (antiAlias[0].lowPass (x));
        const auto newest = (double) written++;

        if (--untilNextGrain <= 0)
            startGrain (ratio, newest);

        auto sum = 0.0f, weight = 0.0f;
        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            // A ratio that moved since the grain started could carry it past either end of the history.
            grain.position = juce::jlimit (newest - maxDelay, newest - minTapDelay, grain.position);

            const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) grain.age / (float) grain.length);
            sum += w * read (grain.position);
            weight += w;

            grain.position += ratio;
            if (++grain.age >= grain.length)
                grain.active = false;
        }

        // Four Hann windows a quarter apart sum to 2; dividing by the live sum keeps the level
        // steady while the grain length changes with the ratio.
        return sum / juce::jmax (weight, 1.0f);
    }

private:
    static constexpr double maxDelaySeconds = 0.4;
    static constexpr float searchSeconds = 0.005f;
    static constexpr float matchSeconds = 0.006f;
    static constexpr int overlap = 4;
    static constexpr int coarseStep = 3, matchStep = 3;
    static constexpr double minTapDelay = 4.0;   // keeps the cubic interpolator behind the write head

    struct Grain
    {
        bool active = false;
        double position = 0.0;   // read position, in samples since prepare()
        int age = 0, length = 1;
    };

    static float grainSeconds (float ratio) noexcept
    {
        return ratio >= 1.0f ? juce::jlimit (0.0075f, 0.03f, 0.06f / ratio)
                             : juce::jlimit (0.03f, 0.12f, 0.03f / ratio);
    }

    void startGrain (float ratio, double newest) noexcept
    {
        const auto length = juce::jmax (overlap, juce::roundToInt (grainSeconds (ratio) * sampleRate));
        untilNextGrain += length / overlap;

        // Far enough back that the search can move it either way and an upward grain still ends
        // behind the write head.
        const auto delay = minTapDelay + searchRadius + juce::jmax (0.0, (double) (ratio - 1.0f) * length);
        auto start = std::round (newest - delay);
        if (newestGrain >= 0 && grains[(size_t) newestGrain].active)
            start = bestMatch (start, std::round (grains[(size_t) newestGrain].position));

        // Take a free slot, or the grain closest to finishing if a sudden ratio change left none.
        auto slot = 0;
        auto mostDone = -1.0f;
        for (int i = 0; i < (int) grains.size(); ++i)
        {
            const auto& grain = grains[(size_t) i];
            const auto done = grain.active ? (float) grain.age / (float) grain.length : 2.0f;
            if (done > mostDone)
            {
                mostDone = done;
                slot = i;
            }
        }

        grains[(size_t) slot] = { true, start, 0, length };
        newestGrain = slot;
    }

    /** The start within searchRadius of nominal whose recent past looks most like the reference's. */
    double bestMatch (double nominal, double reference) const noexcept
    {
        const auto ref = (int64_t) reference;
        auto refEnergy = 0.0f;
        for (int j = 0; j < matchLength; j += matchStep)
            refEnergy += juce::square (sample (ref - j));

        if (refEnergy < 1.0e-8f)
            return nominal;

        const auto score = [this, ref] (int64_t candidate)
        {
            auto cross = 0.0f, energy = 1.0e-9f;
            for (int j = 0; j < matchLength; j += matchStep)
            {
                const auto c = sample (candidate - j);
                cross += c * sample (ref - j);
                energy += c * c;
            }
            return cross / std::sqrt (energy);
        };

        const auto centre = (int64_t) nominal;
        auto best = centre;
        auto bestScore = score (centre);
        const auto consider = [&] (int64_t candidate)
        {
            const auto s = score (candidate);
            if (s > bestScore)
            {
                bestScore = s;
                best = candidate;
            }
        };

        for (int offset = -searchRadius; offset <= searchRadius; offset += coarseStep)
            consider (centre + offset);

        const auto coarse = best;
        for (int offset = 1 - coarseStep; offset < coarseStep; ++offset)
            if (offset != 0 && std::abs (coarse + offset - centre) <= searchRadius)
                consider (coarse + offset);

        return (double) best;
    }

    /** Shifting up by r moves everything above fs / (2r) past Nyquist, so filter it out first. */
    void updateAntiAlias (float ratio) noexcept
    {
        filterRatio = ratio;
        const auto cutoff = juce::jmin (20000.0f, 0.45f * sampleRate / juce::jmax (1.0f, ratio));
        antiAlias[0].setLowPass (cutoff, 0.5412f, sampleRate);   // fourth-order Butterworth
        antiAlias[1].setLowPass (cutoff, 1.3066f, sampleRate);
    }

    float sample (int64_t index) const noexcept { return buffer[(size_t) (index & mask)]; }

    /** Cubic Hermite read at an absolute position. */
    float read (double position) const noexcept
    {
        const auto base = (int64_t) std::floor (position);
        const auto t = (float) (position - (double) base);

        const auto xm1 = sample (base - 1);
        const auto x0  = sample (base);
        const auto x1  = sample (base + 1);
        const auto x2  = sample (base + 2);

        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    /** Topology-preserving state-variable low-pass. */
    struct Svf
    {
        void setLowPass (float cutoffHz, float q, float rate) noexcept
        {
            const auto g = std::tan (juce::MathConstants<float>::pi * cutoffHz / rate);
            const auto k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        float lowPass (float v0) noexcept
        {
            const auto v3 = v0 - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return v2;
        }

        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    };

    std::vector<float> buffer;
    int mask = 0;
    int64_t written = 0;
    float sampleRate = 44100.0f;
    double maxDelay = 0.0;
    int searchRadius = 0, matchLength = 0;

    std::array<Grain, 6> grains;
    int untilNextGrain = 0, newestGrain = -1;

    std::array<Svf, 2> antiAlias;
    float filterRatio = 0.0f;
};
