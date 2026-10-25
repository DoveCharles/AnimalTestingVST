#include "CabModel.h"

namespace cab
{
namespace
{
    using Complex = std::complex<float>;

    struct Peak { float hz, db, q; };

    struct Speaker
    {
        bool openBack;
        float resonanceHz, resonanceQ;   // box + speaker low resonance
        float rolloffHz;                 // where the cone stops radiating highs
        int rolloffOrder;
        std::vector<Peak> breakup;       // cone breakup and box colouration
    };

    // Voiced by ear and against published speaker curves, then checked against close-mic'd captures
    // of real cabs for the broad shape (low end, low mids, the 1 kHz region, the top-end slope).
    // None of it is copied from one cab.
    const std::array<Speaker, 6> speakers
    {{
        // Closed 4x12 with old 25 W ceramic speakers: thick low mids, a dip around 1 kHz, a ragged
        // upper-mid hump.
        { false, 105.0f, 1.3f, 5000.0f, 4, { { 250, 2.5f, 0.6f }, { 1100, -2.5f, 1.5f }, { 2200, 3.0f, 2.2f },
                                             { 2900, -4.0f, 3.0f }, { 3900, 3.0f, 3.0f } } },
        // Closed 4x12 with modern 70 W speakers: tight lows, scooped 1 kHz, an aggressive 2.6 kHz peak.
        { false, 100.0f, 1.3f, 5600.0f, 5, { { 250, 2.0f, 0.8f }, { 900, -1.5f, 2.5f }, { 1600, 1.0f, 2.5f },
                                             { 2600, 2.5f, 1.8f }, { 3500, -2.0f, 5.0f }, { 4600, 2.5f, 3.5f } } },
        // Open-back 2x12 with alnico speakers: loose lows, full mids, chimey 3 kHz.
        { true,  95.0f,  1.1f, 5200.0f, 4, { { 400, 2.5f, 0.7f }, { 800, -1.0f, 1.5f }, { 1700, 1.0f, 2.0f },
                                             { 3000, 3.0f, 1.6f } } },
        // Open-back 1x12 combo: punchy low mids, a bright but narrow top.
        { true,  100.0f, 1.0f, 4800.0f, 4, { { 600, 2.5f, 1.0f }, { 1800, 1.5f, 2.0f },
                                             { 3500, 1.5f, 2.0f }, { 4300, -2.0f, 4.0f } } },
        // Open-back 4x10: deep but soft lows, rounded top.
        { true,  82.0f,  0.9f, 4300.0f, 4, { { 180, 1.5f, 1.2f }, { 1200, 2.0f, 1.6f }, { 2800, 3.0f, 1.8f },
                                             { 3800, -2.5f, 4.0f } } },
        // Closed 1x8 practice amp: no lows, honky mids, early roll-off.
        { false, 160.0f, 2.5f, 3900.0f, 3, { { 700, 4.0f, 1.8f }, { 1500, -2.0f, 2.5f }, { 2500, 3.0f, 2.0f } } },
    }};

    float bell (float hz, Peak p)
    {
        const auto x = p.q * (hz / p.hz - p.hz / hz);
        return p.db / (1.0f + x * x);
    }

    float lowPassDb (float hz, float cutoff, int order)
    {
        return -10.0f * std::log10 (1.0f + std::pow (hz / cutoff, 2.0f * (float) order));
    }

    float highPass1Db (float hz, float cutoff)
    {
        const auto r = (hz / cutoff) * (hz / cutoff);
        return 10.0f * std::log10 (r / (1.0f + r));
    }

    float highPass2Db (float hz, float cutoff, float q)
    {
        const auto w2 = (hz / cutoff) * (hz / cutoff);
        return 10.0f * std::log10 (w2 * w2 / ((1.0f - w2) * (1.0f - w2) + w2 / (q * q)));
    }

