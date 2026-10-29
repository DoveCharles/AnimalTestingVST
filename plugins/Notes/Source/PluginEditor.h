#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::ChangeListener
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setFontSize (float);

    PluginProcessor& notes;
    juce::SharedResourcePointer<at::LookAndFeel> sharedLookAndFeel;

    juce::TextEditor page;
    juce::TextButton copy { "Copy" }, paste { "Paste" };
    juce::TextButton smaller { "A-" }, larger { "A+" }, backups { "Backups" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
