// Offline checks for Upright, without a DAW.
//
//   UprightRender note <output.wav> [--note=60] [--velocity=0.7] [--seconds=6] [--<param>=<value> ...]
//       Strikes one key, holds it for half the time, then lets go.
//
//   UprightRender demo <output.wav> [--<param>=<value> ...]
//       A short pub-piano passage: stride bass, chords and runs, with the sustain pedal.
//
//   UprightRender measure [--voicing=Mills|Studio]
//       For a spread of keys: first-partial tuning error, measured vs. modelled inharmonicity,
//       loop design, peak level and decay times, from the engine's bridge signal.
//
//   UprightRender cpu
//       Times the per-voicing setup and rendering of a dense passage.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double sampleRate = 48000.0;

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption())
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            if (id == "note" || id == "velocity" || id == "seconds")
                continue;

            const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                             .withNumChannels (buffer.getNumChannels())
                                                             .withBitsPerSample (24);
        std::unique_ptr<juce::OutputStream> output (std::move (stream));
        auto writer = wav.createWriterFor (output, options);
        return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    /** Renders timed MIDI through the processor. */
    juce::AudioBuffer<float> renderMidi (PluginProcessor& processor, const juce::MidiBuffer& events, double seconds)
    {
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        juce::Thread::sleep (300); // the soundboard IRs load in the background

        const auto total = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> block (2, blockSize);

        for (int start = 0; start < total; start += blockSize)
        {
            const auto n = juce::jmin (blockSize, total - start);
            block.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            midi.addEvents (events, start, n, -start);
            processor.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, start, block, ch, 0, n);
        }

        const auto peak = out.getMagnitude (0, total);
        std::cout << "Peak " << juce::Decibels::gainToDecibels (peak) << " dBFS\n";
        return out;
    }

    int note (const juce::ArgumentList& args)
    {
        const auto key = args.getValueForOption ("--note").getIntValue();
        const auto velocity = args.containsOption ("--velocity") ? args.getValueForOption ("--velocity").getFloatValue() : 0.7f;
        const auto seconds = args.containsOption ("--seconds") ? args.getValueForOption ("--seconds").getDoubleValue() : 6.0;

        PluginProcessor processor;
        applyParameters (processor, args);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, key == 0 ? 60 : key, velocity), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, key == 0 ? 60 : key), (int) (seconds * 0.5 * sampleRate));

        return writeWav (args[1].resolveAsFile(), renderMidi (processor, midi, seconds)) ? 0 : 1;
    }

    int demo (const juce::ArgumentList& args)
    {
        PluginProcessor processor;
        applyParameters (processor, args);

        // Stride left hand (root low, chord high) under a right-hand chord melody, 4 bars of
        // C - F - C/G G7 - C at 116 bpm, with a run at the end.
        juce::MidiBuffer midi;
        const auto beat = 60.0 / 116.0;
        auto at = [&] (double beats) { return (int) (beats * beat * sampleRate) + 100; };
        auto play = [&] (double startBeat, double lengthBeats, std::initializer_list<int> notes, float velocity)
        {
            for (auto n : notes)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, n, velocity), at (startBeat));
                midi.addEvent (juce::MidiMessage::noteOff (1, n), at (startBeat + lengthBeats));
            }
        };

        const std::array<std::pair<int, std::initializer_list<int>>, 4> bars { { { 36, { 52, 55, 60 } }, { 41, { 53, 57, 60 } },
                                                                                 { 43, { 53, 55, 59 } }, { 36, { 52, 55, 60 } } } };
        const std::array<std::initializer_list<int>, 4> right { { { 72, 76, 79 }, { 72, 77, 81 }, { 71, 74, 77 }, { 72, 76, 84 } } };

        for (int bar = 0; bar < 4; ++bar)
        {
            const auto b = bar * 4.0;
            const auto& [root, chord] = bars[(size_t) bar];
            play (b + 0, 0.9, { root }, 0.8f);
            play (b + 1, 0.5, chord, 0.6f);
            play (b + 2, 0.9, { root + 7 }, 0.75f);
            play (b + 3, 0.5, chord, 0.6f);

            play (b + 0, 1.4, right[(size_t) bar], 0.75f);
            play (b + 1.5, 0.5, right[(size_t) bar], 0.6f);
            play (b + 2.5, 1.4, right[(size_t) bar], 0.7f);
        }

        // A run up to the top, then a held final chord with the pedal.
        const std::array<int, 8> run { 72, 74, 76, 77, 79, 81, 83, 84 };
        for (size_t i = 0; i < run.size(); ++i)
            play (16.0 + 0.25 * (double) i, 0.25, { run[i] }, 0.55f + 0.04f * (float) i);

        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), at (18.0));
        play (18.0, 3.0, { 36, 48, 64, 67, 72, 76, 84 }, 0.85f);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), at (24.0));

        return writeWav (args[1].resolveAsFile(), renderMidi (processor, midi, 26.0 * beat + 1.0)) ? 0 : 1;
    }

    /** Peak frequency near `hz` (within +-searchCents), parabolic-interpolated. */
    double findPeak (const std::vector<float>& magnitude, int fftSize, double hz, double searchCents)
    {
        const auto lo = juce::jmax (1, (int) std::floor (hz * std::pow (2.0, -searchCents / 1200.0) * fftSize / sampleRate));
        const auto hi = juce::jmin (fftSize / 2 - 2, (int) std::ceil (hz * std::pow (2.0, searchCents / 1200.0) * fftSize / sampleRate));
        auto best = lo;
        for (int k = lo; k <= hi; ++k)
            if (magnitude[(size_t) k] > magnitude[(size_t) best])
                best = k;

        const auto a = std::log (magnitude[(size_t) best - 1] + 1e-20f);
        const auto b = std::log (magnitude[(size_t) best] + 1e-20f);
        const auto c = std::log (magnitude[(size_t) best + 1] + 1e-20f);
        const auto offset = 0.5 * (a - c) / (a - 2.0 * b + c);
        return (best + offset) * sampleRate / fftSize;
    }

    int measure (const juce::ArgumentList& args)
    {
        const auto mills = ! args.getValueForOption ("--voicing").equalsIgnoreCase ("studio");
        const auto voicing = mills ? piano::Voicing::mills : piano::Voicing::studio;
        std::cout << (mills ? "Mills" : "Studio") << " voicing, " << sampleRate << " Hz\n"
                  << " note   f1 err(c)  B model  B meas  stages     a     pole  peak(dB)  T60 early  T60 late\n";

        piano::Engine engine;
        engine.prepare (sampleRate, voicing);
        piano::Controls controls;
        controls.unison = 0.0f;
        engine.setControls (controls);

        for (int key : { 21, 28, 33, 40, 48, 55, 60, 64, 69, 76, 84, 91, 96, 103, 108 })
        {
            const auto spec = piano::specFor (key, voicing);
            const auto design = piano::designLoop (spec, sampleRate);

            engine.allNotesOff();
            engine.noteOn (key, 0.7f);

            const auto total = (int) (sampleRate * 6.0);
            std::vector<float> signal ((size_t) total, 0.0f);
            for (int start = 0; start < total; start += blockSize)
                engine.render (signal.data() + start, juce::jmin (blockSize, total - start));

            // Partials from 0.2 s on, Hann window, 2^17 points (0.37 Hz bins).
            constexpr int order = 17;
            constexpr int fftSize = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> data ((size_t) fftSize * 2, 0.0f);
            const auto offset = (int) (0.2 * sampleRate);
            for (int i = 0; i < fftSize && offset + i < total; ++i)
                data[(size_t) i] = signal[(size_t) (offset + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize));
            fft.performFrequencyOnlyForwardTransform (data.data(), true);

            // Unison off, so the voicing's own detune is the only spread; measure the middle string's pitch.
            const auto f1 = findPeak (data, fftSize, spec.frequency, 30.0);
            const auto k = juce::jmin (10, (int) (0.4 * sampleRate / spec.frequency / 1.2));
            double bMeasured = 0.0;
            if (k >= 3)
            {
                const auto expected = k * spec.frequency * std::sqrt ((1.0 + spec.inharmonicity * k * k) / (1.0 + spec.inharmonicity));
                const auto fk = findPeak (data, fftSize, expected, 40.0);
                // fk / (k f1) = sqrt ((1 + B k^2) / (1 + B))  =>  B = (r^2 - 1) / (k^2 - r^2)
                const auto r2 = juce::square (fk / (k * f1));
                bMeasured = (r2 - 1.0) / (k * k - r2);
            }

            // Decay from a least-squares line through the 50 ms RMS envelope (in dB), so unison
            // beating averages out: 0.1-0.6 s (prompt) and 2-5 s (aftersound).
            auto t60 = [&] (double from, double to)
            {
                const auto frame = (int) (0.05 * sampleRate);
                double n = 0.0, st = 0.0, sd = 0.0, stt = 0.0, std_ = 0.0;
                for (auto i = (int) (from * sampleRate); i + frame <= (int) (to * sampleRate); i += frame)
                {
                    double sum = 0.0;
                    for (int j = i; j < i + frame; ++j)
                        sum += juce::square ((double) signal[(size_t) j]);
                    const auto t = (i + 0.5 * frame) / sampleRate;
                    if (sum <= 0.0)
                        break; // the key has died away and reset
                    const auto db = 10.0 * std::log10 (sum / frame);
                    n += 1.0; st += t; sd += db; stt += t * t; std_ += t * db;
                }
                const auto slope = (n * std_ - st * sd) / (n * stt - st * st); // dB per second
                return n >= 4.0 && slope < -0.01 ? -60.0 / slope : 0.0;
            };

            float peak = 0.0f;
            for (auto s : signal)
                peak = juce::jmax (peak, std::abs (s));

            std::printf (" %4d %+9.2f  %.2e  %.2e  %5d  %+.3f  %.3f  %7.1f  %8.2f  %8.2f\n",
                         key, 1200.0 * std::log2 (f1 / spec.frequency), spec.inharmonicity, bMeasured,
                         design.stages, design.dispersionCoef, design.lossPole,
                         juce::Decibels::gainToDecibels (peak), t60 (0.1, 0.6), t60 (2.0, 5.0));
        }
        return 0;
    }

    int cpu()
    {
        for (auto voicing : { piano::Voicing::studio, piano::Voicing::mills })
        {
            piano::Engine engine;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            engine.prepare (sampleRate, voicing);
            const auto t1 = juce::Time::getMillisecondCounterHiRes();

            // 10 s with a new 6-note chord every 250 ms and the pedal down: up to 48 keys ringing.
            engine.setSustain (true);
            const auto total = (int) (10.0 * sampleRate);
            std::vector<float> out ((size_t) blockSize);
            juce::Random r (7);
            for (int start = 0, next = 0; start < total; start += blockSize)
            {
                if (start >= next)
                {
                    for (int i = 0; i < 6; ++i)
                        engine.noteOn (30 + r.nextInt (70), 0.4f + 0.5f * r.nextFloat());
                    next += (int) (0.25 * sampleRate);
                }
                engine.render (out.data(), blockSize);
            }
            const auto t2 = juce::Time::getMillisecondCounterHiRes();

            std::printf ("%s: setup %.0f ms, 10 s of dense pedalled chords in %.0f ms (%.1f%% of one core)\n",
                         voicing == piano::Voicing::mills ? "Mills " : "Studio", t1 - t0, t2 - t1, (t2 - t1) / 100.0);
        }
        return 0;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::ArgumentList args (argc, argv);
    const auto command = args.size() > 0 ? args[0].text : juce::String();

    if (command == "note" && args.size() >= 2)  return note (args);
    if (command == "demo" && args.size() >= 2)  return demo (args);
    if (command == "measure")                   return measure (args);
    if (command == "cpu")                       return cpu();

    std::cout << "Usage:\n"
                 "  UprightRender note <output.wav> [--note=60] [--velocity=0.7] [--seconds=6] [--<param>=<value> ...]\n"
                 "  UprightRender demo <output.wav> [--<param>=<value> ...]\n"
                 "  UprightRender measure [--voicing=Mills|Studio]\n"
                 "  UprightRender cpu\n";
    return 1;
}
