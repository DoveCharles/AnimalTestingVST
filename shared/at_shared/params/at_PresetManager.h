#pragma once

namespace at
{
    /** File-based user presets stored as XML in
        ~/Documents/Animal Testing/<Plugin>/Presets (macOS) or Documents\Animal Testing\... (Windows). */
    class PresetManager
    {
    public:
        static inline const juce::String extension { ".atpreset" };

        explicit PresetManager (juce::AudioProcessorValueTreeState&);

        juce::File getPresetDirectory() const;
        juce::StringArray getPresetNames() const;

        bool savePreset (const juce::File&);
        bool loadPreset (const juce::String& name);
        void resetToDefaults();

        juce::String getCurrentPresetName() const { return currentPreset; }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::String currentPreset;
    };
}
