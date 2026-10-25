// Offline checks for Cabinet, without a DAW.
//
//   CabinetRender render <input audio> <output.wav> [--ir=<file>] [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them).
//
//   CabinetRender ir <output.wav> [--rate=48000] [--ir=<file>] [--<param>=<value> ...]
//       Writes the stereo IR that the given settings build.
//
//   CabinetRender compare <reference IR> [--<param>=<value> ...]
//       Prints a built-in cab's response next to a reference IR's, both 1/6-octave smoothed and
//       levelled over 150 Hz - 4 kHz, plus the RMS difference over 80 Hz - 8 kHz.
//
//   CabinetRender snapshot <output.png> [--ir=<file>] [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption() || arg.text.startsWith ("--ir=") || arg.text.startsWith ("--rate="))
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    cab::Settings settingsFrom (PluginProcessor& processor)
    {
        const auto value = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        cab::Settings s;
        s.model    = (int) value (ParamIDs::model);
        s.mic      = (int) value (ParamIDs::mic);
        s.position = value (ParamIDs::position) / 100.0f;
        s.distance = value (ParamIDs::distance) / 100.0f;
        s.room     = value (ParamIDs::room) / 100.0f;
        return s;
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

    /** 1/6-octave smoothed magnitude in dB at the given frequencies. */
    std::vector<float> smoothedDb (const float* ir, int length, double sampleRate, const std::vector<float>& freqs)
    {
        const auto order = juce::jmax (15, (int) std::ceil (std::log2 ((double) length)));
        juce::dsp::FFT fft (order);
        const auto n = fft.getSize();
        std::vector<float> data ((size_t) n * 2, 0.0f);
        std::copy (ir, ir + juce::jmin (length, n), data.begin());
        fft.performFrequencyOnlyForwardTransform (data.data(), true);

        std::vector<float> result;
        for (auto hz : freqs)
        {
            const auto lo = juce::jlimit (1, n / 2, (int) std::floor (hz * std::pow (2.0f, -1.0f / 12.0f) * n / sampleRate));
            const auto hi = juce::jlimit (lo, n / 2, (int) std::ceil (hz * std::pow (2.0f, 1.0f / 12.0f) * n / sampleRate));
            auto power = 0.0f;
            for (int k = lo; k <= hi; ++k)
                power += data[(size_t) k] * data[(size_t) k];
            result.push_back (juce::Decibels::gainToDecibels (std::sqrt (power / (float) (hi - lo + 1)), -120.0f));
        }
        return result;
    }

    void levelOver (std::vector<float>& db, const std::vector<float>& freqs, float lo, float hi)
    {
        auto sum = 0.0f;
        auto count = 0;
        for (size_t i = 0; i < freqs.size(); ++i)
            if (freqs[i] >= lo && freqs[i] <= hi) { sum += db[i]; ++count; }
        for (auto& d : db)
            d -= sum / (float) juce::jmax (1, count);
    }

    std::shared_ptr<PluginProcessor> makeProcessor (const juce::ArgumentList& args, double sampleRate, int numChannels)
    {
        auto processor = std::make_shared<PluginProcessor>();
        const auto layout = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        processor->setPlayConfigDetails (numChannels, 2, sampleRate, blockSize);
        juce::ignoreUnused (layout);

        if (args.containsOption ("--ir"))
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (args.getValueForOption ("--ir"));
            if (! processor->loadImpulseFile (file))
                std::cerr << "Couldn't read IR: " << file.getFullPathName() << "\n";
        }

        applyParameters (*processor, args);
        return processor;
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
        const auto tail = (int) (reader->sampleRate * 0.5);

        auto processor = makeProcessor (args, reader->sampleRate, numInputs);
        processor->prepareToPlay (reader->sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, length + tail);
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, blockSize);
        for (int start = 0; start < audio.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, audio.getNumSamples() - start);
            block.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, audio, ch, start, n);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                audio.copyFrom (ch, start, block, ch, 0, n);
        }

        const auto peak = audio.getMagnitude (0, audio.getNumSamples());
        std::cout << "Peak " << juce::Decibels::toString (juce::Decibels::gainToDecibels (peak)) << "\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    int writeImpulse (const juce::ArgumentList& args)
    {
        const auto output = args[1].resolveAsFile();
        const auto rate = args.containsOption ("--rate") ? args.getValueForOption ("--rate").getDoubleValue() : 48000.0;
        auto processor = makeProcessor (args, rate, 1);

        std::optional<cab::UserImpulse> file;
        if (args.containsOption ("--ir"))
            file = cab::readImpulseFile (juce::File::getCurrentWorkingDirectory().getChildFile (args.getValueForOption ("--ir")));

        const auto ir = cab::buildImpulse (settingsFrom (*processor), rate, file ? &*file : nullptr);
        std::cout << "IR length " << ir.getNumSamples() << " samples\n";
        return writeWav (output, ir, rate) ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args, 48000.0, 1);
        processor->prepareToPlay (48000.0, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

        // Let the IR builder catch up with the parameters and the editor pick up its response.
        juce::Thread::sleep (300);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = args[1].resolveAsFile();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }

    int compare (const juce::ArgumentList& args)
    {
        const auto referenceFile = args[1].resolveAsExistingFile();
        auto reference = cab::readImpulseFile (referenceFile);
        if (! reference)
        {
            std::cerr << "Couldn't read " << referenceFile.getFullPathName() << "\n";
            return 1;
        }

        const auto rate = reference->sampleRate;
        auto processor = makeProcessor (args, rate, 1);
        const auto settings = settingsFrom (*processor);
        const auto built = cab::buildImpulse (settings, rate, nullptr);

        std::vector<float> freqs;
        for (auto hz = 40.0f; hz < 16000.0f; hz *= std::pow (2.0f, 1.0f / 6.0f))
            freqs.push_back (hz);

        auto ref = smoothedDb (reference->samples.getReadPointer (0), reference->samples.getNumSamples(), rate, freqs);
        auto ours = smoothedDb (built.getReadPointer (0), built.getNumSamples(), rate, freqs);
        levelOver (ref, freqs, 150.0f, 4000.0f);
        levelOver (ours, freqs, 150.0f, 4000.0f);

        auto squared = 0.0f;
        auto count = 0;
        std::cout << "     Hz    ref   ours   diff\n";
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            const auto diff = ours[i] - ref[i];
            std::cout << juce::String (freqs[i], 0).paddedLeft (' ', 7)
                      << juce::String (ref[i], 1).paddedLeft (' ', 7)
                      << juce::String (ours[i], 1).paddedLeft (' ', 7)
                      << juce::String (diff, 1).paddedLeft (' ', 7) << "  "
                      << juce::String::repeatedString (diff > 0 ? "+" : "-", juce::jmin (30, juce::roundToInt (std::abs (diff))))
                      << "\n";
            if (freqs[i] >= 80.0f && freqs[i] <= 8000.0f)
            {
                squared += diff * diff;
                ++count;
            }
        }
        std::cout << "RMS difference 80 Hz - 8 kHz: " << juce::String (std::sqrt (squared / (float) count), 2) << " dB\n";
        return 0;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    const auto command = args.size() > 0 ? args[0].text : juce::String();
    if (command == "render" && args.size() >= 3)  return render (args);
    if (command == "ir" && args.size() >= 2)      return writeImpulse (args);
    if (command == "compare" && args.size() >= 2) return compare (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  CabinetRender render <input> <output.wav> [--ir=<file>] [--<param>=<value> ...]\n"
                 "  CabinetRender ir <output.wav> [--rate=48000] [--ir=<file>] [--<param>=<value> ...]\n"
                 "  CabinetRender compare <reference IR> [--<param>=<value> ...]\n"
                 "  CabinetRender snapshot <output.png> [--ir=<file>] [--<param>=<value> ...]\n";
    return 1;
}
