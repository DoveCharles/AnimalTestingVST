#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshFont(); }
    void timerCallback() override;

    void refreshFont();
    void showPlaying();
    void step (int delta);
    void chooseFile();

    PluginProcessor& owner;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    at::PresetBar presetBar;
    juce::TextButton loadButton { "Load SF2..." };
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::ComboBox presetBox;
    at::Knob volume, transpose, tune, bendRange, velocity, release, reverb;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    magpie::FontInfo info;
    juce::Rectangle<int> fontArea;
    int shownPreset = -2, shownVoices = -1;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
