#pragma once

#include <at_shared/at_shared.h>

/** Dynamic resonance suppressor: an STFT that cuts whatever sticks up out of its own spectrum.

    Every hop, each channel's frame is analysed in dB and compared with a smoothed copy of
    itself (a moving average across log frequency). Bins standing above that local average by
    more than the Selectivity threshold are resonances; the excess becomes a cut, limited to
    Depth, weighted by frequency, smoothed across neighbouring bins (so it doesn't comb) and
    across frames with attack and release (so it doesn't warble). Broad tilts and transients
    don't stick out of their own neighbourhood, so they pass.

    sqrt-Hann analysis and synthesis windows at 4x overlap; latency is one frame. */
class ResonanceSuppressor
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int displayBands = 96;
    static constexpr int numWeights = 5;
    static constexpr std::array<float, numWeights> weightHz { 100.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };

    struct Settings
    {
        float depthDb = 6.0f;          // most a bin can be cut
        float sharpness = 0.5f;        // 0..1: wide (broad bumps count) to narrow (only sharp peaks)
        float selectivity = 0.5f;      // 0..1: how far a bin must stick out before it's cut
        float attackSeconds = 0.005f, releaseSeconds = 0.1f;
        std::array<float, numWeights> weights { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float mix = 1.0f;
        bool delta = false;            // output only what's being removed
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        const auto order = sampleRate <= 50000.0 ? 11 : (sampleRate <= 100000.0 ? 12 : 13);
        fft = std::make_unique<juce::dsp::FFT> (order);
        size = 1 << order;
        hop = size / 4;
        bins = size / 2 + 1;
        resolutionHalfBins = juce::jmax (2, (int) std::ceil (resolutionHz * 0.5 * size / sampleRate));

        window.resize ((size_t) size);
        for (int n = 0; n < size; ++n)
            window[(size_t) n] = std::sqrt (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) size));

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            input[(size_t) ch].assign ((size_t) size, 0.0f);
            output[(size_t) ch].assign ((size_t) size, 0.0f);
            cut[(size_t) ch].assign ((size_t) bins, 0.0f);
            power[(size_t) ch].assign ((size_t) bins, 0.0f);
        }
        clipped.assign ((size_t) bins, 0.0f);
        frame.assign ((size_t) size * 2, 0.0f);
        level.assign ((size_t) bins, 0.0f);
        prefix.assign ((size_t) bins + 1, 0.0);
        excess.assign ((size_t) bins, 0.0f);
        weightCurve.assign ((size_t) bins, 1.0f);
        lastWeights = {};
        lastWeights[0] = -1.0f;

        // Display bands are log spaced 20 Hz to 20 kHz; each bin feeds the band it falls in.
        displayBand.assign ((size_t) bins, -1);
        for (int k = 1; k < bins; ++k)
        {
            const auto hz = binHz (k);
            const auto band = (int) std::floor (displayBands * std::log2 (hz / 20.0f) / std::log2 (1000.0f));
            displayBand[(size_t) k] = band >= 0 && band < displayBands ? band : -1;
        }

        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            std::fill (input[(size_t) ch].begin(), input[(size_t) ch].end(), 0.0f);
            std::fill (output[(size_t) ch].begin(), output[(size_t) ch].end(), 0.0f);
            std::fill (cut[(size_t) ch].begin(), cut[(size_t) ch].end(), 0.0f);
            std::fill (power[(size_t) ch].begin(), power[(size_t) ch].end(), 0.0f);
        }
        pos = 0;
        hopCount = 0;
        for (auto& band : reduction) band.store (0.0f);
        for (auto& band : spectrum) band.store (-120.0f);
    }

    int getLatency() const noexcept { return size; }

    void setSettings (const Settings& s) noexcept { settings = s; }

    /** In place, one sample per channel. */
    void process (float* samples, int numChannels) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            input[(size_t) ch][(size_t) pos] = samples[ch];
            auto& slot = output[(size_t) ch][(size_t) pos];
            samples[ch] = slot;
            slot = 0.0f;
        }

        pos = (pos + 1) & (size - 1);
        if (++hopCount == hop)
        {
            hopCount = 0;
            processFrames (numChannels);
        }
    }

    /** For the editor: cut per display band in dB (positive), and the input spectrum in dBFS. */
    std::array<std::atomic<float>, displayBands> reduction {}, spectrum {};

    static float displayBandHz (int band) noexcept
    {
        return 20.0f * std::pow (1000.0f, ((float) band + 0.5f) / (float) displayBands);
    }

