#include "FormantVoice.h"

namespace parrot
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float nasalPoleHz = 270.0f;
    constexpr int controlInterval = 8;

    // Levels, set with ParrotRender so a held "ah" peaks around -12 dBFS at full velocity.
    constexpr float cascadeLevel = 0.08f;
    constexpr float aspirationLevel = 0.6f;
    constexpr float fricationLevel = 0.32f;

    struct Shape { float f1, f2, f3; };

    /** Adult male vowel formants; Size scales them. */
    Shape vowelShape (Ph p, bool secondPart)
    {
        switch (p)
        {
            case Ph::IY: return { 300, 2300, 3000 };
            case Ph::IH: return { 420, 2000, 2650 };
            case Ph::EH: return { 560, 1800, 2550 };
            case Ph::AE: return { 680, 1700, 2450 };
            case Ph::AA: return { 750, 1200, 2500 };
            case Ph::AO: return { 620, 950, 2500 };
            case Ph::OH: return { 480, 900, 2400 };
            case Ph::UH: return { 450, 1100, 2350 };
            case Ph::UW: return { 320, 900, 2250 };
            case Ph::AH: return { 620, 1200, 2550 };
            case Ph::ER: return { 470, 1350, 1700 };
            case Ph::UU: return { 340, 1300, 2300 };
            case Ph::AY: return secondPart ? vowelShape (Ph::IH, false) : vowelShape (Ph::AA, false);
            case Ph::EY: return secondPart ? vowelShape (Ph::IY, false) : Shape { 480, 2050, 2650 };
            case Ph::OW: return secondPart ? vowelShape (Ph::UW, false) : vowelShape (Ph::OH, false);
            case Ph::AW: return secondPart ? vowelShape (Ph::UH, false) : vowelShape (Ph::AA, false);
            case Ph::OY: return secondPart ? vowelShape (Ph::IY, false) : vowelShape (Ph::AO, false);
            default:     return { 500, 1500, 2500 };
        }
    }

    float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    FormantVoice::Segment segment (const FormantVoice::Target& t, float ms, float formTau, float ampTau)
    {
        FormantVoice::Segment s;
        s.target = t;
        s.ms = ms;
        s.formTau = formTau;
        s.ampTau = ampTau;
        return s;
    }

    void setFrication (FormantVoice::Target& t, float f1, float q1, float g1, float f2, float q2, float g2)
    {
        t.fricF = { f1, f2 };
        t.fricQ = { q1, q2 };
        t.fricGain = { g1, g2 };
    }
}

//==============================================================================
void FormantVoice::Resonator::set (float f, float bw, float sampleRate) noexcept
{
    const auto r = std::exp (-pi * bw / sampleRate);
    c = -r * r;
    b = 2.0f * r * std::cos (2.0f * pi * f / sampleRate);
    a = 1.0f - b - c;
}

void FormantVoice::AntiResonator::set (float f, float bw, float sampleRate) noexcept
{
    const auto r = std::exp (-pi * bw / sampleRate);
    const auto rc = -r * r;
    const auto rb = 2.0f * r * std::cos (2.0f * pi * f / sampleRate);
    const auto ra = 1.0f - rb - rc;
    a = 1.0f / ra;
    b = -rb / ra;
    c = -rc / ra;
}

