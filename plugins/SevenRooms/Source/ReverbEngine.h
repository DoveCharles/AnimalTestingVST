#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

/** The seven positions of the Reverb Type selector, in dial order (clockwise, as on the original). */
namespace ReverbTypes
{
    enum Type { room, plate, reverse, modulated, gated, hall, spring, numTypes };
}

/** Building blocks for the reverb. Plain C++ with no JUCE, so the engine can be tested on its own. */
namespace rv
{
    inline constexpr float pi = 3.14159265358979f;

    /** Coefficient for a one-pole low-pass: state += coeff * (x - state). */
    inline float onePoleCoeff (float cutoffHz, float sampleRate) noexcept
    {
        return 1.0f - std::exp (-2.0f * pi * std::min (cutoffHz, 0.45f * sampleRate) / sampleRate);
    }

    /** Gain per pass that gives a 60 dB decay in t60 seconds for a loop of loopSamples. */
    inline float loopGain (float loopSamples, float t60, float sampleRate) noexcept
    {
        return std::pow (10.0f, -3.0f * loopSamples / (t60 * sampleRate));
    }

    /** Power-of-two circular delay line with linearly interpolated reads. */
    class DelayLine
    {
    public:
        void prepare (int maxDelaySamples)
        {
            int size = 1;
            while (size < maxDelaySamples + 4)
                size <<= 1;
            buffer.assign ((size_t) size, 0.0f);
            mask = size - 1;
            reset();
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writeIndex = 0;
        }

        void push (float x) noexcept
        {
            buffer[(size_t) writeIndex] = x;
            writeIndex = (writeIndex + 1) & mask;
        }

        /** The sample pushed delay + 1 pushes ago (read before pushing gives a delay of delay + 1). */
        float read (float delay) const noexcept
        {
            const auto whole = (int) delay;
            const auto frac = delay - (float) whole;
            const auto a = buffer[(size_t) ((writeIndex - 1 - whole) & mask)];
            const auto b = buffer[(size_t) ((writeIndex - 2 - whole) & mask)];
            return a + frac * (b - a);
        }

        float maxDelay() const noexcept { return (float) mask - 4.0f; }

    private:
        std::vector<float> buffer;
        int mask = 0, writeIndex = 0;
    };

    /** Schroeder all-pass diffuser with a fixed delay. */
    class Allpass
    {
    public:
        void prepare (int delaySamples)
        {
            buffer.assign ((size_t) std::max (1, delaySamples), 0.0f);
            reset();
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }

        float process (float x, float g) noexcept
        {
            const auto delayed = buffer[(size_t) index];
            const auto w = x + g * delayed;
            buffer[(size_t) index] = w;
            if (++index == (int) buffer.size())
                index = 0;
            return delayed - g * w;
        }

    private:
        std::vector<float> buffer;
        int index = 0;
    };

    /** In-place orthonormal 8-point Hadamard transform: the FDN's lossless mixing matrix. */
    inline void hadamard8 (std::array<float, 8>& v) noexcept
    {
        for (int span = 1; span < 8; span <<= 1)
            for (int i = 0; i < 8; i += span * 2)
                for (int j = i; j < i + span; ++j)
                {
                    const auto a = v[(size_t) j], b = v[(size_t) (j + span)];
                    v[(size_t) j] = a + b;
                    v[(size_t) (j + span)] = a - b;
                }

        constexpr float scale = 0.35355339f;   // 1 / sqrt (8)
        for (auto& x : v)
            x *= scale;
    }

    /** Eight-line feedback delay network for the Room, Plate, Modulated and Hall types:

          input diffusers -> 8 delay lines (each damped and scaled for the decay) -> Hadamard mix -> back in

        Size scales the line lengths, diffusion sets how smeared the input is before it enters the tank,
        and modulation wobbles each line's read position so the tail shimmers instead of ringing. */
    class Fdn
    {
    public:
        static constexpr int numLines = 8;
        static constexpr float maxSize = 1.6f;
        static constexpr float maxModMs = 1.5f;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            for (int i = 0; i < numLines; ++i)
                lines[(size_t) i].prepare ((int) std::ceil ((baseMs[(size_t) i] * maxSize + maxModMs + 1.0f) * 0.001f * fs));

            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < numDiffusers; ++k)
                    diffusers[(size_t) ch][(size_t) k].prepare ((int) (diffuserMs[(size_t) ch][(size_t) k] * 0.001f * fs));

