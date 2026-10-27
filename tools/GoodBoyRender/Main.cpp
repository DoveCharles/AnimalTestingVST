// Offline checks for Good Boy, without a DAW.
//
//   GoodBoyRender render <input audio> <output.wav> [--<param>=<value> ...]
//       Plays a file through the processor (all parameters by ID, values as the host shows them).
//
//   GoodBoyRender measure
//       Feeds a pulsing pinkish-noise "mix" through every mode at several Amounts and prints the
//       RMS and peak change, the change in low, mid and high level, how flat Amount 0 is and
//       the CPU cost.
//
//   GoodBoyRender snapshot <output.png> [--<param>=<value> ...]
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

    float toDb (float gain) { return juce::Decibels::gainToDecibels (gain, -200.0f); }
    juce::String db (float gain) { return juce::String (toDb (gain), 1) + " dB"; }
    juce::String signedDb (float value) { return (value >= 0.0f ? "+" : "") + juce::String (value, 1); }

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

        juce::AudioBuffer<float> audio (2, length + (int) (reader->sampleRate * 0.2));
        audio.clear();
        reader->read (&audio, 0, length, 0, true, numInputs > 1);
        if (numInputs == 1)
            audio.copyFrom (1, 0, audio, 0, 0, audio.getNumSamples());

        process (*processor, audio);
        std::cout << "Peak " << db (audio.getMagnitude (0, audio.getNumSamples())) << "\n";
        return writeWav (output, audio, reader->sampleRate) ? 0 : 1;
    }

    /** RMS of one channel through a band-pass made of two second-order filters. */
    float bandRms (const juce::AudioBuffer<float>& audio, int from, int length, float lowHz, float highHz)
    {
        juce::dsp::IIR::Filter<float> hp, lp;
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (measureRate, lowHz);
        lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (measureRate, highHz);
        double sum = 0.0;
        for (int n = 0; n < from + length; ++n)
        {
            const auto y = lp.processSample (hp.processSample (audio.getSample (0, n)));
            if (n >= from)
                sum += (double) y * y;
        }
        return (float) std::sqrt (sum / length);
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

    int measure()
    {
        const auto length = (int) (measureRate * 8.0);

        // Pinkish noise pulsing at 120 bpm eighths (a hit, then a decay), peaks around -2 dBFS.
        juce::AudioBuffer<float> dry (2, length);
        {
            juce::Random random (1234);
            juce::dsp::IIR::Filter<float> tilt;
            tilt.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (measureRate, 800.0f);
            const auto period = (int) (measureRate * 0.25);
            for (int n = 0; n < length; ++n)
            {
                const auto envelope = 0.25f + 0.75f * std::exp (-(float) (n % period) / (0.04f * (float) measureRate));
                const auto x = 1.9f * envelope * tilt.processSample (2.0f * random.nextFloat() - 1.0f);
                dry.setSample (0, n, x);
                dry.setSample (1, n, x);
            }
        }

        const auto from = length / 4, span = length / 2;
        const auto dryRms = dry.getRMSLevel (0, from, span), dryPeak = dry.getMagnitude (0, from, span);
        const std::array<std::pair<float, float>, 3> bands {{ { 30.0f, 150.0f }, { 400.0f, 2000.0f }, { 6000.0f, 16000.0f } }};
        std::array<float, 3> dryBands {};
        for (size_t b = 0; b < bands.size(); ++b)
            dryBands[b] = bandRms (dry, from, span, bands[b].first, bands[b].second);

        std::cout << "Input        RMS " << db (dryRms) << ", peak " << db (dryPeak) << "\n\n";
        std::cout << "Mode Amount   RMS    Peak    Low    Mid   High   (dB change; peak is absolute dBFS)\n";

        double totalMs = 0.0;
        int runs = 0;
        for (int mode = 0; mode < 4; ++mode)
        {
            for (auto amount : { 25, 50, 75, 100 })
            {
                auto processor = makeProcessor ({ "--mode=" + Modes::names[mode], "--amount=" + juce::String (amount) }, measureRate, 2);
                const auto latency = processor->getLatencySamples();
                auto wet = dry;
                const auto started = juce::Time::getMillisecondCounterHiRes();
                process (*processor, wet);
                totalMs += juce::Time::getMillisecondCounterHiRes() - started;
                ++runs;

                juce::String line;
                line << Modes::names[mode] << "    " << juce::String (amount).paddedLeft (' ', 3) << "%  "
                     << signedDb (toDb (wet.getRMSLevel (0, from + latency, span) / dryRms)).paddedLeft (' ', 6)
                     << juce::String (toDb (wet.getMagnitude (0, from + latency, span)), 1).paddedLeft (' ', 7);
                for (size_t b = 0; b < bands.size(); ++b)
                    line << signedDb (toDb (bandRms (wet, from + latency, span, bands[b].first, bands[b].second) / dryBands[b])).paddedLeft (' ', 7);
                std::cout << line << "\n";
            }
            std::cout << "\n";
        }

        // Amount 0 should only shift phase: same spectrum, same level.
        {
            auto processor = makeProcessor ({ "--amount=0" }, measureRate, 2);
            const auto latency = processor->getLatencySamples();
            auto wet = dry;
            process (*processor, wet);
            const auto before = averageSpectrum (dry, from, span), after = averageSpectrum (wet, from + latency, span);
            const auto binHz = measureRate / 4096.0;
            auto worst = 0.0f;
            for (auto b = (size_t) (40.0 / binHz); b <= (size_t) (16000.0 / binHz); ++b)
                worst = juce::jmax (worst, std::abs (after[b] - before[b]));
            std::cout << "Amount 0     " << juce::String (worst, 2) << " dB worst spectrum change 40 Hz - 16 kHz, RMS "
                      << signedDb (toDb (wet.getRMSLevel (0, from + latency, span) / dryRms)) << " dB, latency "
                      << latency << " samples\n";
        }

        const auto seconds = (double) length / measureRate;
        std::cout << "CPU          " << juce::String (100.0 * totalMs / runs / (1000.0 * seconds), 3) << " % of one core (stereo)\n";
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
                 "  GoodBoyRender render <input> <output.wav> [--<param>=<value> ...]\n"
                 "  GoodBoyRender measure\n"
                 "  GoodBoyRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
