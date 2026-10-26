#pragma once

#include <at_shared/at_shared.h>

/** A lo-fi tape machine, fitted to measurements of a cassette-style tape plugin (see
    tools/TapeCapture/mellofi-targets.md). Signal path:

      Drive    a symmetric two-stage soft clipper, 4x oversampled. Drive pushes the input up and
               pulls the output back, so it saturates harder without getting much louder.
      Noise    mono hiss, added after the drive and before the Tone voicing, so it takes its shape.
      Tone     the tape voicing: a band-pass with a low cut and a 2-3 kHz presence, morphed
               across five fitted settings. Off leaves only the gentle base path.
      Wear     amplitude flutter (fast level noise) and random speed jitter.
      Transport  wow (0.82 Hz), flutter (a few fixed rates) and wear jitter all move the read
               point of one delay line; Tape Stop slows it to a halt and catches up. Speed below
               1 plays the tape slower than real time, so the read point falls further behind:
               the line grows to five minutes (allocated off the audio thread, see offerLongTape)
               and when that runs out the tape holds there at normal speed.
      Filter   bipolar: below zero a low-pass, above zero a high-pass.
      Mechanical noise  a looped motor rumble with a 23 Hz rhythm, added after the voicing.

    Latency is the oversampler's plus a few samples for the delay line's interpolation. */
class TapeMachine
{
public:
    static constexpr int maxChannels = 2;

    struct Settings
    {
        float drive = 0.0f;            // 0..1
        bool boost = false;
        float tone = 0.5f;             // 0..1
        bool toneEnabled = true;
        float filter = 0.0f;           // -1 (low-pass) .. 0 (off) .. 1 (high-pass)
        float wow = 0.0f, flutter = 0.0f, wear = 0.0f;   // 0..1
        float noiseDb = -70.0f, mechanicalDb = -70.0f;   // -70 is off
        bool stereoWidth = false;      // each channel gets its own wow, flutter and wear
        float outputDb = 0.0f;
        bool tapeStop = false;
        double stopSeconds = 2.0;      // how long the tape takes to stop
        bool fastForward = false;      // catch up by speeding through the backlog, or jump back
        float speed = 1.0f;            // playback speed, 0.25..1
    };

