#pragma once

namespace at
{
    /** Preset selector with Save and Init buttons, driven by at::PresetManager. */
    class PresetBar : public juce::Component
    {
    public:
        explicit PresetBar (PresetManager&);

        void resized() override;

    private:
        void refresh();
        void savePreset();

        PresetManager& manager;
        juce::ComboBox presetBox;
        juce::TextButton saveButton { "Save" }, initButton { "Init" };
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
    };
}
