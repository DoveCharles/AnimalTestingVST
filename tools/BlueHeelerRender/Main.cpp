// Offline checks for Blue Heeler, without a DAW.
//
//   BlueHeelerRender measure [--<param>=<value> ...]
//       Plays a 220 Hz sine at input levels from -36 to 0 dBFS and prints the output level and the
//       2nd, 3rd and 5th harmonics, then the small-signal frequency response.
//
//   BlueHeelerRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them),
//       with a second of tail.
//
//   BlueHeelerRender snapshot <output.png> [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption())
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    std::unique_ptr<PluginProcessor> makeProcessor (const juce::ArgumentList& args, double sampleRate, int numInputs)
    {
        auto processor = std::make_unique<PluginProcessor>();
        processor->setPlayConfigDetails (numInputs, 2, sampleRate, blockSize);
        processor->setNonRealtime (true);
        applyParameters (*processor, args);
        processor->prepareToPlay (sampleRate, blockSize);
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

    /** Level of one frequency in a buffer, by correlating with a sine and cosine. */
    float toneLevel (const float* data, int numSamples, double frequency, double sampleRate)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < numSamples; ++n)
        {
            const auto phase = juce::MathConstants<double>::twoPi * frequency * n / sampleRate;
            re += data[n] * std::cos (phase);
            im += data[n] * std::sin (phase);
        }
        return (float) (2.0 * std::sqrt (re * re + im * im) / numSamples);
    }

    /** Plays a sine through a fresh processor and returns the settled second half (left channel). */
    juce::AudioBuffer<float> playSine (const juce::ArgumentList& args, double frequency, float amplitude)
    {
        constexpr double sampleRate = 48000.0;
        auto processor = makeProcessor (args, sampleRate, 2);

        const auto length = (int) sampleRate;
        juce::AudioBuffer<float> audio (2, length);
        for (int n = 0; n < length; ++n)
        {
            const auto v = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * n / sampleRate);
            audio.setSample (0, n, v);
            audio.setSample (1, n, v);
        }
        process (*processor, audio);

        juce::AudioBuffer<float> settled (1, length / 2);
        settled.copyFrom (0, 0, audio, 0, length / 2, length / 2);
        return settled;
    }

    int measure (const juce::ArgumentList& args)
    {
        constexpr double sampleRate = 48000.0;
        constexpr double fundamental = 220.0;

        std::cout << "Input     Output    Gain     H2       H3       H5   (harmonics relative to the fundamental)\n";
        for (auto inputDb = -36.0f; inputDb <= 0.0f; inputDb += 6.0f)
        {
            const auto amplitude = juce::Decibels::decibelsToGain (inputDb);
            const auto out = playSine (args, fundamental, amplitude);
            const auto* data = out.getReadPointer (0);
            const auto n = out.getNumSamples();

            const auto f1 = toneLevel (data, n, fundamental, sampleRate);
            auto rel = [&] (int h) { return juce::String (juce::Decibels::gainToDecibels (toneLevel (data, n, fundamental * h, sampleRate) / f1, -120.0f), 1); };
            const auto outDb = juce::Decibels::gainToDecibels (f1, -200.0f);

            std::cout << juce::String (inputDb, 0).paddedLeft (' ', 4) << " dB  "
                      << juce::String (outDb, 1).paddedLeft (' ', 6) << " dB  "
                      << juce::String (outDb - inputDb, 1).paddedLeft (' ', 5) << " dB  "
                      << rel (2).paddedLeft (' ', 6) << "   " << rel (3).paddedLeft (' ', 6) << "   " << rel (5).paddedLeft (' ', 6) << "\n";
        }

        std::cout << "\nSmall-signal response (-40 dBFS in), relative to 1 kHz:\n";
        const auto reference = toneLevel (playSine (args, 1000.0, 0.01f).getReadPointer (0), 24000, 1000.0, sampleRate);
        for (auto hz : { 40.0, 80.0, 160.0, 320.0, 640.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 })
        {
            const auto out = playSine (args, hz, 0.01f);
            const auto level = toneLevel (out.getReadPointer (0), out.getNumSamples(), hz, sampleRate);
            std::cout << juce::String (hz, 0).paddedLeft (' ', 6) << " Hz  "
                      << juce::String (juce::Decibels::gainToDecibels (level / reference, -120.0f), 1).paddedLeft (' ', 6) << " dB\n";
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

        juce::AudioBuffer<float> audio (2, length + (int) (reader->sampleRate * 1.0));
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);
        if (numInputs == 1)
            audio.clear (1, 0, audio.getNumSamples());

        process (*processor, audio);
        std::cout << "Peak " << db (audio.getMagnitude (0, audio.getNumSamples())) << "\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args, 48000.0, 1);
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
                 "  BlueHeelerRender measure [--<param>=<value> ...]\n"
                 "  BlueHeelerRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  BlueHeelerRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
