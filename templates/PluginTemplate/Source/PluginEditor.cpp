#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
      gain (p.apvts, ParamIDs::gain, "Gain")
{
    setLookAndFeel (&sharedLookAndFeel.get());

    addAndMakeVisible (presetBar);
    addAndMakeVisible (gain);

    setSize (320, 260);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "PluginTemplate", JucePlugin_VersionString);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (12);

    gain.setBounds (area.withSizeKeepingCentre (120, area.getHeight()));
}