    float speakerDb (const Speaker& s, float hz, float distance)
    {
        // Above the roll-off the cone still fizzes a little; that floor keeps the very top from going dead.
        const auto rolloff = lowPassDb (hz, s.rolloffHz, s.rolloffOrder);
        const auto withFizz = 10.0f * std::log10 (std::pow (10.0f, rolloff / 10.0f) + 0.0025f);
        auto db = highPass2Db (hz, s.resonanceHz, s.resonanceQ) + withFizz;

        // An open back radiates from both sides of the cone, and the two cancel in the lows.
        if (s.openBack)
            db += highPass1Db (hz, 0.8f * s.resonanceHz);

        // Further back, the other speakers and the cab walls blur the individual breakup peaks.
        const auto sharpness = 1.0f - 0.45f * distance;
        for (const auto& p : s.breakup)
            db += sharpness * bell (hz, p);

        return db;
    }

    float positionDb (float hz, float position)
    {
        // Dust cap: bright and fizzy. Edge: the highs fall away and the body fills in.
        const auto capFizz = 3.5f * (1.0f - position) * (1.0f - position);
        const auto edgeCutoff = 9000.0f * std::pow (2300.0f / 9000.0f, position);
        const auto notchHz = 6500.0f * std::pow (0.45f, position);
        const auto notchDb = -4.0f * std::sin (juce::MathConstants<float>::pi * position);

        return bell (hz, { 3800, capFizz, 1.6f })
             + lowPassDb (hz, edgeCutoff, 2)
             + bell (hz, { 250, 1.8f * position, 0.8f })
             + bell (hz, { notchHz, notchDb, 3.0f });
    }

    float micDb (float hz, int mic)
    {
        switch (mic)
        {
            case ribbon:    return lowPassDb (hz, 10000.0f, 1) + lowPassDb (hz, 16000.0f, 2) + bell (hz, { 3000, -1.5f, 0.7f });
            case condenser: return bell (hz, { 170, -2.5f, 1.0f }) + lowPassDb (hz, 12000.0f, 1);
            default:        return bell (hz, { 5000, 4.5f, 0.9f }) + lowPassDb (hz, 14000.0f, 2) + highPass1Db (hz, 60.0f);
        }
    }

    /** How much bass a mic gains right up against the grille (cardioid = 1, figure-8 more, omni-ish less). */
    float proximity (int mic)
    {
        switch (mic)
        {
            case ribbon:    return 0.7f;
            case condenser: return 0.25f;
            default:        return 1.0f;
        }
    }

    float distanceDb (float hz, float distance, float proximityAmount)
    {
        const auto closeness = std::pow (1.0f - distance, 1.5f);
        const auto proximityBoost = proximityAmount * 7.0f * closeness / (1.0f + (hz / 180.0f) * (hz / 180.0f));
        const auto air = -2.0f * distance * juce::jmin (1.0f, (hz / 12000.0f) * (hz / 12000.0f));
        return proximityBoost + air;
    }

    float builtInDb (const Settings& s, float hz)
    {
        const auto& speaker = speakers[(size_t) juce::jlimit (0, (int) speakers.size() - 1, s.model)];
        return speakerDb (speaker, hz, s.distance) + positionDb (hz, s.position) + micDb (hz, s.mic)
             + distanceDb (hz, s.distance, proximity (s.mic));
    }

    //==============================================================================
    int fftOrderFor (size_t size)
    {
        return juce::jmax (1, (int) std::ceil (std::log2 ((double) juce::jmax ((size_t) 2, size))));
    }

