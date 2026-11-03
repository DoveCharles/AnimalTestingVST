// Saves a picture of a plugin's editor, for plugins that have no render tool of their own.
// Built once per plugin from that plugin's sources (see CMakeLists.txt), so the binary is named
// after it, e.g. SaturatorSnapshot.
//
//   <Name>Snapshot <output.png> [--wait=200] [--<param>=<value> ...]
//       Parameters by ID, values as the host shows them.

#include <juce_audio_processors/juce_audio_processors.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::ArgumentList args (argc, argv);

    if (args.size() < 1 || args[0].isOption())
    {
        std::cerr << "Usage: " << argv[0] << " <output.png> [--wait=200] [--<param>=<value> ...]\n";
        return 1;
    }

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (JucePlugin_IsSynth ? 0 : 2, 2, 48000.0, 512);
    processor->prepareToPlay (48000.0, 512);

    for (auto& arg : args.arguments)
    {
        if (! arg.isLongOption() || arg.text.startsWith ("--wait"))
            continue;

        const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
        const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
        auto found = false;
        for (auto* p : processor->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
            {
                ranged->setValueNotifyingHost (ranged->getValueForText (value));
                found = true;
            }
        if (! found)
            std::cerr << "Unknown parameter: " << id << "\n";
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (args.containsOption ("--wait") ? args.getValueForOption ("--wait").getIntValue() : 200);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto file = args[0].resolveAsFile();
    file.deleteFile();
    juce::FileOutputStream stream (file);
    return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
}