void FormantVoice::BandPass::set (float f, float q, float sampleRate) noexcept
{
    const auto w0 = 2.0f * pi * f / sampleRate;
    const auto alpha = std::sin (w0) / (2.0f * q);
    const auto a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * std::cos (w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

//==============================================================================
void FormantVoice::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    reset();
}

void FormantVoice::reset()
{
    queueHead = queueSize = 0;
    segmentSamplesLeft = -1;
    current = target = vowelTarget (Ph::AA, false);
    current.av = target.av = 0.0f;
    nasalPole.set (nasalPoleHz, 100.0f, fs);
    nasalPole.clear();
    nasalZero.clear();
    for (auto& r : formants)
        r.clear();
    for (auto& b : fricBands)
        b.clear();
    updateFilters();
    phase = tilt = dcIn = dcOut = 0.0f;
    gain = gainTarget = 0.0f;
    pitch = pitchTarget = 60.0f;
    vibratoPhase = 0.0f;
    vowelAge = 0;
    drift = driftTarget = 0.0f;
    holding = false;
    currentNote = -1;
    syllable = {};
}

//==============================================================================
FormantVoice::Target FormantVoice::vowelTarget (Ph p, bool secondPart) const
{
    const auto shape = vowelShape (p, secondPart);
    const auto k = settings.size;
    Target t;
    t.f = { shape.f1 * k, shape.f2 * k, shape.f3 * k, 3500.0f * k, 4500.0f * k };
    t.bw = { 70.0f, 90.0f, 150.0f, 200.0f, 250.0f };
    t.av = 1.0f;
    return t;
}

FormantVoice::Target FormantVoice::heldTarget (Ph p) const
{
    if (isVowel (p))
        return vowelTarget (p, false);

    // A held consonant ("mm", "shh", ん): its steady part, taken from a coda.
    SegmentList list;
    appendConsonant (list, p == Ph::Nsyl ? Ph::N : p, vowelTarget (Ph::AH, false), true, false);
    return list.size > 0 ? list.items[(size_t) list.size - 1].target : vowelTarget (Ph::AH, false);
}

void FormantVoice::appendConsonant (SegmentList& list, Ph c, const Target& vowel, bool inCoda, bool first) const
{
    const auto d = settings.diction;
    const auto k = settings.size;

    auto locus = [&] (float f1, float f2, float f3)
    {
        Target t = vowel;
        t.f[0] = f1 * k;
        t.f[1] = f2 * k;
        t.f[2] = f3 * k;
        t.av = t.ah = t.af = 0.0f;
        t.fricGain = { 0.0f, 0.0f };
        return t;
    };

    switch (c)
    {
        case Ph::P: case Ph::B: case Ph::T: case Ph::D: case Ph::K: case Ph::G:
        {
            const bool voiceless = c == Ph::P || c == Ph::T || c == Ph::K;
            const bool labial = c == Ph::P || c == Ph::B, velar = c == Ph::K || c == Ph::G;
            const float f2 = labial ? 800.0f : (velar ? 2100.0f : 1700.0f);
            const float f3 = labial ? 2200.0f : (velar ? 2300.0f : 2600.0f);

            auto closure = locus (250.0f, f2, f3);
            closure.av = voiceless ? 0.0f : 0.12f;
            list.push (segment (closure, inCoda ? 60.0f : (first ? 30.0f : 45.0f), 10.0f, 4.0f));

            auto burst = locus (300.0f, f2, f3);
            burst.af = (voiceless ? 0.9f : 0.55f) * d;
            if (labial)      setFrication (burst, 1200.0f, 0.7f, 0.5f, 3000.0f, 0.8f, 0.2f);
            else if (velar)  setFrication (burst, f2 * k, 2.5f, 0.9f, 3500.0f, 2.0f, 0.3f);
            else             setFrication (burst, 4500.0f, 1.2f, 0.8f, 7000.0f, 1.2f, 0.4f);
            list.push (segment (burst, velar ? 12.0f : 8.0f, 6.0f, 1.0f));

            if (voiceless)
            {
                // Aspiration: the vowel's formants with a wide F1 and noise in place of voice.
                auto aspiration = vowel;
                aspiration.bw[0] *= 2.5f;
                aspiration.av = 0.0f;
                aspiration.ah = (inCoda ? 0.25f : 0.5f) * d;
                aspiration.af = 0.15f * d;
                aspiration.fricF = burst.fricF;
                aspiration.fricQ = burst.fricQ;
                aspiration.fricGain = burst.fricGain;
                list.push (segment (aspiration, inCoda ? 35.0f : 45.0f, 15.0f, 4.0f));
            }
            else if (inCoda)
            {
                auto release = locus (350.0f, f2, f3);
                release.av = 0.2f;
                list.push (segment (release, 25.0f, 10.0f, 6.0f));
            }
            return;
        }

        case Ph::S: case Ph::Z: case Ph::SH: case Ph::ZH:
        case Ph::F: case Ph::V: case Ph::TH: case Ph::DH:
        {
            const bool voiced = c == Ph::Z || c == Ph::ZH || c == Ph::V || c == Ph::DH;
            Target t;
            float level = 0.5f;
            if (c == Ph::S || c == Ph::Z)
            {
                t = locus (300.0f, 1700.0f, 2600.0f);
                setFrication (t, 6200.0f, 2.2f, 1.0f, 8500.0f, 1.5f, 0.35f);
            }
            else if (c == Ph::SH || c == Ph::ZH)
            {
                t = locus (300.0f, 1900.0f, 2400.0f);
                setFrication (t, 2700.0f, 2.5f, 0.8f, 4500.0f, 2.0f, 0.6f);
            }
            else if (c == Ph::F || c == Ph::V)
            {
                t = locus (300.0f, 900.0f, 2200.0f);
                setFrication (t, 3000.0f, 0.5f, 0.35f, 7000.0f, 0.7f, 0.25f);
                level = 0.35f;
            }
            else
            {
                t = locus (300.0f, 1500.0f, 2700.0f);
                setFrication (t, 4500.0f, 0.6f, 0.3f, 7500.0f, 0.8f, 0.25f);
                level = 0.3f;
            }

            t.af = level * (voiced ? 0.6f : 1.0f) * d;
            t.av = voiced ? (c == Ph::DH || c == Ph::V ? 0.45f : 0.35f) : 0.0f;

            const bool sibilant = c == Ph::S || c == Ph::Z || c == Ph::SH || c == Ph::ZH;
            float ms = voiced ? 65.0f : (sibilant ? 105.0f : 85.0f);
            if (inCoda)
                ms *= 1.35f;
            list.push (segment (t, ms, 12.0f, 6.0f));
            return;
        }

        case Ph::H:
        {
            auto t = vowel;
            t.bw[0] *= 2.0f;
            t.av = 0.0f;
            t.ah = 0.45f * d;
            list.push (segment (t, 60.0f, 4.0f, 6.0f));
            return;
        }

        case Ph::CH: case Ph::JH:
        {
            const bool voiced = c == Ph::JH;
            auto closure = locus (250.0f, 1800.0f, 2500.0f);
            closure.av = voiced ? 0.12f : 0.0f;
            list.push (segment (closure, inCoda ? 50.0f : (first ? 25.0f : 40.0f), 10.0f, 4.0f));

            auto fric = locus (300.0f, 1900.0f, 2400.0f);
            setFrication (fric, 2700.0f, 2.5f, 0.8f, 4500.0f, 2.0f, 0.6f);
            fric.af = (voiced ? 0.35f : 0.55f) * d;
            fric.av = voiced ? 0.3f : 0.0f;
            list.push (segment (fric, voiced ? 50.0f : 70.0f, 8.0f, 1.5f));
            return;
        }

        case Ph::M: case Ph::N: case Ph::NG:
        {
            auto t = c == Ph::M ? locus (260.0f, 1000.0f, 2200.0f)
                   : c == Ph::N ? locus (260.0f, 1500.0f, 2600.0f)
                                : locus (260.0f, 2100.0f, 2500.0f);
            t.nasalZero = c == Ph::M ? 1000.0f : (c == Ph::N ? 1800.0f : 3000.0f);
            t.bw = { 60.0f, 150.0f, 200.0f, 250.0f, 300.0f };
            t.av = 0.55f;
            list.push (segment (t, inCoda ? 100.0f : 65.0f, 6.0f, 6.0f));
            return;
        }

        case Ph::L: case Ph::R: case Ph::W: case Ph::Y:
        {
            auto t = c == Ph::L ? locus (330.0f, 1100.0f, 2800.0f)
                   : c == Ph::R ? locus (360.0f, 1200.0f, 1550.0f)
                   : c == Ph::W ? locus (300.0f, 650.0f, 2200.0f)
                                : locus (270.0f, 2200.0f, 3000.0f);
            t.av = c == Ph::W ? 0.75f : 0.8f;
            const float ms = c == Ph::Y ? 45.0f : (c == Ph::W ? 50.0f : 60.0f);
            list.push (segment (t, inCoda ? ms * 1.4f : ms, 12.0f, 8.0f));
            return;
        }

        case Ph::RT:
        {
            auto t = locus (350.0f, 1600.0f, 2600.0f);
            t.av = 0.55f;
            list.push (segment (t, 22.0f, 4.0f, 3.0f));
            return;
        }

        case Ph::Q:
        {
            auto t = locus (250.0f, vowel.f[1] / k, vowel.f[2] / k);
            list.push (segment (t, 80.0f, 10.0f, 3.0f));
            return;
        }

        default:
            return;
    }
}

void FormantVoice::appendOnset (SegmentList& list, const Syllable& syl, bool fromSilence) const
{
    const auto vowel = heldTarget (syl.nucleus);
    for (int i = 0; i < syl.numOnset; ++i)
        appendConsonant (list, syl.onset[(size_t) i], vowel, false, i == 0 && fromSilence);
}

void FormantVoice::appendCoda (SegmentList& list, const Syllable& syl, bool release) const
{
    auto vowel = heldTarget (syl.nucleus);
    if (isDiphthong (syl.nucleus))
    {
        vowel = vowelTarget (syl.nucleus, true);
        list.push (segment (vowel, 120.0f, 35.0f, 10.0f));
    }

    for (int i = 0; i < syl.numCoda; ++i)
        appendConsonant (list, syl.coda[(size_t) i], vowel, true, false);

    if (release)
    {
        auto silent = list.size > 0 ? list.items[(size_t) list.size - 1].target : vowel;
        // A coda ends on its own consonant's sound; a bare vowel fades out over the release.
        silent.av = silent.ah = silent.af = 0.0f;
        const auto releaseMs = juce::jmax (10.0f, settings.releaseSeconds * 1000.0f);
        list.push (segment (silent, releaseMs * 1.5f, 30.0f, syl.numCoda > 0 ? 8.0f : releaseMs / 3.0f));
    }
}

float FormantVoice::onsetSeconds (const Syllable& syl) const
{
    if (syl.slur)
        return 0.0f;
    SegmentList list;
    appendOnset (list, syl, false);
    return list.totalMs() * 0.001f;
}

float FormantVoice::codaSeconds (const Syllable& syl) const
{
    SegmentList list;
    appendCoda (list, syl, false);
    return list.totalMs() * 0.001f;
}

//==============================================================================
void FormantVoice::noteOn (const Syllable& syl, int note, float velocity)
{
    const bool legato = holding;
    const bool fromSilence = ! isActive();
    SegmentList list;

    Segment vowel;
    vowel.ms = -1.0f;
    vowel.pitchNote = note;
    vowel.glide = legato;
    vowel.startsVowel = true;
    vowel.ampTau = 10.0f;

    if (syl.slur)
    {
        // Carry the vowel on to the new note; the syllable (and so its coda) stays the same.
        vowel.target = heldTarget (syllable.nucleus);
        vowel.formTau = 30.0f;
        vowel.startsVowel = ! legato;
    }
    else
    {
        if (legato)
            appendCoda (list, syllable, false);
        syllable = syl;
        appendOnset (list, syl, fromSilence);
        vowel.target = heldTarget (syl.nucleus);
        vowel.formTau = list.size > 0 ? 18.0f : (legato ? 30.0f : 2.0f);
    }

    list.push (vowel);

    if (fromSilence)
    {
        // Start from the first sound's shape, so nothing sweeps in from the last note.
        const auto& first = list.items[0].target;
        current.f = first.f;
        current.bw = first.bw;
        current.nasalZero = first.nasalZero;
        current.fricF = first.fricF;
        current.fricQ = first.fricQ;
        updateFilters();
    }

    if (! legato)
    {
        pitch = pitchTarget = (float) note;
        vowelAge = 0;
    }

    gainTarget = 0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, velocity);
    holding = true;
    currentNote = note;
    startQueue (list);
}

