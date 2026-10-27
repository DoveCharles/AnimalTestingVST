#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

/** The three positions of the Mode switch. */
namespace ReverseModes
{
    enum Mode { reverb, delay, wash, numModes };
}

/** Building blocks for the reverse engine. Plain C++ with no JUCE, so it can be tested on its own. */
namespace rv
{
    inline constexpr float pi = 3.14159265358979f;

    /** The longest window any mode plays, in seconds. */
    inline constexpr float maxWindowSeconds = 2.0f;

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

    /** Eight-line feedback delay network, the hall behind the Wash mode:

          input diffusers -> 8 delay lines (each damped and scaled for the decay) -> Hadamard mix -> back in

        Modulation wobbles each line's read position so the tail shimmers instead of ringing. */
    class Fdn
    {
    public:
        static constexpr int numLines = 8;
        static constexpr float size = 1.3f;
        static constexpr float diffusion = 0.7f;
        static constexpr float modDepthMs = 0.35f;
        static constexpr float modRateHz = 0.4f;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            modDepth = modDepthMs * 0.001f * fs;
            for (int i = 0; i < numLines; ++i)
            {
                lengths[(size_t) i] = std::max (2.0f, baseMs[(size_t) i] * size * 0.001f * fs - 1.0f);
                lines[(size_t) i].prepare ((int) std::ceil (lengths[(size_t) i] + 2.0f * modDepth + 4.0f));

                // Spread the rates a little so the lines never move in step.
                modIncrement[(size_t) i] = modRateHz * (0.8f + 0.06f * (float) i) / fs;
                modPhase[(size_t) i] = (float) i / (float) numLines;
            }

            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < numDiffusers; ++k)
                    diffusers[(size_t) ch][(size_t) k].prepare ((int) (diffuserMs[(size_t) ch][(size_t) k] * 0.001f * fs));

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

        void setDecay (float t60) noexcept
        {
            for (int i = 0; i < numLines; ++i)
                gains[(size_t) i] = loopGain (lengths[(size_t) i] + modDepth + 1.0f, t60, fs);
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
                auto& phase = modPhase[(size_t) i];
                phase += modIncrement[(size_t) i];
                phase -= std::floor (phase);
                const auto delay = lengths[(size_t) i] + modDepth * (1.0f + std::sin (2.0f * pi * phase));

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

        float fs = 44100.0f, modDepth = 0.0f, dampCoeff = 1.0f;
        std::array<DelayLine, numLines> lines;
        std::array<std::array<Allpass, numDiffusers>, 2> diffusers;
        std::array<float, numLines> lengths {}, gains {}, damp {}, modPhase {}, modIncrement {};
    };

    /** The RV-7's reverse reverb: a burst of closely spaced echoes over a window whose gains rise
        towards the end, so each note swells up out of nothing and stops dead one window later.
        A few all-passes smear the taps into a wash. */
    class ReverseBurst
    {
    public:
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

                float energy = 0.0f;
                for (int k = 0; k < numTaps; ++k)
                {
                    const auto f = fractions[(size_t) ch][(size_t) k];

                    // A short taper at the very end keeps the cut abrupt without clicking.
                    const auto tail = std::min (1.0f, (1.0f - f) / 0.02f + 0.05f);
                    const auto g = f * f * tail;
                    tapGains[(size_t) ch][(size_t) k] = (k & 1) != 0 ? -g : g;
                    energy += g * g;
                }
                for (auto& g : tapGains[(size_t) ch])
                    g /= std::sqrt (energy);

                for (int k = 0; k < numDiffusers; ++k)
                    diffusers[(size_t) ch][(size_t) k].prepare ((int) (diffuserMs[(size_t) ch][(size_t) k] * 0.001f * fs));
            }
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

    /** A true reverse delay: each window of sound, played backwards once.

        A read head sweeps back through the line at twice real time, so over one window it plays
        the window just recorded from its end to its start. At the end of each sweep a new head
        starts on the next window while the old one carries on a few milliseconds into the window
        before, under an equal-power crossfade, which hides the jump. A head keeps the window
        length it started with, so turning Time mid-sweep never bends or jumps the sound. */
    class ReverseDelay
    {
    public:
        static constexpr float fadeSeconds = 0.012f;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            for (auto& line : lines)
                line.prepare ((int) std::ceil (2.0f * (maxWindowSeconds + fadeSeconds) * fs) + 4);
            reset();
        }

        void reset() noexcept
        {
            for (auto& line : lines)
                line.reset();
            age = 0.0f;
            headWindow = previousWindow = window;
        }

        void setWindow (float seconds) noexcept
        {
            window = std::clamp (seconds * fs, 64.0f, maxWindowSeconds * fs);
        }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            lines[0].push (inL);
            lines[1].push (inR);

            if (age >= headWindow)
            {
                age = 0.0f;
                previousWindow = headWindow;
                headWindow = window;
            }

            const auto delay = 2.0f * age;
            outL = lines[0].read (delay);
            outR = lines[1].read (delay);

