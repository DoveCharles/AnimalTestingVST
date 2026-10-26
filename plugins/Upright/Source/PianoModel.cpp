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

        struct LoopTuning { int delay; double coef; };

        /** Splits what's left of a loop's period, after its filters, into whole samples and a
            first-order allpass that makes up the fraction exactly at the fundamental. */
        LoopTuning tuneLoop (double sampleRate, double frequency, int stages, double dispersionCoef,
                             double lossPole, int maxDelay)
        {
            const auto w1 = twoPi * frequency / sampleRate;
            const auto remaining = sampleRate / frequency
                                 - stages * (stages > 0 ? allpassPhaseDelay (dispersionCoef, w1) : 0.0)
                                 - onePolePhaseDelay (lossPole, w1);

            const auto delay = juce::jlimit (1, maxDelay, (int) std::floor (remaining - 0.5));
            const auto fraction = remaining - delay;

            double lo = -0.95, hi = 0.95;
            for (int i = 0; i < 40; ++i)
            {
                const auto c = 0.5 * (lo + hi);
                (allpassPhaseDelay (c, w1) > fraction ? lo : hi) = c;
            }
            return { delay, 0.5 * (lo + hi) };
        }

        /** Coefficients of a TPT state-variable bandpass; its output times k peaks at unity. */
        void bandpassCoefs (double frequency, double q, double sampleRate, float& a1, float& a2, float& a3, float& k)
        {
            const auto g = std::tan (juce::MathConstants<double>::pi * juce::jmin (frequency, 0.45 * sampleRate) / sampleRate);
            k = (float) (1.0 / q);
            a1 = (float) (1.0 / (1.0 + g * (g + k)));
            a2 = (float) (g * a1);
            a3 = (float) (g * a2);
        }

        /** One step of that bandpass; returns the band output (times k for unity peak). */
        float bandpass (float x, float a1, float a2, float a3, float& ic1, float& ic2)
        {
            const auto v3 = x - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return v1;
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
            // Against a recording of the real thing the first decay is slower than a modern
            // upright's and the jangle of the upper partials hangs on: the old board takes energy
            // from the strings less eagerly.
            s.promptT60 *= 3.0;
            s.highT60 *= 4.0;
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
        else if (mills)
            gainDb += 9.5; // the Mills unisons couple more loosely (promptT60 above), so move the bridge less
        s.outputGain = juce::Decibels::decibelsToGain (gainDb);

        // Tension modulation is strongest on the long, heavy bass strings, which swing furthest.
        // Phantom partials gather around the longitudinal resonance, roughly 17 times the
        // fundamental (Conklin); above the tenor it is out of hearing range.
        s.tension = across (n, std::array<Anchor, 4> { { { 21, 0.015 }, { 48, 0.01 }, { 72, 0.004 }, { 96, 0.0 } } });
        s.phantom = across (n, std::array<Anchor, 4> { { { 21, 0.05 }, { 60, 0.035 }, { 84, 0.012 }, { 100, 0.0 } } });
        s.longitudinalFrequency = 17.0 * s.frequency;

        // The horizontal plane: a lone bass string has nothing else to carry its tail, so its
        // plane is louder and slower; on a unison it only roughens the aftersound a little.
        s.planeCents = (0.3 + 0.6 * std::abs (keyRandom (midiNote, 9))) * (mills ? 1.6 : 1.0);
        s.planeLevel = (s.numStrings == 1 ? 0.2 : 0.02) * (1.0 + 0.3 * keyRandom (midiNote, 10));
        s.planeDecay = s.numStrings == 1 ? 2.5 : 0.9;

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
    juce::AudioBuffer<float> makeSoundboard (double sampleRate, Voicing voicing, DrivingPoint point)
    {
        const auto mills = voicing == Voicing::mills;
        const auto length = (int) (sampleRate * (mills ? 0.4 : 0.5));
        juce::AudioBuffer<float> ir (2, length);
        ir.clear();

        // The same random board for every driving point.
        juce::Random rng (mills ? 1905 : 1977);

        // Board mobility as heard from the front of an upright: small board, weak low end.
        auto envelopeDb = [mills] (double f)
        {
            const auto lowCorner = mills ? 105.0 : 75.0;
            double db = 0.0;
            if (f < lowCorner) db += 40.0 * std::log10 (f / lowCorner);
            if (f > 1000.0)    db -= (mills ? 6.6 : 13.3) * std::log10 (f / 1000.0);
            if (f > 5000.0)    db -= (mills ? 13.3 : 26.6) * std::log10 (f / 5000.0);

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

        // Positions on a 1.4 x 0.9 m board, in metres from the bass end and the bottom edge.
        struct Place { double x, y; };
        const Place drive = point == DrivingPoint::bass  ? Place { 0.30, 0.30 }
                          : point == DrivingPoint::tenor ? Place { 0.60, 0.35 }
                                                         : Place { 1.15, 0.72 };
        const std::array<Place, 2> mics { { { 0.30, 0.60 }, { 1.10, 0.60 } } };

        // Each mode's shape near a point is roughly a plane wave of bending: a 9 mm spruce board
        // has wavenumber k = 0.286 sqrt (2 pi f) rad/m, so low modes move nearby points together
        // and high ones don't.
        auto addMode = [&] (double f, double amplitude, double t60)
        {
            const auto k = 0.286 * std::sqrt (twoPi * f);
            const auto direction = twoPi * rng.nextDouble();
            const auto phase = twoPi * rng.nextDouble();
            const auto kx = k * std::cos (direction), ky = k * std::sin (direction);
            auto shape = [&] (Place p) { return std::sqrt (2.0) * std::cos (kx * p.x + ky * p.y + phase); };

            // Below a few hundred hertz the board is small against the wavelength in air, so both
            // mics hear its net motion alike; only higher up do they pick out their own patch of it.
            const auto local = f * f / (f * f + 250.0 * 250.0);
            auto heard = [&] (Place p) { return std::sqrt (1.0 - local) + std::sqrt (local) * shape (p); };

            const auto w = twoPi * f / sampleRate;
            const auto r = std::pow (10.0, -3.0 / (t60 * sampleRate));
            const auto samples = juce::jmin (length, (int) (t60 * sampleRate * 1.4));
            const std::complex<double> step = std::polar (r, w);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto z = std::complex<double> (amplitude * shape (drive) * heard (mics[(size_t) ch]), 0.0);
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

    juce::AudioBuffer<float> makeRoom (double sampleRate)
    {
        const auto length = (int) (sampleRate * 1.3);
        juce::AudioBuffer<float> ir (2, length);
        ir.clear();
        juce::Random rng (1931); // the year Studio Two opened

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* out = ir.getWritePointer (ch);

            // Early reflections off the floor, the screens round the piano and the near walls,
            // darker the later they arrive.
            constexpr int numReflections = 14;
            for (int i = 0; i < numReflections; ++i)
            {
                const auto time = 0.006 + 0.07 * std::pow ((i + rng.nextDouble()) / numReflections, 1.4);
                const auto gain = (rng.nextBool() ? 1.0 : -1.0) * 0.5 * std::exp (-time / 0.05);
                const auto at = (int) (time * sampleRate);
                const auto smear = 1 + (int) (time * sampleRate * 0.08); // absorbed highs
                for (int j = 0; j < smear && at + j < length; ++j)
                    out[at + j] += (float) (gain / smear);
            }

            // Diffuse tail: noise faded in as the reflections thin out, decaying with a 1.1 s
            // T60, and darker as it goes on.
            float lowPass = 0.0f;
            for (int i = 0; i < length; ++i)
            {
                const auto t = i / sampleRate;
                const auto fadeIn = juce::jlimit (0.0, 1.0, (t - 0.02) / 0.06);
                const auto envelope = 0.12 * fadeIn * std::pow (10.0, -3.0 * t / 1.1);
                const auto cutoff = 9000.0 * std::pow (0.2, t / 1.1);
                const auto coef = (float) (1.0 - std::exp (-twoPi * cutoff / sampleRate));
                lowPass += ((float) (rng.nextDouble() * 2.0 - 1.0) - lowPass) * coef;
                out[i] += (float) envelope * lowPass;
            }
        }

        double energy = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                energy += juce::square ((double) ir.getSample (ch, i));
        ir.applyGain ((float) (1.0 / std::sqrt (energy * 0.5)));

        const auto fade = length / 6;
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
        // The felt soaks up the high partials in a few passes; well above the fundamental, so it
        // barely pulls the pitch on the way down.
        dampCoef = (float) (1.0 - std::exp (-twoPi * juce::jmin (juce::jmax (8.0 * spec.frequency, 1200.0), 0.4 * sampleRate) / sampleRate));

        energyCoef = (float) (1.0 - std::exp (-1.0 / (0.01 * sampleRate)));
        phantomLowCoef = (float) (1.0 - std::exp (-twoPi * 0.5 * spec.frequency / sampleRate));
        bandpassCoefs (spec.longitudinalFrequency, 2.5, sampleRate, bandA1, bandA2, bandA3, bandK);
        reset();
    }

    void Key::reset()
    {
        for (auto& s : strings)
        {
            std::fill (s.line.begin(), s.line.end(), 0.0f);
            s.pos = 0;
            s.lossState = s.tuneState = s.dampState = 0.0f;
            s.apState.fill (0.0f);
        }
        forceLength = forcePos = 0;
        energy = phantomLow = bandIc1 = bandIc2 = 0.0f;
        damper = 0.0f;
        active = held = dampersOn = fastRelease = false;
        level = 0.0f;
        age = quietSamples = 0;
    }

    void Key::tuneString (String& s, double frequency, double t60)
    {
        const auto w1 = twoPi * frequency / rate;
        const auto tuning = tuneLoop (rate, frequency, design.stages, design.dispersionCoef, design.lossPole, s.mask - 1);

        s.delay = tuning.delay;
        s.tuneCoef = (float) tuning.coef;
        s.apCoef = (float) design.dispersionCoef;
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

        // The horizontal plane: hit at a slight angle, so weakly excited, and drained by its own
        // loss rather than through the vertical bridge.
        auto& plane = strings[(size_t) numStrings];
        tuneString (plane, spec.frequency * std::pow (2.0, (spec.unisonCents[0] + spec.planeCents) / 1200.0),
                    spec.sustainT60 * spec.planeDecay * decay);
        plane.exciteGain = (float) (1.0 / (2.0 * numStrings * spec.impedance));
        plane.damperLoopGain = strings[0].damperLoopGain;
        planeCoupling = (float) (spec.planeLevel * coupling * numStrings);

        auto shortest = strings[0].delay;
        for (int i = 1; i <= numStrings; ++i)
            shortest = juce::jmin (shortest, strings[(size_t) i].delay);
        maxShift = (float) juce::jmax (0, shortest - 3); // the read looks two samples ahead
        tensionSamples = (float) (spec.tension * period);

        loopLength = (int) std::ceil (period) + 1;
        active = true;
        fastRelease = false;
        level = 1.0f;
        age = quietSamples = 0;
    }

    void Key::render (float* bridgeOut, float* radiatedOut, int numSamples)
    {
        const auto damperTarget = dampersOn ? 1.0f : 0.0f;
        const auto fastLoopGain = (float) perPassGain (0.03, spec.frequency);
        const auto outputGain = (float) spec.outputGain;
        const auto normalise = outputGain / referenceLevel;
        const auto phantomGain = (float) spec.phantom * referenceLevel;
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

            // A loud string is stretched further, so it's tauter and waves go round faster:
            // reading the line `shift` samples nearer the write head shortens the loop.
            const auto shift = juce::jmin (tensionSamples * energy, maxShift);
            const auto whole = (int) shift;
            const auto frac = shift - (float) whole;

            std::array<float, maxStrings + 1> arriving {};
            float sum = 0.0f;
            for (int i = 0; i <= numStrings; ++i)
            {
                auto& s = strings[(size_t) i];
                // Cubic Lagrange: a linear read would lowpass the loop on every pass while the
                // shift is fractional, and strip the attack of its upper partials.
                const auto at = s.pos - s.delay + whole;
                const auto ym1 = s.line[(size_t) ((at - 1) & s.mask)];
                const auto y0 = s.line[(size_t) (at & s.mask)];
                const auto y1 = s.line[(size_t) ((at + 1) & s.mask)];
                const auto y2 = s.line[(size_t) ((at + 2) & s.mask)];
                const auto d = frac;
                auto y = -d * (d - 1.0f) * (d - 2.0f) / 6.0f * ym1 + (d + 1.0f) * (d - 1.0f) * (d - 2.0f) / 2.0f * y0
                         - (d + 1.0f) * d * (d - 2.0f) / 2.0f * y1 + (d + 1.0f) * d * (d - 1.0f) / 6.0f * y2;

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
                if (i < numStrings)
                    sum += o;
            }

            const auto vertical = coupling * sum;
            const auto bridge = vertical + planeCoupling * arriving[(size_t) numStrings];

            for (int i = 0; i <= numStrings; ++i)
            {
                auto& s = strings[(size_t) i];
                const auto dampedGain = fastRelease ? fastLoopGain : s.damperLoopGain;
                const auto loopGain = 1.0f - damper * (1.0f - dampedGain);
                const auto reflected = i < numStrings ? vertical : 0.0f;
                auto w = (arriving[(size_t) i] - reflected + excite * s.exciteGain) * loopGain;

                // The felt takes the high partials off first.
                s.dampState += (w - s.dampState) * dampCoef;
                w += 0.5f * damper * (s.dampState - w);

                s.line[(size_t) s.pos] = w;
                s.pos = (s.pos + 1) & s.mask;
                peak = juce::jmax (peak, std::abs (w));
            }

            // The stretch goes with the square of the motion, so the force it puts on the bridge
            // has the sums (and differences) of the string's frequencies: phantom partials. The
            // slow beats among the differences are filtered off, and the longitudinal resonance
            // brings out the partials near it.
            const auto u = bridge * normalise;
            const auto squared = u * u;
            energy += (squared - energy) * energyCoef;
            phantomLow += (squared - phantomLow) * phantomLowCoef;
            const auto high = squared - phantomLow;
            const auto band = bandpass (high, bandA1, bandA2, bandA3, bandIc1, bandIc2);

            // While the felt is landing it rattles against the moving string.
            auto buzz = 0.0f;
            const auto landing = damper * (1.0f - damper);
            if (dampersOn && landing > 0.01f)
                buzz = feltNoise * 1.4f * landing * bridge * (random.nextFloat() * 2.0f - 1.0f);

            bridgeOut[n] += bridge;
            radiatedOut[n] += (bridge + buzz) * outputGain + phantomGain * (high + 3.0f * bandK * band);
        }

        level = peak;
        age += numSamples;
        quietSamples = peak < 3e-6f ? quietSamples + numSamples : 0;
    }

    //==============================================================================
    void Resonator::prepare (const KeySpec& spec, const LoopDesign& design, double sampleRate)
    {
        rate = sampleRate;
        frequency = spec.frequency;
        sustainT60 = spec.sustainT60;

        const auto longest = (int) std::ceil (sampleRate / frequency * 1.01) + 4;
        line.assign ((size_t) juce::nextPowerOfTwo (longest), 0.0f);
        mask = (int) line.size() - 1;
        loopLength = (int) std::ceil (sampleRate / frequency) + 1;

        const auto tuning = tuneLoop (sampleRate, frequency, 0, 0.0, design.lossPole, mask - 1);
        delay = tuning.delay;
        tuneCoef = (float) tuning.coef;
        lossPole = (float) design.lossPole;
        lossScale = (float) (1.0 / onePoleMagnitude (design.lossPole, twoPi * frequency / sampleRate));
        tunedDecay = 0.0;

        damperLoopGain = (float) perPassGain (0.25, frequency);
        damperCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sampleRate)));

        // What the strings give back to the board, through a bridge as stiff as their key's.
        const auto g = perPassGain (spec.promptT60, frequency);
        const auto x = (1.0 - g) / (1.0 + g);
        outputGain = (float) (2.0 * x / (1.0 + x) * spec.outputGain);

        reset();
    }

    void Resonator::reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        pos = 0;
        lossState = tuneState = 0.0f;
        damper = 1.0f;
        quietSamples = loopLength + 1;
    }

    void Resonator::render (const float* input, float* radiatedOut, int numSamples, bool undamped, float drive, float decay)
    {
        if (! juce::exactlyEqual ((double) decay, tunedDecay))
        {
            tunedDecay = decay;
            lossGain = (float) juce::jmin (0.99999, perPassGain (sustainT60 * decay, frequency) * lossScale);
        }

        const auto damperTarget = undamped ? 0.0f : 1.0f;
        float peak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            damper += (damperTarget - damper) * damperCoef;

            lossState = lossGain * (1.0f - lossPole) * line[(size_t) ((pos - delay) & mask)] + lossPole * lossState;
            const auto o = tuneCoef * lossState + tuneState;
            tuneState = lossState - tuneCoef * o;

            const auto w = (o + drive * input[n]) * (1.0f - damper * (1.0f - damperLoopGain));
            line[(size_t) pos] = w;
            pos = (pos + 1) & mask;

            radiatedOut[n] += o * outputGain;
            peak = juce::jmax (peak, std::abs (w));
        }

        quietSamples = peak < 3e-7f ? quietSamples + numSamples : 0;
    }

    //==============================================================================
    void ActionNoise::prepare (double sampleRate)
    {
        rate = sampleRate;
        reset();
    }

    void ActionNoise::reset()
    {
        for (auto& e : events)
            e.remaining = 0;
    }

    bool ActionNoise::isActive() const noexcept
    {
        return std::any_of (events.begin(), events.end(), [] (const Event& e) { return e.remaining > 0; });
    }

    void ActionNoise::add (const Sound& sound)
    {
        // A free slot, or else the one nearest its end.
        auto* slot = &events[0];
        for (auto& e : events)
        {
            if (e.remaining <= 0) { slot = &e; break; }
            if (e.remaining < slot->remaining) slot = &e;
        }

        auto& e = *slot;
        e = {};
        e.wait = (int) (sound.waitMs * 0.001 * rate);
        e.toStrings = sound.toStrings;

        e.noise = (float) (sound.noiseLevel * amount);
        e.noiseDecay = (float) std::exp (-1000.0 / (sound.noiseMs * rate));
        bandpassCoefs (sound.noiseHz, sound.noiseQ, rate, e.a1, e.a2, e.a3, e.k);

        e.thump = (float) (sound.thumpLevel * amount);
        e.thumpDecay = (float) std::exp (-1000.0 / (sound.thumpMs * rate));
        // No two knocks ring at quite the same pitch, so a chord's don't pile up in phase.
        const auto thumpHz = sound.thumpHz * (0.85 + 0.3 * random.nextDouble());
        e.cosW = (float) std::cos (twoPi * thumpHz / rate);
        e.sinW = (float) std::sin (twoPi * thumpHz / rate);

        e.remaining = e.wait + (int) (7.0 * juce::jmax (sound.noiseMs, sound.thumpMs) * 0.001 * rate);
    }

    // Levels are in the units of a key's radiated output (a key at velocity 0.7 peaks near
    // referenceLevel), except the damper rail's rush, which is bridge velocity into the strings.

    void ActionNoise::keyDown (int midiNote, float velocity01)
    {
        const auto v = std::pow ((double) juce::jlimit (0.0f, 1.0f, velocity01), 1.5);
        const auto place = (midiNote - lowestNote) / (double) (numKeys - 1);
        // The key bottoming on the keybed, just after the hammer lands: a click and a knock
        // through the key frame.
        add ({ 1.5, 1800.0 + 800.0 * place, 1.2, 5.0, 0.004 * v, 95.0 + 40.0 * place, 18.0, 0.003 * v, false });
    }

    void ActionNoise::keyUp (int midiNote, bool damperLands)
    {
        juce::ignoreUnused (midiNote);
        // The key knocking back against its rail.
        add ({ 0.0, 1100.0, 1.0, 4.0, 0.0012, 140.0, 10.0, 0.0012, false });
        // The damper felt settling back on the strings.
        if (damperLands)
            add ({ 8.0, 500.0, 0.8, 25.0, 0.001, 100.0, 10.0, 0.0, false });
    }

    void ActionNoise::pedal (bool down)
    {
        if (down)
        {
            // The rail lifts every damper at once: a thump, and a faint rush as the felts leave
            // the strings and set the open ones ringing.
            add ({ 0.0, 350.0, 0.7, 70.0, 0.003, 75.0, 40.0, 0.005, false });
            add ({ 5.0, 900.0, 0.5, 60.0, 0.0006, 100.0, 10.0, 0.0, true });
        }
        else
        {
            add ({ 0.0, 300.0, 0.7, 50.0, 0.0025, 65.0, 30.0, 0.004, false });
        }
    }

    void ActionNoise::render (float* radiatedOut, float* stringsOut, int numSamples)
    {
        for (auto& e : events)
        {
            auto* out = e.toStrings ? stringsOut : radiatedOut;
            for (int n = 0; n < numSamples && e.remaining > 0; ++n)
            {
                --e.remaining;
                if (e.wait > 0)
                {
                    --e.wait;
                    continue;
                }

                const auto band = bandpass (random.nextFloat() * 2.0f - 1.0f, e.a1, e.a2, e.a3, e.ic1, e.ic2);
                const auto re = e.re * e.cosW - e.im * e.sinW;
                e.im = e.re * e.sinW + e.im * e.cosW;
                e.re = re;

                out[n] += e.noise * e.k * band + e.thump * e.im;
                e.noise *= e.noiseDecay;
                e.thump *= e.thumpDecay;
            }
        }
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
        noise.prepare (sampleRate);

        for (int i = 0; i < numKeys; ++i)
        {
            const auto spec = specFor (lowestNote + i, voicing);
            const auto design = designLoop (spec, sampleRate);
            keys[(size_t) i].prepare (spec, design, sampleRate);
            resonators[(size_t) i].prepare (spec, design, sampleRate);

            // Singles and bichords sit on the bass bridge; the rest along the treble bridge. The
            // board feeds sum to one, so the mono sum is the same as before the split.
            auto& r = routing[(size_t) i];
            r.fill (0.0f);
            if (spec.numStrings < 3)
            {
                r[bassBoard] = 1.0f;
            }
            else
            {
                const auto t = (float) (lowestNote + i - 47) / (float) (highestNote - 47);
                r[tenorBoard] = 1.0f - t;
                r[topBoard] = t;
            }

            // The close mics hear the strings themselves too, bass on the left.
            const auto place = (float) i / (float) (numKeys - 1);
            const auto angle = juce::MathConstants<float>::pi * (0.25f + 0.32f * (place - 0.5f));
            r[dryLeft] = std::sqrt (2.0f) * std::cos (angle);
            r[dryRight] = std::sqrt (2.0f) * std::sin (angle);
        }
        noiseRouting = { 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
        down.fill (false);
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
        down[(size_t) (midiNote - lowestNote)] = true;
        noise.keyDown (midiNote, velocity01);
    }

    void Engine::noteOff (int midiNote)
    {
        if (midiNote < lowestNote || midiNote > highestNote)
            return;

        const auto i = (size_t) (midiNote - lowestNote);
        auto& key = keys[i];
        key.held = false;

        // Only for keys struck here: the other voicing's engine sees the note-off too.
        if (down[i])
            noise.keyUp (midiNote, key.active && key.getSpec().hasDamper && ! sustain);
        down[i] = false;
    }

    void Engine::setSustain (bool newDown, bool audible)
    {
        if (newDown != sustain && audible)
            noise.pedal (newDown);
        sustain = newDown;
    }

    void Engine::allNotesOff()
    {
        for (auto& k : keys)
            k.reset();
        for (auto& r : resonators)
            r.reset();
        noise.reset();
        down.fill (false);
        sustain = soft = false;
    }

    void Engine::updateDampers()
    {
        for (auto& k : keys)
            k.dampersOn = k.fastRelease || (! k.held && ! sustain && k.getSpec().hasDamper);
    }

    void Engine::render (const Buses& buses, int numSamples)
    {
        updateDampers();
        noise.setAmount (2.0f * controls.noise);
        for (auto& k : keys)
            k.feltNoise = 2.0f * controls.noise;

        // Off its own pitches each open string still passes some of the bridge motion back, and
        // with up to 88 of them that adds up: at full, the halo is about as loud as the note.
        const auto drive = 0.65f * controls.resonance;

        for (int start = 0; start < numSamples; start += subBlock)
        {
            const auto n = juce::jmin (subBlock, numSamples - start);
            std::array<float, subBlock> bridge {}, radiated;
            std::array<bool, numKeys> sounding {};
            bool anySounding = false;

            auto route = [&] (const std::array<float, numBuses>& gains)
            {
                for (int b = 0; b < numBuses; ++b)
                    if (gains[(size_t) b] != 0.0f)
                        juce::FloatVectorOperations::addWithMultiply (buses[(size_t) b] + start, radiated.data(), gains[(size_t) b], n);
            };

            for (size_t i = 0; i < keys.size(); ++i)
            {
                auto& k = keys[i];
                if (! k.active)
                    continue;

                auto& own = keyBridge[i];
                std::fill_n (own.begin(), n, 0.0f);
                std::fill_n (radiated.begin(), n, 0.0f);
                k.render (own.data(), radiated.data(), n);
                juce::FloatVectorOperations::add (bridge.data(), own.data(), n);
                route (routing[i]);
                sounding[i] = anySounding = true;

                if (k.isSilent())
                    k.reset();
            }

            std::fill_n (radiated.begin(), n, 0.0f);
            noise.render (radiated.data(), bridge.data(), n);
            route (noiseRouting);

            // The open strings ring with everything else on the bridge, but not with themselves:
            // a key's own strings are already in its loops.
            const auto driving = drive > 0.0f && (anySounding || noise.isActive());
            for (size_t i = 0; i < resonators.size(); ++i)
            {
                auto& r = resonators[i];
                const auto undamped = down[i] || sustain || ! keys[i].getSpec().hasDamper;
                if (! (undamped && driving) && r.isQuiet())
                    continue;

                std::array<float, subBlock> input;
                for (int j = 0; j < n; ++j)
                    input[(size_t) j] = bridge[(size_t) j] - (sounding[i] ? keyBridge[i][(size_t) j] : 0.0f);
                std::fill_n (radiated.begin(), n, 0.0f);
                r.render (input.data(), radiated.data(), n, undamped, drive, controls.decay);
                route (routing[i]);
            }
        }
    }

    void Engine::render (float* out, int numSamples)
    {
        for (int start = 0; start < numSamples; start += subBlock)
        {
            const auto n = juce::jmin (subBlock, numSamples - start);
            std::array<std::array<float, subBlock>, numBuses> bus {};
            render (Buses { bus[0].data(), bus[1].data(), bus[2].data(), bus[3].data(), bus[4].data() }, n);
            for (int b = bassBoard; b <= topBoard; ++b)
                juce::FloatVectorOperations::add (out + start, bus[(size_t) b].data(), n);
        }
    }
}
