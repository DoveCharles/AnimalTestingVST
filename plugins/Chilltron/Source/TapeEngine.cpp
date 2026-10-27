#include "TapeEngine.h"

namespace chilltron
{
    namespace
    {
        constexpr float voiceLevel = 0.2f;   // ten-note chords with reverb stay under 0 dBFS
        constexpr float halfPi = juce::MathConstants<float>::halfPi;

        // The return spring pulls a strip home far faster than the capstan plays it.
        constexpr double rewindSpeed = 9.0;

        // Loop mode keeps the bowed or blown body of the note: skip the first second, stop short
        // of the tail, and crossfade the seam.
        constexpr double loopStartSeconds = 1.0, loopTailSeconds = 0.35, loopFadeSeconds = 0.3;
        constexpr double runOutFadeSeconds = 0.03;

        // Capstan wobble at full depth, in cents.
        constexpr float wowCents = 9.0f, flutterCents1 = 3.0f, flutterCents2 = 1.5f, driftCents = 6.0f;
        constexpr float rollerCents = 4.0f;

        float hermite (const juce::int16* pcm, int length, double position)
        {
            const auto i = (int) position;
            const auto f = (float) (position - i);
            const auto x0 = (float) pcm[juce::jmax (i - 1, 0)];
            const auto x1 = (float) pcm[juce::jmin (i, length - 1)];
            const auto x2 = (float) pcm[juce::jmin (i + 1, length - 1)];
            const auto x3 = (float) pcm[juce::jmin (i + 2, length - 1)];
            const auto c1 = 0.5f * (x2 - x0);
            const auto c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
            const auto c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            return (((c3 * f + c2) * f + c1) * f + x1) * (1.0f / 32768.0f);
        }

        std::pair<float, float> panFor (int key, float spread)
        {
            const auto p = juce::jlimit (-1.0f, 1.0f, (key - 60) / 17.0f) * spread;
            const auto angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            return { std::cos (angle) * juce::MathConstants<float>::sqrt2, std::sin (angle) * juce::MathConstants<float>::sqrt2 };
        }
    }

    Engine::Engine()
    {
        // A stand-in for TapeKlik.sf2: the pressure pad slapping the tape onto the head.
        constexpr double rate = 44100.0;
        const auto length = (int) (0.015 * rate);
        juce::Random noise (7);
        float lp = 0.0f;
        fallbackTape.name = "Click";
        fallbackTape.pcm.resize ((size_t) length);
        for (int i = 0; i < length; ++i)
        {
            const auto t = (float) (i / rate);
            lp += 0.35f * (noise.nextFloat() * 2.0f - 1.0f - lp);
            const auto thump = std::sin (juce::MathConstants<float>::twoPi * 900.0f * t) * std::exp (-t / 0.003f);
            const auto s = 0.6f * lp * std::exp (-t / 0.0015f) + 0.4f * thump;
            fallbackTape.pcm[(size_t) i] = (juce::int16) juce::jlimit (-32767.0f, 32767.0f, s * 20000.0f);
        }
        fallbackTape.zones.push_back ({ 0, length, rate });
        for (auto& k : fallbackTape.keys)
            k = { 0, 1.0 };
    }

    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for (auto& v : voices)
            v.stage = Stage::idle;
        for (auto& c : clicks)
            c.active = false;
        keyTime.fill (0.0);
        keyHeld.fill (false);
        sustainDown = false;

