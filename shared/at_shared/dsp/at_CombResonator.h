#pragma once

namespace at::dsp
{
    /** Tuned feedback comb filter with one-pole damping in the loop (a Karplus-Strong style
        resonator). Feed it any signal and it rings at the set pitch.

        - setFrequency() glides to the new pitch (no clicks when the note changes).
        - setDecay() is the RT60 in seconds: how long the ring takes to fall by 60 dB.
        - setBrightness() 0..1 controls the damping filter in the loop (0 = dark, 1 = open).

        The loop delay is compensated for the damping filter's phase delay, so it stays in tune
        at any brightness. Input is energy-normalised so long decays don't blow up. */
    class CombResonator
    {
    public:
        void prepare (double newSampleRate, float lowestFrequencyHz)
        {
            sampleRate = newSampleRate;

            const auto maxPeriod = (int) std::ceil (sampleRate / lowestFrequencyHz) + 4;
            buffer.assign ((size_t) juce::nextPowerOfTwo (maxPeriod), 0.0f);
            mask = (int) buffer.size() - 1;

            period.reset (sampleRate, 0.04);
            period.setCurrentAndTargetValue (periodFor (frequency));
            reset();
            updateCoefficients();
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
            loopFilter = dcIn = dcOut = 0.0f;
        }

        void setFrequency (float hz) noexcept
        {
            frequency = hz;
            period.setTargetValue (periodFor (hz));
        }

        void setDecay (float rt60Seconds) noexcept
        {
            if (! juce::exactlyEqual (decaySeconds, rt60Seconds))
            {
                decaySeconds = rt60Seconds;
                updateCoefficients();
            }
        }

        void setBrightness (float amount01) noexcept
        {
            if (! juce::exactlyEqual (brightness, amount01))
            {
                brightness = amount01;
                updateCoefficients();
            }
        }

        float processSample (float input) noexcept
        {
            if (period.isSmoothing())
            {
                period.getNextValue();
                updateCoefficients();
            }

            const auto out = read (loopDelay);
            loopFilter += damping * (out - loopFilter);
            buffer[(size_t) writePos] = input * inputGain + feedback * loopFilter;
            writePos = (writePos + 1) & mask;

            // DC blocker: the loop has high gain at 0 Hz.
            const auto y = out - dcIn + 0.995f * dcOut;
            dcIn = out;
            dcOut = y;
            return y;
        }

    private:
        float periodFor (float hz) const noexcept
        {
            return juce::jlimit (4.0f, (float) (buffer.empty() ? 4 : buffer.size() - 4),
                                 (float) (sampleRate / juce::jmax (1.0f, hz)));
        }

        void updateCoefficients() noexcept
        {
            const auto p = period.getCurrentValue();

            // One-pole low-pass in the loop: 0.08 (very dark) .. 1.0 (no damping).
            damping = 0.08f + 0.92f * brightness * brightness;

            // Its phase delay at the fundamental would detune the comb, so subtract it.
            const auto w = juce::MathConstants<float>::twoPi / p;
            const auto pole = 1.0f - damping;
            const auto phase = std::atan2 (pole * std::sin (w), 1.0f - pole * std::cos (w));
            loopDelay = juce::jmax (3.0f, p - phase / w);

            feedback = juce::jmin (0.9995f, std::pow (10.0f, -3.0f * p / (decaySeconds * (float) sampleRate)));
            inputGain = std::sqrt (1.0f - feedback * feedback);
        }

        /** 4-point Hermite read, `delay` samples behind the write head. */
        float read (float delay) const noexcept
        {
            const auto pos  = (float) writePos - delay;
            const auto base = (int) std::floor (pos);
            const auto t    = pos - (float) base;

            const auto xm1 = buffer[(size_t) ((base - 1) & mask)];
            const auto x0  = buffer[(size_t) ( base      & mask)];
            const auto x1  = buffer[(size_t) ((base + 1) & mask)];
            const auto x2  = buffer[(size_t) ((base + 2) & mask)];

            const auto c1 = 0.5f * (x1 - xm1);
            const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }

        std::vector<float> buffer;
        int mask = 0, writePos = 0;
        double sampleRate = 44100.0;

        juce::SmoothedValue<float> period;
        float frequency = 220.0f, decaySeconds = 1.0f, brightness = 0.5f;
        float loopDelay = 100.0f, damping = 1.0f, feedback = 0.0f, inputGain = 1.0f;
        float loopFilter = 0.0f, dcIn = 0.0f, dcOut = 0.0f;
    };
}
