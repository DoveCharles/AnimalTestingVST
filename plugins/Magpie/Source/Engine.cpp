#include "Engine.h"

namespace magpie
{
    namespace
    {
        constexpr int controlBlock = 16;      // modulators, pitch and filter are updated this often
        constexpr float silence = 1.0e-5f;    // -100 dB, where a releasing voice stops
        constexpr double fadeSeconds = 0.005; // for stolen and exclusive-class voices

        /** Absolute cents (8.176 Hz at 0) to Hz, for LFO rates and filter cutoffs. */
        double absoluteCentsToHz (double cents) { return 8.176 * std::exp2 (cents / 1200.0); }

        float centibelsToGain (float cb) { return std::pow (10.0f, -cb / 200.0f); }

        /** Step for an exponential segment that falls 96 dB over n samples. */
        float stepFor96dB (int n) { return n > 0 ? std::pow (10.0f, -4.8f / (float) n) : 0.0f; }

        float hermite (float xm1, float x0, float x1, float x2, float t)
        {
            const auto c1 = 0.5f * (x1 - xm1);
            const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }
    }

    //==============================================================================
    void Engine::Envelope::start() noexcept
    {
        stage = Stage::delay;
        counter = 0;
        level = 0.0f;
    }

    void Engine::Envelope::release() noexcept
    {
        if (stage == Stage::done)
            return;
        // Nothing to fade if it hasn't started yet.
        stage = (stage == Stage::delay || level <= 0.0f) ? Stage::done : Stage::release;
        if (stage == Stage::done)
            level = 0.0f;
    }

    float Engine::Envelope::next() noexcept
    {
        const auto out = level;

        switch (stage)
        {
            case Stage::delay:
                if (++counter >= delay)
                {
                    stage = Stage::attack;
                    counter = 0;
                }
                break;

            case Stage::attack:
                if (attack <= 0 || ++counter >= attack)
                {
                    level = 1.0f;
                    stage = Stage::hold;
                    counter = 0;
                }
                else
                {
                    level = (float) counter / (float) attack;
                }
                break;

            case Stage::hold:
                if (++counter >= hold)
                    stage = Stage::decay;
                break;

            case Stage::decay:
                level = exponential ? level * decayStep : level - decayStep;
                if (level <= sustain)
                {
                    level = sustain;
                    stage = sustain > 0.0f ? Stage::sustain : Stage::done;
                }
                break;

            case Stage::release:
                level = exponential ? level * releaseStep : level - releaseStep;
                if (level <= (exponential ? silence : 0.0f))
                {
                    level = 0.0f;
                    stage = Stage::done;
                }
                break;

            case Stage::sustain:
            case Stage::done:
                break;
        }

        return out;
    }

    void Engine::Envelope::advance (int numSamples) noexcept
    {
        for (int i = 0; i < numSamples && stage != Stage::sustain && stage != Stage::done; ++i)
            next();
    }

    float Engine::Lfo::advance (int numSamples) noexcept
    {
        if (delay > 0)
        {
            delay -= numSamples;
            return 0.0f;
        }

        const auto p = phase;
        phase += increment * (float) numSamples;
        phase -= std::floor (phase);
        return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
    }

