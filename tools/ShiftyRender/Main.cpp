// Offline check for Shifty, without a DAW.
//
//   ShiftyRender snapshot <output.png> [--wait=300] [--<param>=<value> ...]
//       Saves a picture of the editor after letting its animation run for --wait milliseconds.

#include "PluginProcessor.h"

namespace
{
    int snapshot (const juce::ArgumentList& args)
    {
        PluginProcessor processor;
        auto wait = 300;

        for (int i = 2; i < args.size(); ++i)
        {
            const auto setting = args[i].text;
            const auto id = setting.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto value = setting.fromFirstOccurrenceOf ("=", false, false);

            if (id == "wait")
                wait = value.getIntValue();
            else if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (wait);

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

    if (args.size() >= 2 && args[0].text == "snapshot")
        return snapshot (args);

    std::cerr << "Usage:\n"
                 "  ShiftyRender snapshot <output.png> [--wait=300] [--<param>=<value> ...]\n";
    return 1;
}