    /** Minimum-phase IR with the given magnitude (dB as a function of Hz), via the real cepstrum. */
    std::vector<float> minimumPhase (const std::function<float (float)>& magnitudeDb, double sampleRate, int length)
    {
        // A long grid keeps the cepstrum from aliasing on the sharp resonances.
        const auto order = juce::jmax (13, fftOrderFor ((size_t) (sampleRate * 0.35)));
        const auto n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<Complex> a ((size_t) n), b ((size_t) n);

        constexpr float dbToNeper = 0.11512925f;   // ln (10) / 20
        for (int k = 0; k <= n / 2; ++k)
        {
            const auto hz = juce::jmax (5.0f, (float) (k * sampleRate / n));
            const auto logMagnitude = juce::jmax (-140.0f, magnitudeDb (hz)) * dbToNeper;
            a[(size_t) k] = logMagnitude;
            a[(size_t) ((n - k) % n)] = logMagnitude;
        }

        fft.perform (a.data(), b.data(), true);

        // Fold the cepstrum onto positive time: that makes the phase minimum.
        for (int i = 1; i < n / 2; ++i)
            b[(size_t) i] = 2.0f * b[(size_t) i].real();
        b[0] = b[0].real();
        b[(size_t) n / 2] = b[(size_t) n / 2].real();
        std::fill (b.begin() + n / 2 + 1, b.end(), Complex {});

        fft.perform (b.data(), a.data(), false);
        for (auto& c : a)
            c = std::exp (c);
        fft.perform (a.data(), b.data(), true);

        std::vector<float> ir ((size_t) length);
        for (int i = 0; i < length; ++i)
            ir[(size_t) i] = b[(size_t) i].real();

        // Fade the last 30 % out, so truncating the low resonance doesn't click.
        const auto fadeStart = (int) (length * 0.7f);
        for (int i = fadeStart; i < length; ++i)
        {
            const auto t = (float) (i - fadeStart) / (float) (length - fadeStart);
            ir[(size_t) i] *= 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * t));
        }

