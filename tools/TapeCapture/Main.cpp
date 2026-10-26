// Measures a tape plugin from the outside, without a DAW: loads the installed VST3 or AU, plays
// test signals through it and prints what comes out. Used to take the numbers off Arturia's
// Tape MELLO-FI so our own model can be matched to them.
//
// Parameters are set by name (case and spaces ignored) with normalised values 0..1, for example
// --Drive=0.5 --"Stereo Width"=1. Every command prints the settings it ran with as the plugin
// shows them. Anything not given stays at the plugin's default.
//
//   TapeCapture params
//       Every real parameter with its default, and the text it shows at 0, .25, .5, .75 and 1.
//
//   TapeCapture levels [--freq=1000] [--<param>=<value> ...]
//       A sine stepped from -48 to +12 dBFS: output level, gain, harmonics 2-5 and THD.
//
//   TapeCapture response [--level=-24] [--<param>=<value> ...]
//       Stepped sines on third-octave centres: level relative to 1 kHz (robust to wow).
//
//   TapeCapture wow [--seconds=30] [--csv=<file>] [--<param>=<value> ...]
//       A 3150 Hz tone, demodulated: pitch offset, wow and flutter RMS and peak, the strongest
//       modulation rates, and the level wobble. --csv writes the pitch trace (4 kHz rate).
//
//   TapeCapture noise [--seconds=10] [--<param>=<value> ...]
//       Silence in: noise level and third-octave spectrum, clicks, and any rhythm in the noise.
//
//   TapeCapture stereo [--<param>=<value> ...]
//       Mono noise in: L/R correlation overall and per octave, side/mid level.
//
//   TapeCapture sweep <param> [--steps=11] [--level=-18] [--<param>=<value> ...]
//       Steps one parameter from 0 to 1; each row has gain, THD, H2/H3, response points, wow and
//       flutter RMS and noise level.
//
//   TapeCapture render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the plugin, for listening A/B against our model.
//
//   TapeCapture selftest
//       Runs the analysis on signals with known answers (no plugin).
//
// The plugin defaults to the installed Tape MELLO-FI VST3; --plugin=<path> picks another.

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <iostream>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr auto defaultPlugin = "/Library/Audio/Plug-Ins/VST3/Tape MELLO-FI.vst3";

    // Host-side parameters every Arturia plugin exposes; none of them touch the sound.
    bool isHousekeeping (const juce::String& name)
    {
        for (auto* prefix : { "MIDI CC helper", "VST_ProgramChange", "MPE_", "Hardware", "Select ", "Navigate ",
                              "Add/Remove", "Clear Filters", "Active Filter", "Browse Back", "SendValues",
                              "Previous Preset", "Next Preset", "HW Controller", "Oct ", "Saves the", "Undo the", "Redo the" })
            if (name.startsWith (prefix))
                return true;
        return false;
    }

    juce::String squash (const juce::String& s) { return s.removeCharacters (" _-").toLowerCase(); }

    juce::String db (double gain) { return juce::String (juce::Decibels::gainToDecibels (gain, -200.0), 1); }

    juce::String column (const juce::String& s, int width) { return s.paddedLeft (' ', width); }

    //==============================================================================
    struct Host
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        juce::Array<juce::AudioProcessorParameter*> params;

        bool load (const juce::ArgumentList& args)
        {
            const auto path = args.containsOption ("--plugin") ? args.getValueForOption ("--plugin") : juce::String (defaultPlugin);

            juce::AudioPluginFormatManager formats;
            juce::addDefaultFormatsToManager (formats);

            juce::OwnedArray<juce::PluginDescription> found;
            for (auto* format : formats.getFormats())
                if (format->fileMightContainThisPluginType (path))
                    format->findAllTypesForFile (found, path);

            if (found.isEmpty())
            {
                std::cerr << "No plugin found at " << path << "\n";
                return false;
            }

            juce::String error;
            plugin = formats.createPluginInstance (*found[0], sampleRate, blockSize, error);
            if (plugin == nullptr)
            {
                std::cerr << "Could not load " << path << ": " << error << "\n";
                return false;
            }

            plugin->enableAllBuses();
            plugin->prepareToPlay (sampleRate, blockSize);

            // The wrapper reports 0 for every parameter until the host sets it, so set the defaults
            // ourselves; then what we print is what is playing.
            for (auto* p : plugin->getParameters())
                if (! isHousekeeping (p->getName (128)))
                {
                    params.add (p);
                    set (*p, p->getDefaultValue());
                }

            return applyParameters (args);
        }

        juce::AudioProcessorParameter* find (const juce::String& name) const
        {
            for (auto* p : params)
                if (squash (p->getName (128)) == squash (name))
                    return p;
            return nullptr;
        }

        bool applyParameters (const juce::ArgumentList& args)
        {
            static const juce::StringArray ours { "plugin", "freq", "level", "seconds", "csv", "steps" };

            for (auto& arg : args.arguments)
            {
                if (! arg.isLongOption())
                    continue;

                const auto name = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
                if (ours.contains (name))
                    continue;

                auto* p = find (name);
                if (p == nullptr)
                {
                    std::cerr << "Unknown parameter: " << name << " (see TapeCapture params)\n";
                    return false;
                }
                set (*p, arg.text.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            }
            return true;
        }

        void set (juce::AudioProcessorParameter& p, float value)
        {
            p.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
        }

        void printSettings() const
        {
            juce::StringArray parts;
            for (auto* p : params)
                parts.add (p->getName (128) + " " + p->getCurrentValueAsText());
            std::cout << "Settings: " << parts.joinIntoString (", ") << "\n\n";
        }

        // Plays mono or stereo audio through the plugin and returns stereo, latency removed.
        juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input)
        {
            const auto latency = plugin->getLatencySamples();
            const auto length = input.getNumSamples();
            const auto channels = juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels());

            juce::AudioBuffer<float> output (2, length);
            juce::AudioBuffer<float> block (channels, blockSize);
            juce::MidiBuffer midi;

            for (int start = 0; start < length + latency; start += blockSize)
            {
                const auto n = blockSize;
                block.clear();
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto source = juce::jmin (ch, input.getNumChannels() - 1);
                    const auto available = juce::jlimit (0, n, length - start);
                    if (available > 0)
                        block.copyFrom (ch, 0, input, source, start, available);
                }

                plugin->processBlock (block, midi);
                midi.clear();

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        if (const auto at = start + i - latency; at >= 0 && at < length)
                            output.setSample (ch, at, block.getSample (ch, i));
            }
            return output;
        }
    };

    //==============================================================================
    // Signals

    juce::AudioBuffer<float> sine (double freq, double gainDb, double seconds)
    {
        const auto amplitude = juce::Decibels::decibelsToGain (gainDb);
        juce::AudioBuffer<float> buffer (1, (int) (seconds * sampleRate));
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            buffer.setSample (0, n, (float) (amplitude * std::sin (juce::MathConstants<double>::twoPi * freq * n / sampleRate)));
        return buffer;
    }

    juce::AudioBuffer<float> whiteNoise (double gainDb, double seconds)
    {
        // Uniform noise has RMS = peak / sqrt(3); scale so the RMS lands on gainDb.
        const auto peak = juce::Decibels::decibelsToGain (gainDb) * std::sqrt (3.0);
        juce::Random random (1234);
        juce::AudioBuffer<float> buffer (1, (int) (seconds * sampleRate));
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            buffer.setSample (0, n, (float) (peak * (2.0 * random.nextDouble() - 1.0)));
        return buffer;
    }

    juce::AudioBuffer<float> silence (double seconds)
    {
        juce::AudioBuffer<float> buffer (1, (int) (seconds * sampleRate));
        buffer.clear();
        return buffer;
    }

    //==============================================================================
    // Analysis

    using Spectrum = std::vector<std::complex<float>>;

    // Hann-windowed FFT of n samples from x, zero-padded to 2^order.
    Spectrum fft (const float* x, int n, int order)
    {
        const auto size = 1 << order;
        n = juce::jmin (n, size);
        Spectrum data ((size_t) size * 2);
        for (int i = 0; i < n; ++i)
        {
            const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1));
            data[(size_t) i] = x[i] * w;
        }
        juce::dsp::FFT transform (order);
        Spectrum out ((size_t) size);
        transform.perform (data.data(), out.data(), false);
        return out;
    }

    // Power within a band around freq (tolerant of wow smearing the line).
    double bandPower (const Spectrum& s, double freq, double halfWidth)
    {
        const auto size = (double) s.size();
        const auto lo = juce::jmax (1, (int) std::floor ((freq - halfWidth) * size / sampleRate));
        const auto hi = juce::jmin ((int) s.size() / 2 - 1, (int) std::ceil ((freq + halfWidth) * size / sampleRate));
        double sum = 0.0;
        for (int k = lo; k <= hi; ++k)
            sum += std::norm (s[(size_t) k]);
        return sum;
    }

    double halfWidthFor (double freq, double size) { return freq * 0.02 + 4.0 * sampleRate / size; }

    // Amplitude of a sinusoid at freq in x, calibrated against a unit sine analysed the same way.
    double toneAmplitude (const float* x, int n, double freq, int order = 15)
    {
        const auto halfWidth = halfWidthFor (freq, (double) (1 << order));
        const auto reference = sine (freq, 0.0, (double) n / sampleRate);
        const auto refPower = bandPower (fft (reference.getReadPointer (0), n, order), freq, halfWidth);
        return std::sqrt (bandPower (fft (x, n, order), freq, halfWidth) / refPower);
    }

    double rms (const float* x, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / juce::jmax (1, n));
    }

    double mid (const juce::AudioBuffer<float>& b, int i) { return 0.5 * (b.getSample (0, i) + b.getSample (1, i)); }

    juce::AudioBuffer<float> monoMix (const juce::AudioBuffer<float>& b)
    {
        juce::AudioBuffer<float> m (1, b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i)
            m.setSample (0, i, (float) mid (b, i));
        return m;
    }

    struct Harmonics
    {
        double fundamental = 0.0;            // amplitude
        std::array<double, 4> relative {};   // H2..H5 relative to the fundamental
        double thd = 0.0;                    // ratio, H2..H9
    };

    Harmonics harmonics (const float* x, int n, double freq)
    {
        constexpr int order = 15;
        const auto s = fft (x, n, order);
        const auto size = (double) (1 << order);
        const auto f1 = bandPower (s, freq, halfWidthFor (freq, size));

        Harmonics h;
        h.fundamental = toneAmplitude (x, n, freq, order);
        double sum = 0.0;
        for (int k = 2; k <= 9 && k * freq < sampleRate * 0.45; ++k)
        {
            const auto p = bandPower (s, k * freq, halfWidthFor (k * freq, size)) / f1;
            if (k <= 5)
                h.relative[(size_t) k - 2] = std::sqrt (p);
            sum += p;
        }
        h.thd = std::sqrt (sum);
        return h;
    }

    // Instantaneous frequency of a tone near f0, at 4 kHz, plus its amplitude envelope. The
    // baseband filter is wide (0.35 f0) so even ±15 % swings at full Wow get through.
    struct Demodulated
    {
        std::vector<double> deviationHz, amplitude;
        static constexpr double rate = 4000.0;
    };

    Demodulated demodulate (const float* x, int n, double f0)
    {
        constexpr int decimation = (int) (sampleRate / Demodulated::rate);
        using Filter = juce::dsp::IIR::Filter<double>;
        auto coeffs = juce::dsp::IIR::Coefficients<double>::makeLowPass (sampleRate, juce::jmin (0.35 * f0, 0.4 * Demodulated::rate));
        Filter re[3], im[3];
        for (int stage = 0; stage < 3; ++stage)
        {
            re[stage].coefficients = coeffs;
            im[stage].coefficients = coeffs;
        }

        Demodulated d;
        double lastPhase = 0.0, unwrapped = 0.0, lastUnwrapped = 0.0;
        bool first = true;

        for (int i = 0; i < n; ++i)
        {
            const auto angle = juce::MathConstants<double>::twoPi * f0 * i / sampleRate;
            auto a = (double) x[i] * std::cos (angle);
            auto b = -(double) x[i] * std::sin (angle);
            for (int stage = 0; stage < 3; ++stage)
            {
                a = re[stage].processSample (a);
                b = im[stage].processSample (b);
            }

            if (i % decimation != 0)
                continue;

            const auto phase = std::atan2 (b, a);
            if (first)
            {
                unwrapped = lastUnwrapped = phase;
                first = false;
            }
            else
            {
                auto step = phase - lastPhase;
                while (step > juce::MathConstants<double>::pi)   step -= juce::MathConstants<double>::twoPi;
                while (step < -juce::MathConstants<double>::pi)  step += juce::MathConstants<double>::twoPi;
                unwrapped += step;
                d.deviationHz.push_back ((unwrapped - lastUnwrapped) * Demodulated::rate / juce::MathConstants<double>::twoPi);
                d.amplitude.push_back (2.0 * std::hypot (a, b));
                lastUnwrapped = unwrapped;
            }
            lastPhase = phase;
        }

        // Drop the filter's settling.
        const auto skip = (size_t) juce::jmin ((int) d.deviationHz.size(), (int) (0.2 * Demodulated::rate));
        d.deviationHz.erase (d.deviationHz.begin(), d.deviationHz.begin() + (long) skip);
        d.amplitude.erase (d.amplitude.begin(), d.amplitude.begin() + (long) skip);
        return d;
    }

    struct Peak { double freq, amplitude; };

    // The strongest lines in a slowly-sampled signal (rate in Hz), as sinusoid amplitudes.
    std::vector<Peak> linePeaks (const std::vector<double>& signal, double rate, double minFreq, double maxFreq, int count, double& totalRms)
    {
        int order = 1;
        while ((1 << order) < (int) signal.size())
            ++order;
        std::vector<float> x (signal.size());
        double mean = 0.0;
        for (auto v : signal)
            mean += v / (double) signal.size();
        for (size_t i = 0; i < signal.size(); ++i)
            x[i] = (float) (signal[i] - mean);

        const auto s = fft (x.data(), (int) x.size(), order);
        const auto size = (double) (1 << order);
        const auto n = (double) x.size();
        // Hann: a sinusoid of amplitude A gives a peak magnitude of A * n / 4.
        const auto scale = 4.0 / n;

        totalRms = rms (x.data(), (int) x.size());

        std::vector<Peak> peaks;
        for (int k = 2; k < (int) size / 2 - 2; ++k)
        {
            const auto f = k * rate / size;
            if (f < minFreq || f > maxFreq)
                continue;
            const auto m = std::abs (s[(size_t) k]);
            if (m <= std::abs (s[(size_t) k - 1]) || m < std::abs (s[(size_t) k + 1]))
                continue;

            // Parabolic interpolation on log magnitude for the true line frequency and height.
            const auto l = std::log (std::abs (s[(size_t) k - 1]) + 1e-30), c = std::log (m + 1e-30), r = std::log (std::abs (s[(size_t) k + 1]) + 1e-30);
            const auto offset = 0.5 * (l - r) / (l - 2.0 * c + r);
            const auto height = std::exp (c - 0.25 * (l - r) * offset);
            peaks.push_back ({ (k + offset) * rate / size, height * scale });
        }

        std::sort (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.amplitude > b.amplitude; });

        // Hann sidelobes (-31 dB and falling) beside a strong line are not lines of their own.
        const auto binWidth = rate / n;
        std::vector<Peak> lines;
        for (auto& peak : peaks)
        {
            const auto sidelobe = std::any_of (lines.begin(), lines.end(), [&] (const Peak& stronger)
            {
                return std::abs (peak.freq - stronger.freq) < 8.0 * binWidth && peak.amplitude < 0.03 * stronger.amplitude;
            });
            if (! sidelobe)
                lines.push_back (peak);
        }
        peaks = lines;

        if ((int) peaks.size() > count)
            peaks.resize ((size_t) count);
        return peaks;
    }

    double rmsInBand (const std::vector<double>& signal, double rate, double lo, double hi)
    {
        int order = 1;
        while ((1 << order) < (int) signal.size())
            ++order;
        std::vector<float> x (signal.begin(), signal.end());
        const auto s = fft (x.data(), (int) x.size(), order);
        const auto size = (double) (1 << order);
        double band = 0.0, all = 0.0;
        for (int k = 1; k < (int) size / 2; ++k)
        {
            const auto p = std::norm (s[(size_t) k]);
            all += p;
            if (const auto f = k * rate / size; f >= lo && f < hi)
                band += p;
        }
        // Share the time-domain RMS out by band power, so the window's gain cancels.
        const auto total = rms (x.data(), (int) x.size());
        return all > 0.0 ? total * std::sqrt (band / all) : 0.0;
    }

    std::vector<double> thirdOctaves (double lo, double hi)
    {
        std::vector<double> centres;
        for (int i = -20; i <= 13; ++i)
            if (const auto f = 1000.0 * std::pow (2.0, i / 3.0); f >= lo * 0.99 && f <= hi * 1.01)
                centres.push_back (f);
        return centres;
    }

    juce::String freqLabel (double f)
    {
        return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + "k" : juce::String (juce::roundToInt (f));
    }

    //==============================================================================
    // Measurements

    // Sine at freq/level through the plugin: returns the last 0.68 s of the mono output.
    juce::AudioBuffer<float> playTone (Host& host, double freq, double level, double seconds = 1.2)
    {
        return monoMix (host.run (sine (freq, level, seconds)));
    }

    Harmonics measureTone (Host& host, double freq, double level)
    {
        const auto out = playTone (host, freq, level);
        constexpr int n = 32768;
        return harmonics (out.getReadPointer (0, out.getNumSamples() - n), n, freq);
    }

    double responseAt (Host& host, double freq, double level)
    {
        const auto out = playTone (host, freq, level, 0.6);
        constexpr int n = 16384;
        return toneAmplitude (out.getReadPointer (0, out.getNumSamples() - n), n, freq, 14) / juce::Decibels::decibelsToGain (level);
    }

    struct WowResult
    {
        double offsetCents = 0.0, totalRms = 0.0, wowRms = 0.0, flutterRms = 0.0, peakToPeak = 0.0;
        double amDb = 0.0, dipDb = 0.0;
        std::vector<Peak> lines;
        Demodulated trace;
    };

    constexpr double wowTone = 3150.0;

    WowResult analyseWow (const juce::AudioBuffer<float>& out)
    {
        constexpr double f0 = wowTone;
        WowResult w;
        const auto start = (int) sampleRate; // skip the first second
        w.trace = demodulate (out.getReadPointer (0, start), out.getNumSamples() - start, f0);

        auto& dev = w.trace.deviationHz;
        double mean = 0.0;
        for (auto v : dev)
            mean += v / (double) dev.size();

        std::vector<double> percent (dev.size());
        double lo = 1e9, hi = -1e9;
        for (size_t i = 0; i < dev.size(); ++i)
        {
            percent[i] = 100.0 * (dev[i] - mean) / f0;
            lo = juce::jmin (lo, percent[i]);
            hi = juce::jmax (hi, percent[i]);
        }

        w.offsetCents = 1200.0 * std::log2 ((f0 + mean) / f0);
        w.peakToPeak = hi - lo;
        w.wowRms = rmsInBand (percent, Demodulated::rate, 0.05, 6.0);
        w.flutterRms = rmsInBand (percent, Demodulated::rate, 6.0, 200.0);
        w.lines = linePeaks (percent, Demodulated::rate, 0.05, 200.0, 6, w.totalRms);

        std::vector<double> ampDb;
        double ampMean = 0.0;
        for (auto a : w.trace.amplitude)
        {
            ampDb.push_back (juce::Decibels::gainToDecibels (a, -200.0));
            ampMean += ampDb.back() / (double) w.trace.amplitude.size();
        }
        double var = 0.0, dip = 0.0;
        for (auto v : ampDb)
        {
            var += (v - ampMean) * (v - ampMean) / (double) ampDb.size();
            dip = juce::jmin (dip, v - ampMean);
        }
        w.amDb = std::sqrt (var);
        w.dipDb = dip;
        return w;
    }

    WowResult measureWow (Host& host, double seconds)
    {
        return analyseWow (monoMix (host.run (sine (wowTone, -18.0, seconds + 1.0))));
    }

    //==============================================================================
    // Commands

    int params (Host& host)
    {
        std::cout << column ("Parameter", 18) << column ("default", 10) << column ("0", 10) << column (".25", 10)
                  << column (".5", 10) << column (".75", 10) << column ("1", 10) << "\n";
        for (auto* p : host.params)
        {
            std::cout << column (p->getName (128), 18) << column (juce::String (p->getDefaultValue(), 3), 10);
            for (auto v : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                std::cout << column (p->getText (v, 32), 10);
            std::cout << "\n";
        }
        return 0;
    }

    int levels (Host& host, const juce::ArgumentList& args)
    {
        const auto freq = args.containsOption ("--freq") ? args.getValueForOption ("--freq").getDoubleValue() : 1000.0;
        host.printSettings();
        std::cout << "Sine " << freqLabel (freq) << "Hz\n"
                  << column ("in dBFS", 8) << column ("out dBFS", 10) << column ("gain dB", 9) << column ("H2 dB", 8)
                  << column ("H3 dB", 8) << column ("H4 dB", 8) << column ("H5 dB", 8) << column ("THD %", 9) << "\n";

        for (double level = -48.0; level <= 12.0; level += 3.0)
        {
            const auto h = measureTone (host, freq, level);
            const auto in = juce::Decibels::decibelsToGain (level);
            std::cout << column (juce::String (level, 0), 8) << column (db (h.fundamental), 10) << column (db (h.fundamental / in), 9);
            for (auto r : h.relative)
                std::cout << column (db (r), 8);
            std::cout << column (juce::String (100.0 * h.thd, 2), 9) << "\n";
        }
        return 0;
    }

    int response (Host& host, const juce::ArgumentList& args)
    {
        const auto level = args.containsOption ("--level") ? args.getValueForOption ("--level").getDoubleValue() : -24.0;
        host.printSettings();

        const auto reference = responseAt (host, 1000.0, level);
        std::cout << "Stepped sines at " << level << " dBFS; 1 kHz gain " << db (reference) << " dB\n"
                  << column ("Hz", 7) << column ("dB re 1k", 10) << "\n";
        for (auto f : thirdOctaves (20.0, 20000.0))
            std::cout << column (freqLabel (f), 7) << column (db (responseAt (host, f, level) / reference), 10) << "\n";
        return 0;
    }

    int wow (Host& host, const juce::ArgumentList& args)
    {
        const auto seconds = args.containsOption ("--seconds") ? args.getValueForOption ("--seconds").getDoubleValue() : 30.0;
        host.printSettings();

        const auto w = measureWow (host, seconds);
        std::cout << "3150 Hz tone, " << seconds << " s\n"
                  << "Pitch offset    " << juce::String (w.offsetCents, 2) << " cents\n"
                  << "Speed RMS       " << juce::String (w.totalRms, 4) << " %  (wow <6 Hz " << juce::String (w.wowRms, 4)
                  << " %, flutter >6 Hz " << juce::String (w.flutterRms, 4) << " %)\n"
                  << "Peak-to-peak    " << juce::String (w.peakToPeak, 4) << " %\n"
                  << "Level wobble    " << juce::String (w.amDb, 2) << " dB RMS, deepest dip " << juce::String (w.dipDb, 1) << " dB\n\n"
                  << "Strongest speed lines (sine amplitude, % of speed):\n";

        double periodic = 0.0;
        for (auto& line : w.lines)
        {
            std::cout << column (juce::String (line.freq, 3) + " Hz", 12) << column (juce::String (line.amplitude, 4) + " %", 12) << "\n";
            periodic += line.amplitude * line.amplitude / 2.0;
        }
        if (w.totalRms > 0.0)
            std::cout << "These lines carry " << juce::roundToInt (100.0 * periodic / (w.totalRms * w.totalRms))
                      << "% of the speed variation; the rest is random drift.\n";

        if (args.containsOption ("--csv"))
        {
            juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (args.getValueForOption ("--csv")));
            juce::String csv ("seconds,deviation_percent,level_db\n");
            double mean = 0.0;
            for (auto v : w.trace.deviationHz)
                mean += v / (double) w.trace.deviationHz.size();
            for (size_t i = 0; i < w.trace.deviationHz.size(); ++i)
                csv << juce::String ((double) i / Demodulated::rate, 3) << "," << juce::String (100.0 * (w.trace.deviationHz[i] - mean) / 3150.0, 5)
                    << "," << db (w.trace.amplitude[i]) << "\n";
            file.replaceWithText (csv);
            std::cout << "\nWrote " << file.getFullPathName() << "\n";
        }
        return 0;
    }

    int noise (Host& host, const juce::ArgumentList& args)
    {
        const auto seconds = args.containsOption ("--seconds") ? args.getValueForOption ("--seconds").getDoubleValue() : 10.0;
        host.printSettings();

        const auto out = host.run (silence (seconds + 1.0));
        const auto start = (int) sampleRate;
        const auto n = out.getNumSamples() - start;
        std::cout << "Silence in, " << seconds << " s\n"
                  << "RMS  L " << db (rms (out.getReadPointer (0, start), n)) << " dBFS, R " << db (rms (out.getReadPointer (1, start), n))
                  << " dBFS; peak " << db (out.getMagnitude (start, n)) << " dBFS\n\n";

        // Welch-averaged spectrum, summed into third octaves.
        constexpr int order = 13, size = 1 << order;
        const auto m = monoMix (out);
        std::vector<double> power (size / 2, 0.0);
        int frames = 0;
        for (int at = start; at + size <= out.getNumSamples(); at += size / 2, ++frames)
        {
            const auto s = fft (m.getReadPointer (0, at), size, order);
            for (int k = 0; k < size / 2; ++k)
                power[(size_t) k] += std::norm (s[(size_t) k]);
        }
        // Hann power gain is 3/8; one-sided spectrum sums to mean square * size^2 * 3/8 / 2.
        const auto norm = 2.0 / (juce::jmax (1, frames) * (double) size * (double) size * 0.375);
        std::cout << column ("Hz", 7) << column ("dBFS", 8) << "   (third-octave band level, mid)\n";
        for (auto f : thirdOctaves (25.0, 20000.0))
        {
            double sum = 0.0;
            for (int k = 1; k < size / 2; ++k)
                if (const auto bin = k * sampleRate / size; bin >= f / std::pow (2.0, 1.0 / 6.0) && bin < f * std::pow (2.0, 1.0 / 6.0))
                    sum += power[(size_t) k];
            std::cout << column (freqLabel (f), 7) << column (db (std::sqrt (sum * norm)), 8) << "\n";
        }

        // Clicks: high-passed peaks well above the high-passed noise floor.
        juce::dsp::IIR::Filter<float> highpass;
        highpass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 2000.0);
        std::vector<float> hp ((size_t) n);
        for (int i = 0; i < n; ++i)
            hp[(size_t) i] = std::abs (highpass.processSample (m.getSample (0, start + i)));
        auto sorted = hp;
        std::nth_element (sorted.begin(), sorted.begin() + n / 2, sorted.end());
        const auto threshold = juce::jmax (1.0e-7f, 10.0f * sorted[(size_t) n / 2]);
        std::vector<int> clicks;
        for (int i = 0; i < n; ++i)
            if (hp[(size_t) i] > threshold && (clicks.empty() || i - clicks.back() > (int) (0.005 * sampleRate)))
                clicks.push_back (i);

        std::cout << "\nClicks (>20 dB over the high-passed floor): " << clicks.size() << " in " << seconds << " s";
        if (clicks.size() > 2)
        {
            std::vector<int> gaps;
            for (size_t i = 1; i < clicks.size(); ++i)
                gaps.push_back (clicks[i] - clicks[i - 1]);
            std::nth_element (gaps.begin(), gaps.begin() + (long) gaps.size() / 2, gaps.end());
            std::cout << ", median gap " << juce::String (gaps[gaps.size() / 2] / sampleRate, 3) << " s";
        }
        std::cout << "\n";

        // Rhythm: the 100 Hz RMS envelope's strongest lines (a motor or capstan turning).
        std::vector<double> envelope;
        const auto frame = (int) (sampleRate / 100.0);
        for (int at = start; at + frame <= out.getNumSamples(); at += frame)
            envelope.push_back (juce::Decibels::gainToDecibels (rms (m.getReadPointer (0, at), frame), -200.0));
        double total = 0.0;
        const auto lines = linePeaks (envelope, 100.0, 0.2, 45.0, 4, total);
        std::cout << "Level envelope varies " << juce::String (total, 2) << " dB RMS; strongest rhythms:";
        for (auto& line : lines)
            std::cout << "  " << juce::String (line.freq, 2) << " Hz (" << juce::String (line.amplitude, 2) << " dB)";
        std::cout << "\n";
        return 0;
    }

    int stereo (Host& host)
    {
        host.printSettings();
        const auto out = host.run (whiteNoise (-18.0, 6.0));
        const auto start = (int) sampleRate;
        const auto n = out.getNumSamples() - start;

        double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
        for (int i = start; i < out.getNumSamples(); ++i)
        {
            const double l = out.getSample (0, i), r = out.getSample (1, i);
            ll += l * l; rr += r * r; lr += l * r;
            mm += 0.25 * (l + r) * (l + r);
            ss += 0.25 * (l - r) * (l - r);
        }
        std::cout << "Mono white noise in at -18 dBFS RMS\n"
                  << "L " << db (std::sqrt (ll / n)) << " dBFS, R " << db (std::sqrt (rr / n)) << " dBFS\n"
                  << "Correlation " << juce::String (lr / std::sqrt (ll * rr + 1e-30), 3)
                  << ", side re mid " << db (std::sqrt (ss / (mm + 1e-30))) << " dB\n\n"
                  << column ("octave", 8) << column ("corr", 8) << "\n";

        constexpr int order = 13, size = 1 << order;
        std::vector<std::complex<double>> cross (size / 2);
        std::vector<double> pl (size / 2), pr (size / 2);
        for (int at = start; at + size <= out.getNumSamples(); at += size / 2)
        {
            const auto l = fft (out.getReadPointer (0, at), size, order);
            const auto r = fft (out.getReadPointer (1, at), size, order);
            for (int k = 0; k < size / 2; ++k)
            {
                cross[(size_t) k] += std::complex<double> (l[(size_t) k]) * std::conj (std::complex<double> (r[(size_t) k]));
                pl[(size_t) k] += std::norm (l[(size_t) k]);
                pr[(size_t) k] += std::norm (r[(size_t) k]);
            }
        }
        for (double f = 62.5; f < 20000.0; f *= 2.0)
        {
            std::complex<double> c;
            double a = 0, b = 0;
            for (int k = 1; k < size / 2; ++k)
                if (const auto bin = k * sampleRate / size; bin >= f / std::sqrt (2.0) && bin < f * std::sqrt (2.0))
                {
                    c += cross[(size_t) k];
                    a += pl[(size_t) k];
                    b += pr[(size_t) k];
                }
            std::cout << column (freqLabel (f), 8) << column (juce::String (c.real() / std::sqrt (a * b + 1e-30), 3), 8) << "\n";
        }
        return 0;
    }

    int sweep (Host& host, const juce::ArgumentList& args)
    {
        auto* p = host.find (args[1].text);
        if (p == nullptr)
        {
            std::cerr << "Unknown parameter: " << args[1].text << "\n";
            return 1;
        }

        const auto steps = args.containsOption ("--steps") ? juce::jmax (2, args.getValueForOption ("--steps").getIntValue()) : 11;
        const auto level = args.containsOption ("--level") ? args.getValueForOption ("--level").getDoubleValue() : -18.0;
        host.printSettings();

        std::cout << "Sweep " << p->getName (128) << "; tone tests at " << level << " dBFS\n"
                  << column ("value", 6) << column ("shows", 10) << column ("gain", 7) << column ("THD%", 7) << column ("H2", 7) << column ("H3", 7)
                  << column ("60", 6) << column ("200", 6) << column ("3k", 6) << column ("8k", 6) << column ("12k", 6) << column ("16k", 6)
                  << column ("wow%", 8) << column ("flut%", 8) << column ("noise", 7) << "\n";

        for (int i = 0; i < steps; ++i)
        {
            const auto value = (float) i / (float) (steps - 1);
            host.set (*p, value);
            host.run (silence (0.3)); // let parameter smoothing settle

            const auto h = measureTone (host, 1000.0, level);
            const auto ref = responseAt (host, 1000.0, level);
            std::cout << column (juce::String (value, 2), 6) << column (p->getCurrentValueAsText(), 10)
                      << column (db (h.fundamental / juce::Decibels::decibelsToGain (level)), 7) << column (juce::String (100.0 * h.thd, 2), 7)
                      << column (db (h.relative[0]), 7) << column (db (h.relative[1]), 7);
            for (auto f : { 60.0, 200.0, 3000.0, 8000.0, 12000.0, 16000.0 })
                std::cout << column (db (responseAt (host, f, level) / ref), 6);

            const auto w = measureWow (host, 6.0);
            const auto quiet = host.run (silence (2.0));
            std::cout << column (juce::String (w.wowRms, 4), 8) << column (juce::String (w.flutterRms, 4), 8)
                      << column (db (rms (quiet.getReadPointer (0, (int) sampleRate), (int) sampleRate)), 7) << "\n";
        }
        return 0;
    }

    // Checks the analysis against signals whose answers are known, no plugin involved.
    int selftest()
    {
        bool ok = true;
        auto check = [&] (const char* what, double got, double want, double tolerance)
        {
            const auto pass = std::abs (got - want) <= tolerance;
            ok = ok && pass;
            std::cout << (pass ? "  ok    " : "  FAIL  ") << what << ": " << got << " (want " << want << ")\n";
        };

        // Tone at 3150 Hz with 0.1 % speed wobble at 0.6 Hz, 0.03 % at 12 Hz and 1 dB of level wobble at 2 Hz.
        const auto seconds = 21.0;
        juce::AudioBuffer<float> fm (1, (int) (seconds * sampleRate));
        double phase = 0.0;
        for (int n = 0; n < fm.getNumSamples(); ++n)
        {
            const auto t = n / sampleRate;
            const auto speed = 1.0 + 0.001 * std::sin (juce::MathConstants<double>::twoPi * 0.6 * t)
                                   + 0.0003 * std::sin (juce::MathConstants<double>::twoPi * 12.0 * t);
            phase += juce::MathConstants<double>::twoPi * wowTone * speed / sampleRate;
            const auto level = juce::Decibels::decibelsToGain (-18.0 + std::sin (juce::MathConstants<double>::twoPi * 2.0 * t));
            fm.setSample (0, n, (float) (level * std::sin (phase)));
        }
        const auto w = analyseWow (fm);
        std::cout << "Wow and flutter\n";
        check ("wow RMS %", w.wowRms, 0.1 / std::sqrt (2.0), 0.005);
        check ("flutter RMS %", w.flutterRms, 0.03 / std::sqrt (2.0), 0.002);
        check ("strongest line Hz", w.lines[0].freq, 0.6, 0.02);
        check ("strongest line %", w.lines[0].amplitude, 0.1, 0.005);
        check ("second line Hz", w.lines[1].freq, 12.0, 0.05);
        check ("second line %", w.lines[1].amplitude, 0.03, 0.002);
        check ("level wobble dB RMS", w.amDb, 1.0 / std::sqrt (2.0), 0.05);

        // Full-Wow territory: 8 % at 0.8 Hz must come through without fake harmonics or dropouts.
        phase = 0.0;
        for (int n = 0; n < fm.getNumSamples(); ++n)
        {
            phase += juce::MathConstants<double>::twoPi * wowTone * (1.0 + 0.08 * std::sin (juce::MathConstants<double>::twoPi * 0.8 * n / sampleRate)) / sampleRate;
            fm.setSample (0, n, (float) (0.125 * std::sin (phase)));
        }
        const auto heavy = analyseWow (fm);
        check ("heavy line %", heavy.lines[0].amplitude, 8.0, 0.1);
        check ("heavy next line %", heavy.lines[1].amplitude, 0.0, 0.1);
        check ("heavy deepest dip dB", heavy.dipDb, 0.0, 0.5);

        // tanh of a 1 kHz sine at amplitude 1: known harmonic levels.
        juce::AudioBuffer<float> shaped (1, 32768);
        for (int n = 0; n < shaped.getNumSamples(); ++n)
            shaped.setSample (0, n, (float) std::tanh (std::sin (juce::MathConstants<double>::twoPi * 1000.0 * n / sampleRate)));
        // Reference by direct projection onto sin(k w t) over a whole number of cycles (48 cycles = 2304 samples).
        auto project = [&] (int k)
        {
            double sum = 0.0;
            for (int n = 0; n < 2304; ++n)
                sum += shaped.getSample (0, n) * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * k * n / sampleRate);
            return std::abs (2.0 * sum / 2304.0);
        };
        const auto h = harmonics (shaped.getReadPointer (0), shaped.getNumSamples(), 1000.0);
        std::cout << "Harmonics\n";
        check ("fundamental", h.fundamental, project (1), 0.002);
        check ("H3 dB", juce::Decibels::gainToDecibels (h.relative[1]), juce::Decibels::gainToDecibels (project (3) / project (1)), 0.1);
        check ("H5 dB", juce::Decibels::gainToDecibels (h.relative[3]), juce::Decibels::gainToDecibels (project (5) / project (1)), 0.2);

        std::cout << (ok ? "All checks passed\n" : "Some checks FAILED\n");
        return ok ? 0 : 1;
    }

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                             .withNumChannels (buffer.getNumChannels())
                                                             .withBitsPerSample (24);
        std::unique_ptr<juce::OutputStream> output (std::move (stream));
        auto writer = wav.createWriterFor (output, options);
        return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    int render (Host& host, const juce::ArgumentList& args)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (args[1].resolveAsFile()));
        if (reader == nullptr)
        {
            std::cerr << "Could not read " << args[1].text << "\n";
            return 1;
        }
        if (! juce::approximatelyEqual (reader->sampleRate, sampleRate))
            std::cerr << "Note: input is " << reader->sampleRate << " Hz; the plugin runs at 48 kHz, so it will play back at the wrong speed.\n";

        juce::AudioBuffer<float> input ((int) juce::jmin (2u, reader->numChannels), (int) reader->lengthInSamples);
        reader->read (&input, 0, input.getNumSamples(), 0, true, true);

        host.printSettings();
        return writeWav (args[2].resolveAsFile(), host.run (input), sampleRate) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);
    const auto command = args.size() > 0 ? args[0].text : juce::String();
    if (command == "selftest")
        return selftest();

    const juce::StringArray commands { "params", "levels", "response", "wow", "noise", "stereo", "sweep", "render" };
    if (! commands.contains (command) || (command == "sweep" && args.size() < 2) || (command == "render" && args.size() < 3))
    {
        std::cerr << "Usage: TapeCapture <params|levels|response|wow|noise|stereo|sweep <param>|render <in> <out.wav>>\n"
                     "                   [--plugin=<path>] [--<param>=<0..1> ...]  (see the top of Main.cpp)\n";
        return 1;
    }

    Host host;
    if (! host.load (args))
        return 1;

    if (command == "params")   return params (host);
    if (command == "levels")   return levels (host, args);
    if (command == "response") return response (host, args);
    if (command == "wow")      return wow (host, args);
    if (command == "noise")    return noise (host, args);
    if (command == "stereo")   return stereo (host);
    if (command == "sweep")    return sweep (host, args);
    return render (host, args);
}
