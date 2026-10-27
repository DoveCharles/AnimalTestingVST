#pragma once

#include <at_shared/at_shared.h>

/** Finds percussive hits in a voice signal and describes each one with a short feature vector.

    Onsets: a fast (1 ms) and a slow (30 ms) power envelope; a hit starts when the fast one is
    above the threshold and has jumped well above the slow one, outside the retrigger gap.

    Features, gathered over the listen window that follows the onset: the energy in nine
    constant-Q bands (60 Hz to 12 kHz) as a level-free spectral shape, how much the sound decays
    across the window, and its zero-crossing rate. Everything is in units of roughly 10 dB so
    the distances in HitClassifier weigh the dimensions about equally. */
class HitDetector
{
public:
    static constexpr int numBands = 9;
    static constexpr int numFeatures = numBands + 2;
    using Features = std::array<float, numFeatures>;

    struct Hit
    {
        Features features {};
        float levelDb = -100.0f;     // peak over the window
        float centroidHz = 0.0f;     // energy-weighted (log-frequency) centre of the bands
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        constexpr std::array<double, numBands> centres { 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 };
        for (int b = 0; b < numBands; ++b)
        {
            bandHz[(size_t) b] = (float) juce::jmin (centres[(size_t) b], sampleRate * 0.42);
            bands[(size_t) b].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, bandHz[(size_t) b], 1.0);
            bands[(size_t) b].reset();
        }
        highPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0);
        highPass.reset();

        fastCoef = 1.0f - std::exp (-1.0f / (float) (0.001 * sampleRate));
        slowCoef = 1.0f - std::exp (-1.0f / (float) (0.030 * sampleRate));
        fast = slow = 0.0f;
        capturing = false;
        sinceOnset = std::numeric_limits<int>::max() / 2;
        level = 0.0f;
    }

    void setThresholdDb (float db) noexcept         { threshold = juce::Decibels::decibelsToGain (db); }
    void setListenSeconds (float seconds) noexcept  { listenSamples = juce::jmax (16, (int) (seconds * sampleRate)); }
    void setGapSeconds (float seconds) noexcept     { gapSamples = (int) (seconds * sampleRate); }

    /** Feeds one sample. Returns true when a hit's listen window has just finished, filling `hit`. */
    bool push (float input, Hit& hit) noexcept
    {
        const auto x = highPass.processSample (input);
        const auto power = x * x;
        fast += fastCoef * (power - fast);
        slow += slowCoef * (power - slow);
        level = juce::jmax (std::abs (x), level * 0.9995f);

        std::array<float, numBands> out;
        for (size_t b = 0; b < (size_t) numBands; ++b)
            out[b] = bands[b].processSample (x);

        ++sinceOnset;

        if (! capturing)
        {
            if (fast > threshold * threshold && fast > onsetRatio * slow && sinceOnset >= gapSamples)
            {
                capturing = true;
                sinceOnset = 0;
                energy.fill (0.0f);
                firstHalf = secondHalf = 0.0f;
                peak = 0.0f;
                crossings = 0;
                previous = x;
            }
            else
            {
                return false;
            }
        }

        for (size_t b = 0; b < (size_t) numBands; ++b)
            energy[b] += out[b] * out[b];
        (sinceOnset < listenSamples / 2 ? firstHalf : secondHalf) += power;
        peak = juce::jmax (peak, std::abs (x));
        crossings += (previous < 0.0f) != (x < 0.0f) ? 1 : 0;
        previous = x;

        if (sinceOnset + 1 < listenSamples)
            return false;

        capturing = false;
        describe (hit);
        return true;
    }

    /** Input level for a meter, with a slow release. */
    float getLevel() const noexcept { return level; }

