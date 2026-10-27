#pragma once

#include <at_shared/at_shared.h>

/** One-knob multiband maximizer: a three-band Linkwitz-Riley split, a stereo-linked compressor
    on each band with makeup, a touch of saturation and high-band width, and a lookahead peak
    limiter. Four voicings set how the bands are treated; Amount scales all of it from nothing
    (flat, only the crossover's phase) to the full voicing. */
class Maximizer
{
public:
    static constexpr int numBands = 3;

    struct Band
    {
        float depthDb;     // how far the threshold drops at full Amount
        float ratio;       // ratio at full Amount
        float attackMs, releaseMs;
        float makeupDb;    // makeup at full Amount
    };

    struct Voicing
    {
        float lowSplitHz, highSplitHz;
        std::array<Band, numBands> bands;
        float drive;       // soft saturation on the sum at full Amount, 0..1
        float width;       // extra side signal on the high band at full Amount
    };

    /** A: warm and punchy, B: bright and airy, C: loud and even, D: big and dense. */
    static inline const std::array<Voicing, 4> voicings {{
        { 120.0f, 2500.0f, {{ { 16.0f, 4.0f, 25.0f, 180.0f, 5.0f }, { 10.0f, 2.5f, 10.0f, 120.0f, 2.5f }, {  8.0f, 2.0f, 5.0f,  90.0f, 1.5f } }}, 0.30f, 0.0f  },
        { 180.0f, 4000.0f, {{ {  8.0f, 2.0f, 20.0f, 150.0f, 1.5f }, { 12.0f, 3.0f,  8.0f, 100.0f, 3.0f }, { 18.0f, 4.0f, 3.0f,  70.0f, 6.5f } }}, 0.15f, 0.15f },
        { 150.0f, 5000.0f, {{ { 16.0f, 5.0f, 15.0f, 120.0f, 6.0f }, { 16.0f, 5.0f,  6.0f,  90.0f, 6.0f }, { 16.0f, 5.0f, 3.0f,  70.0f, 6.5f } }}, 0.45f, 0.0f  },
        { 100.0f, 3000.0f, {{ { 22.0f, 6.0f, 30.0f, 200.0f, 8.5f }, { 18.0f, 4.0f, 12.0f, 140.0f, 7.0f }, { 18.0f, 4.0f, 4.0f,  90.0f, 8.0f } }}, 0.60f, 0.25f },
    }};

    /** For the meters: gain reduction per band and at the limiter (dB, positive), and output peak. */
    std::array<std::atomic<float>, numBands> bandReduction {};
    std::atomic<float> limiterReduction { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
            f->prepare (spec);
        lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

        lookahead = juce::jmax (2, juce::roundToInt (0.0015 * sampleRate));
        for (auto& line : delay)
            line.assign ((size_t) lookahead, 0.0f);
        boxGains.assign ((size_t) lookahead, 1.0f);
        holdQueue.assign ((size_t) lookahead + 1, {});
        limiterRelease = coefficient (60.0f);

        current = effective (target);
        reset();
    }

    void reset()
    {
        for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
            f->reset();
        setSplits (current.lowSplitHz, current.highSplitHz);
        reduction.fill (0.0f);
        for (auto& line : delay)
            std::fill (line.begin(), line.end(), 0.0f);
        std::fill (boxGains.begin(), boxGains.end(), 1.0f);
        boxSum = (double) lookahead;
        writePos = 0;
        holdHead = holdTail = 0;
        sampleIndex = 0;
        limiterEnvelope = 1.0f;
        currentMakeup = makeupGains (current);
        currentOutput = outputGain;
    }

    /** Samples of delay from the limiter's lookahead; report to the host. */
    int getLatency() const noexcept { return lookahead - 1; }

    void setSettings (int voicing, float amount, float outputDb)
    {
        target = { voicings[(size_t) juce::jlimit (0, 3, voicing)], juce::jlimit (0.0f, 1.0f, amount) };
        outputGain = juce::Decibels::decibelsToGain (outputDb);
    }