        return ir;
    }

    std::vector<float> convolve (const std::vector<float>& x, const std::vector<float>& h)
    {
        if (x.empty() || h.empty())
            return {};

        const auto outLength = x.size() + h.size() - 1;
        const auto order = fftOrderFor (outLength);
        const auto n = (size_t) 1 << order;
        juce::dsp::FFT fft (order);

        std::vector<Complex> xs (n), hs (n), out (n);
        std::copy (x.begin(), x.end(), xs.begin());
        std::copy (h.begin(), h.end(), hs.begin());
        fft.perform (xs.data(), out.data(), false);
        fft.perform (hs.data(), xs.data(), false);
        for (size_t i = 0; i < n; ++i)
            out[i] *= xs[i];
        fft.perform (out.data(), hs.data(), true);

        std::vector<float> y (outLength);
        for (size_t i = 0; i < outLength; ++i)
            y[i] = hs[i].real();
        return y;
    }

    std::vector<float> onePoleLowPass (std::vector<float> x, double sampleRate, float cutoff)
    {
        const auto coefficient = (float) std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate);
        auto state = 0.0f;
        for (auto& sample : x)
            sample = state = sample + coefficient * (state - sample);
        return x;
    }

    /** Adds a copy of the IR reflected off the floor between the cab and the mic. */
    void addFloorBounce (std::vector<float>& ir, double sampleRate, float distance)
    {
        constexpr float height = 0.4f;   // speaker and mic both 40 cm up
        const auto direct = 0.03f + 1.2f * distance;
        const auto reflected = std::sqrt (direct * direct + 4.0f * height * height);
        const auto delay = (float) ((reflected - direct) / 343.0f * sampleRate);
        const auto gain = 0.5f * direct / reflected;

        const auto bounce = onePoleLowPass (ir, sampleRate, 5000.0f);
        const auto whole = (int) delay;
        const auto frac = delay - (float) whole;

        for (int i = (int) ir.size() - 1; i > whole; --i)
        {
            const auto a = bounce[(size_t) (i - whole)];
            const auto b = bounce[(size_t) (i - whole - 1)];
            ir[(size_t) i] += gain * (a + frac * (b - a));
        }
    }

    /** Early reflections and a short diffuse tail around the IR. Each channel gets its own seed. */
    std::vector<float> addRoom (const std::vector<float>& ir, double sampleRate, float amount, juce::int64 seed)
    {
        if (amount <= 0.0f)
            return ir;

        juce::Random random (seed);
        constexpr float rt60 = 0.3f;
        const auto tailLength = (size_t) (sampleRate * rt60 * 1.1);
        std::vector<float> out (ir.size() + tailLength);
        std::copy (ir.begin(), ir.end(), out.begin());

        // Early reflections: walls close by, getting darker as they arrive later.
        constexpr int numReflections = 12;
        std::array<float, numReflections> times {}, gains {};
        auto energy = 0.0f;
        for (int i = 0; i < numReflections; ++i)
        {
            times[(size_t) i] = 0.002f + 0.033f * ((float) i + random.nextFloat()) / (float) numReflections;
            gains[(size_t) i] = std::exp (-times[(size_t) i] / 0.015f) * (random.nextFloat() < 0.3f ? -0.7f : 1.0f);
            energy += gains[(size_t) i] * gains[(size_t) i];
        }

        const auto brightReflections = onePoleLowPass (ir, sampleRate, 4500.0f);
        const auto darkReflections = onePoleLowPass (ir, sampleRate, 2500.0f);
        const auto reflectionScale = 0.75f * amount / std::sqrt (energy);

        for (int i = 0; i < numReflections; ++i)
        {
            const auto& source = i < numReflections / 2 ? brightReflections : darkReflections;
            const auto offset = (size_t) (times[(size_t) i] * sampleRate);
            const auto gain = gains[(size_t) i] * reflectionScale;
            for (size_t j = 0; j < source.size() && offset + j < out.size(); ++j)
                out[offset + j] += gain * source[j];
        }

        // Diffuse tail: decaying noise, faded in once the reflections thin out.
        std::vector<float> tail (tailLength);
        auto tailEnergy = 0.0f;
        for (size_t i = 0; i < tailLength; ++i)
        {
            const auto t = (float) ((double) i / sampleRate);
            const auto fadeIn = juce::jlimit (0.0f, 1.0f, (t - 0.008f) / 0.015f);
            tail[i] = (random.nextFloat() * 2.0f - 1.0f) * fadeIn * std::exp (-6.91f * t / rt60);
        }
        tail = onePoleLowPass (std::move (tail), sampleRate, 3500.0f);
        for (auto s : tail)
            tailEnergy += s * s;

        const auto tailScale = 0.6f * amount / std::sqrt (juce::jmax (1.0e-9f, tailEnergy));
        const auto wetTail = convolve (tail, ir);
        for (size_t i = 0; i < wetTail.size() && i < out.size(); ++i)
            out[i] += tailScale * wetTail[i];

        return out;
    }

    /** Scales the channels so the 150 Hz - 4 kHz band sits at unity gain (log-frequency weighted). */
    void levelMatch (std::vector<std::vector<float>>& channels, double sampleRate)
    {
        const auto order = juce::jmax (12, fftOrderFor (channels[0].size()));
        const auto n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) n * 2);

        auto weightedPower = 0.0, totalWeight = 0.0;
        for (const auto& channel : channels)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            std::copy_n (channel.begin(), juce::jmin ((size_t) n, channel.size()), data.begin());
            fft.performFrequencyOnlyForwardTransform (data.data(), true);

            for (int k = 1; k < n / 2; ++k)
            {
                const auto hz = k * sampleRate / n;
                if (hz < 150.0 || hz > 4000.0)
                    continue;
                weightedPower += (double) data[(size_t) k] * data[(size_t) k] / hz;
                totalWeight += 1.0 / hz;
            }
        }

        if (weightedPower <= 0.0)
            return;

        const auto scale = (float) std::sqrt (totalWeight / weightedPower);
        for (auto& channel : channels)
            for (auto& s : channel)
                s *= scale;
    }

    std::vector<float> resample (const float* source, int length, double fromRate, double toRate)
    {
        if (std::abs (fromRate - toRate) < 0.5)
            return { source, source + length };

        const auto ratio = fromRate / toRate;
        const auto outLength = (int) std::ceil (length / ratio);

        // The interpolator reads a little past the end, so give it some silence to read.
        std::vector<float> padded ((size_t) length + 64);
        std::copy_n (source, length, padded.begin());

        std::vector<float> out ((size_t) outLength);
        juce::WindowedSincInterpolator interpolator;
        interpolator.process (ratio, padded.data(), out.data(), outLength);
        return out;
    }
}

