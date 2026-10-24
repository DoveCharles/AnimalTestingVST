#pragma once

namespace at::dsp
{
    /** Stateless waveshaping curves. All are odd-symmetric and have unity slope at zero. */
    struct Waveshapers
    {
        template <typename T>
        static T tanhClip (T x) noexcept { return std::tanh (x); }

        /** Cubic soft clip: smooth knee, hard ceiling at +-2/3. */
        template <typename T>
        static T cubicClip (T x) noexcept
        {
            x = juce::jlimit (T (-1), T (1), x);
            return x - x * x * x / T (3);
        }

        template <typename T>
        static T hardClip (T x) noexcept { return juce::jlimit (T (-1), T (1), x); }
    };
}