void FormantVoice::noteOff()
{
    if (! holding)
        return;
    holding = false;
    SegmentList list;
    appendCoda (list, syllable, true);
    startQueue (list);
}

void FormantVoice::glideTo (int note)
{
    currentNote = note;
    pitchTarget = (float) note;
}

void FormantVoice::stop()
{
    holding = false;
    SegmentList list;
    auto silent = target;
    silent.av = silent.ah = silent.af = 0.0f;
    list.push (segment (silent, 30.0f, 20.0f, 5.0f));
    startQueue (list);
}

bool FormantVoice::isActive() const noexcept
{
    return holding || queueSize > 0 || segmentSamplesLeft > 0
        || current.av + current.ah + current.af > 1.0e-4f;
}

void FormantVoice::startQueue (const SegmentList& list)
{
    queueHead = 0;
    queueSize = list.size;
    for (int i = 0; i < list.size; ++i)
        queue[(size_t) i] = list.items[(size_t) i];
    segmentSamplesLeft = 0;   // the first segment begins on the next sample
}

void FormantVoice::beginSegment (const Segment& s)
{
    target = s.target;
    formCoef = 1.0f - std::exp (-(float) controlInterval / juce::jmax (1.0f, s.formTau * 0.001f * fs));
    ampCoef = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, s.ampTau * 0.001f * fs));
    segmentSamplesLeft = s.ms < 0.0f ? -1 : juce::jmax (1, juce::roundToInt (s.ms * 0.001f * fs));

    if (s.pitchNote >= 0)
    {
        pitchTarget = (float) s.pitchNote;
        if (! s.glide)
            pitch = pitchTarget;
    }
    if (s.startsVowel)
        vowelAge = 0;
}