            for (int i = 0; i < numLines; ++i)
                modPhase[(size_t) i] = (float) i / (float) numLines;

            reset();
        }

        void reset() noexcept
        {
            for (auto& line : lines)
                line.reset();
            for (auto& channel : diffusers)
                for (auto& d : channel)
                    d.reset();
            damp.fill (0.0f);
        }

        void setShape (float size, float newDiffusion, float modDepthMs, float modRateHz) noexcept
        {
            diffusion = newDiffusion;
            modDepth = modDepthMs * 0.001f * fs;
            for (int i = 0; i < numLines; ++i)
            {
                lengths[(size_t) i] = std::max (2.0f, baseMs[(size_t) i] * size * 0.001f * fs - 1.0f);

                // Spread the rates a little so the lines never move in step.
                modIncrement[(size_t) i] = modRateHz * (0.8f + 0.06f * (float) i) / fs;
            }
        }

        void setDecay (float t60) noexcept
        {
            for (int i = 0; i < numLines; ++i)
                gains[(size_t) i] = loopGain (lengths[(size_t) i] + 1.0f, t60, fs);
        }

        void setDamping (float cutoffHz) noexcept { dampCoeff = onePoleCoeff (cutoffHz, fs); }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            for (int k = 0; k < numDiffusers; ++k)
            {
                inL = diffusers[0][(size_t) k].process (inL, diffusion);
                inR = diffusers[1][(size_t) k].process (inR, diffusion);
            }

            std::array<float, numLines> y {};
            for (int i = 0; i < numLines; ++i)
            {
                auto delay = lengths[(size_t) i];
                if (modDepth > 0.0f)
                {
                    auto& phase = modPhase[(size_t) i];
                    phase += modIncrement[(size_t) i];
                    phase -= std::floor (phase);
                    delay += modDepth * (1.0f + std::sin (2.0f * pi * phase));
                }

                auto& state = damp[(size_t) i];
                state += dampCoeff * (lines[(size_t) i].read (delay) - state);
                y[(size_t) i] = state * gains[(size_t) i];
            }

            outL = 0.5f * (y[0] - y[2] + y[4] - y[6]);
            outR = 0.5f * (y[1] - y[3] + y[5] - y[7]);

            hadamard8 (y);
            for (int i = 0; i < numLines; ++i)
                lines[(size_t) i].push (y[(size_t) i] + ((i & 1) != 0 ? inR : inL));
        }

    private:
        static constexpr int numDiffusers = 4;

        // Mutually prime-ish lengths (ms at size 1), spread so no two lines share echoes.
        static constexpr std::array<float, numLines> baseMs { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 79.3f };
        static constexpr std::array<std::array<float, numDiffusers>, 2> diffuserMs {{
            { 4.77f, 3.59f, 12.73f, 9.31f },
            { 5.29f, 3.91f, 11.87f, 8.69f } }};

        float fs = 44100.0f;
        std::array<DelayLine, numLines> lines;
        std::array<std::array<Allpass, numDiffusers>, 2> diffusers;
        std::array<float, numLines> lengths {}, gains {}, damp {}, modPhase {}, modIncrement {};
        float diffusion = 0.6f, modDepth = 0.0f, dampCoeff = 1.0f;
    };

    /** A burst of closely spaced echoes over a fixed window, then nothing: the Reverse and Gated types.
        Reverse gives the taps rising gains, so the burst swells and stops dead; Gated keeps them level,
        the 80s drum sound. A few all-passes smear the taps into a wash. */
    class TapBurst
    {
    public:
        static constexpr float maxWindowSeconds = 1.0f;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> spread (0.02f, 1.0f);
            for (int ch = 0; ch < 2; ++ch)
            {
                lines[(size_t) ch].prepare ((int) std::ceil (maxWindowSeconds * fs) + 2);
                for (auto& f : fractions[(size_t) ch])
                    f = spread (rng);
                std::sort (fractions[(size_t) ch].begin(), fractions[(size_t) ch].end());

                for (int k = 0; k < numDiffusers; ++k)
                    diffusers[(size_t) ch][(size_t) k].prepare ((int) (diffuserMs[(size_t) ch][(size_t) k] * 0.001f * fs));
            }
            setReverse (false);
            reset();
        }

        void reset() noexcept
        {
            for (auto& line : lines)
                line.reset();
            for (auto& channel : diffusers)
                for (auto& d : channel)
                    d.reset();
        }

        void setReverse (bool reverse) noexcept
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float energy = 0.0f;
                for (int k = 0; k < numTaps; ++k)
                {
                    const auto f = fractions[(size_t) ch][(size_t) k];

                    // A short taper at the very end keeps the cut abrupt without clicking.
                    const auto tail = std::min (1.0f, (1.0f - f) / (reverse ? 0.02f : 0.08f) + 0.05f);
                    const auto g = (reverse ? f * f : 1.0f - 0.25f * f) * tail;
                    tapGains[(size_t) ch][(size_t) k] = (k & 1) != 0 ? -g : g;
                    energy += g * g;
                }
                for (auto& g : tapGains[(size_t) ch])
                    g /= std::sqrt (energy);
            }
        }

        void setWindow (float seconds) noexcept { window = std::min (seconds, maxWindowSeconds) * fs; }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            outL = processChannel (0, inL);
            outR = processChannel (1, inR);
        }

    private:
        static constexpr int numTaps = 64;
        static constexpr int numDiffusers = 3;
        static constexpr std::array<std::array<float, numDiffusers>, 2> diffuserMs {{
            { 1.73f, 3.11f, 5.27f }, { 1.91f, 3.43f, 4.93f } }};

        float processChannel (int ch, float x) noexcept
        {
            auto& line = lines[(size_t) ch];
            line.push (x);

            float sum = 0.0f;
            for (int k = 0; k < numTaps; ++k)
                sum += tapGains[(size_t) ch][(size_t) k] * line.read (fractions[(size_t) ch][(size_t) k] * window);

            for (auto& d : diffusers[(size_t) ch])
                sum = d.process (sum, 0.6f);
            return sum;
        }

        float fs = 44100.0f, window = 0.0f;
        std::array<DelayLine, 2> lines;
        std::array<std::array<float, numTaps>, 2> fractions {}, tapGains {};
        std::array<std::array<Allpass, numDiffusers>, 2> diffusers;
    };

    /** One spring: a delay loop with a chain of first-order all-passes inside it. The all-passes delay
        low frequencies more than highs, and every trip round the loop adds more of that smear, which
        is what turns each echo into the springy "boing". The loop is dark and bass-light, like a tank. */
    class SpringTank
    {
    public:
        void prepare (float sampleRate, float loopMs)
        {
            fs = sampleRate;
            loopDelay = loopMs * 0.001f * fs;
            line.prepare ((int) std::ceil (loopDelay) + 2);
            highPassCoeff = onePoleCoeff (180.0f, fs);
            reset();
        }

        void reset() noexcept
        {
            line.reset();
            chain.fill (0.0f);
            lowPass = highPass = 0.0f;
        }

        void setDecay (float t60) noexcept
        {
            // At low frequencies each all-pass adds about (1 - a) / (1 + a) samples to the loop.
            const auto dispersion = (float) numAllpasses * (1.0f - dispersionCoeff) / (1.0f + dispersionCoeff);

            // The loop filters take roughly filterLossDbPerSecond on their own, so ask the feedback
            // for that much less, never quite reaching unity.
            const auto loopRate = std::max (60.0f / t60 - filterLossDbPerSecond, 60.0f / maxLoopT60);
            feedback = loopGain (loopDelay + dispersion, 60.0f / loopRate, fs);
        }

        void setDamping (float cutoffHz) noexcept { lowPassCoeff = onePoleCoeff (cutoffHz, fs); }

        float process (float x) noexcept
        {
            auto y = line.read (loopDelay);
            for (auto& state : chain)
            {
                const auto out = dispersionCoeff * y + state;
                state = y - dispersionCoeff * out;
                y = out;
            }

            lowPass += lowPassCoeff * (y - lowPass);
            highPass += highPassCoeff * (lowPass - highPass);
            y = lowPass - highPass;

            line.push (x + feedback * y);
            return y;
        }

    private:
        static constexpr int numAllpasses = 32;
        static constexpr float dispersionCoeff = -0.68f;
        static constexpr float filterLossDbPerSecond = 14.0f;
        static constexpr float maxLoopT60 = 30.0f;

        float fs = 44100.0f, loopDelay = 0.0f, feedback = 0.0f;
        float lowPassCoeff = 1.0f, highPassCoeff = 0.0f, lowPass = 0.0f, highPass = 0.0f;
        DelayLine line;
        std::array<float, numAllpasses> chain {};
    };
}

