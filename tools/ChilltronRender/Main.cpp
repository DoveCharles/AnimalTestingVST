// Offline checks for Chilltron, without a DAW.
//
//   ChilltronRender render <output.wav> [--notes=48,52,55] [--hold=2] [--length=4] [--vel=100] [--<param>=<value> ...]
//       Holds the notes for --hold seconds, renders --length seconds, and prints peak and RMS.
//
//   ChilltronRender snapshot <output.png> [--notes=48,55] [--<param>=<value> ...]
//       Saves a picture of the editor, with the notes held for a second so the strips show.
//
// Tapes come from the same folder the plugin uses; both commands wait for them to load.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double sampleRate = 48000.0;

    juce::String option (const juce::ArgumentList& args, const juce::String& name, const juce::String& fallback)
    {
        for (auto& arg : args.arguments)
            if (arg.text.startsWith ("--" + name + "="))
                return arg.text.fromFirstOccurrenceOf ("=", false, false);
        return fallback;
    }

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption())
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            if (id == "notes" || id == "hold" || id == "length" || id == "vel")
                continue;

            const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    std::unique_ptr<PluginProcessor> makeProcessor (const juce::ArgumentList& args)
    {
        auto processor = std::make_unique<PluginProcessor>();
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        applyParameters (*processor, args);
        processor->prepareToPlay (sampleRate, blockSize);

        // One block asks the library for the chosen tapes; then wait for them.
        juce::AudioBuffer<float> scratch (2, blockSize);
        juce::MidiBuffer midi;
        processor->processBlock (scratch, midi);

        const auto a = (int) processor->apvts.getRawParameterValue (ParamIDs::tapeA)->load();
        const auto b = (int) processor->apvts.getRawParameterValue (ParamIDs::tapeB)->load();
        using Status = chilltron::TapeLibrary::Status;
        for (int i = 0; i < 300; ++i)
        {
            if (processor->library->getStatus (a) != Status::loading && processor->library->getStatus (b) != Status::loading)
                break;
            juce::Thread::sleep (50);
        }

        for (auto t : { a, b })
            std::cout << chilltron::tapeDisplayNames()[t] << ": "
                      << (processor->library->getStatus (t) == Status::ready ? "ready" : "MISSING") << "\n";
        return processor;
    }

    std::vector<int> notes (const juce::ArgumentList& args, const juce::String& fallback)
    {
        std::vector<int> result;
        for (auto& n : juce::StringArray::fromTokens (option (args, "notes", fallback), ",", ""))
            if (n.trim().isNotEmpty())
                result.push_back (n.getIntValue());
        return result;
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

    juce::String db (float gain) { return juce::String (juce::Decibels::gainToDecibels (gain, -200.0f), 1) + " dB"; }

    int render (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args);
        const auto keys = notes (args, "48,52,55,60");
        const auto hold = option (args, "hold", "2").getDoubleValue();
        const auto length = option (args, "length", "4").getDoubleValue();
        const auto vel = (juce::uint8) juce::jlimit (1, 127, option (args, "vel", "100").getIntValue());

        const auto total = (int) (length * sampleRate);
        const auto release = (int) (hold * sampleRate);
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> block (2, blockSize);

        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += blockSize)
        {
            const auto n = juce::jmin (blockSize, total - pos);
            block.setSize (2, n, false, false, true);

            juce::MidiBuffer midi;
            for (auto key : keys)
            {
                if (pos == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, key, vel), 0);
                if (release >= pos && release < pos + n)
                    midi.addEvent (juce::MidiMessage::noteOff (1, key), release - pos);
            }

            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;

        std::cout << "Peak " << db (out.getMagnitude (0, total))
                  << "   RMS (held) " << db (0.5f * (out.getRMSLevel (0, 0, juce::jmin (total, release))
                                               + out.getRMSLevel (1, 0, juce::jmin (total, release))))
                  << "   CPU " << juce::String (100.0 * elapsed / length, 2) << "% of real time\n";

        return writeWav (juce::File::getCurrentWorkingDirectory().getChildFile (args[1].text), out) ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;
        for (auto key : notes (args, ""))
            midi.addEvent (juce::MidiMessage::noteOn (1, key, (juce::uint8) 100), 0);
        for (int i = 0; i < (int) (1.2 * sampleRate) / blockSize; ++i)
        {
            processor->processBlock (block, midi);
            midi.clear();
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (args[1].text);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::ArgumentList args (argc, argv);
    const auto command = args.size() > 0 ? args[0].text : juce::String();

    if (command == "render" && args.size() >= 2)   return render (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  ChilltronRender render <output.wav> [--notes=48,52,55] [--hold=2] [--length=4] [--vel=100] [--<param>=<value> ...]\n"
                 "  ChilltronRender snapshot <output.png> [--notes=48,55] [--<param>=<value> ...]\n";
    return 1;
}
