#include "PluginEditor.h"

namespace
{
    constexpr int sectionTitleHeight = 22;

    juce::String noteName (float hz)
    {
        if (hz <= 0.0f)
            return "--";
        const auto midi = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
        return juce::MidiMessage::getMidiNoteName (midi, true, true, 3);
    }
}

PluginEditor::Choice::Choice (PluginProcessor& p, const char* paramId, const juce::String& caption)
    : attachment (p.apvts, paramId, box)
{
    // The attachment fills the box from the parameter's choices only if they're added first.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (paramId)))
    {
        box.addItemList (choice->choices, 1);
        box.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }
    addAndMakeVisible (box);

    label.setText (caption.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, at::LookAndFeel::Palette::textDim);
    addAndMakeVisible (label);
}

void PluginEditor::Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (52));
    box.setBounds (r.reduced (0, 2));
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      larynx (p),
      presetBar (p.presets),
      mode     (p, ParamIDs::mode,     "Mode"),
      key      (p, ParamIDs::key,      "Key"),
      scale    (p, ParamIDs::scale,    "Scale"),
      pitch    (p.apvts, ParamIDs::pitch,    "Pitch"),
      formant  (p.apvts, ParamIDs::formant,  "Formant"),
      retune   (p.apvts, ParamIDs::retune,   "Retune"),
      gender   (p.apvts, ParamIDs::gender,   "Gender"),
      chest    (p.apvts, ParamIDs::chest,    "Chest Cut"),
      air      (p.apvts, ParamIDs::air,      "Air"),
      breath   (p.apvts, ParamIDs::breath,   "Breath"),
      drive    (p.apvts, ParamIDs::drive,    "Drive"),
      vibRate  (p.apvts, ParamIDs::vibRate,  "Rate"),
      vibDepth (p.apvts, ParamIDs::vibDepth, "Depth"),
      vibDelay (p.apvts, ParamIDs::vibDelay, "Delay"),
      mix      (p.apvts, ParamIDs::mix,      "Mix"),
      output   (p.apvts, ParamIDs::output,   "Output")
{
    setLookAndFeel (&sharedLookAndFeel.get());

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &mode, &key, &scale,
                                                             &pitch, &formant, &retune, &gender,
                                                             &chest, &air, &breath, &drive,
                                                             &vibRate, &vibDepth, &vibDelay, &mix, &output })
        addAndMakeVisible (c);

    setSize (720, 470);
    startTimerHz (15);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    const auto in = larynx.inputHz.load(), out = larynx.outputHz.load();
    const auto text = in > 0.0f ? "IN  " + noteName (in) + "   " + juce::String (in, 0) + " Hz\nOUT " + noteName (out) + "   " + juce::String (out, 0) + " Hz"
                                : juce::String ("IN  --\nOUT --");
    if (text != readout)
    {
        readout = text;
        repaint (readoutArea);
    }
}

void PluginEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    g.setColour (at::LookAndFeel::Palette::panel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (at::LookAndFeel::Palette::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), area.reduced (10, 0).removeFromTop (sectionTitleHeight),
                juce::Justification::centredLeft);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Larynx", JucePlugin_VersionString);

    drawSection (g, shiftArea, "Shift");

    g.setColour (at::LookAndFeel::Palette::textDim);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    g.drawFittedText (readout, readoutArea, juce::Justification::centredLeft, 2);
    drawSection (g, characterArea, "Character");
    drawSection (g, vibratoArea, "Vibrato");
    drawSection (g, outputArea, "Output");
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);

    const auto rowHeight = (area.getHeight() - 10) / 2;
    auto top = area.removeFromTop (rowHeight);
    area.removeFromTop (10);
    auto bottom = area;

    const auto layoutKnobs = [] (juce::Rectangle<int> r, std::initializer_list<at::Knob*> knobs)
    {
        const auto width = r.getWidth() / (int) knobs.size();
        for (auto* knob : knobs)
            knob->setBounds (r.removeFromLeft (width).reduced (2, 0));
    };

    // Shift: choices down the left, then the four main knobs.
    shiftArea = top;
    auto shift = shiftArea.reduced (8).withTrimmedTop (sectionTitleHeight - 6);
    auto choices = shift.removeFromLeft (190);
    const auto choiceHeight = juce::jmin (30, choices.getHeight() / 3);
    for (auto* choice : { &mode, &key, &scale })
        choice->setBounds (choices.removeFromTop (choiceHeight).withTrimmedRight (10));
    readoutArea = choices.withTrimmedTop (6).withTrimmedRight (10);
    layoutKnobs (shift, { &pitch, &formant, &retune, &gender });

    characterArea = bottom.removeFromLeft (bottom.getWidth() * 4 / 9);
    bottom.removeFromLeft (10);
    vibratoArea = bottom.removeFromLeft ((bottom.getWidth() - 10) * 3 / 5);
    bottom.removeFromLeft (10);
    outputArea = bottom;

    const auto inner = [] (juce::Rectangle<int> r) { return r.reduced (8).withTrimmedTop (sectionTitleHeight - 6); };
    layoutKnobs (inner (characterArea), { &chest, &air, &breath, &drive });
    layoutKnobs (inner (vibratoArea), { &vibRate, &vibDepth, &vibDelay });
    layoutKnobs (inner (outputArea), { &mix, &output });
}
