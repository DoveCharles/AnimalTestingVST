#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    void drawScrew (juce::Graphics& g, juce::Point<float> centre)
    {
        const auto head = juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d4), head.getTopLeft(),
                                                 juce::Colour (0xff7d7d78), head.getBottomRight(), false));
        g.fillEllipse (head);
        g.setColour (juce::Colour (0xff4a4a46));
        g.drawLine (centre.x - 4.0f, centre.y - 2.0f, centre.x + 4.0f, centre.y + 2.0f, 1.6f);
    }
}

void FootSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;

    // Hex nut, then the chrome plunger on top.
    juce::Path nut;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = centre.getPointOnCircumference (r, (float) i * juce::MathConstants<float>::twoPi / 6.0f);
        i == 0 ? nut.startNewSubPath (p) : nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcfca), bounds.getTopLeft(),
                                             juce::Colour (0xff6c6c68), bounds.getBottomRight(), false));
    g.fillPath (nut);

    const auto plunger = juce::Rectangle<float> (r * 1.15f, r * 1.15f).withCentre (centre.translated (0.0f, down ? 1.0f : 0.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, plunger.getTopLeft(),
                                             juce::Colour (0xff8f8f8a), plunger.getBottomRight(), false));
    g.fillEllipse (plunger);
    g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.5f : 0.35f));
    g.drawEllipse (plunger, 1.0f);
}

void ModeSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto bounds = getLocalBounds().toFloat();
    const auto custom = getToggleState();

    // S and C silkscreened either side of the bushing.
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    const auto sArea = bounds.removeFromLeft (20.0f);
    const auto cArea = bounds.removeFromRight (20.0f);
    g.setColour (Palette::ink.withAlpha (custom ? 0.45f : 1.0f));
    g.drawText ("S", sArea, juce::Justification::centred);
    g.setColour (Palette::ink.withAlpha (custom ? 1.0f : 0.45f));
    g.drawText ("C", cArea, juce::Justification::centred);

    // Chrome bushing with the bat lever leaning towards the selected side.
    const auto centre = bounds.getCentre();
    const auto nut = juce::Rectangle<float> (18.0f, 18.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0e0dc), nut.getTopLeft(),
                                             juce::Colour (0xff6c6c68), nut.getBottomRight(), false));
    g.fillEllipse (nut);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (nut, 1.0f);

    const auto tip = centre.translated (custom ? 14.0f : -14.0f, -3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawLine ({ centre.translated (0.0f, 2.0f), tip.translated (0.0f, 2.0f) }, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip,
                                             juce::Colour (0xff8f8f8a), centre, false));
    g.drawLine ({ centre, tip }, 5.0f);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tip));

    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillEllipse (nut.expanded (3.0f));
    }
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      level (p.apvts, ParamIDs::level, "Level"),
      tone  (p.apvts, ParamIDs::tone,  "Tone"),
      gain  (p.apvts, ParamIDs::gain,  "Gain"),
      modeAttachment (p.apvts, ParamIDs::custom, modeSwitch),
      footSwitchAttachment (p.apvts, ParamIDs::bypass, footSwitch)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &level, &tone, &gain })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::ink);
        knob->getLabel().setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (*knob);
    }

    modeSwitch.setTooltip ("Standard: the classic voicing. Custom: more low end, smoother and longer sustain.");
    addAndMakeVisible (modeSwitch);

    footSwitch.setTooltip ("Bypass");
    footSwitch.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (footSwitch);

    setSize (460, 430);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Gloss blue enclosure with a top-down sheen, and the base plate's shadow around it.
    g.fillAll (juce::Colour (0xff111827));
    const auto box = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.18f), box.getTopLeft(),
                                             Palette::enclosure.darker (0.2f), box.getBottomLeft(), false));
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (Palette::enclosureDark);
    g.drawRoundedRectangle (box, 10.0f, 2.0f);

    // Soft highlight along the top edge, like light catching the paint.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), box.getTopLeft(),
                                             juce::Colours::transparentWhite, box.getTopLeft().translated (0.0f, 60.0f), false));
    g.fillRoundedRectangle (box.withHeight (60.0f), 10.0f);

    for (auto corner : { box.getTopLeft().translated (14.0f, 14.0f), box.getTopRight().translated (-14.0f, 14.0f),
                         box.getBottomLeft().translated (14.0f, -14.0f), box.getBottomRight().translated (-14.0f, -14.0f) })
        drawScrew (g, corner);

    // Silkscreen: the name over a thin rule, the kind of pedal under it.
    auto title = titleArea.toFloat();
    g.setColour (Palette::ink);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold).withHorizontalScale (0.92f));
    g.drawText ("BLUE HEELER", title.removeFromTop (44.0f), juce::Justification::centred);
    g.fillRect (title.removeFromTop (2.0f).withSizeKeepingCentre (220.0f, 2.0f));
    title.removeFromTop (6.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("OVERDRIVE  \xc2\xb7  BLUES  \xc2\xb7  S / C")),
                title, juce::Justification::centredTop);

    // Status LED: lit while the effect is engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = ! footSwitch.getToggleState();
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.25f));
        g.fillEllipse (led.expanded (6.0f));
    }
    g.setColour (engaged ? Palette::ledOn : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (engaged ? 0.6f : 0.15f));
    g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-1.5f, -1.5f));
    g.setColour (juce::Colour (0xff9a9a95));
    g.drawEllipse (led.expanded (1.5f), 1.5f);

    // Maker's line at the foot.
    g.setColour (Palette::ink.withAlpha (0.75f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ANIMAL TESTING  v" JucePlugin_VersionString, getLocalBounds().removeFromBottom (34),
                juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (28, 22);

    auto knobRow = area.removeFromTop (130);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &level, &tone, &gain })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (6, 0));

    modeSwitch.setBounds (juce::Rectangle<int> (84, 30).withCentre ({ area.getCentreX(), area.getY() + 18 }));
    area.removeFromTop (36);

    titleArea = area.removeFromTop (76);
    area.removeFromTop (6);

    ledArea = juce::Rectangle<int> (14, 14).withCentre ({ area.getCentreX(), area.getY() + 8 });
    area.removeFromTop (24);
    footSwitch.setBounds (juce::Rectangle<int> (76, 76).withCentre ({ area.getCentreX(), area.getY() + 44 }));
}
