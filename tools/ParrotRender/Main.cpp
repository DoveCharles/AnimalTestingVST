// Offline checks for Parrot, without a DAW.
//
//   ParrotRender parse "<lyrics>" [--spelling=Romaji]
//       Prints each syllable as the notes would sing it.
//
//   ParrotRender render <output.wav> [--lyrics="..."] [--melody=60:1,62:1,r:1] [--bpm=100] [--gap=0.1]
//                       [--vel=100] [--<param>=<value> ...]
//       Sings the melody (note:beats, "r" rests) and prints peak and RMS. --gap is the fraction of
//       each note left silent before the next; 0 plays legato (the next note overlaps slightly).
//
//   ParrotRender snapshot <output.png> [--lyrics="..."] [--<param>=<value> ...]
//       Saves a picture of the editor after singing two syllables.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double sampleRate = 48000.0;

    juce::String option (const juce::ArgumentList& args, const juce::String& name, const juce::String& fallback)
    {
        for (auto& arg : args.arguments)
            if (arg.text.startsWith ("--" + name + "="))
                return arg.text.fromFirstOccurrenceOf ("=", false, false).unquoted();
        return fallback;
    }

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption())
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            if (id == "lyrics" || id == "melody" || id == "bpm" || id == "gap" || id == "vel")
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
        processor->setLyrics (option (args, "lyrics", PluginProcessor::defaultLyrics));
        processor->prepareToPlay (sampleRate, blockSize);
        std::cout << "Latency " << processor->getLatencySamples() << " samples\n";
        return processor;
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

    int parse (const juce::ArgumentList& args)
    {
        const auto spelling = option (args, "spelling", "English").equalsIgnoreCase ("Romaji") ? parrot::Spelling::romaji
                                                                                              : parrot::Spelling::english;
        const auto text = args[1].text.unquoted();
        int i = 0;
        for (const auto& syl : parrot::parseLyrics (text, spelling))
            std::cout << juce::String (++i).paddedLeft (' ', 3) << "  "
                      << text.substring (syl.textStart, syl.textEnd).paddedRight (' ', 12) << "  "
                      << parrot::describe (syl) << "\n";
        return 0;
    }

    struct Note { int key; double beats; };

    int render (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args);

        std::vector<Note> melody;
        for (auto& token : juce::StringArray::fromTokens (option (args, "melody", "60:1,62:1,64:1,65:1,67:2"), ",", ""))
        {
            const auto t = token.trim();
            if (t.isEmpty())
                continue;
            const auto key = t.upToFirstOccurrenceOf (":", false, false);
            const auto beats = t.contains (":") ? t.fromFirstOccurrenceOf (":", false, false).getDoubleValue() : 1.0;
            melody.push_back ({ key.equalsIgnoreCase ("r") ? -1 : key.getIntValue(), beats });
        }

        const auto beat = 60.0 / option (args, "bpm", "100").getDoubleValue();
        const auto gap = option (args, "gap", "0.1").getDoubleValue();
        const auto vel = (juce::uint8) juce::jlimit (1, 127, option (args, "vel", "100").getIntValue());

        // Note-ons and note-offs in samples from the start, with a beat of silence first.
        juce::MidiBuffer score;
        double t = beat;
        for (const auto& n : melody)
        {
            const auto length = n.beats * beat;
            if (n.key >= 0)
            {
                const auto on = (int) (t * sampleRate);
                const auto off = gap > 0.0 ? (int) ((t + length * (1.0 - gap)) * sampleRate)
                                           : (int) ((t + length) * sampleRate) + 64;   // legato: overlap the next note-on
                score.addEvent (juce::MidiMessage::noteOn (1, n.key, vel), on);
                score.addEvent (juce::MidiMessage::noteOff (1, n.key), off);
            }
            t += length;
        }

        const auto total = (int) ((t + 1.0) * sampleRate) + processor->getLatencySamples();
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> block (2, blockSize);

        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += blockSize)
        {
            const auto n = juce::jmin (blockSize, total - pos);
            block.setSize (2, n, false, false, true);

            juce::MidiBuffer midi;
            for (const auto meta : score)
                if (meta.samplePosition >= pos && meta.samplePosition < pos + n)
                    midi.addEvent (meta.getMessage(), meta.samplePosition - pos);

            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        const auto seconds = total / sampleRate;

        std::cout << "Peak " << db (out.getMagnitude (0, total))
                  << "   RMS " << db (out.getRMSLevel (0, 0, total))
                  << "   CPU " << juce::String (100.0 * elapsed / seconds, 2) << "% of real time\n";

        // Level per beat-long window, to see where each syllable lands against the beat.
        const auto latency = processor->getLatencySamples();
        const auto window = (int) (beat * sampleRate / 4);
        juce::String line;
        for (int pos = latency; pos + window <= total; pos += window)
            line << juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (out.getRMSLevel (0, pos, window), -99.0f))).paddedLeft (' ', 4);
        std::cout << "RMS per quarter beat (latency removed):\n" << line << "\n";

        return writeWav (juce::File::getCurrentWorkingDirectory().getChildFile (args[1].text), out) ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args);
        juce::AudioBuffer<float> block (2, blockSize);
        for (int i = 0; i < (int) (1.5 * sampleRate) / blockSize; ++i)
        {
            juce::MidiBuffer midi;
            if (i == 4)  midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (i == 30) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            if (i == 40) midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            processor->processBlock (block, midi);
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

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

    if (command == "parse" && args.size() >= 2)    return parse (args);
    if (command == "render" && args.size() >= 2)   return render (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  ParrotRender parse \"<lyrics>\" [--spelling=Romaji]\n"
                 "  ParrotRender render <output.wav> [--lyrics=\"...\"] [--melody=60:1,62:1,r:1] [--bpm=100] [--gap=0.1] [--vel=100] [--<param>=<value> ...]\n"
                 "  ParrotRender snapshot <output.png> [--lyrics=\"...\"] [--<param>=<value> ...]\n";
    return 1;
}
