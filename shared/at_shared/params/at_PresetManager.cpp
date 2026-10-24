namespace at
{
    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

    juce::File PresetManager::getPresetDirectory() const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile (JucePlugin_Manufacturer)
                       .getChildFile (JucePlugin_Name)
                       .getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    juce::StringArray PresetManager::getPresetNames() const
    {
        juce::StringArray names;
        for (const auto& f : getPresetDirectory().findChildFiles (juce::File::findFiles, false, "*" + extension))
            names.add (f.getFileNameWithoutExtension());
        names.sortNatural();
        return names;
    }

    bool PresetManager::savePreset (const juce::File& chosen)
    {
        const auto file = chosen.withFileExtension (extension);
        if (auto xml = state.copyState().createXml(); xml != nullptr && xml->writeTo (file))
        {
            currentPreset = file.getFileNameWithoutExtension();
            return true;
        }
        return false;
    }

    bool PresetManager::loadPreset (const juce::String& name)
    {
        const auto file = getPresetDirectory().getChildFile (name + extension);
        if (auto xml = juce::parseXML (file); xml != nullptr && xml->hasTagName (state.state.getType()))
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = name;
            return true;
        }
        return false;
    }

    void PresetManager::resetToDefaults()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
        currentPreset = {};
    }
}
