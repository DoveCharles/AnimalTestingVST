#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
      voicingAttachment (p.apvts, ParamIDs::voicing, voicing),
      hardness (p.apvts, ParamIDs::hardness, "Hardness"),
      unison   (p.apvts, ParamIDs::unison, "Unison"),
      decay    (p.apvts, ParamIDs::decay, "Decay"),
      body     (p.apvts, ParamIDs::body, "Body"),
      volume   (p.apvts, ParamIDs::volume, "Volume"),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&sharedLookAndFeel.get());

    // The attachment was made before the items existed; add them and let it resync.
    voicing.addItemList (p.apvts.getParameter (ParamIDs::voicing)->getAllValueStrings(), 1);
    voicing.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (ParamIDs::voicing)->load(), juce::dontSendNotification);

    voicingLabel.setText ("VOICING", juce::dontSendNotification);
    voicingLabel.setJustificationType (juce::Justification::centredRight);
    voicingLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    voicingLabel.setColour (juce::Label::textColourId, at::LookAndFeel::Palette::textDim);

    keyboard.setAvailableRange (piano::lowestNote, piano::highestNote);
    keyboard.setKeyWidth (14.0f);
    keyboard.setLowestVisibleKey (36);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (voicingLabel);
    addAndMakeVisible (voicing);
    for (auto* k : { &hardness, &unison, &decay, &body, &volume })
        addAndMakeVisible (*k);
    addAndMakeVisible (keyboard);

    setSize (620, 380);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Upright", JucePlugin_VersionString);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);

    auto voicingRow = area.removeFromTop (26);
    voicing.setBounds (voicingRow.removeFromRight (140));
    voicingLabel.setBounds (voicingRow.removeFromRight (80));
    area.removeFromTop (6);

    keyboard.setBounds (area.removeFromBottom (90));
    area.removeFromBottom (10);

    const auto knobWidth = area.getWidth() / 5;
    for (auto* k : { &hardness, &unison, &decay, &body, &volume })
        k->setBounds (area.removeFromLeft (knobWidth));
}
