#include "PianoModel.h"

namespace piano
{
    namespace
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        constexpr double centsPerLog = 1200.0 / 0.69314718055994530942; // cents per natural-log unit

        struct Anchor { double note, value; };

        /** Piecewise-linear across the keyboard through measured points. */
        template <size_t N>
        double across (double note, const std::array<Anchor, N>& anchors)
        {
            if (note <= anchors.front().note) return anchors.front().value;
            if (note >= anchors.back().note)  return anchors.back().value;

            for (size_t i = 1; i < N; ++i)
            {
                if (note <= anchors[i].note)
                {
                    const auto& a = anchors[i - 1];
                    const auto& b = anchors[i];
                    return a.value + (b.value - a.value) * (note - a.note) / (b.note - a.note);
                }
            }
            return anchors.back().value;
        }

        /** A fixed random number in [-1, 1] for each key, so a voicing is the same every time. */
        double keyRandom (int note, int salt)
        {
            juce::Random r ((juce::int64) note * 7919 + salt * 104729);
            return r.nextDouble() * 2.0 - 1.0;
        }

        double allpassPhaseDelay (double a, double w)
        {
            const auto phase = std::atan2 (-std::sin (w), a + std::cos (w))
                             - std::atan2 (-a * std::sin (w), 1.0 + a * std::cos (w));
            return -phase / w;
        }

        double onePolePhaseDelay (double p, double w)
        {
            return std::atan2 (p * std::sin (w), 1.0 - p * std::cos (w)) / w;
        }

        /** |H| of (1 - p) / (1 - p z^-1), which is 1 at DC. */
        double onePoleMagnitude (double p, double w)
        {
            return (1.0 - p) / std::sqrt (1.0 - 2.0 * p * std::cos (w) + p * p);
        }