/** Stereo digital reverb with seven types, after the late-2000s compact stereo reverb with
    Lexicon algorithms: Level, Liveliness (brightness), Decay and Reverb Type.

      pre-delay + early reflections -> type engine (FDN, tap burst or springs) -> output low-pass

    Returns the wet signal only. Decay and Liveliness are 0 to 1 and mean different things per type:
    Decay is the reverb time for the tank types and the window length for Reverse and Gated. */
class ReverbEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        fdn.prepare (fs);
        burst.prepare (fs);
        springA.prepare (fs, 33.7f);
        springB.prepare (fs, 41.3f);
        for (auto& line : preDelay)
            line.prepare ((int) std::ceil (maxPreDelayMs * 0.001f * fs));
        setType (type);
        reset();
    }

    void reset() noexcept
    {
        fdn.reset();
        burst.reset();
        springA.reset();
        springB.reset();
        for (auto& line : preDelay)
            line.reset();
        outputL = outputR = 0.0f;
    }

    /** Switch types between resets; the processor fades the wet path out around this. */
    void setType (int newType) noexcept
    {
        type = std::clamp (newType, 0, (int) ReverbTypes::numTypes - 1);
        const auto& s = settings();
        fdn.setShape (s.size, s.diffusion, s.modDepthMs, s.modRateHz);
        burst.setReverse (type == ReverbTypes::reverse);
        setControls (decay, liveliness);
    }

    void setControls (float newDecay, float newLiveliness) noexcept
    {
        decay = newDecay;
        liveliness = newLiveliness;
        const auto& s = settings();

        const auto time = decayTime (type, decay);
        const auto damping = s.dampLo * std::pow (s.dampHi / s.dampLo, liveliness);
        fdn.setDecay (time);
        fdn.setDamping (damping);
        burst.setWindow (time);
        for (auto* tank : { &springA, &springB })
        {
            tank->setDecay (time);
            tank->setDamping (damping);
        }
        outputCoeff = rv::onePoleCoeff (damping * 1.6f, fs);
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const auto& s = settings();

        preDelay[0].push (inL);
        preDelay[1].push (inR);
        const auto pre = s.preDelayMs * 0.001f * fs;
        auto l = preDelay[0].read (pre);
        auto r = preDelay[1].read (pre);

        float wetL = 0.0f, wetR = 0.0f;
        switch (type)
        {
            case ReverbTypes::reverse:
            case ReverbTypes::gated:
                burst.process (l, r, wetL, wetR);
                break;

            case ReverbTypes::spring:
            {
                // A tank is mono and driven hard; its two springs give the stereo spread.
                const auto drive = std::tanh (1.5f * 0.5f * (l + r));
                const auto a = springA.process (drive);
                const auto b = springB.process (drive);
                wetL = a + 0.3f * b;
                wetR = b + 0.3f * a;
                break;
            }

            default:
                fdn.process (l, r, wetL, wetR);
                if (s.earlyLevel > 0.0f)
                    addEarlyReflections (s, wetL, wetR);
                break;
        }

        outputL += outputCoeff * (wetL - outputL);
        outputR += outputCoeff * (wetR - outputR);
        outL = s.outputGain * outputL;
        outR = s.outputGain * outputR;
    }

    /** Decay as the editor shows it: reverb time, or window length for Reverse and Gated. */
    static float decayTime (int type, float decay01) noexcept
    {
        const auto& s = settingsFor (type);
        return s.decayLo * std::pow (s.decayHi / s.decayLo, decay01);
    }

    /** True for the types where Decay sets a window length rather than a reverb time. */
    static bool decayIsWindow (int type) noexcept
    {
        return type == ReverbTypes::reverse || type == ReverbTypes::gated;
    }

