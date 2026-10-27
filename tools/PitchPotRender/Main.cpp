// Offline checks for Pitch-Pot, without a DAW.
//
//   PitchPotRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them).
//
//   PitchPotRender measure
//       Shifts a sine through every interval up and down and prints the measured pitch error, how
//       much of the output is the wanted tone, the level ripple and the delay; then checks a chord,
//       a glide and the CPU cost.
//
//   PitchPotRender snapshot <output.png> [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double measureRate = 48000.0;

    void applyParameters (PluginProcessor& processor, const juce::StringArray& settings)
    {
        for (auto& setting : settings)
        {
            const auto id = setting.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto value = setting.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    juce::StringArray optionsOf (const juce::ArgumentList& args)
    {
        juce::StringArray settings;
        for (auto& arg : args.arguments)
            if (arg.isLongOption())
                settings.add (arg.text);
        return settings;
    }

    std::unique_ptr<PluginProcessor> makeProcessor (const juce::StringArray& settings, double sampleRate, int numInputs)
    {
        auto processor = std::make_unique<PluginProcessor>();
        processor->setPlayConfigDetails (numInputs, 2, sampleRate, blockSize);
        applyParameters (*processor, settings);
        processor->prepareToPlay (sampleRate, blockSize);
        return processor;
    }

    /** Runs audio through in blocks; between blocks, onBlock may change parameters. */
    void process (PluginProcessor& processor, juce::AudioBuffer<float>& audio,
                  const std::function<void (int)>& onBlock = {})
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, blockSize);
        for (int start = 0; start < audio.getNumSamples(); start += blockSize)
        {
            if (onBlock != nullptr)
                onBlock (start);
            const auto n = juce::jmin (blockSize, audio.getNumSamples() - start);
            block.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, audio, ch, start, n);
            processor.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                audio.copyFrom (ch, start, block, ch, 0, n);
        }
    }

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
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

    juce::String db (float gain) { return juce::String (juce::Decibels::gainToDecibels (gain, -200.0f), 1) + " dB"; }

    int render (const juce::ArgumentList& args)
    {
        const auto input = args[1].resolveAsExistingFile();
        const auto output = args[2].resolveAsFile();

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (input));
        if (reader == nullptr)
        {
            std::cerr << "Couldn't read " << input.getFullPathName() << "\n";
            return 1;
        }

        const auto numInputs = (int) juce::jmin (2u, reader->numChannels);
        const auto length = (int) reader->lengthInSamples;
        auto processor = makeProcessor (optionsOf (args), reader->sampleRate, numInputs);

        juce::AudioBuffer<float> audio (2, length + (int) (reader->sampleRate * 0.3));
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);
        if (numInputs == 1)
            audio.copyFrom (1, 0, audio, 0, 0, audio.getNumSamples());

        process (*processor, audio);
        std::cout << "Peak " << db (audio.getMagnitude (0, audio.getNumSamples())) << "\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    juce::AudioBuffer<float> tones (std::initializer_list<float> frequencies, int length, float level = 0.5f)
    {
        juce::AudioBuffer<float> audio (2, length);
        const auto gain = level / (float) frequencies.size();
        for (int n = 0; n < length; ++n)
        {
            auto x = 0.0f;
            for (auto f : frequencies)
                x += gain * std::sin (juce::MathConstants<float>::twoPi * f * (float) n / (float) measureRate);
            audio.setSample (0, n, x);
            audio.setSample (1, n, x);
        }
        return audio;
    }

    /** Power spectrum of channel 0 over [from, from + 2^16), Hann window. */
    struct Spectrum
    {
        static constexpr int order = 16, size = 1 << order;
        std::vector<float> power;

        Spectrum (const juce::AudioBuffer<float>& audio, int from)
        {
            juce::dsp::FFT fft (order);
            std::vector<float> data ((size_t) size * 2, 0.0f);
            for (int n = 0; n < size; ++n)
                data[(size_t) n] = audio.getSample (0, from + n)
                                 * 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) size));
            fft.performFrequencyOnlyForwardTransform (data.data(), true);
            power.resize ((size_t) size / 2);
            for (size_t b = 0; b < power.size(); ++b)
                power[b] = data[b] * data[b];
        }

        static double binHz() { return measureRate / size; }

        /** Strongest peak between lowHz and highHz, refined by parabolic interpolation on log power. */
        double peakHz (double lowHz, double highHz) const
        {
            auto best = (size_t) (lowHz / binHz());
            for (auto b = best; b <= (size_t) (highHz / binHz()); ++b)
                if (power[b] > power[best])
                    best = b;
            const auto a = std::log (power[best - 1] + 1e-20), c = std::log (power[best + 1] + 1e-20), m = std::log (power[best] + 1e-20);
            const auto offset = 0.5 * (a - c) / (a - 2.0 * m + c);
            return ((double) best + offset) * binHz();
        }

        /** Share of the power (20 Hz up) that lies within a semitone-wide band around hz. */
        double shareNear (double hz) const
        {
            double near = 0.0, total = 0.0;
            for (auto b = (size_t) (20.0 / binHz()); b < power.size(); ++b)
            {
                const auto f = (double) b * binHz();
                total += power[b];
                if (std::abs (12.0 * std::log2 (f / hz)) < 0.5)
                    near += power[b];
            }
            return near / juce::jmax (1e-20, total);
        }

        double levelNear (double hz) const
        {
            double near = 0.0;
            for (auto b = (size_t) (hz * 0.97 / binHz()); b <= (size_t) (hz * 1.03 / binHz()); ++b)
                near += power[b];
            return near;
        }
    };

    /** Spread (max - min, dB) of the 10 ms RMS over [from, from + length). */
    float rippleDb (const juce::AudioBuffer<float>& audio, int from, int length)
    {
        const auto window = (int) (0.01 * measureRate);
        auto lo = 1.0e9f, hi = -1.0e9f;
        for (int start = from; start + window <= from + length; start += window)
        {
            const auto level = juce::Decibels::gainToDecibels (audio.getRMSLevel (0, start, window), -200.0f);
            lo = juce::jmin (lo, level);
            hi = juce::jmax (hi, level);
        }
        return hi - lo;
    }

    /** Samples from the start of a tone burst at n = 0 until the output first reaches half its steady level. */
    int onsetDelay (const juce::AudioBuffer<float>& audio, int steadyFrom)
    {
        const auto steady = audio.getMagnitude (0, steadyFrom, (int) (0.1 * measureRate));
        for (int n = 0; n < audio.getNumSamples(); ++n)
            if (std::abs (audio.getSample (0, n)) > 0.5f * steady)
                return n;
        return -1;
    }

    int measure()
    {
        const auto length = (int) (measureRate * 2.5);
        const auto from = (int) (measureRate * 0.8);
        constexpr float input = 220.0f;

        std::cout << "Sine " << input << " Hz, Blend 100% (wet only)\n\n";
        std::cout << "Shift          Dir    Target Hz   Error ct   In tone   Ripple   Delay\n";

        for (int shift = 0; shift < Shifts::names.size(); ++shift)
        {
            for (int dir : { (int) Directions::up, (int) Directions::down })
            {
                auto processor = makeProcessor ({ "--shift=" + Shifts::names[shift], "--direction=" + Directions::names[dir],
                                                  "--blend=100" }, measureRate, 2);
                auto audio = tones ({ input }, length);
                process (*processor, audio);

                const auto target = input * std::exp2 ((dir == Directions::up ? 1.0f : -1.0f) * Shifts::semitones (shift) / 12.0f);
                const Spectrum spectrum (audio, from);
                const auto measured = spectrum.peakHz (target * 0.8, target * 1.25);
                const auto cents = 1200.0 * std::log2 (measured / target);

                juce::String line;
                line << Shifts::names[shift].paddedRight (' ', 15) << Directions::names[dir].paddedRight (' ', 5)
                     << juce::String (target, 1).paddedLeft (' ', 11)
                     << juce::String (cents, 1).paddedLeft (' ', 11)
                     << (juce::String (100.0 * spectrum.shareNear (target), 1) + " %").paddedLeft (' ', 10)
                     << (juce::String (rippleDb (audio, from, (int) measureRate), 1) + " dB").paddedLeft (' ', 9)
                     << (juce::String (1000.0 * onsetDelay (audio, from) / measureRate, 1) + " ms").paddedLeft (' ', 9);
                std::cout << line << "\n";
            }
        }

        // A G major triad up an octave: each note should come through, none much louder than the rest.
        {
            auto processor = makeProcessor ({ "--shift=1 Octave", "--direction=Up", "--blend=100" }, measureRate, 2);
            auto audio = tones ({ 196.0f, 246.94f, 293.66f }, length);
            process (*processor, audio);
            const Spectrum spectrum (audio, from);
            std::cout << "\nG major triad +1 octave, level of each note:";
            const auto reference = spectrum.levelNear (392.0);
            for (auto f : { 392.0, 493.88, 587.33 })
                std::cout << "  " << (int) f << " Hz " << juce::String (10.0 * std::log10 (spectrum.levelNear (f) / reference), 1) << " dB";
            std::cout << "\n                                 in tone " << juce::String (100.0 * (spectrum.shareNear (392.0) + spectrum.shareNear (493.88) + spectrum.shareNear (587.33)), 1) << " %\n";
        }

        // Dual: both the octave up and the octave down, at equal level.
        {
            auto processor = makeProcessor ({ "--shift=1 Octave", "--direction=Dual", "--blend=100" }, measureRate, 2);
            auto audio = tones ({ input }, length);
            process (*processor, audio);
            const Spectrum spectrum (audio, from);
            std::cout << "Dual 1 octave: 440 Hz vs 110 Hz "
                      << juce::String (10.0 * std::log10 (spectrum.levelNear (440.0) / spectrum.levelNear (110.0)), 1) << " dB\n";
        }

        // Glide: start bypassed, engage at 0.5 s with a 1 s glide, sample the pitch on the way up.
        {
            auto processor = makeProcessor ({ "--shift=1 Octave", "--direction=Up", "--blend=100", "--glide=1", "--bypass=1" }, measureRate, 2);
            auto audio = tones ({ input }, (int) (measureRate * 4.0));
            auto* bypass = processor->apvts.getParameter (ParamIDs::bypass);
            const auto engageAt = (int) (measureRate * 0.5);
            process (*processor, audio, [&] (int start) { if (start >= engageAt) bypass->setValueNotifyingHost (0.0f); });

            std::cout << "Glide 1 s, pitch after engaging:";
            for (auto t : { 0.25, 0.5, 0.75 })
            {
                // A short analysis window around each point in time.
                juce::AudioBuffer<float> slice (1, Spectrum::size);
                slice.clear();
                const auto centre = engageAt + (int) (t * measureRate);
                const auto half = (int) (0.02 * measureRate);
                for (int n = 0; n < 2 * half; ++n)
                    slice.setSample (0, n, audio.getSample (0, centre - half + n)
                                         * 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) (2 * half))));
                const Spectrum spectrum (slice, 0);
                const auto hz = spectrum.peakHz (200.0, 480.0);
                std::cout << "  " << t << " s " << juce::String (12.0 * std::log2 (hz / input), 1) << " st";
            }
            std::cout << "  (expect about 3, 6, 9)\n";
        }

        // CPU: the heaviest setting, Dual three octaves, on a stereo chord.
        {
            auto processor = makeProcessor ({ "--shift=3 Octaves", "--direction=Dual", "--blend=50" }, measureRate, 2);
            auto audio = tones ({ 196.0f, 246.94f, 293.66f }, (int) (measureRate * 10.0));
            const auto started = juce::Time::getMillisecondCounterHiRes();
            process (*processor, audio);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - started;
            std::cout << "CPU Dual 3 oct: " << juce::String (100.0 * ms / 10000.0, 2) << " % of one core (stereo)\n";
        }
        return 0;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (optionsOf (args), 48000.0, 2);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = args[1].resolveAsFile();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    const auto command = args.size() > 0 ? args[0].text : juce::String();
    if (command == "render" && args.size() >= 3)   return render (args);
    if (command == "measure")                      return measure();
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  PitchPotRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  PitchPotRender measure\n"
                 "  PitchPotRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
