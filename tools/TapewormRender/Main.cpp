// Offline checks for Tapeworm, without a DAW.
//
//   TapewormRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them),
//       with a second of tail.
//
//   TapewormRender snapshot <output.png> [--<param>=<value> ...]
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
    if (command == "render" && args.size() >= 3)   return render (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  TapewormRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  TapewormRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