private:
    void describe (Hit& hit) const noexcept
    {
        constexpr auto tiny = 1.0e-12f;

        std::array<float, numBands> logs;
        auto mean = 0.0f, total = 0.0f, weightedLog = 0.0f;
        for (size_t b = 0; b < (size_t) numBands; ++b)
        {
            logs[b] = std::log10 (energy[b] + tiny);
            mean += logs[b] / (float) numBands;
            total += energy[b];
            weightedLog += energy[b] * std::log (bandHz[b]);
        }

        for (size_t b = 0; b < (size_t) numBands; ++b)
            hit.features[b] = logs[b] - mean;   // log10 of energy = 10 dB units

        hit.features[numBands] = std::log10 ((secondHalf + tiny) / (firstHalf + tiny));

        const auto seconds = (float) listenSamples / (float) sampleRate;
        hit.features[numBands + 1] = std::log2 (((float) crossings + 1.0f) / seconds / 1000.0f) * 0.5f;

        hit.levelDb = juce::Decibels::gainToDecibels (peak, -100.0f);
        hit.centroidHz = total > tiny ? std::exp (weightedLog / total) : 0.0f;
    }

    static constexpr float onsetRatio = 3.0f;

    double sampleRate = 48000.0;
    std::array<juce::dsp::IIR::Filter<float>, numBands> bands;
    std::array<float, numBands> bandHz {};
    juce::dsp::IIR::Filter<float> highPass;

    float threshold = 0.01f;
    int listenSamples = 864, gapSamples = 3360;
    float fastCoef = 0.0f, slowCoef = 0.0f, fast = 0.0f, slow = 0.0f, level = 0.0f;

    bool capturing = false;
    int sinceOnset = 0;
    std::array<float, numBands> energy {};
    float firstHalf = 0.0f, secondHalf = 0.0f, peak = 0.0f, previous = 0.0f;
    int crossings = 0;
};

/** Decides which pad a hit belongs to.

    With no training it goes by brightness alone: low centroid is a kick, middle a snare, high a
    hi-hat. Once any pad has examples, it compares the hit with every trained pad's examples
    (mean of the two nearest) and picks the closest; untrained pads are left out. */
struct HitClassifier
{
    static constexpr int numSlots = 5;
    static constexpr int maxExamples = 24;

    struct Example
    {
        HitDetector::Features features {};
        float levelDb = -18.0f;
    };

    std::array<std::vector<Example>, numSlots> examples;

    bool isTrained() const noexcept
    {
        return std::any_of (examples.begin(), examples.end(), [] (auto& e) { return ! e.empty(); });
    }

    /** Typical level of a pad's examples, so each pad's velocity is measured against its own loudness. */
    float referenceDb (int slot) const noexcept
    {
        const auto& list = examples[(size_t) slot];
        if (list.empty())
            return -18.0f;
        auto sum = 0.0f;
        for (auto& e : list)
            sum += e.levelDb;
        return sum / (float) list.size();
    }

    int classify (const HitDetector::Hit& hit) const noexcept
    {
        if (! isTrained())
            return classifyByBrightness (hit);

        auto best = -1;
        auto bestDistance = std::numeric_limits<float>::max();
        for (int slot = 0; slot < numSlots; ++slot)
        {
            if (examples[(size_t) slot].empty())
                continue;

            auto nearest = std::numeric_limits<float>::max(), second = nearest;
            for (auto& e : examples[(size_t) slot])
            {
                auto d = 0.0f;
                for (size_t i = 0; i < (size_t) HitDetector::numFeatures; ++i)
                    d += juce::square (hit.features[i] - e.features[i]);
                if (d < nearest)      { second = nearest; nearest = d; }
                else if (d < second)  { second = d; }
            }

            const auto distance = second < std::numeric_limits<float>::max() ? 0.5f * (nearest + second) : nearest;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = slot;
            }
        }
        return best;
    }

    static int classifyByBrightness (const HitDetector::Hit& hit) noexcept
    {
        if (hit.centroidHz < kickBelowHz)  return 0;
        if (hit.centroidHz < snareBelowHz) return 1;
        return 2;
    }

    static constexpr float kickBelowHz = 400.0f;
    static constexpr float snareBelowHz = 3000.0f;
};