            const auto fade = std::min (fadeSeconds * fs, 0.25f * headWindow);
            if (age < fade)
            {
                // Linear, not equal-power: the seam is heard twice, once on each side, and with
                // Repeats those two copies can line up, so their gains must add to one.
                const auto in = age / fade;
                const auto oldDelay = 2.0f * (age + previousWindow);
                outL = in * outL + (1.0f - in) * lines[0].read (oldDelay);
                outR = in * outR + (1.0f - in) * lines[1].read (oldDelay);
            }

            age += 1.0f;
        }

    private:
        float fs = 44100.0f, window = 22050.0f, age = 0.0f;
        float headWindow = 22050.0f, previousWindow = 22050.0f;
        std::array<DelayLine, 2> lines;
    };
}

/** The reverse engine behind Reverser. Three modes share Time, Repeats and Tone:

      Reverb  the RV-7's reverse reverb: each note swells up and cuts off one window later
      Delay   a reverse delay: each window of sound played backwards
      Wash    the reverse delay sent into a long modulated hall

    Repeats brings the sound round again, darker each time (through Tone, a low cut and a soft
    limiter): in Reverb as echoes of each swell one window apart, in Delay and Wash by feeding the
    reversed sound back into the reverse delay, so it alternates backwards and forwards.

    Returns the wet signal only. */
class ReverseEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        burst.prepare (fs);
        reverse.prepare (fs);
        hall.prepare (fs);
        for (auto& line : echo)
            line.prepare ((int) std::ceil (rv::maxWindowSeconds * fs) + 2);
        lowCutCoeff = rv::onePoleCoeff (110.0f, fs);
        setControls (window, repeats, tone);
        reset();
    }

    void reset() noexcept
    {
        burst.reset();
        reverse.reset();
        hall.reset();
        for (auto& line : echo)
            line.reset();
        toneState.fill (0.0f);
        loopToneState.fill (0.0f);
        lowCutState.fill (0.0f);
        loop.fill (0.0f);
    }

    /** Switch modes between resets; the processor fades the wet path out around this. */
    void setMode (int newMode) noexcept
    {
        mode = std::clamp (newMode, 0, (int) ReverseModes::numModes - 1);
    }

    /** windowSeconds is the length of a swell or reversed phrase; repeats and tone are 0 to 1. */
    void setControls (float windowSeconds, float newRepeats, float newTone) noexcept
    {
        window = std::clamp (windowSeconds, 0.02f, rv::maxWindowSeconds);
        repeats = newRepeats;
        tone = newTone;

        burst.setWindow (window);
        reverse.setWindow (window);
        hall.setDecay (std::clamp (4.0f * window, 2.0f, 7.0f));
        hall.setDamping (0.7f * toneCutoff (tone));
        toneCoeff = rv::onePoleCoeff (toneCutoff (tone), fs);
        feedback = 0.92f * repeats;
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        std::array<float, 2> in { inL, inR }, wet {};

        if (mode == ReverseModes::reverb)
        {
            burst.process (in[0], in[1], wet[0], wet[1]);
            for (size_t ch = 0; ch < 2; ++ch)
            {
                toneState[ch] += toneCoeff * (wet[ch] - toneState[ch]);
                wet[ch] = toneState[ch] + echo[ch].read (window * fs - 1.0f);

                loopToneState[ch] += toneCoeff * (wet[ch] - loopToneState[ch]);
                echo[ch].push (loopSignal (ch, loopToneState[ch]));
            }
        }
        else
        {
            float revL, revR;
            reverse.process (in[0] + loop[0], in[1] + loop[1], revL, revR);
            std::array<float, 2> rev { revL, revR };
            for (size_t ch = 0; ch < 2; ++ch)
            {
                toneState[ch] += toneCoeff * (rev[ch] - toneState[ch]);
                wet[ch] = toneState[ch];
                loop[ch] = loopSignal (ch, toneState[ch]);
            }

            // The hall sits outside the loop, so Repeats only ever recirculates the reversed sound.
            if (mode == ReverseModes::wash)
            {
                float hallL, hallR;
                hall.process (wet[0], wet[1], hallL, hallR);
                wet[0] = 0.45f * wet[0] + hallL;
                wet[1] = 0.45f * wet[1] + hallR;
            }
        }

        outL = wet[0];
        outR = wet[1];
    }

    /** Tone's low-pass cutoff, from dark to open. */
    static float toneCutoff (float tone01) noexcept
    {
        return 1200.0f * std::pow (16000.0f / 1200.0f, tone01);
    }

private:
    /** The fed-back copy loses its lows and is softly limited, so long Repeats stay clean. */
    float loopSignal (size_t ch, float x) noexcept
    {
        lowCutState[ch] += lowCutCoeff * (x - lowCutState[ch]);
        return feedback * std::tanh (x - lowCutState[ch]);
    }

    float fs = 44100.0f;
    int mode = ReverseModes::reverb;
    float window = 0.5f, repeats = 0.0f, tone = 0.6f;
    float toneCoeff = 1.0f, lowCutCoeff = 0.0f, feedback = 0.0f;
    std::array<float, 2> toneState {}, loopToneState {}, lowCutState {}, loop {};

    rv::ReverseBurst burst;
    rv::ReverseDelay reverse;
    rv::Fdn hall;
    std::array<rv::DelayLine, 2> echo;   // Reverb's repeats
};
