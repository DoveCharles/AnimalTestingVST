#include "HousePiano.h"

namespace piano
{
    namespace
    {
        /** Roots of the multisample: one recording stretched over about a fifth, as a ROM of the
            time would have to. */
        constexpr int firstRoot = 24, rootSpacing = 7, lastRoot = 108;

        /** Where the loop starts, after the strike has settled. */
        constexpr double loopStartSeconds = 0.25;

        /** The loop is a whole number of periods at least this long: short enough to sound static. */
        constexpr double minLoopSeconds = 0.04;

        /** The loop's level against the strike, and how much the recordings are brightened. */
        constexpr double loopLevel = 0.45;
        constexpr float brightness = 3.0f;

        /** Output level against the modelled voicings at the same velocity. */
        constexpr float outputLevel = 0.15f;

        constexpr double chorusDelayMs = 7.0, chorusDepthMs = 2.5, chorusHz = 0.7;

        float multiplierFor (double seconds, double rate)
        {
            return (float) std::exp (-6.9 / (juce::jmax (0.001, seconds) * rate));
        }

        /** Built-in decay of the amplitude envelope: long in the bass, short at the top. */
        double t60For (int note, double decayScale)
        {
            return juce::jlimit (0.8, 14.0, decayScale * 14.0 * std::pow (2.0, -(note - 36) / 24.0));
        }