void FormantVoice::updateFilters() noexcept
{
    const auto limit = fs * 0.45f;

    // As singers do on high notes: F1 never drops below the fundamental, and the lower formants
    // widen with pitch, so the level doesn't swing with where the harmonics happen to fall.
    const auto hz = juce::jmin (sungHz, 1200.0f);
    formants[0].set (juce::jmin (juce::jmax (current.f[0], 1.1f * hz), limit), juce::jmax (current.bw[0], 0.4f * hz), fs);
    formants[1].set (juce::jmin (current.f[1], limit), juce::jmax (current.bw[1], 0.3f * hz), fs);
    for (size_t i = 2; i < formants.size(); ++i)
        formants[i].set (juce::jmin (current.f[i], limit), current.bw[i], fs);
    nasalZero.set (current.nasalZero, 100.0f, fs);
    for (size_t i = 0; i < fricBands.size(); ++i)
        fricBands[i].set (juce::jmin (current.fricF[i], limit), current.fricQ[i], fs);
}

float FormantVoice::nextNoise() noexcept
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return (float) (std::int32_t) noiseState * (1.0f / 2147483648.0f);
}

//==============================================================================
void FormantVoice::render (float* out, int numSamples) noexcept
{
    const auto openQuotient = juce::jmap (settings.tone, 0.72f, 0.42f);
    const auto sourceLevel = 0.6f / openQuotient;
    const auto tiltHz = juce::jmap (settings.tone, 1800.0f, 9000.0f);
    const auto tiltCoef = 1.0f - std::exp (-2.0f * pi * tiltHz / fs);
    const auto pitchCoef = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, settings.glideSeconds * 0.5f * fs));
    const auto gainCoef = 1.0f - std::exp (-1.0f / (0.01f * fs));
    const auto driftCoef = 1.0f - std::exp (-1.0f / (0.15f * fs));
    const auto vibratoDepth = (settings.vibratoCents + settings.modWheel * 60.0f) * 0.01f;
    const auto vibratoDelay = (int) (0.25f * fs), vibratoRamp = (int) (0.3f * fs);
    const auto breath = settings.breath * 0.35f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! isActive())
        {
            out[i] = 0.0f;
            continue;
        }

        // Segments
        while (segmentSamplesLeft == 0)
        {
            if (queueSize > 0)
            {
                beginSegment (queue[(size_t) queueHead]);
                ++queueHead;
                --queueSize;
            }
            else
            {
                segmentSamplesLeft = -1;
            }
        }
        if (segmentSamplesLeft > 0)
            --segmentSamplesLeft;

        if (--controlCountdown <= 0)
        {
            controlCountdown = controlInterval;
            for (size_t k = 0; k < 5; ++k)
            {
                current.f[k] += (target.f[k] - current.f[k]) * formCoef;
                current.bw[k] += (target.bw[k] - current.bw[k]) * formCoef;
            }
            current.nasalZero += (target.nasalZero - current.nasalZero) * formCoef;
            for (size_t k = 0; k < 2; ++k)
            {
                current.fricF[k] += (target.fricF[k] - current.fricF[k]) * formCoef;
                current.fricQ[k] += (target.fricQ[k] - current.fricQ[k]) * formCoef;
            }
            updateFilters();

            if (--driftCountdown <= 0)
            {
                driftCountdown = (int) (0.25f * fs) / controlInterval;
                driftTarget = nextNoise() * 0.08f;
            }
        }

        current.av += (target.av - current.av) * ampCoef;
        current.ah += (target.ah - current.ah) * ampCoef;
        current.af += (target.af - current.af) * ampCoef;
        current.fricGain[0] += (target.fricGain[0] - current.fricGain[0]) * ampCoef;
        current.fricGain[1] += (target.fricGain[1] - current.fricGain[1]) * ampCoef;
        gain += (gainTarget - gain) * gainCoef;

        // Pitch
        pitch += (pitchTarget - pitch) * pitchCoef;
        drift += (driftTarget - drift) * driftCoef;
        ++vowelAge;
        const auto ramp = juce::jlimit (0.0f, 1.0f, (float) (vowelAge - vibratoDelay) / (float) vibratoRamp);
        vibratoPhase += settings.vibratoRate / fs;
        if (vibratoPhase >= 1.0f)
            vibratoPhase -= 1.0f;
        const auto vibrato = std::sin (2.0f * pi * vibratoPhase) * vibratoDepth * ramp;
        const auto note = pitch + settings.pitchBend + vibrato + drift;
        const auto dt = juce::jlimit (1.0e-5f, 0.45f, 440.0f * std::exp2 ((note - 69.0f) / 12.0f) / fs);
        sungHz = dt * fs;

        // Glottal source: the derivative of a KLGLOTT88 flow pulse, open for the first part of each period.
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;

        float g = 0.0f;
        if (phase < openQuotient)
        {
            const auto t = phase / openQuotient;
            g = 2.0f * t - 3.0f * t * t;
        }
        auto sinceClosure = phase - openQuotient;
        if (sinceClosure < 0.0f)
            sinceClosure += 1.0f;
        g += 0.5f * polyBlep (sinceClosure, dt);   // the jump back to zero at closure

        tilt += (g * sourceLevel * current.av - tilt) * tiltCoef;

        const auto noise = nextNoise();
        const auto open = phase < openQuotient ? 1.0f : 0.35f;
        const auto aspiration = noise * (current.ah + breath * current.av * open) * aspirationLevel;

        auto y = nasalPole.process (tilt + aspiration);
        y = nasalZero.process (y);
        for (auto& r : formants)
            y = r.process (y);

        const auto fric = (fricBands[0].process (noise) * current.fricGain[0]
                           + fricBands[1].process (noise) * current.fricGain[1]) * current.af * fricationLevel;

        const auto x = (y * cascadeLevel + fric) * gain;

        // Block DC from the voiced stops' closures.
        dcOut = x - dcIn + 0.995f * dcOut;
        dcIn = x;
        out[i] = dcOut;
    }
}
}