private:
    float binHz (int k) const noexcept { return (float) (k * sampleRate / size); }

    void updateWeights()
    {
        if (settings.weights == lastWeights)
            return;
        lastWeights = settings.weights;

        // Piecewise linear in log frequency between the band centres, flat beyond the ends.
        for (int k = 0; k < bins; ++k)
        {
            const auto octave = std::log2 (juce::jmax (1.0f, binHz (k)));
            auto w = settings.weights.front();
            if (octave >= std::log2 (weightHz.back()))
                w = settings.weights.back();
            else
                for (int i = 0; i + 1 < numWeights; ++i)
                {
                    const auto a = std::log2 (weightHz[(size_t) i]), b = std::log2 (weightHz[(size_t) i + 1]);
                    if (octave >= a && octave < b)
                    {
                        const auto t = (octave - a) / (b - a);
                        w = settings.weights[(size_t) i] + t * (settings.weights[(size_t) i + 1] - settings.weights[(size_t) i]);
                        break;
                    }
                }
            weightCurve[(size_t) k] = w;
        }
    }

    /** Moving average of `values` across a window `octaves` wide around each bin, via prefix sums. */
    void smoothAcrossFrequency (const std::vector<float>& values, std::vector<float>& out, float octaves, int minHalfBins = 2)
    {
        prefix[0] = 0.0;
        for (int k = 0; k < bins; ++k)
            prefix[(size_t) k + 1] = prefix[(size_t) k] + values[(size_t) k];

        const auto spread = std::exp2 (0.5f * octaves);
        for (int k = 0; k < bins; ++k)
        {
            // At least minHalfBins either side, so the lowest bins still get a neighbourhood.
            const auto lo = juce::jlimit (0, bins - 1, juce::jmin (k - minHalfBins, (int) std::floor ((float) k / spread)));
            const auto hi = juce::jlimit (0, bins - 1, juce::jmax (k + minHalfBins, (int) std::ceil ((float) k * spread)));
            out[(size_t) k] = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (hi - lo + 1));
        }
    }

    void processFrames (int numChannels)
    {
        updateWeights();

        // Sharpness 0 compares each bin with the 2 octaves around it, 1 with a third of an octave.
        const auto envelopeOctaves = 2.0f * std::pow (1.0f / 6.0f, settings.sharpness);
        const auto threshold = 0.5f + 8.0f * settings.selectivity;
        const auto depth = juce::jmax (0.0f, settings.depthDb);
        const auto framesPerSecond = (float) sampleRate / (float) hop;
        const auto coef = [framesPerSecond] (float seconds)
        {
            return seconds <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (seconds * framesPerSecond));
        };
        const auto attack = coef (settings.attackSeconds), release = coef (settings.releaseSeconds);

        // A single frame of noise is spiky (bins scatter several dB around their mean), so the
        // detector looks at power averaged over the last few frames. A resonance survives the
        // averaging; random spikes don't.
        const auto detect = coef (detectorSeconds);

        std::array<float, displayBands> bandCut {}, bandLevel {};
        bandLevel.fill (-120.0f);
        auto& envelope = smoothed;
        envelope.resize ((size_t) bins);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& in = input[(size_t) ch];
            for (int n = 0; n < size; ++n)
                frame[(size_t) n] = in[(size_t) ((pos + n) & (size - 1))] * window[(size_t) n];
            std::fill (frame.begin() + size, frame.end(), 0.0f);
            fft->performRealOnlyForwardTransform (frame.data(), true);

            // Level in dB; the window's coherent gain (about 0.64 for sqrt-Hann) is close enough
            // to unity for a display, and the detection only uses differences.
            const auto norm = 2.0f / (float) size;
            auto& averaged = power[(size_t) ch];
            for (int k = 0; k < bins; ++k)
            {
                const auto re = frame[(size_t) (2 * k)], im = frame[(size_t) (2 * k + 1)];
                auto& p = averaged[(size_t) k];
                p += detect * (norm * norm * (re * re + im * im) - p);
            }

            // Pitched sources are combs of harmonics, each standing well clear of the gaps between
            // them. Averaging power over a sixth of an octave, and never less than the widest
            // harmonic spacing we care about, fills the comb in, so what's left to stick out is a
            // resonance in the spectral shape and not a partial.
            smoothAcrossFrequency (averaged, excess, 1.0f / 6.0f, resolutionHalfBins);
            for (int k = 0; k < bins; ++k)
                level[(size_t) k] = 10.0f * std::log10 (excess[(size_t) k] + 1.0e-14f);

            // Two passes, so a strong peak doesn't drag its own reference up: the second
            // average sees every bin clipped to the first average.
            smoothAcrossFrequency (level, envelope, envelopeOctaves);
            for (int k = 0; k < bins; ++k)
                clipped[(size_t) k] = juce::jmin (level[(size_t) k], envelope[(size_t) k]);
            smoothAcrossFrequency (clipped, envelope, envelopeOctaves);
            const auto bias = 1.5f;   // the clipped average sits low by about this much on noise

            // Above the threshold the cut grows one for one and then bends over towards Depth;
            // bins near silence are left alone so the noise floor isn't chewed up.
            for (int k = 0; k < bins; ++k)
            {
                const auto over = level[(size_t) k] - envelope[(size_t) k] - bias - threshold;
                const auto audible = juce::jlimit (0.0f, 1.0f, (level[(size_t) k] + 90.0f) / 20.0f);
                excess[(size_t) k] = over > 0.0f && depth > 0.0f ? audible * weightCurve[(size_t) k] * depth * std::tanh (over / depth) : 0.0f;
            }
            excess[0] = 0.0f;

            // A narrow smear (a twelfth of an octave) so neighbouring bins move together.
            smoothAcrossFrequency (excess, envelope, 1.0f / 12.0f);

            auto& gains = cut[(size_t) ch];
            for (int k = 0; k < bins; ++k)
            {
                // Take the peak of the raw and smeared cut, so a single-bin ring is still caught.
                const auto target = juce::jmax (envelope[(size_t) k], excess[(size_t) k]);
                auto& c = gains[(size_t) k];
                c += (target > c ? attack : release) * (target - c);

                const auto g = c > 1.0e-4f ? juce::Decibels::decibelsToGain (-c) : 1.0f;
                const auto removed = settings.mix * (1.0f - g);
                const auto apply = settings.delta ? removed : 1.0f - removed;
                frame[(size_t) (2 * k)] *= apply;
                frame[(size_t) (2 * k + 1)] *= apply;

                const auto band = displayBand[(size_t) k];
                if (band >= 0)
                {
                    bandCut[(size_t) band] = juce::jmax (bandCut[(size_t) band], c * settings.mix);
                    bandLevel[(size_t) band] = juce::jmax (bandLevel[(size_t) band], level[(size_t) k]);
                }
            }

            fft->performRealOnlyInverseTransform (frame.data());

            // sqrt-Hann squared is Hann, and four Hanns a quarter apart sum to 2.
            auto& out = output[(size_t) ch];
            for (int n = 0; n < size; ++n)
                out[(size_t) ((pos + n) & (size - 1))] += 0.5f * frame[(size_t) n] * window[(size_t) n];
        }

        // Display bands too low to hold a bin borrow from their neighbour below.
        for (int b = 1; b < displayBands; ++b)
            if (bandLevel[(size_t) b] <= -120.0f)
            {
                bandLevel[(size_t) b] = bandLevel[(size_t) b - 1];
                bandCut[(size_t) b] = bandCut[(size_t) b - 1];
            }
        for (int b = 0; b < displayBands; ++b)
        {
            reduction[(size_t) b].store (bandCut[(size_t) b]);
            spectrum[(size_t) b].store (bandLevel[(size_t) b]);
        }
    }

    Settings settings;
    double sampleRate = 44100.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    int size = 2048, hop = 512, bins = 1025, pos = 0, hopCount = 0;

    std::vector<float> window, frame, level, excess, smoothed, weightCurve;
    std::vector<double> prefix;
    std::vector<int> displayBand;
    std::vector<float> clipped;
    std::array<std::vector<float>, maxChannels> input, output, cut, power;
    static constexpr float detectorSeconds = 0.05f;
    static constexpr double resolutionHz = 300.0;
    int resolutionHalfBins = 7;
    std::array<float, numWeights> lastWeights {};
};