        float hermite (float ym1, float y0, float y1, float y2, float t)
        {
            const auto c1 = 0.5f * (y1 - ym1);
            const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * t + c2) * t + c1) * t + y0;
        }
    }

    void HousePiano::prepare (double sampleRate)
    {
        rate = sampleRate;
        samples.clear();

        // Record the strikes from the Studio voicing: bright, close unisons, no action noise.
        auto engine = std::make_unique<Engine>();
        engine->prepare (sampleRate, Voicing::studio);
        Controls recording;
        recording.hardness = 0.65f;
        recording.unison = 0.2f;
        recording.resonance = 0.0f;
        recording.noise = 0.0f;
        engine->setControls (recording);

        for (int root = firstRoot; root <= lastRoot; root += rootSpacing)
        {
            Multisample s;
            s.root = root;

            const auto period = sampleRate / (440.0 * std::pow (2.0, (root - 69) / 12.0));
            const auto periods = std::ceil (minLoopSeconds * sampleRate / period);
            const auto loopLength = (int) std::round (periods * period);
            s.loopStart = (int) (loopStartSeconds * sampleRate);
            s.loopEnd = s.loopStart + loopLength;

            s.data.assign ((size_t) s.loopEnd, 0.0f);
            engine->allNotesOff();
            engine->noteOn (juce::jmin (root, highestNote), 0.85f);
            for (int start = 0; start < s.loopEnd; start += 512)
                engine->render (s.data.data() + start, juce::jmin (512, s.loopEnd - start));

            // Brighten: the bridge signal is dominated by the fundamental, and a ROM piano is not.
            for (int i = s.loopEnd - 1; i > 0; --i)
                s.data[(size_t) i] += brightness * (s.data[(size_t) i] - s.data[(size_t) i - 1]);

            // Bring the tail up towards the strike, as the short loops of the time were: ramp the
            // gain from the end of the strike to the loop so the loop sits at loopLevel below it.
            auto rms = [&] (int from, int length)
            {
                double sum = 0.0;
                for (int i = from; i < from + length; ++i)
                    sum += (double) s.data[(size_t) i] * s.data[(size_t) i];
                return std::sqrt (sum / length) + 1.0e-12;
            };
            const auto strikeEnd = (int) (0.05 * sampleRate);
            const auto lift = (float) juce::jmax (1.0, loopLevel * rms (0, strikeEnd) / rms (s.loopStart, loopLength));
            for (int i = strikeEnd; i < s.loopEnd; ++i)
            {
                const auto t = juce::jmin (1.0f, (float) (i - strikeEnd) / (float) (s.loopStart - strikeEnd));
                s.data[(size_t) i] *= 1.0f + (lift - 1.0f) * t;
            }

            // Crossfade the end of the loop into what precedes its start, so the wrap is seamless.
            const auto fade = loopLength / 2;
            for (int i = 0; i < fade; ++i)
            {
                const auto a = (float) i / (float) fade;
                auto& y = s.data[(size_t) (s.loopEnd - fade + i)];
                y = y * (1.0f - a) + s.data[(size_t) (s.loopStart - fade + i)] * a;
            }

            const auto peak = juce::FloatVectorOperations::findMaximum (s.data.data(), s.loopEnd)
                              - juce::FloatVectorOperations::findMinimum (s.data.data(), s.loopEnd);
            if (peak > 0.0f)
                juce::FloatVectorOperations::multiply (s.data.data(), 2.0f / peak, s.loopEnd);

            samples.push_back (std::move (s));

            // The next recording starts from silence.
            engine->allNotesOff();
            std::vector<float> flush (8192);
            for (int i = 0; i < 4; ++i)
                engine->render (flush.data(), (int) flush.size());
        }

        chorusLine.assign ((size_t) (0.03 * sampleRate) + 4, 0.0f);
        allNotesOff();
    }

    const HousePiano::Multisample& HousePiano::sampleFor (int midiNote) const
    {
        const auto index = juce::jlimit (0, (int) samples.size() - 1,
                                         (int) std::round ((midiNote - firstRoot) / (double) rootSpacing));
        return samples[(size_t) index];
    }

    void HousePiano::startVoice (int playedNote, int pitchNote, float velocity01, float level)
    {
        auto* voice = &voices[0];
        for (auto& v : voices)
        {
            if (! v.active()) { voice = &v; break; }
            if (v.envelope * (v.releasing ? 0.1f : 1.0f) < voice->envelope * (voice->releasing ? 0.1f : 1.0f))
                voice = &v;
        }

        const auto& sample = sampleFor (pitchNote);
        Voice v;
        v.sample = &sample;
        v.note = playedNote;
        v.increment = std::pow (2.0, (pitchNote - sample.root) / 12.0);
        v.gain = level * (0.2f + 0.8f * std::pow (velocity01, 1.6f));
        v.envelope = 1.0f;
        v.decay = multiplierFor (t60For (playedNote, controls.decay), rate);
        v.release = multiplierFor (0.18, rate);

        // The filter, not the strike, follows velocity; hardness shifts it and it tracks the key.
        const auto octaves = 4.5 * velocity01 + 2.0 * (controls.hardness - 0.5) + (playedNote - 60) / 24.0;
        v.cutoff = (float) juce::jlimit (150.0, 0.45 * rate, 650.0 * std::pow (2.0, octaves));
        v.cutoffFall = multiplierFor (6.0, rate); // closes slowly as the note sounds
        v.pan = 0.5f + 0.35f * (float) (playedNote - 64) / 44.0f;
        v.held = true;
        v.age = ++voiceCounter;
        *voice = v;
    }

    void HousePiano::noteOn (int midiNote, float velocity01)
    {
        // A restruck key cuts off its previous note quickly, as one ROM voice per key would.
        for (auto& v : voices)
            if (v.active() && v.note == midiNote)
            {
                v.releasing = true;
                v.release = multiplierFor (0.03, rate);
            }

        keyHeld[(size_t) midiNote] = true;
        startVoice (midiNote, midiNote, velocity01, 1.0f);
        startVoice (midiNote, midiNote - 12, velocity01, 0.45f); // the 16' layer
    }

    void HousePiano::noteOff (int midiNote)
    {
        keyHeld[(size_t) midiNote] = false;
        for (auto& v : voices)
            if (v.active() && v.note == midiNote)
            {
                v.held = false;
                if (! sustain)
                    v.releasing = true;
            }
    }

    void HousePiano::setSustain (bool on)
    {
        sustain = on;
        if (! on)
            for (auto& v : voices)
                if (v.active() && ! v.held)
                    v.releasing = true;
    }

    void HousePiano::allNotesOff()
    {
        for (auto& v : voices)
            v = {};
        keyHeld = {};
        sustain = false;
    }

    void HousePiano::render (float* left, float* right, int numSamples)
    {
        constexpr int chunk = 64;
        std::array<float, chunk> l {}, r {};

        for (int start = 0; start < numSamples; start += chunk)
        {
            const auto n = juce::jmin (chunk, numSamples - start);
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);

            for (auto& v : voices)
            {
                if (! v.active())
                    continue;

                const auto& s = *v.sample;
                const auto loopLength = s.loopEnd - s.loopStart;
                auto at = [&] (int i)
                {
                    if (i >= s.loopEnd)
                        i = s.loopStart + (i - s.loopStart) % loopLength;
                    return s.data[(size_t) juce::jmax (0, i)];
                };

                // Butterworth lowpass (TPT state variable), coefficients updated per chunk.
                const auto g = (float) std::tan (juce::MathConstants<double>::pi * juce::jmin ((double) v.cutoff, 0.45 * rate) / rate);
                constexpr float k = 1.41421356f;
                const auto a1 = 1.0f / (1.0f + g * (g + k));
                const auto a2 = g * a1, a3 = g * a2;
                const auto gl = std::cos (v.pan * juce::MathConstants<float>::halfPi) * v.gain * outputLevel;
                const auto gr = std::sin (v.pan * juce::MathConstants<float>::halfPi) * v.gain * outputLevel;

                for (int i = 0; i < n; ++i)
                {
                    const auto index = (int) v.position;
                    const auto x = hermite (at (index - 1), at (index), at (index + 1), at (index + 2),
                                            (float) (v.position - index));
                    v.position += v.increment;
                    if (v.position >= s.loopEnd)
                        v.position -= loopLength;

                    const auto v3 = x - v.s2;
                    const auto v1 = a1 * v.s1 + a2 * v3;
                    const auto v2 = v.s2 + a2 * v.s1 + a3 * v3;
                    v.s1 = 2.0f * v1 - v.s1;
                    v.s2 = 2.0f * v2 - v.s2;

                    const auto y = v2 * v.envelope;
                    v.envelope *= v.releasing ? v.release : v.decay;
                    l[(size_t) i] += y * gl;
                    r[(size_t) i] += y * gr;
                }

                v.cutoff = juce::jmax (150.0f, v.cutoff * std::pow (v.cutoffFall, (float) n));
                if (v.envelope < 1.0e-4f)
                    v = {};
            }

            // Chorus on the mono sum, read at two taps in quadrature for width.
            const auto depth = 0.25f + 0.5f * controls.unison;
            const auto size = (int) chorusLine.size();
            const auto phaseStep = juce::MathConstants<double>::twoPi * chorusHz / rate;
            for (int i = 0; i < n; ++i)
            {
                chorusLine[(size_t) chorusWrite] = 0.5f * (l[(size_t) i] + r[(size_t) i]);
                auto tap = [&] (double delayMs)
                {
                    const auto d = delayMs * 0.001 * rate;
                    auto p = chorusWrite - d;
                    if (p < 0.0)
                        p += size;
                    const auto i0 = (int) p;
                    const auto frac = (float) (p - i0);
                    return chorusLine[(size_t) i0] * (1.0f - frac) + chorusLine[(size_t) ((i0 + 1) % size)] * frac;
                };
                left[start + i] += l[(size_t) i] + depth * tap (chorusDelayMs + chorusDepthMs * std::sin (chorusPhase));
                right[start + i] += r[(size_t) i] + depth * tap (chorusDelayMs + chorusDepthMs * std::cos (chorusPhase));
                chorusPhase += phaseStep;
                if (chorusPhase > juce::MathConstants<double>::twoPi)
                    chorusPhase -= juce::MathConstants<double>::twoPi;
                chorusWrite = (chorusWrite + 1) % size;
            }
        }
    }
}