    //==============================================================================
    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        allSoundOff();
    }

    void Engine::setFont (const SoundFont* newFont) noexcept
    {
        allSoundOff();
        font = newFont;
        preset = nullptr;
    }

    void Engine::setPreset (const Preset* newPreset) noexcept
    {
        // Sounding notes keep playing the old preset's regions; the font still owns them.
        preset = newPreset;
    }

    int Engine::toSamples (int timecents) const noexcept
    {
        return (int) (std::exp2 (juce::jlimit (-12000, 8000, timecents) / 1200.0) * sampleRate);
    }

    Engine::Voice& Engine::findFreeVoice() noexcept
    {
        Voice* oldestReleased = nullptr;
        Voice* oldest = nullptr;

        for (auto& v : voices)
        {
            if (! v.isActive())
                return v;
            if (! v.held && ! v.sustained && (oldestReleased == nullptr || v.age < oldestReleased->age))
                oldestReleased = &v;
            if (oldest == nullptr || v.age < oldest->age)
                oldest = &v;
        }

        return oldestReleased != nullptr ? *oldestReleased : *oldest;
    }

    void Engine::noteOn (int playedKey, int vel) noexcept
    {
        if (vel <= 0)
        {
            noteOff (playedKey);
            return;
        }
        if (font == nullptr || preset == nullptr)
            return;

        const auto key = juce::jlimit (0, 127, playedKey + controls.transpose);

        // An exclusive class (hi-hats, mostly) cuts off whatever else of that class is sounding.
        for (const auto& r : preset->regions)
        {
            const auto cls = r[exclusiveClass];
            if (cls == 0 || ! r.matches (key, vel))
                continue;

            for (auto& v : voices)
            {
                if (v.isActive() && ! v.fading && (*v.region)[exclusiveClass] == cls)
                {
                    v.fading = true;
                    v.volEnv.releaseStep = stepFor96dB ((int) (fadeSeconds * sampleRate));
                    releaseVoice (v);
                }
            }
        }

        for (const auto& r : preset->regions)
            if (r.matches (key, vel))
                startVoice (r, key, playedKey, vel);
    }

    void Engine::startVoice (const Region& r, int key, int playedKey, int vel) noexcept
    {
        const auto& s = font->samples[(size_t) r.sample];
        const auto pcmLength = (juce::int64) font->pcm.size();

        auto& v = findFreeVoice();
        v = Voice {};
        v.region = &r;
        v.key = key;
        v.playedKey = playedKey;
        v.velocity = vel;
        v.age = nextAge++;
        v.held = true;

        const auto effectiveKey = r[keynum] >= 0 ? juce::jmin (127, r[keynum]) : key;
        const auto effectiveVel = r[velocity] > 0 ? juce::jmin (127, r[velocity]) : vel;

        constexpr juce::int64 coarse = 32768;
        v.start     = juce::jlimit ((juce::int64) 0, pcmLength, s.start + r[startAddrsOffset] + coarse * r[startAddrsCoarseOffset]);
        v.end       = juce::jlimit (v.start, pcmLength, s.end + r[endAddrsOffset] + coarse * r[endAddrsCoarseOffset]);
        v.loopStart = juce::jlimit (v.start, v.end, s.loopStart + r[startloopAddrsOffset] + coarse * r[startloopAddrsCoarseOffset]);
        v.loopEnd   = juce::jlimit (v.start, v.end, s.loopEnd + r[endloopAddrsOffset] + coarse * r[endloopAddrsCoarseOffset]);
        v.loopMode  = r[sampleModes] & 3;
        v.looping   = (v.loopMode == 1 || v.loopMode == 3) && v.loopEnd - v.loopStart >= 2;
        v.position  = (double) v.start;

        const auto root = r[overridingRootKey] >= 0 ? r[overridingRootKey] : s.originalPitch;
        v.pitchCents = (float) ((effectiveKey - root) * r[scaleTuning] + r[coarseTune] * 100 + r[fineTune] + s.correction);
        v.rateRatio = s.sampleRate / sampleRate;

        // Attenuation is scaled by 0.4, as on the EMU hardware the format came from (and in FluidSynth),
        // which is what most SoundFonts were voiced against. The default velocity curve is the spec's
        // concave 96 dB one, which works out to velocity squared.
        const auto attenuation = 0.4f * (float) juce::jlimit (0, 1440, r[initialAttenuation]);
        const auto velocityGain = std::pow ((float) effectiveVel / 127.0f, 2.0f * controls.velocity);
        v.gain = centibelsToGain (attenuation) * velocityGain;
        v.panGen = (float) juce::jlimit (-500, 500, r[pan]) / 500.0f;

        const auto keyOffset = 60 - effectiveKey;
        const auto releaseSamples = [&] (Gen g)
        {
            return juce::jmax ((int) (fadeSeconds * sampleRate), (int) ((float) toSamples (r[g]) * controls.releaseScale));
        };

        auto& vol = v.volEnv;
        vol.exponential = true;
        vol.delay  = toSamples (r[delayVolEnv]);
        vol.attack = toSamples (r[attackVolEnv]);
        vol.hold   = toSamples (r[holdVolEnv] + r[keynumToVolEnvHold] * keyOffset);
        vol.decayStep = stepFor96dB (toSamples (r[decayVolEnv] + r[keynumToVolEnvDecay] * keyOffset));
        vol.releaseStep = stepFor96dB (releaseSamples (releaseVolEnv));
        const auto sustainCb = juce::jlimit (0, 1440, r[sustainVolEnv]);
        vol.sustain = sustainCb >= 960 ? 0.0f : centibelsToGain ((float) sustainCb);
        vol.start();

        auto& mod = v.modEnv;
        mod.delay  = toSamples (r[delayModEnv]);
        mod.attack = toSamples (r[attackModEnv]);
        mod.hold   = toSamples (r[holdModEnv] + r[keynumToModEnvHold] * keyOffset);
        mod.decayStep = 1.0f / (float) juce::jmax (1, toSamples (r[decayModEnv] + r[keynumToModEnvDecay] * keyOffset));
        mod.releaseStep = 1.0f / (float) releaseSamples (releaseModEnv);
        mod.sustain = 1.0f - (float) juce::jlimit (0, 1000, r[sustainModEnv]) / 1000.0f;
        mod.start();

        v.modLfo.delay = toSamples (r[delayModLFO]);
        v.modLfo.increment = (float) (absoluteCentsToHz (juce::jlimit (-16000, 4500, r[freqModLFO])) / sampleRate);
        v.vibLfo.delay = toSamples (r[delayVibLFO]);
        v.vibLfo.increment = (float) (absoluteCentsToHz (juce::jlimit (-16000, 4500, r[freqVibLFO])) / sampleRate);

        const auto fc = juce::jlimit (1500, 13500, r[initialFilterFc]);
        v.filtered = fc < 13500 || r[modEnvToFilterFc] != 0 || r[modLfoToFilterFc] != 0;
        if (v.filtered)
        {
            // The spec's default velocity-to-cutoff modulator, only where the font uses its filter.
            v.filterCents = (float) fc - (fc < 13500 ? 2400.0f * (1.0f - (float) effectiveVel / 127.0f) : 0.0f);
            const auto resonance = std::pow (10.0f, (float) juce::jlimit (0, 960, r[initialFilterQ]) / 200.0f);
            v.filterQ = 0.7071f * resonance;
            v.filterGain = 1.0f / std::sqrt (resonance);
        }
    }

    void Engine::releaseVoice (Voice& v) noexcept
    {
        v.held = false;
        v.sustained = false;
        v.volEnv.release();
        v.modEnv.release();
        if (v.loopMode == 3)
            v.looping = false;   // "loop while held, then play out the rest of the sample"
    }

    void Engine::noteOff (int playedKey) noexcept
    {
        for (auto& v : voices)
        {
            if (v.isActive() && v.held && v.playedKey == playedKey)
            {
                if (sustainDown)
                {
                    v.held = false;
                    v.sustained = true;
                }
                else
                {
                    releaseVoice (v);
                }
            }
        }
    }

    void Engine::setSustain (bool down) noexcept
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.isActive() && v.sustained)
                    releaseVoice (v);
    }

    void Engine::allNotesOff() noexcept
    {
        sustainDown = false;
        for (auto& v : voices)
            if (v.isActive() && (v.held || v.sustained))
                releaseVoice (v);
    }

    void Engine::allSoundOff() noexcept
    {
        for (auto& v : voices)
            v = Voice {};
        sustainDown = false;
    }

    int Engine::getNumActiveVoices() const noexcept
    {
        return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.isActive(); });
    }

    std::bitset<128> Engine::getSoundingKeys() const noexcept
    {
        std::bitset<128> keys;
        for (const auto& v : voices)
            if (v.isActive() && (v.held || v.sustained) && juce::isPositiveAndBelow (v.playedKey, 128))
                keys.set ((size_t) v.playedKey);
        return keys;
    }

    //==============================================================================
    float Engine::readSample (const Voice& v, juce::int64 index) const noexcept
    {
        if (v.looping && index >= v.loopEnd)
            index -= v.loopEnd - v.loopStart;
        if (index < v.start)
            index = v.start;
        if (index >= v.end)
            return 0.0f;
        return (float) font->pcm[(size_t) index] * (1.0f / 32768.0f);
    }

    void Engine::render (float* left, float* right, int numSamples) noexcept
    {
        if (font == nullptr)
            return;

        for (auto& v : voices)
            if (v.isActive())
                renderVoice (v, left, right, numSamples);
    }

    void Engine::renderVoice (Voice& v, float* left, float* right, int numSamples) noexcept
    {
        const auto& r = *v.region;
        const auto pi = juce::MathConstants<double>::pi;

        for (int done = 0; done < numSamples && v.isActive();)
        {
            const auto n = juce::jmin (controlBlock, numSamples - done);

            const auto modEnv = v.modEnv.level;
            v.modEnv.advance (n);
            const auto modLfo = v.modLfo.advance (n);
            const auto vibLfo = v.vibLfo.advance (n);

            const auto cents = v.pitchCents + controls.tuneCents + bend * controls.bendRange * 100.0f
                             + modEnv * (float) r[modEnvToPitch]
                             + modLfo * (float) r[modLfoToPitch]
                             + vibLfo * ((float) r[vibLfoToPitch] + modWheel * 50.0f);   // mod wheel: the spec's 50-cent vibrato
            const auto step = v.rateRatio * std::exp2 ((double) cents / 1200.0);

            if (v.filtered)
            {
                const auto fc = v.filterCents + modEnv * (float) r[modEnvToFilterFc] + modLfo * (float) r[modLfoToFilterFc];
                const auto hz = juce::jlimit (20.0, 0.45 * sampleRate, absoluteCentsToHz (fc));
                const auto g = (float) std::tan (pi * hz / sampleRate);
                const auto k = 1.0f / v.filterQ;
                v.a1 = 1.0f / (1.0f + g * (g + k));
                v.a2 = g * v.a1;
                v.a3 = g * v.a2;
            }

            const auto targetModGain = r[modLfoToVolume] != 0 ? centibelsToGain (-modLfo * (float) r[modLfoToVolume]) : 1.0f;
            const auto modGainStep = (targetModGain - v.modGain) / (float) n;

            const auto pan = juce::jlimit (-1.0f, 1.0f, v.panGen + channelPan);
            const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const auto gainLeft = std::cos (angle) * v.gain * (v.filtered ? v.filterGain : 1.0f);
            const auto gainRight = std::sin (angle) * v.gain * (v.filtered ? v.filterGain : 1.0f);

            for (int i = done; i < done + n; ++i)
            {
                const auto env = v.volEnv.next();
                if (v.volEnv.stage == Envelope::Stage::done && env <= 0.0f)
                {
                    v.region = nullptr;
                    break;
                }

                const auto index = (juce::int64) v.position;
                const auto frac = (float) (v.position - (double) index);
                auto x = hermite (readSample (v, index - 1), readSample (v, index), readSample (v, index + 1), readSample (v, index + 2), frac);

                if (v.filtered)
                {
                    const auto v3 = x - v.ic2;
                    const auto v1 = v.a1 * v.ic1 + v.a2 * v3;
                    const auto v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
                    v.ic1 = 2.0f * v1 - v.ic1;
                    v.ic2 = 2.0f * v2 - v.ic2;
                    x = v2;
                }

                v.modGain += modGainStep;
                x *= env * v.modGain;
                left[i]  += x * gainLeft;
                right[i] += x * gainRight;

                v.position += step;
                if (v.looping)
                {
                    while (v.position >= (double) v.loopEnd)
                        v.position -= (double) (v.loopEnd - v.loopStart);
                }
                else if (v.position >= (double) v.end)
                {
                    v.region = nullptr;
                    break;
                }
            }

            done += n;
        }
    }
}
