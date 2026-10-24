#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
      drive (p.apvts, ParamIDs::drive, "Drive"),
      tone  (p.apvts, ParamIDs::tone,  "Tone"),
      mix   (p.apvts, ParamIDs::mix,   "Mix")
{
    setLookAndFeel (&sharedLookAndFeel.get());

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &drive, &tone, &mix })
        addAndMakeVisible (c);

    setSize (420, 260);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Saturator", JucePlugin_VersionString);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (12);

    const auto knobWidth = area.getWidth() / 3;
    for (auto* knob : { &drive, &tone, &mix })
        knob->setBounds (area.removeFromLeft (knobWidth).reduced (4, 0));
}
