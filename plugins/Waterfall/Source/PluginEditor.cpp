#include "PluginEditor.h"

namespace
{
    const juce::Colour accent { 0xff6fe3ff };   // icy cyan instead of the house orange, to sit on the blue visual
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      visual (p),
      key        (p.apvts, ParamIDs::key,        "Key"),
      chord      (p.apvts, ParamIDs::chord,      "Chord"),
      brightness (p.apvts, ParamIDs::brightness, "Brightness"),
      decay      (p.apvts, ParamIDs::decay,      "Decay"),
      mix        (p.apvts, ParamIDs::mix,        "Mix")
{
    setLookAndFeel (&sharedLookAndFeel.get());

    addAndMakeVisible (visual);
    addAndMakeVisible (controlsPanel);

    for (auto* knob : { &key, &chord, &brightness, &decay, &mix })
    {
        auto& s = knob->getSlider();
        s.setColour (juce::Slider::rotarySliderFillColourId, accent);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff1a2e5c));
        s.setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
        addAndMakeVisible (*knob);
    }

    setSize (600, 360);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::resized()
{
    // No header or preset bar: the spectrogram is the whole window, with the controls floating on it.
    auto area = getLocalBounds();
    visual.setBounds (area);

    area.reduce (14, 14);
    auto strip = area.removeFromBottom (128);
    controlsPanel.setBounds (strip);

    strip.reduce (8, 8);
    const auto knobWidth = strip.getWidth() / 5;
    for (auto* knob : { &key, &chord, &brightness, &decay, &mix })
        knob->setBounds (strip.removeFromLeft (knobWidth));
}