        double perPassGain (double t60, double frequency)
        {
            return std::pow (10.0, -3.0 / (t60 * frequency));
        }
    }

    //==============================================================================
    KeySpec specFor (int midiNote, Voicing voicing)
    {
        const auto n = (double) juce::jlimit (lowestNote, highestNote, midiNote);
        const auto mills = voicing == Voicing::mills;
        KeySpec s;

        s.numStrings = n <= 32 ? 1 : (n <= 46 ? 2 : 3);

        // Stretch tuning (Railsback curve): the tuner matches inharmonic partials across octaves.
        auto cents = n > 69 ? 0.0197 * (n - 69) * (n - 69) : -0.013 * (69 - n) * (69 - n);
        if (mills)
            cents = cents * 1.25 + 3.0 * keyRandom (midiNote, 1); // not touched up since the session
        s.frequency = 440.0 * std::pow (2.0, (n - 69.0) / 12.0 + cents / 1200.0);

        // Uprights have short, stiff strings: more inharmonicity than a grand, most in the bass.
        s.inharmonicity = std::pow (10.0, across (n, std::array<Anchor, 8> { { { 21, -3.2 }, { 33, -3.55 }, { 45, -3.72 }, { 60, -3.5 },
                                                                                { 69, -3.3 }, { 84, -2.72 }, { 96, -2.2 }, { 108, -1.7 } } }));

        s.impedance = across (n, std::array<Anchor, 6> { { { 21, 9.0 }, { 36, 5.1 }, { 48, 3.3 }, { 60, 2.3 }, { 84, 1.7 }, { 108, 1.2 } } });

        // Hammer data after Chaigne & Askenfelt (1994), lightened for an upright action.
        s.hammerMass = 0.001 * across (n, std::array<Anchor, 5> { { { 21, 7.0 }, { 36, 4.8 }, { 60, 3.0 }, { 84, 2.4 }, { 108, 1.8 } } });
        s.feltExponent = across (n, std::array<Anchor, 5> { { { 21, 2.2 }, { 36, 2.3 }, { 60, 2.5 }, { 84, 2.8 }, { 108, 3.1 } } });
        s.feltStiffness = std::pow (10.0, across (n, std::array<Anchor, 5> { { { 21, 8.4 }, { 36, 8.6 }, { 60, 9.65 }, { 84, 10.5 }, { 108, 11.2 } } }));

        s.strikePosition = across (n, std::array<Anchor, 4> { { { 21, 0.12 }, { 60, 0.118 }, { 84, 0.1 }, { 108, 0.07 } } });

        s.sustainT60 = std::pow (10.0, across (n, std::array<Anchor, 7> { { { 21, 1.15 }, { 48, 1.1 }, { 60, 1.0 }, { 72, 0.85 },
                                                                           { 84, 0.6 }, { 96, 0.3 }, { 108, -0.15 } } }));
        s.promptT60 = s.sustainT60 / across (n, std::array<Anchor, 3> { { { 21, 5.0 }, { 60, 6.0 }, { 108, 3.0 } } });
        s.highT60 = across (n, std::array<Anchor, 5> { { { 21, 0.8 }, { 48, 1.2 }, { 60, 1.5 }, { 84, 1.0 }, { 108, 0.35 } } });

        // A fresh tuning: unisons within about a cent.
        s.unisonCents = { -0.5 * std::abs (keyRandom (midiNote, 2)), 0.1 * keyRandom (midiNote, 3), 0.5 * std::abs (keyRandom (midiNote, 4)) };

        if (mills)
        {
            // Hard, worn felt with grooves pressed in: much stiffer, so the tone is bright and jangly.
            s.feltStiffness *= std::pow (10.0, 0.45);
            s.strikePosition *= 0.85;
            // Old strings and a tired board: less sustain, and the upper partials go first. (The
            // wide unisons below already shorten the decay: each string drains on its own.)
            s.sustainT60 *= 0.7;
            s.highT60 *= 0.85;
            // Honky-tonk unisons, a different spread on every key.
            const auto spread = 3.0 + 1.5 * keyRandom (midiNote, 5);
            s.unisonCents = { -spread * (0.6 + 0.4 * std::abs (keyRandom (midiNote, 6))),
                              1.2 * keyRandom (midiNote, 7),
                              spread * (0.6 + 0.4 * std::abs (keyRandom (midiNote, 8))) };
        }

        if (s.numStrings == 2)
            s.unisonCents = { s.unisonCents[0], s.unisonCents[2], 0.0 };
        else if (s.numStrings == 1)
            s.unisonCents = {};

        // Bridge velocity falls away up the keyboard (lighter hammers, shorter pulses), but the board
        // radiates the treble far better than the bass. Measured at the bridge, then levelled to an
        // even scale with a slight treble roll-off. The bass single strings couple to the bridge
        // weakly, so they move it less.
        auto gainDb = across (n, std::array<Anchor, 13> { { { 33, -2.1 }, { 40, -0.5 }, { 48, 2.4 }, { 55, 3.3 }, { 60, 4.2 },
                                                            { 64, 5.4 }, { 69, 6.8 }, { 76, 9.6 }, { 84, 12.0 }, { 91, 15.2 },
                                                            { 96, 17.7 }, { 103, 21.0 }, { 108, 19.2 } } });
        if (s.numStrings == 1)
            gainDb = 13.6;
        s.outputGain = juce::Decibels::decibelsToGain (gainDb);

        s.highT60 = juce::jmin (s.highT60, s.sustainT60);
        s.hasDamper = n <= 88; // the top of the keyboard has no dampers
        return s;
    }

    //==============================================================================
    LoopDesign designLoop (const KeySpec& spec, double sampleRate)
    {
        LoopDesign d;
        const auto f1 = spec.frequency;
        const auto w1 = twoPi * f1 / sampleRate;
        const auto period = sampleRate / f1;

        // Loss filter pole: how much faster a partial at fh dies than the fundamental.
        const auto fh = juce::jmin (juce::jmax (3000.0, 2.5 * f1), 0.4 * sampleRate);
        const auto wh = twoPi * fh / sampleRate;
        const auto targetRatio = perPassGain (spec.highT60, f1) / perPassGain (spec.sustainT60, f1);
        if (targetRatio < 1.0)
        {
            double lo = 0.0, hi = 0.97;
            for (int i = 0; i < 50; ++i)
            {
                const auto p = 0.5 * (lo + hi);
                (onePoleMagnitude (p, wh) / onePoleMagnitude (p, w1) > targetRatio ? lo : hi) = p;
            }
            d.lossPole = 0.5 * (lo + hi);
        }

        // Partials the dispersion filter should place: f_k = k f1 sqrt ((1 + B k^2) / (1 + B)).
        // Only the first 16, up to 5 kHz: one allpass shape can't follow B k^2 much further, and
        // trying to makes it give up the low partials, which carry the pitch.
        const auto B = spec.inharmonicity;
        std::vector<double> omega, loopDelay;
        for (int k = 1; k <= 16; ++k)
        {
            const auto fk = k * f1 * std::sqrt ((1.0 + B * k * k) / (1.0 + B));
            if (fk > juce::jmin (0.42 * sampleRate, 5000.0))
                break;
            omega.push_back (twoPi * fk / sampleRate);
            loopDelay.push_back (k * sampleRate / fk); // the loop must delay partial k by k periods
        }

        const auto lossDelay1 = onePolePhaseDelay (d.lossPole, w1);

        // Squared tuning error of partials 2..K in cents, the fundamental being tuned exactly.
        // Weighted 1/k: the low partials carry the pitch and most of the energy, so they matter most.
        auto cost = [&] (int stages, double a)
        {
            const auto ap1 = stages > 0 ? allpassPhaseDelay (a, w1) : 0.0;
            double sum = 0.0, weights = 0.0;
            for (size_t k = 1; k < omega.size(); ++k)
            {
                const auto ap = stages > 0 ? allpassPhaseDelay (a, omega[k]) : 0.0;
                const auto have = stages * (ap - ap1) + onePolePhaseDelay (d.lossPole, omega[k]) - lossDelay1;
                const auto want = loopDelay[k] - loopDelay[0];
                const auto centsError = centsPerLog * (have - want) / loopDelay[k];
                const auto weight = 1.0 / (double) (k + 1);
                sum += weight * centsError * centsError;
                weights += weight;
            }
            return weights > 0.0 ? sum / weights : 0.0;
        };

        auto fits = [&] (int stages, double a)
        {
            return stages * allpassPhaseDelay (a, w1) + lossDelay1 + 1.6 <= period;
        };

        auto bestScore = cost (0, 0.0);
        for (int stages : { 1, 2, 3, 4, 6, 8, 10, 12, 16 })
        {
            // Coarse search over a, spaced towards -1 where it matters, then golden-section refine.
            constexpr int gridSize = 48;
            auto coefAt = [] (int i) { return -(1.0 - std::pow (10.0, -0.05 * i)); };

            int bestIndex = -1;
            double bestCost = std::numeric_limits<double>::max();
            for (int i = 1; i < gridSize; ++i)
            {
                const auto a = coefAt (i);
                if (! fits (stages, a))
                    break;
                if (const auto c = cost (stages, a); c < bestCost)
                {
                    bestCost = c;
                    bestIndex = i;
                }
            }
            if (bestIndex < 0)
                continue;

            auto lo = coefAt (juce::jmin (bestIndex + 1, gridSize - 1)), hi = coefAt (bestIndex - 1);
            constexpr double golden = 0.6180339887498949;
            for (int i = 0; i < 40; ++i)
            {
                const auto a = hi - golden * (hi - lo);
                const auto b = lo + golden * (hi - lo);
                const auto ca = fits (stages, a) ? cost (stages, a) : std::numeric_limits<double>::max();
                const auto cb = fits (stages, b) ? cost (stages, b) : std::numeric_limits<double>::max();
                (ca < cb ? hi : lo) = ca < cb ? b : a;
            }
            auto a = 0.5 * (lo + hi);
            if (! fits (stages, a) || cost (stages, a) > bestCost)
                a = coefAt (bestIndex);

            // Each stage costs CPU; only keep more of them if the partials get noticeably closer.
            if (const auto score = cost (stages, a) + 0.03 * stages; score < bestScore)
            {
                bestScore = score;
                d.stages = stages;
                d.dispersionCoef = a;
            }
        }

        return d;
    }

    //==============================================================================
    juce::AudioBuffer<float> makeSoundboard (double sampleRate, Voicing voicing)
    {
        const auto mills = voicing == Voicing::mills;
        const auto length = (int) (sampleRate * (mills ? 0.4 : 0.5));
        juce::AudioBuffer<float> ir (2, length);
        ir.clear();

        juce::Random rng (mills ? 1905 : 1977);

        // Board mobility as heard from the front of an upright: small board, weak low end.
        auto envelopeDb = [mills] (double f)
        {
            const auto lowCorner = mills ? 105.0 : 75.0;
            double db = 0.0;
            if (f < lowCorner) db += 40.0 * std::log10 (f / lowCorner);
            if (f > 1000.0)    db -= 13.3 * std::log10 (f / 1000.0);
            if (f > 5000.0)    db -= 26.6 * std::log10 (f / 5000.0);

            auto bump = [f] (double centre, double gainDb, double octaves)
            {
                const auto x = std::log2 (f / centre) / octaves;
                return gainDb * std::exp (-x * x);
            };
            if (mills)
                db += bump (420.0, 4.0, 0.6) + bump (1800.0, 5.0, 0.5) - 2.0; // boxy and forward
            else
                db += bump (250.0, 2.0, 0.8);
            return db;
        };

        auto addMode = [&] (double f, double amplitude, double t60)
        {
            const auto w = twoPi * f / sampleRate;
            const auto r = std::pow (10.0, -3.0 / (t60 * sampleRate));
            const auto samples = juce::jmin (length, (int) (t60 * sampleRate * 1.4));
            const std::complex<double> step = std::polar (r, w);

            for (int ch = 0; ch < 2; ++ch)
            {
                // Two listening positions: same modes, different amplitudes and phases.
                const auto a = amplitude * std::sqrt (-2.0 * std::log (1.0 - rng.nextDouble() * 0.999));
                auto z = std::polar (a, twoPi * rng.nextDouble());
                auto* out = ir.getWritePointer (ch);
                for (int i = 0; i < samples; ++i)
                {
                    out[i] += (float) z.imag();
                    z *= step;
                }
            }
        };

        const auto loss = mills ? 0.028 : 0.022;
        const auto maxT60 = mills ? 0.33 : 0.42;
        const auto top = juce::jmin (16000.0, 0.45 * sampleRate);

        for (auto f = 40.0; f < top;)
        {
            const auto density = 0.04 + f / 30000.0; // modes per Hz; they crowd together going up
            f += -std::log (1.0 - rng.nextDouble() * 0.999) / density;
            const auto amplitude = std::pow (10.0, envelopeDb (f) / 20.0) / std::sqrt (density);
            const auto t60 = juce::jmin (2.2 / (f * loss * (0.8 + 0.4 * rng.nextDouble())), maxT60);
            addMode (f, amplitude, t60);
        }

        // Cabinet and air-cavity resonances.
        struct Resonance { double f, db, t60; };
        const auto cabinet = mills ? std::array<Resonance, 3> { { { 118, 9, 0.3 }, { 240, 7, 0.25 }, { 410, 6, 0.2 } } }
                                   : std::array<Resonance, 3> { { { 105, 8, 0.35 }, { 190, 6, 0.3 }, { 330, 4, 0.25 } } };
        for (const auto& c : cabinet)
            addMode (c.f, std::pow (10.0, (envelopeDb (c.f) + c.db) / 20.0) * 3.0, c.t60);

        // Unit energy, so the board is about as loud as the bridge signal on its own.
        double energy = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                energy += juce::square ((double) ir.getSample (ch, i));
        ir.applyGain ((float) (1.0 / std::sqrt (energy * 0.5)));

        const auto fade = length / 5;
        for (int i = 0; i < fade; ++i)
        {
            const auto g = (float) (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * i / fade));
            for (int ch = 0; ch < 2; ++ch)
                ir.setSample (ch, length - fade + i, ir.getSample (ch, length - fade + i) * g);
        }

        return ir;
    }

    //==============================================================================
    void Hammer::prepare (double sampleRate)
    {
        rate = sampleRate;
        const auto size = juce::nextPowerOfTwo ((int) std::ceil (sampleRate / 26.0 * oversampling) + 16);
        left.assign ((size_t) size, 0.0);
        right.assign ((size_t) size, 0.0);
        mask = size - 1;
    }

    int Hammer::strike (const KeySpec& spec, double periodSamples, double velocity, float hardness,
                        float* force, int maxLength)
    {
        const auto dt = 1.0 / (rate * oversampling);
        const auto Z = spec.impedance * spec.numStrings; // the hammer meets all its strings at once
        const auto halfAdmittance = 1.0 / (2.0 * Z);

        // Harder felt: stiffer and more sharply nonlinear, keeping the force at 0.5 mm in proportion.
        const auto shift = (double) hardness - 0.5;
        const auto p = spec.feltExponent + 0.3 * shift;
        const auto K = spec.feltStiffness * std::pow (10.0, 1.2 * shift) * std::pow (5e-4, spec.feltExponent - p);

        // Round trips from the contact point to each end of the string.
        const auto nearTrip = juce::jmax (1, juce::roundToInt (spec.strikePosition * periodSamples * oversampling));
        const auto farTrip = juce::jmax (1, juce::roundToInt ((1.0 - spec.strikePosition) * periodSamples * oversampling));

        double hammerPos = 0.0, hammerVel = velocity, stringPos = 0.0;
        int length = 0;

        for (int idx = 0; length < maxLength; ++idx)
        {
            const auto inLeft = idx >= nearTrip ? -left[(size_t) ((idx - nearTrip) & mask)] : 0.0;
            const auto inRight = idx >= farTrip ? -0.98 * right[(size_t) ((idx - farTrip) & mask)] : 0.0;

            const auto compression = hammerPos - stringPos;
            const auto f = compression > 0.0 ? K * std::pow (compression, p) : 0.0;
            const auto pushed = f * halfAdmittance;

            left[(size_t) (idx & mask)] = inRight + pushed;
            right[(size_t) (idx & mask)] = inLeft + pushed;

            stringPos += (inLeft + inRight + pushed) * dt;
            hammerVel -= f / spec.hammerMass * dt;
            hammerPos += hammerVel * dt;

            if (idx % oversampling == 0)
                force[length] = 0.0f;
            force[length] += (float) (f / oversampling);

            if (idx % oversampling == oversampling - 1)
            {
                ++length;
                // Done once the hammer has bounced clear of the string.
                if (hammerVel < 0.0 && hammerPos - stringPos < -2e-4)
                    break;
            }
        }

        return length;
    }

    //==============================================================================
    void Key::prepare (const KeySpec& newSpec, const LoopDesign& newDesign, double sampleRate)
    {
        spec = newSpec;
        design = newDesign;
        rate = sampleRate;
        numStrings = spec.numStrings;
        random.setSeed ((juce::int64) (spec.frequency * 1000.0));

        const auto longest = (int) std::ceil (sampleRate / spec.frequency * 1.01) + 4;
        for (auto& s : strings)
        {
            s.line.assign ((size_t) juce::nextPowerOfTwo (longest), 0.0f);
            s.mask = (int) s.line.size() - 1;
        }

        force.assign ((size_t) std::ceil (0.016 * sampleRate) + 8, 0.0f);
        damperCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sampleRate))); // felt settles in ~20 ms
        reset();
    }

    void Key::reset()
    {
        for (auto& s : strings)
        {
            std::fill (s.line.begin(), s.line.end(), 0.0f);
            s.pos = 0;
            s.lossState = s.tuneState = 0.0f;
            s.apState.fill (0.0f);
        }
        forceLength = forcePos = 0;
        damper = 0.0f;
        active = held = dampersOn = fastRelease = false;
        level = 0.0f;
        age = quietSamples = 0;
    }

    void Key::tuneString (String& s, double frequency, double t60)
    {
        const auto w1 = twoPi * frequency / rate;
        const auto a = design.dispersionCoef;
        const auto remaining = rate / frequency
                             - design.stages * (design.stages > 0 ? allpassPhaseDelay (a, w1) : 0.0)
                             - onePolePhaseDelay (design.lossPole, w1);

        s.delay = juce::jlimit (1, s.mask - 1, (int) std::floor (remaining - 0.5));
        const auto fraction = remaining - s.delay;

        // The tuning allpass makes up the fractional delay exactly at the fundamental.
        double lo = -0.95, hi = 0.95;
        for (int i = 0; i < 40; ++i)
        {
            const auto c = 0.5 * (lo + hi);
            (allpassPhaseDelay (c, w1) > fraction ? lo : hi) = c;
        }

        s.tuneCoef = (float) (0.5 * (lo + hi));
        s.apCoef = (float) a;
        s.lossPole = (float) design.lossPole;
        s.lossGain = (float) juce::jmin (0.99999, perPassGain (t60, frequency) / onePoleMagnitude (design.lossPole, w1));
    }

    void Key::strike (Hammer& hammer, double velocity, const Controls& controls)
    {
        if (! active)
            reset();

        const auto period = rate / spec.frequency;
        forceLength = hammer.strike (spec, period, velocity, controls.hardness, force.data(), (int) force.size());
        forcePos = 0;
        combDelay = juce::jmax (1, juce::roundToInt (spec.strikePosition * period));

        static constexpr std::array<std::array<double, maxStrings>, maxStrings> spreads { { { 0.0, 0.0, 0.0 },
                                                                                             { -0.5, 0.5, 0.0 },
                                                                                             { -1.0, 0.1, 1.0 } } };
        const auto& spread = spreads[(size_t) numStrings - 1];
        const auto decay = (double) controls.decay;

        // A lone string has no slow out-of-phase mode, so it rings with the sustain T60 throughout:
        // half its loss goes to the bridge, half to the string itself.
        const auto single = numStrings == 1;
        const auto loopT60 = spec.sustainT60 * decay * (single ? 2.0 : 1.0);

        for (int i = 0; i < numStrings; ++i)
        {
            auto& s = strings[(size_t) i];
            const auto cents = spec.unisonCents[(size_t) i] + 4.0 * controls.unison * spread[(size_t) i];
            tuneString (s, spec.frequency * std::pow (2.0, cents / 1200.0), loopT60);
            // Velocity wave per newton; hammers never hit all strings of a key quite evenly.
            s.exciteGain = (float) ((1.0 + 0.08 * (random.nextDouble() * 2.0 - 1.0)) / (2.0 * numStrings * spec.impedance));
            s.damperLoopGain = (float) perPassGain (0.25, spec.frequency);
        }

        // Bridge conductance, set so the in-phase mode decays with the prompt T60.
        const auto g = perPassGain (single ? loopT60 : spec.promptT60 * decay, spec.frequency);
        const auto x = (1.0 - g) / (1.0 + g);
        coupling = (float) (2.0 * x / (numStrings * (1.0 + x)));

        loopLength = (int) std::ceil (period) + 1;
        active = true;
        fastRelease = false;
        level = 1.0f;
        age = quietSamples = 0;
    }

    void Key::render (float* out, int numSamples)
    {
        const auto damperTarget = dampersOn ? 1.0f : 0.0f;
        const auto outputGain = (float) spec.outputGain;
        const auto fastLoopGain = (float) perPassGain (0.03, spec.frequency);
        float peak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            float excite = 0.0f;
            if (forcePos < forceLength + combDelay)
            {
                // Direct pulse minus its inverted reflection off the near end of the string.
                const auto now = forcePos < forceLength ? force[(size_t) forcePos] : 0.0f;
                const auto back = forcePos - combDelay;
                const auto reflected = back >= 0 && back < forceLength ? force[(size_t) back] : 0.0f;
                excite = now - reflected;
                ++forcePos;
            }

            damper += (damperTarget - damper) * damperCoef;

            std::array<float, maxStrings> arriving {};
            float sum = 0.0f;
            for (int i = 0; i < numStrings; ++i)
            {
                auto& s = strings[(size_t) i];
                auto y = s.line[(size_t) ((s.pos - s.delay) & s.mask)];

                s.lossState = s.lossGain * (1.0f - s.lossPole) * y + s.lossPole * s.lossState;
                y = s.lossState;

                for (int k = 0; k < design.stages; ++k)
                {
                    const auto o = s.apCoef * y + s.apState[(size_t) k];
                    s.apState[(size_t) k] = y - s.apCoef * o;
                    y = o;
                }

                const auto o = s.tuneCoef * y + s.tuneState;
                s.tuneState = y - s.tuneCoef * o;

                arriving[(size_t) i] = o;
                sum += o;
            }

            const auto bridge = coupling * sum;

            for (int i = 0; i < numStrings; ++i)
            {
                auto& s = strings[(size_t) i];
                const auto dampedGain = fastRelease ? fastLoopGain : s.damperLoopGain;
                const auto loopGain = 1.0f - damper * (1.0f - dampedGain);
                const auto w = (arriving[(size_t) i] - bridge + excite * s.exciteGain) * loopGain;
                s.line[(size_t) s.pos] = w;
                s.pos = (s.pos + 1) & s.mask;
                peak = juce::jmax (peak, std::abs (w));
            }

            out[n] += bridge * outputGain;
        }

        level = peak;
        age += numSamples;
        quietSamples = peak < 3e-6f ? quietSamples + numSamples : 0;
    }

    //==============================================================================
    void Engine::prepare (double sampleRate, Voicing newVoicing)
    {
        if (juce::exactlyEqual (sampleRate, rate) && newVoicing == voicing)
        {
            allNotesOff();
            return;
        }

        rate = sampleRate;
        voicing = newVoicing;
        hammer.prepare (sampleRate);

        for (int i = 0; i < numKeys; ++i)
        {
            const auto spec = specFor (lowestNote + i, voicing);
            keys[(size_t) i].prepare (spec, designLoop (spec, sampleRate), sampleRate);
        }
    }

    void Engine::noteOn (int midiNote, float velocity01)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        auto& key = keys[(size_t) (midiNote - lowestNote)];

        int activeCount = 0;
        for (auto& k : keys)
            activeCount += k.active ? 1 : 0;

        if (! key.active && activeCount >= maxActiveKeys)
        {
            // Steal the quietest released key, or else the oldest one.
            Key* victim = nullptr;
            for (auto& k : keys)
            {
                if (! k.active || k.fastRelease)
                    continue;
                if (victim == nullptr
                    || (! k.held && (victim->held || k.level < victim->level))
                    || (k.held && victim->held && k.age > victim->age))
                    victim = &k;
            }
            if (victim != nullptr)
                victim->fastRelease = true;
        }

        // Hammer speed in m/s, roughly exponential in MIDI velocity. The upright soft pedal moves
        // the hammers closer to the strings (half-blow), so they arrive slower.
        auto speed = 0.25 * std::pow (28.0, (double) juce::jlimit (0.0f, 1.0f, velocity01));
        if (soft)
            speed *= 0.65;

        key.strike (hammer, speed, controls);
        key.held = true;
    }

    void Engine::noteOff (int midiNote)
    {
        if (midiNote >= lowestNote && midiNote <= highestNote)
            keys[(size_t) (midiNote - lowestNote)].held = false;
    }

    void Engine::allNotesOff()
    {
        for (auto& k : keys)
            k.reset();
        sustain = soft = false;
    }

    void Engine::updateDampers()
    {
        for (auto& k : keys)
            k.dampersOn = k.fastRelease || (! k.held && ! sustain && k.getSpec().hasDamper);
    }

    void Engine::render (float* out, int numSamples)
    {
        updateDampers();

        for (auto& k : keys)
        {
            if (! k.active)
                continue;

            k.render (out, numSamples);

            if (k.isSilent())
                k.reset();
        }
    }
}