    void process (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto stereo = numChannels > 1;
        auto* left = buffer.getWritePointer (0);
        auto* right = stereo ? buffer.getWritePointer (1) : left;

        // Glide toward the new voicing and Amount over about 50 ms, a block at a time.
        const auto next = effective (target);
        const auto glide = 1.0f - std::exp (-(float) numSamples / (0.05f * (float) sampleRate));
        current = blend (current, next, glide);
        setSplits (current.lowSplitHz, current.highSplitHz);

        std::array<float, numBands> attack {}, release {}, threshold {}, slope {};
        for (size_t b = 0; b < numBands; ++b)
        {
            attack[b] = coefficient (current.bands[b].attackMs);
            release[b] = coefficient (current.bands[b].releaseMs);
            threshold[b] = -4.0f - current.bands[b].depthDb;
            slope[b] = 1.0f / current.bands[b].ratio - 1.0f;
        }

        // Makeup and output gain move per sample from where the last block left them.
        const auto makeup = makeupGains (current);
        std::array<float, numBands> makeupStep {};
        for (size_t b = 0; b < numBands; ++b)
            makeupStep[b] = (makeup[b] - currentMakeup[b]) / (float) numSamples;
        const auto outputStep = (outputGain - currentOutput) / (float) numSamples;

        const auto drive = current.drive;
        const auto width = current.width;
        const auto limitAmount = juce::jlimit (0.0f, 1.0f, current.amount * 4.0f);
        constexpr auto ceiling = 0.966f;   // -0.3 dBFS

        std::array<float, numBands> blockReduction {};
        auto blockLimit = 1.0f, blockPeak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            std::array<std::array<float, numBands>, 2> bands {};
            const std::array<float, 2> in { left[n], right[n] };
            for (int ch = 0; ch < 2; ++ch)
            {
                float low, rest, mid, high;
                lowSplit.processSample (ch, in[(size_t) ch], low, rest);
                highSplit.processSample (ch, rest, mid, high);
                // Keep the low band in phase with the other two, so the bands sum flat.
                low = lowAllpass.processSample (ch, low);
                bands[(size_t) ch] = { low, mid, high };
            }

            for (size_t b = 0; b < numBands; ++b)
            {
                currentMakeup[b] += makeupStep[b];
                const auto peak = juce::jmax (std::abs (bands[0][b]), std::abs (bands[1][b]));
                const auto wanted = -gainChangeDb (juce::Decibels::gainToDecibels (peak, -120.0f), threshold[b], slope[b]);
                reduction[b] += (wanted > reduction[b] ? attack[b] : release[b]) * (wanted - reduction[b]);
                blockReduction[b] = juce::jmax (blockReduction[b], reduction[b]);
                const auto gain = juce::Decibels::decibelsToGain (-reduction[b]) * currentMakeup[b];
                bands[0][b] *= gain;
                bands[1][b] *= gain;
            }

            // Spread the high band.
            const auto mid = 0.5f * (bands[0][2] + bands[1][2]);
            const auto side = 0.5f * (bands[0][2] - bands[1][2]) * (1.0f + width);
            bands[0][2] = mid + side;
            bands[1][2] = mid - side;

            std::array<float, 2> out {};
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const auto x = bands[ch][0] + bands[ch][1] + bands[ch][2];
                out[ch] = x + drive * (std::tanh (x) - x);
            }

            // Lookahead limiter: the gain each sample needs, held for the lookahead, released,
            // then averaged over the lookahead so it is already down when the peak arrives.
            const auto needed = juce::jmin (1.0f, ceiling / juce::jmax (1.0e-9f, std::abs (out[0]), std::abs (out[1])));
            pushHold (needed);
            const auto held = holdQueue[(size_t) holdHead].value;
            limiterEnvelope = held < limiterEnvelope ? held : limiterEnvelope + limiterRelease * (held - limiterEnvelope);

            boxSum += (double) limiterEnvelope - (double) boxGains[(size_t) writePos];
            boxGains[(size_t) writePos] = limiterEnvelope;
            const auto limitGain = (float) (boxSum / (double) lookahead);
            const auto applied = 1.0f - limitAmount * (1.0f - limitGain);
            blockLimit = juce::jmin (blockLimit, applied);

            currentOutput += outputStep;
            // The delay line holds lookahead - 1 samples of history plus the one written now.
            const auto readPos = (writePos + 1) % lookahead;
            for (size_t ch = 0; ch < 2; ++ch)
                delay[ch][(size_t) writePos] = out[ch];
            const auto l = delay[0][(size_t) readPos] * applied * currentOutput;
            const auto r = delay[1][(size_t) readPos] * applied * currentOutput;
            writePos = readPos;
            ++sampleIndex;

