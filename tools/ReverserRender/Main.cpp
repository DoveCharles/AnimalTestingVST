// Offline checks for Reverser, without a DAW.
//
//   ReverserRender measure [--<param>=<value> ...]
//       For each mode: the envelope of a drum-like hit (which should come back backwards), the
//       wet level on noise, how Repeats at full dies away, and CPU use.
//
//   ReverserRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them),
//       with three seconds of tail.
//
//   ReverserRender snapshot <output.png> [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double sampleRate = 48000.0;

    void setParameter (PluginProcessor& processor, const juce::String& id, const juce::String& value)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->getValueForText (value));
        else
            std::cerr << "Unknown parameter: " << id << "\n";
    }

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
            if (arg.isLongOption())
                setParameter (processor,
                              arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false),
                              arg.text.fromFirstOccurrenceOf ("=", false, false));
    }

    /** A processor with the given overrides, then the command line's on top. */
    std::unique_ptr<PluginProcessor> makeProcessor (const juce::ArgumentList& args, double rate, int numInputs,
                                                    std::initializer_list<std::pair<const char*, const char*>> overrides = {})
    {
        auto processor = std::make_unique<PluginProcessor>();
        processor->setPlayConfigDetails (numInputs, 2, rate, blockSize);
        processor->setNonRealtime (true);
        for (auto& [id, value] : overrides)
            setParameter (*processor, id, value);
        applyParameters (*processor, args);
        processor->prepareToPlay (rate, blockSize);
        return processor;
    }

    void process (PluginProcessor& processor, juce::AudioBuffer<float>& audio)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, blockSize);
        for (int start = 0; start < audio.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, audio.getNumSamples() - start);
            block.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, audio, ch, start, n);
            processor.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                audio.copyFrom (ch, start, block, ch, 0, n);
        }
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

    float toDb (float gain) { return juce::Decibels::gainToDecibels (gain, -120.0f); }

    /** Seeded white noise into both channels of [start, start + length), shaped by envelope (0 to 1). */
    template <typename Envelope>
    void addNoise (juce::AudioBuffer<float>& audio, int start, int length, Envelope envelope)
    {
        juce::Random random (1234);
        for (int n = 0; n < length && start + n < audio.getNumSamples(); ++n)
        {
            const auto v = (random.nextFloat() * 2.0f - 1.0f) * envelope ((float) n / (float) length);
            audio.addSample (0, start + n, v);
            audio.addSample (1, start + n, v);
        }
    }

    float rms (const juce::AudioBuffer<float>& audio, int start, int length)
    {
        length = juce::jmin (length, audio.getNumSamples() - start);
        if (length <= 0)
            return 0.0f;
        const auto l = audio.getRMSLevel (0, start, length), r = audio.getRMSLevel (1, start, length);
        return std::sqrt (0.5f * (l * l + r * r));
    }

    void printEnvelope (const juce::AudioBuffer<float>& audio, int frames, int frameLength)
    {
        for (int f = 0; f < frames; ++f)
        {
            const auto level = toDb (rms (audio, f * frameLength, frameLength));
            const auto bars = juce::jlimit (0, 40, (int) ((level + 48.0f) * 40.0f / 48.0f));
            std::cout << "    " << juce::String (f * frameLength * 1000.0 / sampleRate, 0).paddedLeft (' ', 5) << " ms "
                      << juce::String (level, 1).paddedLeft (' ', 6) << " dB  " << juce::String::repeatedString ("#", bars) << "\n";
        }
    }

    int measure (const juce::ArgumentList& args)
    {
        for (int mode = 0; mode < Modes::names.size(); ++mode)
        {
            const auto modeName = Modes::names[mode].toRawUTF8();
            std::cout << "== " << modeName << " ==\n";

            // A snare-like hit: sharp attack, 80 ms decay. Wet only, so the envelope is the effect alone.
            {
                auto processor = makeProcessor (args, sampleRate, 2, { { "mode", modeName }, { "mix", "100" },
                                                                       { "time", "0.5" }, { "repeats", "0" } });
                juce::AudioBuffer<float> audio (2, (int) (1.6 * sampleRate));
                audio.clear();
                addNoise (audio, 0, (int) (0.4 * sampleRate), [] (float t) { return std::exp (-t * 0.4f / 0.08f); });
                process (*processor, audio);
                std::cout << "  Hit at 0 ms, Time 500 ms, Mix 100%:\n";
                printEnvelope (audio, 16, (int) (0.1 * sampleRate));
            }

            // Wet level on steady noise, relative to the input.
            {
                auto processor = makeProcessor (args, sampleRate, 2, { { "mode", modeName }, { "mix", "100" }, { "repeats", "0" } });
                juce::AudioBuffer<float> audio (2, (int) (6.0 * sampleRate));
                audio.clear();
                addNoise (audio, 0, audio.getNumSamples(), [] (float) { return 0.25f; });
                const auto in = rms (audio, (int) (3.0 * sampleRate), (int) (3.0 * sampleRate));
                process (*processor, audio);
                const auto out = rms (audio, (int) (3.0 * sampleRate), (int) (3.0 * sampleRate));
                std::cout << "  Wet level on noise: " << juce::String (toDb (out / in), 1) << " dB\n";
            }

            // Repeats at full: a burst of noise, then silence; the level per two seconds should fall.
            for (auto* time : { "0.05", "0.5", "2.0" })
            {
                auto processor = makeProcessor (args, sampleRate, 2, { { "mode", modeName }, { "mix", "100" },
                                                                       { "time", time }, { "repeats", "100" } });
                juce::AudioBuffer<float> audio (2, (int) (20.0 * sampleRate));
                audio.clear();
                addNoise (audio, 0, (int) (0.5 * sampleRate), [] (float) { return 0.7f; });
                process (*processor, audio);
                std::cout << "  Repeats 100%, Time " << time << " s, peak per 4 s:";
                for (int s = 0; s < 20; s += 4)
                    std::cout << "  " << juce::String (toDb (audio.getMagnitude ((int) (s * sampleRate), (int) (4.0 * sampleRate))), 1);
                std::cout << " dB\n";
            }

            // CPU: ten seconds of stereo noise.
            {
                auto processor = makeProcessor (args, sampleRate, 2, { { "mode", modeName }, { "repeats", "50" } });
                juce::AudioBuffer<float> audio (2, (int) (10.0 * sampleRate));
                audio.clear();
                addNoise (audio, 0, audio.getNumSamples(), [] (float) { return 0.25f; });
                const auto start = juce::Time::getMillisecondCounterHiRes();
                process (*processor, audio);
                const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
                std::cout << "  CPU: " << juce::String (ms / 100.0, 2) << "% of real time\n\n";
            }
        }
        return 0;
    }

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
        auto processor = makeProcessor (args, reader->sampleRate, numInputs);

        juce::AudioBuffer<float> audio (2, length + (int) (reader->sampleRate * 3.0));
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);
        if (numInputs == 1)
            audio.clear (1, 0, audio.getNumSamples());

        process (*processor, audio);
        std::cout << "Peak " << juce::String (toDb (audio.getMagnitude (0, audio.getNumSamples())), 1) << " dB\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args, sampleRate, 2);
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
    if (command == "measure")                      return measure (args);
    if (command == "render" && args.size() >= 3)   return render (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  ReverserRender measure [--<param>=<value> ...]\n"
                 "  ReverserRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  ReverserRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
