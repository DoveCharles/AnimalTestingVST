#include "HousePiano.h"

#include <regex>

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

        /** The key a file is named for, from the note name that ends its name: "Piano_F#0" is
            18 and "Piano_C4" is 60. -1 if it has none. Files' own root keys are not trusted, as
            sets resampled from old ROMs often have them an octave out. */
        int rootFromName (const juce::String& name)
        {
            static const std::regex pattern ("([A-Ga-g])([#b]?)(-?[0-9])$");
            std::smatch match;
            const auto text = name.toStdString();
            if (! std::regex_search (text, match, pattern))
                return -1;

            static constexpr int pitchClasses[] = { 9, 11, 0, 2, 4, 5, 7 }; // A to G
            auto note = pitchClasses[std::toupper (match[1].str()[0]) - 'A'] + 12 * (std::stoi (match[3].str()) + 1);
            if (match[2] == "#") ++note;
            if (match[2] == "b") --note;
            return juce::isPositiveAndBelow (note, 128) ? note : -1;
        }

        /** Scales a recording so its peak-to-peak swing is 2. */
        void normalise (std::vector<float>& data)
        {
            const auto peak = juce::FloatVectorOperations::findMaximum (data.data(), (int) data.size())
                              - juce::FloatVectorOperations::findMinimum (data.data(), (int) data.size());
            if (peak > 0.0f)
                juce::FloatVectorOperations::multiply (data.data(), 2.0f / peak, (int) data.size());
        }
    }

    juce::File HousePiano::sampleFolder()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Animal Testing/Upright/House");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Animal Testing/Upright/House");
       #endif
    }

    void HousePiano::prepare (double sampleRate)
    {
        rate = sampleRate;

        // Samples read from disk do not depend on the rate, so they are only read once.
        if (! fromFolder)
        {
            fromFolder = loadFolder (sampleFolder());
            if (! fromFolder)
                recordFromModel();
        }

        chorusLine.assign ((size_t) (0.03 * sampleRate) + 4, 0.0f);
        allNotesOff();
    }

    bool HousePiano::loadFolder (const juce::File& folder)
    {
        samples.clear();
        if (! folder.isDirectory())
            return false;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.wav;*.aif;*.aiff", juce::File::findFiles))
        {
            const auto file = entry.getFile();
            const auto root = rootFromName (file.getFileNameWithoutExtension());
            if (root < 0)
                continue;

            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples < 16 || reader->lengthInSamples > (1 << 24))
                continue;

            const auto length = (int) reader->lengthInSamples;
            juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
            if (! reader->read (&buffer, 0, length, 0, true, true))
                continue;

            Multisample s;
            s.root = root;
            s.sampleRate = reader->sampleRate;
            s.data.assign ((size_t) length, 0.0f);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                juce::FloatVectorOperations::addWithMultiply (s.data.data(), buffer.getReadPointer (c),
                                                              1.0f / (float) buffer.getNumChannels(), length);
            normalise (s.data);

            // The file's sustain loop if it has one (the WAV loop end is inclusive), else a one-shot.
            const auto& meta = reader->metadataValues;
            const auto loopStart = meta.getValue ("Loop0Start", "-1").getIntValue();
            const auto loopEnd = meta.getValue ("Loop0End", "-1").getIntValue() + 1;
            if (meta.getValue ("NumSampleLoops", "0").getIntValue() > 0
                && loopStart >= 1 && loopEnd - loopStart >= 16 && loopEnd <= length)
            {
                s.loopStart = loopStart;
                s.loopEnd = loopEnd;
            }
            else
            {
                s.loopStart = s.loopEnd = length;
            }

            samples.push_back (std::move (s));
        }

        std::sort (samples.begin(), samples.end(), [] (const auto& a, const auto& b) { return a.root < b.root; });
        return ! samples.empty();
    }

    void HousePiano::recordFromModel()
    {
        const auto sampleRate = rate;
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
            s.sampleRate = sampleRate;

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

            normalise (s.data);
            samples.push_back (std::move (s));

            // The next recording starts from silence.
            engine->allNotesOff();
            std::vector<float> flush (8192);
            for (int i = 0; i < 4; ++i)
                engine->render (flush.data(), (int) flush.size());
        }
    }

    const HousePiano::Multisample& HousePiano::sampleFor (int midiNote) const
    {
        // The nearest root, the lower of two equally near.
        const auto* best = &samples.front();
        for (const auto& s : samples)
            if (std::abs (s.root - midiNote) < std::abs (best->root - midiNote))
                best = &s;
        return *best;
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
        v.increment = std::pow (2.0, (pitchNote - sample.root) / 12.0) * sample.sampleRate / rate;
        v.gain = level * (0.2f + 0.8f * std::pow (velocity01, 1.6f));
        v.envelope = 1.0f;
        // Recorded loops need an envelope to end; a one-shot has its own, shortened below 100%.
        v.decay = sample.looped() || controls.decay < 1.0f ? multiplierFor (t60For (playedNote, controls.decay), rate) : 1.0f;
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
                const auto size = (int) s.data.size();
                auto at = [&] (int i)
                {
                    if (i >= s.loopEnd && s.looped())
                        i = s.loopStart + (i - s.loopStart) % loopLength;
                    return i < size ? s.data[(size_t) juce::jmax (0, i)] : 0.0f;
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
                    if (v.position >= s.loopEnd && s.looped())
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
                if (v.envelope < 1.0e-4f || v.position >= size + 2)
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