    ~TapeMachine() { delete offeredTape.exchange (nullptr); }

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            maxChannels, oversamplingOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maxBlockSize);

        delete offeredTape.exchange (nullptr);
        spareTape.reset();
        tape = std::make_unique<Tape> (lengthFor (shortTapeSeconds));
        longTapeRequested = false;

        for (auto& filter : base)
            filter.reset();
        for (auto& filter : toneFilters)
            filter.reset();
        lowHighPass.reset();
        hissShelf.reset();
        hissLowPass.reset();
        base[0].set (Biquad::highPass, sampleRate, 21.433, 0.523);
        base[1].set (Biquad::lowPass, sampleRate, 20125.66, 0.488);
        hissShelf.set (Biquad::highShelf, sampleRate, 1000.0, 0.6, -6.0);
        hissLowPass.set (Biquad::lowPass, sampleRate, 11000.0, 0.8);

        smoothing = 1.0f - std::exp (-1.0f / (0.2f * (float) sampleRate));
        fastSmoothing = 1.0f - std::exp (-1.0f / (0.01f * (float) sampleRate));
        speedGlide = 1.0 - std::exp (-1.0 / (0.25 * sampleRate));
        jitterPole = std::exp (-juce::MathConstants<float>::twoPi * 10.0f / (float) sampleRate);
        amPole = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 350.0f / (float) sampleRate);

        makeMechanicalLoop();
        reset();
    }

    void reset()
    {
        writeIndex = 0;
        for (auto& line : tape->lines)
            std::fill (line.begin(), line.end(), 0.0f);
        for (auto& m : modulators)
            m = {};
        modulators[1].phaseOffset = 0.37;
        modulators[1].rate = 1.037;
        modulators[1].noise.seed = 0x2545f491u;
        transport = Transport::running;
        speed = 1.0;
        lag = restLag = 0.0;
        jumpGain = 1.0f;
        oversampling->reset();
        shownTone = shownFilter = -2.0f;
        previousIn = previousOut = -1.0f;
        smoothed = {};
        smoothed.output = juce::Decibels::decibelsToGain (settings.outputDb);
        smoothed.toneMix = settings.toneEnabled ? 1.0f : 0.0f;
    }

    void setSettings (const Settings& s) { settings = s; }

    /** True once, on the audio thread, when Speed first drops below 1 and the tape needs to grow:
        the caller should then call offerLongTape() from the message thread. */
    bool needsLongTape() noexcept
    {
        if (longTapeRequested || settings.speed >= 0.999f || tape->size > lengthFor (shortTapeSeconds))
            return false;
        longTapeRequested = true;
        return true;
    }

    /** Message thread: allocates the five-minute tape; process() picks it up. */
    void offerLongTape()
    {
        delete offeredTape.exchange (new Tape (lengthFor (longTapeSeconds)));
    }

    /** Back to real time with a short fade, e.g. when the host starts playing. */
    void resync() noexcept
    {
        if (settings.tapeStop || lag < 0.5)
            return;
        restLag = 0.0;
        transport = Transport::jumping;
    }

    /** The tape's current speed (1 is normal, 0 stopped), for the reels. */
    float getSpeed() const noexcept { return (float) speed; }

    int getLatency() const { return juce::roundToInt (oversampling->getLatencyInSamples()) + minimumDelay; }

    void process (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        numChannels = juce::jmin (numChannels, maxChannels);
        if (auto* longTape = offeredTape.exchange (nullptr))
            adopt (longTape);
        saturate (buffer, numChannels);

        const auto targets = blockTargets();
        auto* const* data = buffer.getArrayOfWritePointers();
        const auto numModulators = settings.stereoWidth && numChannels > 1 ? 2 : 1;

        for (int start = 0; start < buffer.getNumSamples(); start += controlInterval)
        {
            updateFilters();
            const auto end = juce::jmin (buffer.getNumSamples(), start + controlInterval);

            for (int n = start; n < end; ++n)
            {
                smoothed.approach (targets, smoothing, fastSmoothing);
                const auto transportGain = advanceTransport();

                std::array<float, maxChannels> delaySeconds {}, am {};
                for (int m = 0; m < numModulators; ++m)
                    modulators[(size_t) m].advance (smoothed, sampleRate, jitterPole, amPole, delaySeconds[(size_t) m], am[(size_t) m]);
                if (numModulators == 1)
                {
                    delaySeconds[1] = delaySeconds[0];
                    am[1] = am[0];
                }
                const auto hissSample = hissNoise.gaussian();

                const auto mechanical = nextMechanical (smoothed.mechanical);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const auto c = (size_t) ch;
                    auto x = data[ch][n];

                    x += smoothed.hiss * hissLowPass.process (hissShelf.process (hissSample, ch), ch);

                    if (smoothed.toneMix > 1.0e-4f)
                    {
                        auto voiced = toneGain * x;
                        for (auto& filter : toneFilters)
                            voiced = filter.process (voiced, ch);
                        x += smoothed.toneMix * (voiced - x);
                    }

                    x = base[1].process (base[0].process (x, ch), ch);
                    x *= 1.0f + am[c];

                    auto& line = tape->lines[c];
                    line[(size_t) writeIndex] = x;
                    const auto delay = juce::jmax (2.0, (double) minimumDelay + lag + delaySeconds[c] * sampleRate);
                    x = transportGain * readHermite (line, delay);

                    if (filterActive)
                        x = lowHighPass.process (x, ch);

                    x += mechanical[c];
                    data[ch][n] = smoothed.output * x;
                }

                if (++writeIndex == tape->size)
                    writeIndex = 0;
            }
        }
    }

