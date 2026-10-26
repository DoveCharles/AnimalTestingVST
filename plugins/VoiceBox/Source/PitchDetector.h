#pragma once

#include <at_shared/at_shared.h>

/** Monophonic pitch tracker (YIN, de Cheveigné & Kawahara 2002), with the difference function
    computed through an FFT cross-correlation so it stays cheap at full sample rate.

    It analyses a 40 ms window every eighth of a window and keeps a short history of estimates
    stamped with the time at the centre of their window. The shifter looks up the estimate for
    the (delayed) sample it is working on, so pitch marks line up with the audio they describe. */
class PitchDetector
{
public:
    struct Estimate
    {
        float period = 0.0f;   // samples; 0 when unvoiced
        bool voiced = false;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        window = juce::nextPowerOfTwo ((int) (sampleRate * 0.04));
        half = window / 2;
        hop = window / 8;
        minTau = juce::jmax (2, (int) (sampleRate / 1000.0));
        maxTau = juce::jmin (half - 2, (int) (sampleRate / 60.0));

        fft = std::make_unique<juce::dsp::FFT> (juce::roundToInt (std::log2 (window * 2)));
        history.assign ((size_t) window, 0.0f);
        frame.assign ((size_t) window, 0.0f);
        energy.assign ((size_t) window + 1, 0.0);
        spectrumA.assign ((size_t) window * 2, {});
        spectrumB.assign ((size_t) window * 2, {});
        scratch.assign ((size_t) window * 2, {});
        cmnd.assign ((size_t) half, 1.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (history.begin(), history.end(), 0.0f);
        writePos = 0;
        sinceAnalysis = 0;
        now = 0;
        estimates = {};
        nextSlot = 0;
        numEstimates = 0;
        recent = {};
    }

    /** Feeds one (mono) input sample; analyses when a hop has gone by. */
    void push (float x) noexcept
    {
        history[(size_t) writePos] = x;
        writePos = (writePos + 1) % window;
        ++now;

        if (++sinceAnalysis >= hop)
        {
            sinceAnalysis = 0;
            analyse();
        }
    }

    /** The latest estimate whose window centre is at or before the given absolute sample time. */
    Estimate at (juce::int64 time) const noexcept
    {
        for (int i = 0; i < numEstimates; ++i)
        {
            const auto& entry = estimates[(size_t) ((nextSlot - 1 - i + capacity) % capacity)];
            if (entry.centre <= time)
                return entry.estimate;
        }
        return {};
    }

    /** Most recent estimate, for the editor's readout. */
    Estimate latest() const noexcept
    {
        return numEstimates > 0 ? estimates[(size_t) ((nextSlot + capacity - 1) % capacity)].estimate : Estimate{};
    }

    int getWindowSize() const noexcept { return window; }

private:
    static constexpr int capacity = 64;
    static constexpr float threshold = 0.15f;
    static constexpr float voicedLimit = 0.3f;

    struct Entry
    {
        juce::int64 centre = 0;
        Estimate estimate;
    };

    void analyse() noexcept
    {
        for (int i = 0; i < window; ++i)
            frame[(size_t) i] = history[(size_t) ((writePos + i) % window)];

        energy[0] = 0.0;
        for (int i = 0; i < window; ++i)
            energy[(size_t) i + 1] = energy[(size_t) i] + (double) frame[(size_t) i] * frame[(size_t) i];

        Estimate estimate;
        const auto e0 = energy[(size_t) half];

        // Below about -55 dBFS RMS the window is treated as silence.
        if (e0 / half > 3.0e-6)
            estimate = track (e0);

        Entry entry;
        entry.centre = now - half;
        entry.estimate = estimate;
        estimates[(size_t) nextSlot] = entry;
        nextSlot = (nextSlot + 1) % capacity;
        numEstimates = juce::jmin (capacity, numEstimates + 1);
    }

    Estimate track (double e0) noexcept
    {
        // c(tau) = sum_{j < half} x[j] x[j + tau], by FFT cross-correlation.
        const auto size = (size_t) window * 2;
        for (size_t i = 0; i < size; ++i)
        {
            const auto x = i < (size_t) window ? frame[i] : 0.0f;
            scratch[i] = { i < (size_t) half ? x : 0.0f, 0.0f };
            spectrumB[i] = { x, 0.0f };
        }
        fft->perform (scratch.data(), spectrumA.data(), false);
        fft->perform (spectrumB.data(), scratch.data(), false);
        for (size_t i = 0; i < size; ++i)
            spectrumB[i] = std::conj (spectrumA[i]) * scratch[i];
        fft->perform (spectrumB.data(), scratch.data(), true);

        // c(0) is exactly e0, which fixes whatever scaling the FFT backend applies to the inverse.
        const auto scale = scratch[0].real() != 0.0f ? e0 / scratch[0].real() : 0.0;

        // Cumulative mean normalised difference.
        cmnd[0] = 1.0f;
        auto running = 0.0;
        for (int tau = 1; tau <= maxTau + 1; ++tau)
        {
            const auto c = scale * scratch[(size_t) tau].real();
            const auto d = juce::jmax (0.0, e0 + (energy[(size_t) (tau + half)] - energy[(size_t) tau]) - 2.0 * c);
            running += d;
            cmnd[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        // First dip below the threshold, followed down to its minimum; else the global minimum.
        int best = -1;
        for (int tau = minTau; tau <= maxTau; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 <= maxTau && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            best = minTau;
            for (int tau = minTau; tau <= maxTau; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) best])
                    best = tau;
        }

        if (cmnd[(size_t) best] > voicedLimit)
            return {};

        // Parabolic interpolation around the dip.
        auto period = (float) best;
        const auto a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const auto denominator = a - 2.0f * b + c;
        if (denominator > 1.0e-9f)
            period += juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denominator);

        // Median of the last three voiced periods rides over single-frame octave slips.
        recent = { recent[1], recent[2], period };
        auto sorted = recent;
        std::sort (sorted.begin(), sorted.end());
        const auto smoothed = sorted[0] > 0.0f ? sorted[1] : period;

        return { smoothed, true };
    }

    double sampleRate = 44100.0;
    int window = 2048, half = 1024, hop = 256, minTau = 44, maxTau = 735;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> history, frame, cmnd;
    std::vector<double> energy;
    std::vector<std::complex<float>> spectrumA, spectrumB, scratch;
    int writePos = 0, sinceAnalysis = 0;
    juce::int64 now = 0;

    std::array<Entry, capacity> estimates {};
    int nextSlot = 0, numEstimates = 0;
    std::array<float, 3> recent {};
};