            left[n] = l;
            if (stereo)
                right[n] = r;
            blockPeak = juce::jmax (blockPeak, std::abs (l), std::abs (r));
        }

        currentMakeup = makeup;
        currentOutput = outputGain;

        for (size_t b = 0; b < numBands; ++b)
            bandReduction[b] = blockReduction[b];
        limiterReduction = -juce::Decibels::gainToDecibels (blockLimit, -60.0f);
        outputPeak = blockPeak;
    }

private:
    /** The voicing as Amount has it: thresholds, ratios and makeup scaled from nothing. */
    struct Effective
    {
        float lowSplitHz, highSplitHz;
        std::array<Band, numBands> bands;
        float drive, width, amount;
    };

    struct Target
    {
        Voicing voicing;
        float amount;
    };

    struct Held
    {
        float value;
        juce::int64 index;
    };

    static Effective effective (const Target& t)
    {
        Effective e { t.voicing.lowSplitHz, t.voicing.highSplitHz, t.voicing.bands,
                      t.voicing.drive * t.amount, t.voicing.width * t.amount, t.amount };
        for (auto& band : e.bands)
        {
            band.depthDb *= t.amount;
            band.ratio = 1.0f + (band.ratio - 1.0f) * t.amount;
            band.makeupDb *= t.amount;
        }
        return e;
    }

    static Effective blend (const Effective& from, const Effective& to, float t)
    {
        const auto mix = [t] (float a, float b) { return a + t * (b - a); };
        auto e = from;
        // Split points glide in octaves.
        e.lowSplitHz = std::exp (mix (std::log (from.lowSplitHz), std::log (to.lowSplitHz)));
        e.highSplitHz = std::exp (mix (std::log (from.highSplitHz), std::log (to.highSplitHz)));
        for (size_t b = 0; b < numBands; ++b)
        {
            auto& band = e.bands[b];
            const auto& goal = to.bands[b];
            band = { mix (band.depthDb, goal.depthDb), mix (band.ratio, goal.ratio), mix (band.attackMs, goal.attackMs),
                     mix (band.releaseMs, goal.releaseMs), mix (band.makeupDb, goal.makeupDb) };
        }
        e.drive = mix (from.drive, to.drive);
        e.width = mix (from.width, to.width);
        e.amount = mix (from.amount, to.amount);
        return e;
    }

    static std::array<float, numBands> makeupGains (const Effective& e)
    {
        std::array<float, numBands> gains {};
        for (size_t b = 0; b < numBands; ++b)
            gains[b] = juce::Decibels::decibelsToGain (e.bands[b].makeupDb);
        return gains;
    }

    /** Soft-knee (6 dB) static curve: the gain change in dB (zero or negative) for a level. */
    static float gainChangeDb (float levelDb, float thresholdDb, float slope)
    {
        constexpr auto knee = 6.0f;
        const auto over = levelDb - thresholdDb;
        if (2.0f * over <= -knee)
            return 0.0f;
        if (2.0f * over < knee)
            return slope * (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
        return slope * over;
    }

    float coefficient (float ms) const
    {
        return 1.0f - std::exp (-1.0f / (0.001f * ms * (float) sampleRate));
    }

    void setSplits (float lowHz, float highHz)
    {
        lowSplit.setCutoffFrequency (lowHz);
        highSplit.setCutoffFrequency (highHz);
        lowAllpass.setCutoffFrequency (highHz);
    }

    /** Sliding minimum over the last `lookahead` samples (a monotonic queue in a ring). */
    void pushHold (float value)
    {
        const auto size = (int) holdQueue.size();
        while (holdHead != holdTail && holdQueue[(size_t) ((holdTail + size - 1) % size)].value >= value)
            holdTail = (holdTail + size - 1) % size;
        holdQueue[(size_t) holdTail] = { value, sampleIndex };
        holdTail = (holdTail + 1) % size;
        while (holdQueue[(size_t) holdHead].index <= sampleIndex - lookahead)
            holdHead = (holdHead + 1) % size;
    }

    double sampleRate = 44100.0;

    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;

    Target target { voicings[0], 0.5f };
    Effective current = effective (target);
    float outputGain = 1.0f, currentOutput = 1.0f;
    std::array<float, numBands> currentMakeup { 1.0f, 1.0f, 1.0f };
    std::array<float, numBands> reduction {};

    int lookahead = 64, writePos = 0;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> boxGains;
    double boxSum = 0.0;
    std::vector<Held> holdQueue;
    int holdHead = 0, holdTail = 0;
    juce::int64 sampleIndex = 0;
    float limiterEnvelope = 1.0f, limiterRelease = 0.0f;
};