private:
    static constexpr int oversamplingOrder = 2;   // 4x
    static constexpr int minimumDelay = 3;
    static constexpr int controlInterval = 32;

    //==============================================================================
    struct Biquad
    {
        enum Kind { lowPass, highPass, peak, highShelf };

        void set (Kind kind, double fs, double hz, double q, double gainDb = 0.0)
        {
            const auto w = juce::MathConstants<double>::twoPi * juce::jmin (hz, 0.45 * fs) / fs;
            const auto cosW = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const auto A = std::pow (10.0, gainDb / 40.0);
            double b0, b1, b2, a0, a1, a2;
            switch (kind)
            {
                case lowPass:  b0 = (1 - cosW) / 2; b1 = 1 - cosW; b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case highPass: b0 = (1 + cosW) / 2; b1 = -(1 + cosW); b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case peak:     b0 = 1 + alpha * A; b1 = -2 * cosW; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cosW; a2 = 1 - alpha / A; break;
                case highShelf:
                default:
                {
                    const auto root = 2.0 * std::sqrt (A) * alpha;
                    b0 = A * ((A + 1) + (A - 1) * cosW + root);
                    b1 = -2 * A * ((A - 1) + (A + 1) * cosW);
                    b2 = A * ((A + 1) + (A - 1) * cosW - root);
                    a0 = (A + 1) - (A - 1) * cosW + root;
                    a1 = 2 * ((A - 1) - (A + 1) * cosW);
                    a2 = (A + 1) - (A - 1) * cosW - root;
                    break;
                }
            }
            c = { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
        }

        float process (float x, int ch) noexcept
        {
            auto& s = state[(size_t) ch];
            const auto y = c[0] * x + s[0];
            s[0] = c[1] * x - c[3] * y + s[1];
            s[1] = c[2] * x - c[4] * y;
            return y;
        }

        void reset() { state = {}; }

        std::array<float, 5> c { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<std::array<float, 2>, maxChannels> state {};
    };

    struct Noise
    {
        /** Roughly Gaussian, unit variance: the sum of four uniforms. */
        float gaussian() noexcept
        {
            auto sum = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                sum += (float) seed * (1.0f / 4294967296.0f);
            }
            return (sum - 2.0f) * 1.7320508f;
        }

        uint32_t seed = 0x9e3779b9u;
    };

    //==============================================================================
    // Measured curves. Knob positions are evenly spaced from 0 to 1 across each table.

    /** Saturator input and output gains (dB) at Drive 0, .25, .5, .75, 1, and with Boost at 0, .5, 1. */
    static constexpr std::array<float, 5> driveInDb { -16.73f, -9.27f, -0.84f, 7.82f, 16.55f };
    static constexpr std::array<float, 5> driveOutDb { 16.52f, 10.57f, 3.80f, -3.17f, -10.22f };
    static constexpr std::array<float, 3> boostInDb { -5.99f, 10.31f, 27.59f };
    static constexpr std::array<float, 3> boostOutDb { 6.00f, -6.73f, -12.63f };

    /** Tone voicing at Tone 0, .25, .5, .75, 1: gain dB, two high-passes (Hz, Q), three peaks
        (Hz, dB, Q) and two low-passes (Hz, Q). */
    static constexpr int toneValues = 18;
    static constexpr std::array<std::array<float, toneValues>, 5> toneKnots { {
        { -9.11461f, 103.319f, 1.28654f, 89.5173f, 0.499888f, 130.868f, 12.6287f, 0.2f, 243.692f, -3.13692f, 1.17703f, 2980.96f, 6.13468f, 0.484649f, 2016.89f, 0.627747f, 16680.8f, 0.3f },
        { -7.25214f, 102.261f, 1.15124f, 130.514f, 0.697401f, 61.6234f, 12.7306f, 0.221673f, 220.375f, -2.64273f, 1.42295f, 2980.96f, 8.51279f, 0.367904f, 4174.09f, 0.448819f, 19284.7f, 2.71828f },
        { -5.34217f, 102.078f, 1.30897f, 140.785f, 0.500281f, 65.1844f, 12.6496f, 0.226055f, 238.813f, -3.15289f, 1.26123f, 2308.31f, 10.5721f, 0.397805f, 5682.65f, 0.538746f, 20346.8f, 1.09856f },
        { -4.1593f, 103.498f, 1.26869f, 148.413f, 0.489909f, 69.6163f, 12.5949f, 0.218177f, 236.157f, -3.27513f, 1.20815f, 2317.81f, 11.5019f, 0.379083f, 5888.27f, 0.530073f, 20202.0f, 1.18166f },
        { -3.73715f, 102.876f, 1.27319f, 148.413f, 0.486546f, 74.4686f, 12.5661f, 0.222631f, 238.61f, -3.33853f, 1.21624f, 2347.25f, 11.8083f, 0.375057f, 5966.04f, 0.519264f, 20209.9f, 1.18193f },
    } };
    /** Gains interpolate linearly between knots; frequencies and Qs in log. */
    static constexpr std::array<bool, toneValues> toneLinear { true, false, false, false, false, false, true, false, false,
                                                               true, false, false, true, false, false, false, false, false };

    /** Wow speed deviation, RMS %, at 0, .1, ... 1. One sine at 0.824 Hz. */
    static constexpr std::array<float, 11> wowRmsPercent { 0.0f, 0.0845f, 0.2061f, 0.4023f, 0.6719f, 1.0771f, 1.7403f, 2.5958f, 4.0062f, 6.1684f, 9.0021f };
    static constexpr float wowHz = 0.824f;

    /** Flutter speed deviation, RMS %, at 0, .2, ... 1, and its lines at Flutter 1 (Hz, peak %). */
    static constexpr std::array<float, 6> flutterRmsPercent { 0.0f, 0.20f, 0.50f, 0.95f, 1.63f, 2.63f };
    static constexpr int flutterLines = 6;
    static constexpr std::array<float, flutterLines> flutterHz { 1.99f, 3.98f, 6.567f, 13.134f, 14.527f, 29.05f };
    static constexpr std::array<float, flutterLines> flutterPeakPercent { 2.09f, 0.48f, 2.31f, 0.46f, 1.91f, 0.29f };

    /** Wear speed-jitter amount at 0, .1, ... 1 (relative to Wear 1). */
    static constexpr std::array<float, 11> wearJitter { 0.0f, 0.036f, 0.081f, 0.131f, 0.195f, 0.282f, 0.368f, 0.504f, 0.642f, 0.831f, 1.0f };
    static constexpr float jitterSpeedDensity = 2.0e-5f;   // fraction^2 per Hz at Wear 1
    static constexpr float wearAmRms = 0.061f;             // level noise at Wear 1

    /** Calibrated so Noise at 0 dB gives about -46 dBFS (Tone 0.5) and Mechanical Noise -44 dBFS. */
    static constexpr float hissRms = 0.0135f;
    static constexpr float mechanicalRms = 0.00624f;

    /** Mechanical noise third-octave levels (dBFS) from 62 Hz to 20 kHz. */
    static constexpr std::array<float, 26> mechanicalBandsDb { -83.1f, -77.9f, -79.6f, -69.3f, -64.9f, -68.4f, -53.8f, -55.6f, -58.3f,
                                                               -52.6f, -53.7f, -50.2f, -57.6f, -55.5f, -58.8f, -66.3f, -64.2f, -61.3f,
                                                               -62.1f, -61.6f, -59.6f, -62.6f, -63.4f, -68.6f, -83.5f, -99.3f };
    static constexpr float mechanicalFirstBandHz = 62.5f;

    template <size_t N>
    static float lookup (const std::array<float, N>& table, float x) noexcept
    {
        const auto pos = juce::jlimit (0.0f, 1.0f, x) * (float) (N - 1);
        const auto i = juce::jmin ((int) pos, (int) N - 2);
        return table[(size_t) i] + (pos - (float) i) * (table[(size_t) i + 1] - table[(size_t) i]);
    }

    static float dbOrSilence (float db) { return db <= -69.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }

    //==============================================================================
    /** What the per-sample values glide towards: speed-deviation amplitudes as delay (seconds),
        noise and output gains. */
    struct Targets
    {
        float wow = 0.0f, flutter = 0.0f, jitter = 0.0f, am = 0.0f;
        float hiss = 0.0f, mechanical = 0.0f, output = 1.0f, toneMix = 1.0f;

        void approach (const Targets& t, float slow, float fast) noexcept
        {
            wow += slow * (t.wow - wow);
            flutter += slow * (t.flutter - flutter);
            jitter += slow * (t.jitter - jitter);
            am += fast * (t.am - am);
            hiss += fast * (t.hiss - hiss);
            mechanical += fast * (t.mechanical - mechanical);
            output += fast * (t.output - output);
            toneMix += fast * (t.toneMix - toneMix);
        }
    };

    Targets blockTargets() const
    {
        Targets t;
        // A speed deviation of a (fraction) at f Hz moves the read point by a / (2 pi f) seconds.
        t.wow = lookup (wowRmsPercent, settings.wow) * 0.01f * juce::MathConstants<float>::sqrt2;
        t.flutter = lookup (flutterRmsPercent, settings.flutter) / flutterRmsPercent.back();
        // Jitter: white speed noise integrated into the delay; scale per sample so the density holds at any rate.
        t.jitter = lookup (wearJitter, settings.wear) * std::sqrt (jitterSpeedDensity / (2.0f * (float) sampleRate));
        t.am = wearAmRms * std::pow (juce::jlimit (0.0f, 1.0f, settings.wear), 2.6f);
        t.hiss = hissRms * dbOrSilence (settings.noiseDb);
        t.mechanical = mechanicalRms * dbOrSilence (settings.mechanicalDb);
        t.output = juce::Decibels::decibelsToGain (settings.outputDb);
        t.toneMix = settings.toneEnabled ? 1.0f : 0.0f;
        return t;
    }

    /** One channel's wow, flutter and wear. */
    struct Modulator
    {
        void advance (const Targets& amount, double fs, float jitterDecay, float levelCoefficient, float& delaySeconds, float& am) noexcept
        {
            constexpr auto twoPi = juce::MathConstants<double>::twoPi;
            const auto step = rate / fs;

            wowPhase = std::fmod (wowPhase + wowHz * step, 1.0);
            auto delay = (double) amount.wow / (twoPi * wowHz) * (1.0 + std::sin (twoPi * (wowPhase + phaseOffset)));

            for (size_t i = 0; i < flutterLines; ++i)
            {
                flutterPhase[i] = std::fmod (flutterPhase[i] + flutterHz[i] * step, 1.0);
                const auto depth = (double) amount.flutter * flutterPeakPercent[i] * 0.01 / (twoPi * flutterHz[i]);
                delay += depth * (1.0 + std::sin (twoPi * (flutterPhase[i] + phaseOffset * (double) (i + 1))));
            }

            // Jitter rides on an offset of four standard deviations so the read point stays behind the write.
            jitter = jitterDecay * jitter + amount.jitter * noise.gaussian();
            const auto jitterSpread = amount.jitter / std::sqrt (1.0f - jitterDecay * jitterDecay);
            delay += 4.0f * jitterSpread + juce::jlimit (-4.0f * jitterSpread, 4.0f * jitterSpread, jitter);

            level += levelCoefficient * (noise.gaussian() - level);
            am = amount.am * level * std::sqrt ((2.0f - levelCoefficient) / levelCoefficient);
            delaySeconds = (float) delay;
        }

        double wowPhase = 0.0, phaseOffset = 0.0, rate = 1.0;
        std::array<double, flutterLines> flutterPhase {};
        float jitter = 0.0f, level = 0.0f;
        Noise noise;
    };

    //==============================================================================
    void saturate (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) buffer.getNumSamples());
        auto up = oversampling->processSamplesUp (block);

        const auto drive = juce::jlimit (0.0f, 1.0f, settings.drive);
        const auto inGain = juce::Decibels::decibelsToGain (settings.boost ? lookup (boostInDb, drive) : lookup (driveInDb, drive));
        const auto outGain = juce::Decibels::decibelsToGain (settings.boost ? lookup (boostOutDb, drive) : lookup (driveOutDb, drive));
        if (previousIn < 0.0f)
        {
            previousIn = inGain;
            previousOut = outGain;
        }

        const auto length = (int) up.getNumSamples();
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* x = up.getChannelPointer (ch);
            for (int i = 0; i < length; ++i)
            {
                const auto t = (float) (i + 1) / (float) length;
                const auto gIn = previousIn + t * (inGain - previousIn);
                const auto gOut = previousOut + t * (outGain - previousOut);
                x[i] = gOut * shape (gIn * x[i]);
            }
        }
        previousIn = inGain;
        previousOut = outGain;

        oversampling->processSamplesDown (block);
    }

    /** Two soft knees: a gentle u / (1 + k|u|), then a hard-ish limit at 1. */
    static float shape (float u) noexcept
    {
        constexpr auto knee = 2.957f;
        const auto h = u / (1.0f + 0.15298f * std::abs (u));
        return h / std::pow (1.0f + std::pow (std::abs (h), knee), 1.0f / knee);
    }

    //==============================================================================
    void updateFilters()
    {
        const auto tone = juce::jlimit (0.0f, 1.0f, settings.tone);
        if (std::abs (tone - shownTone) > 1.0e-4f)
        {
            shownTone = shownTone < -1.0f ? tone : shownTone + 0.25f * (tone - shownTone);
            if (std::abs (tone - shownTone) < 2.0e-3f)
                shownTone = tone;
            setTone (shownTone);
        }

        const auto filter = juce::jlimit (-1.0f, 1.0f, settings.filter);
        if (std::abs (filter - shownFilter) > 1.0e-4f)
        {
            shownFilter = shownFilter < -1.5f ? filter : shownFilter + 0.25f * (filter - shownFilter);
            if (std::abs (filter - shownFilter) < 2.0e-3f)
                shownFilter = filter;
            setFilter (shownFilter);
        }
    }

    void setTone (float tone)
    {
        const auto pos = tone * 4.0f;
        const auto i = juce::jmin ((int) pos, 3);
        const auto u = (double) (pos - (float) i);
        std::array<double, toneValues> p {};
        for (size_t k = 0; k < (size_t) toneValues; ++k)
        {
            const double a = toneKnots[(size_t) i][k], b = toneKnots[(size_t) i + 1][k];
            p[k] = toneLinear[k] ? a + u * (b - a) : std::exp (std::log (a) + u * (std::log (b) - std::log (a)));
        }

        toneGain = (float) std::pow (10.0, p[0] / 20.0);
        toneFilters[0].set (Biquad::highPass, sampleRate, p[1], p[2]);
        toneFilters[1].set (Biquad::highPass, sampleRate, p[3], p[4]);
        toneFilters[2].set (Biquad::peak, sampleRate, p[5], p[7], p[6]);
        toneFilters[3].set (Biquad::peak, sampleRate, p[8], p[10], p[9]);
        toneFilters[4].set (Biquad::peak, sampleRate, p[11], p[13], p[12]);
        toneFilters[5].set (Biquad::lowPass, sampleRate, p[14], p[15]);
        toneFilters[6].set (Biquad::lowPass, sampleRate, p[16], p[17]);
    }

    /** 63 Hz at full low-pass, 10 kHz at full high-pass, exponential in between. */
    void setFilter (float value)
    {
        const auto amount = (double) std::abs (value);
        filterActive = amount >= 0.01;
        if (value < 0.0f)
            lowHighPass.set (Biquad::lowPass, sampleRate, 63.0 * std::exp (5.82 * (1.0 - amount)), 0.47);
        else
            lowHighPass.set (Biquad::highPass, sampleRate, 66.0 * std::exp (6.28 * (amount - 0.2)), 0.47);
    }

    //==============================================================================
    enum class Transport { running, stopping, catchingUp, jumping };

    /** Moves the tape: lag is how far (samples) the read point has fallen behind real time, and
        restLag is where the Speed setting alone would have it (catching up stops there).
        Returns the level for this sample (a stopping tape fades as it slows). */
    float advanceTransport() noexcept
    {
        const auto target = juce::jlimit (0.25, 1.0, (double) settings.speed);
        const auto atSpeed = target >= 0.999;
        const auto catchUp = settings.fastForward ? Transport::catchingUp : Transport::jumping;

        if (settings.tapeStop)
        {
            transport = Transport::stopping;
            speed = juce::jmax (0.0, juce::jmin (speed, target) - target / (settings.stopSeconds * sampleRate));
        }
        else if (transport == Transport::stopping)
        {
            transport = catchUp;
        }
        else if (transport == Transport::running && atSpeed && lag > 0.5)
        {
            restLag = 0.0;
            transport = catchUp;
        }

        const auto fade = 1.0f / (0.005f * (float) sampleRate);
        switch (transport)
        {
            case Transport::running:
                jumpGain = juce::jmin (1.0f, jumpGain + fade);
                speed += speedGlide * (target - speed);
                break;
            case Transport::stopping:
                break;
            case Transport::catchingUp:
                speed = juce::jmin (4.0, speed + 4.0 / (0.3 * sampleRate));
                if (lag <= restLag)
                {
                    lag = restLag;
                    speed = target;
                    transport = Transport::running;
                }
                break;
            case Transport::jumping:
                jumpGain -= fade;
                if (jumpGain <= 0.0f)
                {
                    jumpGain = 0.0f;
                    lag = restLag;
                    speed = target;
                    transport = Transport::running;
                }
                break;
        }

        // At the end of the tape the read point can fall no further behind: it plays at normal speed.
        lag = juce::jlimit (0.0, maxLag(), lag + 1.0 - speed);
        if (transport == Transport::running)
            restLag = lag;
        else
            restLag = atSpeed ? 0.0 : juce::jmin (maxLag(), restLag + 1.0 - target);

        return jumpGain * (transport == Transport::running ? 1.0f : (float) juce::jmin (1.0, 3.0 * speed));
    }

    float readHermite (const std::vector<float>& line, double delay) const noexcept
    {
        const auto size = tape->size;
        auto pos = (double) writeIndex - delay;
        if (pos < 0.0)
            pos += size;
        const auto floorPos = std::floor (pos);
        const auto i = (int) floorPos;
        const auto f = (float) (pos - floorPos);
        const auto at = [&] (int k) { return line[(size_t) (k < 0 ? k + size : (k >= size ? k - size : k))]; };
        const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** One delay line per channel, in a ring of `size` samples. */
    struct Tape
    {
        explicit Tape (int length) : size (length)
        {
            for (auto& line : lines)
                line.assign ((size_t) length, 0.0f);
        }

        std::array<std::vector<float>, maxChannels> lines;
        int size;
    };

    static constexpr double shortTapeSeconds = 4.0, longTapeSeconds = 300.0;

    /** Room for `seconds` of lag plus the wow and flutter swing on top. */
    int lengthFor (double seconds) const { return (int) ((seconds + 0.1) * sampleRate) + 16; }

    double maxLag() const noexcept { return (double) tape->size - 0.1 * sampleRate - 16.0; }

    /** Swaps in the long tape with everything on the short one, oldest first, so the read point
        keeps its place. The short tape is kept (not freed) to stay off the allocator. */
    void adopt (Tape* longTape) noexcept
    {
        const auto oldSize = tape->size;
        for (size_t c = 0; c < maxChannels; ++c)
            for (int i = 0; i < oldSize; ++i)
                longTape->lines[c][(size_t) i] = tape->lines[c][(size_t) ((writeIndex + i) % oldSize)];
        writeIndex = oldSize;
        spareTape = std::move (tape);
        tape.reset (longTape);
    }

    //==============================================================================
    /** About ten seconds of motor noise with the measured spectrum (random phases), looped.
        The right channel reads half a loop away. */
    void makeMechanicalLoop()
    {
        const auto order = sampleRate <= 50000.0 ? 19 : (sampleRate <= 100000.0 ? 20 : 21);
        const auto size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);

        juce::Random random (23);
        for (int k = 1; k < size / 2; ++k)
        {
            const auto hz = (float) (k * sampleRate / size);
            const auto band = 3.0f * std::log2 (hz / mechanicalFirstBandHz);
            auto levelDb = lookup (mechanicalBandsDb, band / (float) (mechanicalBandsDb.size() - 1));
            if (band < 0.0f)
                levelDb -= 8.0f * -band;   // 24 dB/octave below the first band
            // Band level spread over a third of an octave: power density falls as 1/f.
            const auto amplitude = std::sqrt (std::pow (10.0f, levelDb / 10.0f) / hz);
            const auto phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
            data[(size_t) (2 * k)] = amplitude * std::cos (phase);
            data[(size_t) (2 * k + 1)] = amplitude * std::sin (phase);
        }
        fft.performRealOnlyInverseTransform (data.data());

        mechanicalLoop.assign (data.begin(), data.begin() + size);
        auto sumSquares = 0.0;
        for (auto v : mechanicalLoop)
            sumSquares += (double) v * v;
        const auto scale = (float) (1.0 / std::sqrt (sumSquares / size + 1.0e-30));
        for (auto& v : mechanicalLoop)
            v *= scale;

        mechanicalIndex = 0;
        motorPhase = {};
    }

    /** Motor rhythms: 23 Hz (the strongest) and slower 7.36 and 2.39 Hz. */
    std::array<float, maxChannels> nextMechanical (float gain) noexcept
    {
        if (gain <= 0.0f)
            return {};

        constexpr std::array<double, 3> motorHz { 23.0, 7.36, 2.39 };
        constexpr std::array<float, 3> motorDepth { 0.13f, 0.03f, 0.025f };
        auto level = 1.0f;
        for (size_t i = 0; i < motorHz.size(); ++i)
        {
            motorPhase[i] = std::fmod (motorPhase[i] + motorHz[i] / sampleRate, 1.0);
            level += motorDepth[i] * (float) std::sin (juce::MathConstants<double>::twoPi * motorPhase[i]);
        }

        const auto size = (int) mechanicalLoop.size();
        const auto left = mechanicalLoop[(size_t) mechanicalIndex];
        const auto right = mechanicalLoop[(size_t) ((mechanicalIndex + size / 2) % size)];
        mechanicalIndex = (mechanicalIndex + 1) % size;
        return { gain * level * left, gain * level * right };
    }

    //==============================================================================
    Settings settings;
    double sampleRate = 48000.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    float previousIn = -1.0f, previousOut = -1.0f;

    std::array<Biquad, 2> base;
    std::array<Biquad, 7> toneFilters;
    float toneGain = 1.0f, shownTone = -2.0f;
    Biquad lowHighPass;
    bool filterActive = false;
    float shownFilter = -2.0f;

    Targets smoothed;
    float smoothing = 0.0f, fastSmoothing = 0.0f, jitterPole = 0.0f, amPole = 0.0f;
    std::array<Modulator, maxChannels> modulators;
    Noise hissNoise { 0x1234567u };
    Biquad hissShelf, hissLowPass;

    std::unique_ptr<Tape> tape, spareTape;
    std::atomic<Tape*> offeredTape { nullptr };
    bool longTapeRequested = false;
    int writeIndex = 0;
    Transport transport = Transport::running;
    double speed = 1.0, lag = 0.0, restLag = 0.0, speedGlide = 0.0;
    float jumpGain = 1.0f;

    std::vector<float> mechanicalLoop;
    int mechanicalIndex = 0;
    std::array<double, 3> motorPhase {};
};