private:
    static constexpr float maxPreDelayMs = 100.0f;

    struct Settings
    {
        float decayLo, decayHi;          // seconds at Decay 0 and 1 (T60, or window length)
        float size, diffusion;           // FDN shape
        float modDepthMs, modRateHz;
        float preDelayMs, earlyLevel;
        float dampLo, dampHi;            // loop damping at Liveliness 0 and 1, Hz
        float outputGain;                // evens out the types at the same Level
    };

    static const Settings& settingsFor (int type) noexcept
    {
        static constexpr std::array<Settings, ReverbTypes::numTypes> table {{
            //  decay          size  diff  mod ms  rate  pre ms early  damping         gain
            { 0.25f, 2.5f,     0.45f, 0.55f, 0.0f,  0.0f, 3.0f, 0.6f,  1800.0f, 12000.0f, 1.0f },   // room
            { 0.5f,  7.0f,     0.75f, 0.78f, 0.05f, 0.7f, 0.0f, 0.0f,  2800.0f, 16000.0f, 1.0f },   // plate
            { 0.12f, 0.9f,     1.0f,  0.6f,  0.0f,  0.0f, 0.0f, 0.0f,  2000.0f, 14000.0f, 1.05f },   // reverse
            { 0.8f,  9.0f,     1.1f,  0.7f,  0.9f,  0.6f, 10.0f, 0.0f, 2000.0f, 10000.0f, 1.16f },   // modulated
            { 0.08f, 0.5f,     1.0f,  0.6f,  0.0f,  0.0f, 0.0f, 0.0f,  2500.0f, 14000.0f, 1.05f },   // gated
            { 0.8f,  10.0f,    1.5f,  0.7f,  0.15f, 0.3f, 22.0f, 0.35f, 1800.0f, 11000.0f, 1.19f },  // hall
            { 0.6f,  3.5f,     1.0f,  0.6f,  0.0f,  0.0f, 0.0f, 0.0f,  2500.0f, 6500.0f,  0.9f },   // spring
        }};
        return table[(size_t) std::clamp (type, 0, (int) ReverbTypes::numTypes - 1)];
    }

    const Settings& settings() const noexcept { return settingsFor (type); }

    /** A handful of discrete first reflections off the pre-delay line, scaled with the room. */
    void addEarlyReflections (const Settings& s, float& wetL, float& wetR) const noexcept
    {
        static constexpr std::array<float, 6> tapMsL { 4.3f, 9.7f, 14.9f, 21.1f, 27.6f, 33.9f };
        static constexpr std::array<float, 6> tapMsR { 5.9f, 11.3f, 17.2f, 22.8f, 29.4f, 35.7f };
        static constexpr std::array<float, 6> tapGain { 0.8f, -0.65f, 0.55f, -0.45f, 0.35f, -0.28f };

        const auto scale = s.size * 0.001f * fs;
        float erL = 0.0f, erR = 0.0f;
        for (size_t k = 0; k < tapGain.size(); ++k)
        {
            erL += tapGain[k] * preDelay[0].read (std::min (preDelay[0].maxDelay(), s.preDelayMs * 0.001f * fs + tapMsL[k] * scale));
            erR += tapGain[k] * preDelay[1].read (std::min (preDelay[1].maxDelay(), s.preDelayMs * 0.001f * fs + tapMsR[k] * scale));
        }
        wetL += s.earlyLevel * erL;
        wetR += s.earlyLevel * erR;
    }

    float fs = 44100.0f;
    int type = ReverbTypes::hall;
    float decay = 0.5f, liveliness = 0.5f, outputCoeff = 1.0f;
    float outputL = 0.0f, outputR = 0.0f;

    rv::Fdn fdn;
    rv::TapBurst burst;
    rv::SpringTank springA, springB;
    std::array<rv::DelayLine, 2> preDelay;
};
