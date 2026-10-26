// Offline checks for Sky Chorus, without a DAW.
//
//   SkyChorusRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them).
//
//   SkyChorusRender measure [--tone=1000] [--<param>=<value> ...]
//       Feeds a -12 dBFS sine through the wet path alone (Mix 100, Mono-Safe unless overridden) and
//       prints the pitch sweep in cents, the strongest harmonics and spurs, the hiss floor and the
//       CPU cost.
//
//   SkyChorusRender snapshot <output.png> [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 512;
    constexpr double measureRate = 48000.0;

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption() || arg.text.startsWith ("--tone="))
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

        juce::AudioBuffer<float> audio (2, length + (int) (reader->sampleRate * 0.2));
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);
        if (numInputs == 1)
            audio.clear (1, 0, audio.getNumSamples());

        process (*processor, audio);
        std::cout << "Peak " << db (audio.getMagnitude (0, audio.getNumSamples())) << "\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    int measure (const juce::ArgumentList& args)
    {
        auto withDefaults = args;
        if (! args.containsOption ("--mix"))    withDefaults.arguments.add ({ "--mix=100" });
        if (! args.containsOption ("--stereo")) withDefaults.arguments.add ({ "--stereo=Mono-Safe" });
        const auto toneHz = args.containsOption ("--tone") ? args.getValueForOption ("--tone").getDoubleValue() : 1000.0;

        // Sine: 8 s, analysed over the last 4 so the sweep has settled.
        auto processor = makeProcessor (withDefaults, measureRate, 1);
        const auto length = (int) (measureRate * 8.0);
        juce::AudioBuffer<float> audio (2, length);
        for (int n = 0; n < length; ++n)
            audio.setSample (0, n, 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * n / measureRate));
        audio.clear (1, 0, length);

        const auto started = juce::Time::getMillisecondCounterHiRes();
        process (*processor, audio);
        const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - started;
        const auto* wet = audio.getReadPointer (0);
        const auto from = length / 2;

        // Pitch: upward zero crossings, averaged over 20 periods to smooth out the hold-step jitter.
        std::vector<double> crossings;
        for (int n = from + 1; n < length; ++n)
            if (wet[n - 1] < 0.0f && wet[n] >= 0.0f)
                crossings.push_back (n - 1 + wet[n - 1] / (double) (wet[n - 1] - wet[n]));

        auto lowest = 1e9, highest = 0.0;
        for (size_t i = 20; i < crossings.size(); ++i)
        {
            const auto hz = 20.0 * measureRate / (crossings[i] - crossings[i - 20]);
            lowest = juce::jmin (lowest, hz);
            highest = juce::jmax (highest, hz);
        }
        const auto cents = [&] (double hz) { return 1200.0 * std::log2 (hz / toneHz); };
        std::cout << "Pitch sweep  " << juce::String (cents (lowest), 1) << " to +" << juce::String (cents (highest), 1) << " cents\n";

        // Spectrum of the last 2^16 samples, Hann windowed.
        constexpr int order = 16;
        constexpr int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        for (int n = 0; n < size; ++n)
            data[(size_t) n] = wet[length - size + n] * 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) size));
        fft.performFrequencyOnlyForwardTransform (data.data(), true);

        const auto binHz = measureRate / size;
        const auto scale = 4.0f / (float) size;   // Hann's coherent gain is 0.5
        const auto peakNear = [&] (double hz, double span)
        {
            auto peak = 0.0f;
            for (auto b = (int) ((hz - span) / binHz); b <= (int) ((hz + span) / binHz); ++b)
                if (b > 0 && b < size / 2)
                    peak = juce::jmax (peak, data[(size_t) b] * scale);
            return peak;
        };

        const auto fundamental = peakNear (toneHz, 60.0);
        std::cout << "Fundamental  " << db (fundamental) << " (input -12.0 dB)\n";
        for (int h = 2; h <= 3; ++h)
            if (toneHz * h < measureRate * 0.5)
                std::cout << "Harmonic " << h << "   " << db (peakNear (toneHz * h, 60.0) / fundamental) << " re fundamental\n";

        // Strongest component that isn't the tone or one of its harmonics: aliasing and clock images.
        auto spur = 0.0f;
        auto spurHz = 0.0;
        for (int b = (int) (40.0 / binHz); b < size / 2; ++b)
        {
            const auto hz = b * binHz;
            const auto nearest = std::round (hz / toneHz) * toneHz;
            if (nearest > 0.0 && std::abs (hz - nearest) < 80.0)
                continue;
            if (data[(size_t) b] * scale > spur)
            {
                spur = data[(size_t) b] * scale;
                spurHz = hz;
            }
        }
        std::cout << "Worst spur   " << db (spur / fundamental) << " re fundamental, at " << juce::String (spurHz, 0) << " Hz\n";

        // Hiss with no input.
        auto quiet = makeProcessor (withDefaults, measureRate, 1);
        juce::AudioBuffer<float> silence (2, (int) measureRate * 2);
        silence.clear();
        process (*quiet, silence);
        std::cout << "Hiss         " << db (silence.getRMSLevel (0, (int) measureRate, (int) measureRate)) << " RMS\n";

        std::cout << "CPU          " << juce::String (100.0 * elapsedMs / (1000.0 * length / measureRate), 3) << " % of one core\n";
        return 0;
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
    if (command == "measure")                      return measure (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  SkyChorusRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  SkyChorusRender measure [--tone=1000] [--<param>=<value> ...]\n"
                 "  SkyChorusRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