        blendA.reset (sampleRate, 0.03);
        blendB.reset (sampleRate, 0.03);
        blendA.setCurrentAndTargetValue (std::cos (controls.blend * halfPi));
        blendB.setCurrentAndTargetValue (std::sin (controls.blend * halfPi));
    }

    void Engine::setTapes (const Tape* a, const Tape* b, const Tape* click) noexcept
    {
        tapeA = a;
        tapeB = b;
        clickTape = click != nullptr ? click : &fallbackTape;
    }

    void Engine::setControls (const Controls& c) noexcept
    {
        controls = c;
        blendA.setTargetValue (std::cos (c.blend * halfPi));
        blendB.setTargetValue (std::sin (c.blend * halfPi));
    }

    //==============================================================================
    void Engine::startTrack (Track& track, const Tape* tape, int key, double tapeTime) const
    {
        track = {};
        if (tape == nullptr)
            return;

        const auto& mapping = tape->keys[(size_t) key];
        if (mapping.zone < 0)
            return;

        const auto& zone = tape->zones[(size_t) mapping.zone];
        track.tape = tape;
        track.zone = &zone;
        track.increment = zone.sampleRate * mapping.ratio / sampleRate;
        track.position = tapeTime * zone.sampleRate * mapping.ratio;

        if (controls.loop)
        {
            const auto loopStart = loopStartSeconds * zone.sampleRate;
            const auto loopEnd = zone.length - loopTailSeconds * zone.sampleRate;
            while (loopEnd - loopStart > 1.0 && track.position >= loopEnd)
                track.position -= loopEnd - loopStart;
        }

        track.finished = track.position >= zone.length - 2;
    }

    float Engine::readTrack (Track& track, double speedNow) const
    {
        const auto& zone = *track.zone;
        const auto* pcm = track.tape->pcm.data() + zone.offset;
        const auto rate = zone.sampleRate;
        const auto loopStart = loopStartSeconds * rate;
        const auto loopEnd = zone.length - loopTailSeconds * rate;
        const auto fade = loopFadeSeconds * rate;
        const auto loops = controls.loop && loopEnd - loopStart > 2.0 * fade;

        auto s = hermite (pcm, zone.length, track.position);

        if (loops && track.position > loopEnd - fade)
        {
            const auto f = (float) ((track.position - (loopEnd - fade)) / fade);
            s = s * std::cos (f * halfPi) + hermite (pcm, zone.length, track.position - (loopEnd - loopStart)) * std::sin (f * halfPi);
        }
        else if (! loops)
        {
            const auto remaining = zone.length - 2 - track.position;
            const auto runOut = runOutFadeSeconds * rate;
            if (remaining < runOut)
                s *= (float) juce::jmax (0.0, remaining / runOut);
        }

        track.position += track.increment * speedNow;

        if (loops && track.position >= loopEnd)
            track.position -= loopEnd - loopStart;
        else if (! loops && track.position >= zone.length - 2)
            track.finished = true;

        return s;
    }

    float Engine::voiceTapeTime (const Voice& v) const
    {
        for (const auto& t : v.tracks)
            if (t.zone != nullptr && t.increment > 0.0)
                return (float) (t.position / (t.increment * sampleRate));
        return (float) keyTime[(size_t) v.key];
    }

    //==============================================================================
    void Engine::noteOn (int key, float velocity)
    {
        if (! juce::isPositiveAndBelow (key, 128))
            return;

        // The same strip can't be under the head twice: a repeat takes over from the old note.
        for (auto& v : voices)
            if (v.stage != Stage::idle && v.key == key)
            {
                keyTime[(size_t) key] = voiceTapeTime (v);
                v.stage = Stage::quickFade;
            }

        // The machine itself ignores how hard a key is struck; with Velocity on, the note's level
        // follows it on a squared curve, so a half-velocity note sits about 12 dB down.
        const auto velocityGain = controls.velocity ? velocity * velocity : 1.0f;

        keyHeld[(size_t) key] = true;
        triggerClick (key, controls.click * 0.5f * velocityGain);

        Voice* voice = nullptr;
        for (auto& v : voices)
            if (v.stage == Stage::idle) { voice = &v; break; }

        if (voice == nullptr)
        {
            // Steal the oldest note, preferring one already on its way out.
            for (auto& v : voices)
                if (voice == nullptr
                    || (v.stage == Stage::release && voice->stage != Stage::release)
                    || ((v.stage == Stage::release) == (voice->stage == Stage::release) && v.age < voice->age))
                    voice = &v;
        }

        auto& v = *voice;
        v = Voice {};
        v.key = key;
        v.age = ++voiceCounter;
        v.gain = voiceLevel * velocityGain;
        v.attackStep = 1.0f / (float) juce::jmax (1.0, controls.attack * sampleRate);
        v.releaseCoef = (float) std::exp (-std::log (1000.0) / juce::jmax (1.0, controls.release * sampleRate));
        v.quickStep = 1.0f / (float) (0.004 * sampleRate);
        v.detune = (float) std::exp2 ((random.nextFloat() * 2.0f - 1.0f) * rollerCents * controls.wow / 1200.0f);
        std::tie (v.panLeft, v.panRight) = panFor (key, controls.spread);

        const auto start = keyTime[(size_t) key];
        startTrack (v.tracks[0], tapeA, key, start);
        startTrack (v.tracks[1], tapeB, key, start);

        v.stage = v.tracks[0].finished && v.tracks[1].finished ? Stage::idle : Stage::attack;
    }

    void Engine::release (Voice& v)
    {
        if (v.stage == Stage::attack || v.stage == Stage::hold)
            v.stage = Stage::release;
        v.sustained = false;
    }

    void Engine::noteOff (int key)
    {
        if (! juce::isPositiveAndBelow (key, 128) || ! keyHeld[(size_t) key])
            return;

        keyHeld[(size_t) key] = false;

        for (auto& v : voices)
            if (v.key == key && (v.stage == Stage::attack || v.stage == Stage::hold))
            {
                if (sustainDown)
                    v.sustained = true;
                else
                    release (v);
            }

        if (! sustainDown)
            triggerClick (key, controls.click * 0.25f);
    }

    void Engine::setSustain (bool down)
    {
        sustainDown = down;
        if (down)
            return;

        for (auto& v : voices)
            if (v.sustained && ! keyHeld[(size_t) v.key])
            {
                release (v);
                triggerClick (v.key, controls.click * 0.25f);
            }
    }

    void Engine::allNotesOff()
    {
        sustainDown = false;
        keyHeld.fill (false);
        for (auto& v : voices)
            if (v.stage != Stage::idle)
                v.stage = Stage::quickFade;
    }

    void Engine::triggerClick (int key, float gain)
    {
        if (gain <= 0.0f || clickTape == nullptr || clickTape->zones.empty())
            return;

        auto* slot = &clicks[0];
        for (auto& c : clicks)
            if (! c.active) { slot = &c; break; }

        const auto zoneIndex = juce::jmax (0, clickTape->keys[(size_t) key].zone);
        slot->tape = clickTape;
        slot->zone = &clickTape->zones[(size_t) zoneIndex];
        slot->position = 0.0;
        slot->increment = slot->zone->sampleRate / sampleRate;
        slot->gain = gain * voiceLevel;
        std::tie (slot->panLeft, slot->panRight) = panFor (key, controls.spread);
        slot->active = true;
    }

    //==============================================================================
    void Engine::computeSpeed (int n)
    {
        const auto depth = juce::jlimit (0.0f, 1.25f, controls.wow + 0.6f * modWheel);
        const auto baseCents = controls.pitchCents + 100.0f * bend;
        const auto dt = 1.0 / sampleRate;
        const auto driftCoef = (float) (1.0 - std::exp (-dt / 0.4));

        for (int i = 0; i < n; ++i)
        {
            if (--driftCountdown <= 0)
            {
                driftTarget = random.nextFloat() * 2.0f - 1.0f;
                driftCountdown = (int) (sampleRate * (0.15 + 0.3 * random.nextDouble()));
            }
            drift += driftCoef * (driftTarget - drift);

            wowPhase      += juce::MathConstants<double>::twoPi * 0.55 * dt;
            flutterPhase1 += juce::MathConstants<double>::twoPi * 7.1 * dt;
            flutterPhase2 += juce::MathConstants<double>::twoPi * 11.7 * dt;

            const auto wobble = wowCents * (float) std::sin (wowPhase)
                              + flutterCents1 * (float) std::sin (flutterPhase1)
                              + flutterCents2 * (float) std::sin (flutterPhase2)
                              + driftCents * drift;

            speed[(size_t) i] = std::exp2 ((baseCents + depth * wobble) / 1200.0f);
        }

        for (auto* phase : { &wowPhase, &flutterPhase1, &flutterPhase2 })
            if (*phase > juce::MathConstants<double>::twoPi)
                *phase -= juce::MathConstants<double>::twoPi;
    }

    void Engine::render (float* left, float* right, int numSamples)
    {
        constexpr int chunk = 256;
        if (speed.size() < chunk)
            speed.resize (chunk);

        for (int offset = 0; offset < numSamples; offset += chunk)
        {
            const auto n = juce::jmin (chunk, numSamples - offset);
            auto* l = left + offset;
            auto* r = right + offset;
            computeSpeed (n);

            // Blend is shared by every voice; work it out once per sample.
            std::array<float, chunk> gainA, gainB;
            for (int i = 0; i < n; ++i)
            {
                gainA[(size_t) i] = blendA.getNextValue();
                gainB[(size_t) i] = blendB.getNextValue();
            }

            for (auto& v : voices)
            {
                if (v.stage == Stage::idle)
                    continue;

                for (int i = 0; i < n && v.stage != Stage::idle; ++i)
                {
                    switch (v.stage)
                    {
                        case Stage::attack:
                            v.envelope += v.attackStep;
                            if (v.envelope >= 1.0f) { v.envelope = 1.0f; v.stage = Stage::hold; }
                            break;
                        case Stage::hold:
                            break;
                        case Stage::release:
                            v.envelope *= v.releaseCoef;
                            if (v.envelope < 0.001f) v.stage = Stage::idle;
                            break;
                        case Stage::quickFade:
                            v.envelope -= v.quickStep;
                            if (v.envelope <= 0.0f) { v.envelope = 0.0f; v.stage = Stage::idle; }
                            break;
                        case Stage::idle:
                            break;
                    }

                    const auto s = (double) speed[(size_t) i] * v.detune;
                    float sample = 0.0f;
                    if (! v.tracks[0].finished) sample += gainA[(size_t) i] * readTrack (v.tracks[0], s);
                    if (! v.tracks[1].finished) sample += gainB[(size_t) i] * readTrack (v.tracks[1], s);

                    sample *= v.envelope * v.gain;
                    l[i] += sample * v.panLeft;
                    r[i] += sample * v.panRight;

                    if (v.tracks[0].finished && v.tracks[1].finished)
                        v.stage = Stage::idle;
                }
            }

            for (auto& c : clicks)
            {
                if (! c.active)
                    continue;

                const auto* pcm = c.tape->pcm.data() + c.zone->offset;
                for (int i = 0; i < n; ++i)
                {
                    if (c.position >= c.zone->length - 2)
                    {
                        c.active = false;
                        break;
                    }
                    const auto s = c.gain * hermite (pcm, c.zone->length, c.position);
                    l[i] += s * c.panLeft;
                    r[i] += s * c.panRight;
                    c.position += c.increment;
                }
            }

            // Tape positions: engaged strips follow their note, released ones spring home.
            std::array<bool, 128> engaged {};
            for (const auto& v : voices)
            {
                if (v.stage == Stage::idle || v.stage == Stage::quickFade)
                    continue;
                if (keyHeld[(size_t) v.key] || v.sustained)
                {
                    engaged[(size_t) v.key] = true;
                    keyTime[(size_t) v.key] = voiceTapeTime (v);
                }
            }

            const auto rewind = rewindSpeed * n / sampleRate;
            for (size_t k = 0; k < 128; ++k)
            {
                if (! engaged[k] && ! keyHeld[k])
                    keyTime[k] = juce::jmax (0.0, keyTime[k] - rewind);
                uiPosition[k].store ((float) keyTime[k], std::memory_order_relaxed);
                uiSounding[k].store (engaged[k], std::memory_order_relaxed);
            }
        }
    }
}
