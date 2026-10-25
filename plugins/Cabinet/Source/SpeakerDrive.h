#pragma once

#include <at_shared/at_shared.h>

/** What a speaker does when it's pushed hard, before its frequency response (the IR) is applied:

      voice-coil compression  the coil heats up and loses efficiency on sustained loud playing,
                              so the level sags a little and recovers between phrases
      cone breakup            the cone stops moving as a piston and flattens the peaks, a touch
                              asymmetrically (it moves more easily outwards than in)

    Both scale with Push; at zero this is a wire. */
class SpeakerDrive
{
public:
    void prepare (double sampleRate)
    {
        attack  = (float) std::exp (-1.0 / (0.015 * sampleRate));
        release = (float) std::exp (-1.0 / (0.25 * sampleRate));
        reset();
    }

    void reset() noexcept { envelope = 0.0f; }

    float process (float x, float push) noexcept
    {
        const auto power = x * x;
        const auto coefficient = power > envelope ? attack : release;
        envelope = power + coefficient * (envelope - power);

        // Up to about 5 dB of sag on a full-scale signal at full Push.
        const auto sag = 1.0f / std::sqrt (1.0f + 3.0f * push * envelope);

        const auto drive = 1.0f + 2.0f * push;
        const auto bias = 0.15f * push;
        const auto y = (std::tanh (drive * x * sag + bias) - std::tanh (bias)) / drive;
        return y;
    }

private:
    float attack = 0.0f, release = 0.0f, envelope = 0.0f;
};
