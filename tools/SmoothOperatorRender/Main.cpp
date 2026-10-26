// Offline checks for Smooth Operator, without a DAW.
//
//   SmoothOperatorRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them).
//
//   SmoothOperatorRender measure [--tone=3000] [--<param>=<value> ...]
//       Feeds pinkish noise with a +15 dB resonance at the tone frequency and prints how much the
//       resonance is cut, how much the rest of the spectrum moves, the null with Depth at 0 and the
//       CPU cost.
//
//   SmoothOperatorRender snapshot <output.png> [--<param>=<value> ...]
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

    /** Average power spectrum (dB) of one channel over [from, from + length), 4096-point Hann frames. */
    std::vector<float> averageSpectrum (const juce::AudioBuffer<float>& audio, int from, int length)
    {
        constexpr int order = 12;
        constexpr int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<double> sum ((size_t) size / 2, 0.0);
        std::vector<float> data ((size_t) size * 2);
        int frames = 0;
        for (int start = from; start + size <= from + length; start += size / 2, ++frames)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            for (int n = 0; n < size; ++n)
                data[(size_t) n] = audio.getSample (0, start + n) * 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) size));
            fft.performFrequencyOnlyForwardTransform (data.data(), true);
            for (int b = 0; b < size / 2; ++b)
                sum[(size_t) b] += (double) data[(size_t) b] * data[(size_t) b];
        }
        std::vector<float> out ((size_t) size / 2);
        for (int b = 0; b < size / 2; ++b)
            out[(size_t) b] = (float) (10.0 * std::log10 (sum[(size_t) b] / juce::jmax (1, frames) + 1e-20));
        return out;
    }

    int measure (const juce::ArgumentList& args)
    {
        const auto toneHz = args.containsOption ("--tone") ? args.getValueForOption ("--tone").getDoubleValue() : 3000.0;
        const auto length = (int) (measureRate * 8.0);

        // Pinkish noise (white through a gentle low-pass) with a narrow +15 dB peak on top.
        juce::AudioBuffer<float> dry (2, length);
        {
            juce::Random random (1234);
            juce::dsp::IIR::Filter<float> tilt, peak;
            tilt.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (measureRate, 1500.0f);
            peak.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (measureRate, (float) toneHz, 8.0f,
                                                                                     juce::Decibels::decibelsToGain (15.0f));
            for (int n = 0; n < length; ++n)
            {
                const auto x = 0.2f * peak.processSample (tilt.processSample (2.0f * random.nextFloat() - 1.0f));
                dry.setSample (0, n, x);
                dry.setSample (1, n, x);
            }
        }

        auto processor = makeProcessor (args, measureRate, 2);
        const auto latency = processor->getLatencySamples();
        auto wet = dry;
        const auto started = juce::Time::getMillisecondCounterHiRes();
        process (*processor, wet);
        const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - started;

        // Compare the last 4 s, wet shifted back by the latency.
        const auto from = length / 2, span = length / 2 - latency - 1;
        juce::AudioBuffer<float> aligned (1, span);
        aligned.copyFrom (0, 0, wet, 0, from + latency, span);
        juce::AudioBuffer<float> reference (1, span);
        reference.copyFrom (0, 0, dry, 0, from, span);
        const auto before = averageSpectrum (reference, 0, span), after = averageSpectrum (aligned, 0, span);

        const auto binHz = measureRate / 4096.0;
        const auto change = [&] (double lo, double hi, bool peakOnly)
        {
            std::vector<float> diffs;
            for (auto b = (size_t) (lo / binHz); b <= (size_t) (hi / binHz) && b < before.size(); ++b)
                diffs.push_back (after[b] - before[b]);
            if (diffs.empty())
                return 0.0f;
            if (peakOnly)
                return *std::min_element (diffs.begin(), diffs.end());
            std::sort (diffs.begin(), diffs.end());
            return diffs[diffs.size() / 2];
        };
        const auto dbText = [] (float v) { return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };

        std::cout << "Latency      " << latency << " samples (" << juce::String (1000.0 * latency / measureRate, 1) << " ms)\n";
        std::cout << "Resonance    " << dbText (change (toneHz / 1.03, toneHz * 1.03, true)) << " at " << juce::String (toneHz, 0) << " Hz\n";
        std::cout << "Elsewhere    " << dbText (change (200.0, toneHz / 1.5, false)) << " median below, "
                  << dbText (change (toneHz * 1.5, 16000.0, false)) << " median above\n";
        std::cout << "Overall      " << dbText (juce::Decibels::gainToDecibels (wet.getRMSLevel (0, from + latency, span) / dry.getRMSLevel (0, from, span))) << " RMS\n";

        // Null with nothing to do: Depth 0 should give back the input, delayed.
        {
            auto nullArgs = args;
            nullArgs.arguments.add ({ "--depth=0" });
            auto flat = makeProcessor (nullArgs, measureRate, 2);
            auto copy = dry;
            process (*flat, copy);
            auto worst = 0.0f;
            for (int n = from; n < from + span; ++n)
                worst = juce::jmax (worst, std::abs (copy.getSample (0, n + latency) - dry.getSample (0, n)));
            std::cout << "Null         " << db (worst / dry.getMagnitude (0, from, span)) << " peak re input, Depth 0\n";
        }

        std::cout << "CPU          " << juce::String (100.0 * elapsedMs / (1000.0 * length / measureRate), 3) << " % of one core (stereo)\n";
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
                 "  SmoothOperatorRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  SmoothOperatorRender measure [--tone=3000] [--<param>=<value> ...]\n"
                 "  SmoothOperatorRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