//==============================================================================
float responseDb (const Settings& s, float hz)
{
    return s.model == userIr ? 0.0f : builtInDb (s, hz);
}

juce::AudioBuffer<float> buildImpulse (const Settings& settings, double sampleRate, const UserImpulse* userImpulse)
{
    const auto useFile = settings.model == userIr && userImpulse != nullptr && userImpulse->samples.getNumSamples() > 0;

    std::vector<std::vector<float>> channels;
    if (useFile)
    {
        // The file already has its speaker, mic and placement; only move it back from where it was recorded.
        const auto distanceFilter = minimumPhase ([&] (float hz)
        {
            return distanceDb (hz, settings.distance, 1.0f) - distanceDb (hz, 0.0f, 1.0f);
        }, sampleRate, (int) (sampleRate * 0.02));

        for (int ch = 0; ch < juce::jmin (2, userImpulse->samples.getNumChannels()); ++ch)
        {
            auto ir = resample (userImpulse->samples.getReadPointer (ch), userImpulse->samples.getNumSamples(),
                                userImpulse->sampleRate, sampleRate);
            ir = convolve (ir, distanceFilter);
            ir.resize (ir.size() + (size_t) (sampleRate * 0.005));   // room for the floor bounce
            channels.push_back (std::move (ir));
        }
    }
    else
    {
        auto builtIn = settings;
        if (builtIn.model == userIr)
            builtIn.model = vintage412;

        channels.push_back (minimumPhase ([&] (float hz) { return builtInDb (builtIn, hz); },
                                          sampleRate, (int) (sampleRate * 0.06)));
    }

    for (auto& ir : channels)
        addFloorBounce (ir, sampleRate, settings.distance);

    levelMatch (channels, sampleRate);

    // The room spreads a mono IR into stereo; nearer the back of the room there's more of it.
    if (channels.size() == 1)
        channels.push_back (channels[0]);

    const auto roomAmount = settings.room * (0.6f + 0.4f * settings.distance);
    for (size_t ch = 0; ch < channels.size(); ++ch)
        channels[ch] = addRoom (channels[ch], sampleRate, roomAmount, 0x5eed + (juce::int64) ch * 977);

    const auto length = juce::jmax (channels[0].size(), channels[1].size());
    juce::AudioBuffer<float> result (2, (int) length);
    result.clear();
    for (int ch = 0; ch < 2; ++ch)
        result.copyFrom (ch, 0, channels[(size_t) ch].data(), (int) channels[(size_t) ch].size());

    return result;
}

std::optional<UserImpulse> readImpulseFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return std::nullopt;

    const auto numChannels = (int) juce::jmin (2u, reader->numChannels);
    const auto length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));

    UserImpulse impulse;
    impulse.samples.setSize (numChannels, length);
    reader->read (&impulse.samples, 0, length, 0, true, numChannels > 1);
    impulse.sampleRate = reader->sampleRate;
    impulse.name = file.getFileNameWithoutExtension();

    // Drop leading silence, so a capture with pre-roll doesn't add delay.
    const auto peak = impulse.samples.getMagnitude (0, length);
    if (peak <= 0.0f)
        return std::nullopt;

    auto start = length;
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < start; ++i)
            if (std::abs (impulse.samples.getSample (ch, i)) > peak * 0.001f)
            {
                start = i;
                break;
            }

    start = juce::jmax (0, start - 8);
    if (start > 0)
    {
        juce::AudioBuffer<float> trimmed (numChannels, length - start);
        for (int ch = 0; ch < numChannels; ++ch)
            trimmed.copyFrom (ch, 0, impulse.samples, ch, start, length - start);
        impulse.samples = std::move (trimmed);
    }

    return impulse;
}
}
