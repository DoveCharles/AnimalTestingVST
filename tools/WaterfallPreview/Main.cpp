// Renders the Waterfall editor offline while an audio file plays through the processor, and saves
// PNG snapshots of it. Lets you check the spectrogram with real audio without a DAW. --wav also saves
// the processed audio as waterfall.wav in the output folder.
//
//   WaterfallPreview <audio file> <output folder> [--every=5] [--length=60] [--wav] [--key=E] [--chord=Add9]
//                    [--brightness=86] [--decay=10] [--mix=100]

#include "PluginProcessor.h"
#include "SpectrogramVisual.h"

namespace
{
    constexpr double framesPerSecond = 60.0;
    constexpr int blockSize = 512;

    void applyParameter (PluginProcessor& processor, const juce::String& id, const juce::String& text)
    {
        auto* param = processor.apvts.getParameter (id);
        if (param == nullptr)
        {
            std::cerr << "Unknown parameter: " << id << "\n";
            return;
        }

        param->setValueNotifyingHost (param->getValueForText (text));
        std::cout << id << " = " << param->getCurrentValueAsText() << "\n";
    }

    SpectrogramVisual* findVisual (juce::Component& editor)
    {
        for (auto* child : editor.getChildren())
            if (auto* visual = dynamic_cast<SpectrogramVisual*> (child))
                return visual;

        return nullptr;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    if (args.size() < 2)
    {
        std::cerr << "Usage: WaterfallPreview <audio file> <output folder> [--every=5] [--length=60] [--<param>=<value>]\n";
        return 1;
    }

    const auto input = args[0].resolveAsExistingFile();
    const auto outputDir = args[1].resolveAsFile();
    outputDir.createDirectory();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (input));
    if (reader == nullptr)
    {
        std::cerr << "Can't read " << input.getFullPathName() << "\n";
        return 1;
    }

    const auto sampleRate = reader->sampleRate;
    const auto every = args.getValueForOption ("--every").getDoubleValue();
    const auto snapshotEvery = every > 0.0 ? every : 5.0;
    const auto lengthOption = args.getValueForOption ("--length").getDoubleValue();
    const auto fileLength = (double) reader->lengthInSamples / sampleRate;
    const auto length = lengthOption > 0.0 ? juce::jmin (lengthOption, fileLength) : fileLength;

    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    for (auto* id : { ParamIDs::key, ParamIDs::chord, ParamIDs::brightness, ParamIDs::decay, ParamIDs::mix })
        if (args.containsOption ("--" + juce::String (id)))
            applyParameter (processor, id, args.getValueForOption ("--" + juce::String (id)));

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    auto* visual = findVisual (*editor);
    jassert (visual != nullptr);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::AudioBuffer<float> rendered (2, args.containsOption ("--wav") ? (int) (length * sampleRate) + blockSize : 0);
    juce::MidiBuffer midi;
    juce::int64 position = 0;
    auto nextSnapshot = snapshotEvery;

    for (int frame = 1; frame / framesPerSecond <= length; ++frame)
    {
        const auto frameEnd = (juce::int64) (frame / framesPerSecond * sampleRate);

        while (position < frameEnd)
        {
            const auto n = (int) juce::jmin ((juce::int64) blockSize, frameEnd - position);
            buffer.setSize (2, n, false, false, true);
            reader->read (&buffer, 0, n, position, true, true);
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                if (position + n <= rendered.getNumSamples())
                    rendered.copyFrom (ch, (int) position, buffer, ch, 0, n);
            position += n;
        }

        const auto t = frame / framesPerSecond;
        visual->tick (t);

        if (t >= nextSnapshot)
        {
            const auto file = outputDir.getChildFile (juce::String::formatted ("waterfall_%05.1fs.png", t));
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            juce::FileOutputStream out (file);
            out.setPosition (0);
            out.truncate();
            juce::PNGImageFormat().writeImageToStream (image, out);
            std::cout << "Wrote " << file.getFullPathName() << "\n";
            nextSnapshot += snapshotEvery;
        }
    }

    if (rendered.getNumSamples() > 0)
    {
        const auto file = outputDir.getChildFile ("waterfall.wav");
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24);
        if (auto writer = juce::WavAudioFormat().createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (rendered, 0, (int) juce::jmin ((juce::int64) rendered.getNumSamples(), position));
        std::cout << "Wrote " << file.getFullPathName() << "\n";
    }

    editor.reset();
    processor.releaseResources();
    return 0;
}
