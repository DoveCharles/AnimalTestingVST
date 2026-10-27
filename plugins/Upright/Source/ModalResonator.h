#pragma once

#include <at_shared/at_shared.h>

namespace piano
{
    /** One damped mode of a tine or bar, for the modal voicings: a two-pole resonator
        y = a1 y1 - a2 y2 + gain x, with a1 = 2 r cos theta and a2 = r^2. */
    struct ModalResonator
    {
        float cosTheta = 1.0f, sinTheta = 1.0f; // an unset resonator stays silent and reports no energy
        float heldRadius = 0.0f, dampedRadius = 0.0f; // per-sample decay while ringing and once damped
        float a1 = 0.0f, a2 = 0.0f, gain = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;

        void setRadius (float r) noexcept { a1 = 2.0f * r * cosTheta; a2 = r * r; }

        float process (float x) noexcept
        {
            const auto y = a1 * y1 - a2 * y2 + gain * x;
            y2 = y1;
            y1 = y;
            return y;
        }

        /** The squared amplitude of the free oscillation. */
        float energy() const noexcept
        {
            return (y1 * y1 + y2 * y2 - 2.0f * cosTheta * y1 * y2) / (sinTheta * sinTheta);
        }

        /** The per-sample decay that falls 60 dB in `t60` seconds. */
        static float radiusFor (double t60, double rate)
        {
            return (float) std::exp (-6.9 / (juce::jmax (0.001, t60) * rate));
        }
    };
}
