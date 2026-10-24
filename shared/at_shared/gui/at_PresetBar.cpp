namespace at
{
    PresetBar::PresetBar (PresetManager& pm) : manager (pm)
    {
        presetBox.setTextWhenNothingSelected ("Default");
        presetBox.onChange = [this]
        {
            if (const auto name = presetBox.getText(); name.isNotEmpty())
                manager.loadPreset (name);
        };
        addAndMakeVisible (presetBox);

        saveButton.onClick = [this] { savePreset(); };
        addAndMakeVisible (saveButton);

        initButton.onClick = [this]
        {
            manager.resetToDefaults();
            refresh();
        };
        addAndMakeVisible (initButton);

        refresh();
    }

    void PresetBar::resized()
    {
        auto r = getLocalBounds();
        initButton.setBounds (r.removeFromRight (52));
        r.removeFromRight (6);
        saveButton.setBounds (r.removeFromRight (52));
        r.removeFromRight (6);
        presetBox.setBounds (r);
    }

    void PresetBar::refresh()
    {
        presetBox.clear (juce::dontSendNotification);
        const auto names = manager.getPresetNames();
        for (int i = 0; i < names.size(); ++i)
            presetBox.addItem (names[i], i + 1);

        presetBox.setSelectedItemIndex (names.indexOf (manager.getCurrentPresetName()), juce::dontSendNotification);
    }

    void PresetBar::savePreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save preset", manager.getPresetDirectory(),
                                                       "*" + PresetManager::extension);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (const auto file = fc.getResult(); file != juce::File() && manager.savePreset (file))
                                      refresh();
                              });
    }
}
