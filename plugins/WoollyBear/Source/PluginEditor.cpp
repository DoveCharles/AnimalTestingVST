#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;
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

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      volume  (p.apvts, ParamIDs::volume,  "Volume"),
      tone    (p.apvts, ParamIDs::tone,    "Tone"),
      sustain (p.apvts, ParamIDs::sustain, "Sustain"),
      footSwitchAttachment (p.apvts, ParamIDs::bypass, footSwitch)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &volume, &tone, &sustain })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (*knob);
    }

    footSwitch.setTooltip ("Bypass");
    footSwitch.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (footSwitch);

    setSize (460, 400);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::renderFur()
{
    fur = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (fur);

    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0b0807));

    juce::Path body;
    const auto box = bounds.reduced (6.0f);
    body.addRoundedRectangle (box, 22.0f);
    g.reduceClipRegion (body);

    // The rust band sits behind the title; black bristle runs to both ends.
    const auto band = titleArea.toFloat().withTrimmedTop (6.0f).withTrimmedBottom (-12.0f);
    g.setColour (Palette::furBlack);
    g.fillPath (body);
    g.setColour (Palette::furRust.darker (0.35f));
    g.fillRect (band.withX (box.getX()).withWidth (box.getWidth()));

    // Body segments, faint rings the bristle mostly covers.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (auto y = box.getY() + 26.0f; y < box.getBottom(); y += 30.0f)
        g.drawLine (box.getX(), y, box.getRight(), y, 2.0f);

    // Bristles splay out from the spine, each band blurring into the next. Seeded so the
    // pattern is the same every time.
    juce::Random rng (0x7a11);
    const auto spineX = box.getCentreX();
    const auto count = (int) (box.getWidth() * box.getHeight() / 5.0f);
    for (int i = 0; i < count; ++i)
    {
        const auto x = box.getX() + rng.nextFloat() * box.getWidth();
        const auto y = box.getY() + rng.nextFloat() * box.getHeight();
        const auto jitter = (rng.nextFloat() + rng.nextFloat() - 1.0f) * 10.0f;
        const auto rust = y + jitter > band.getY() && y + jitter < band.getBottom();

        auto colour = rust ? Palette::furRust.withMultipliedBrightness (0.65f + rng.nextFloat() * 0.6f)
                                             .withRotatedHue ((rng.nextFloat() - 0.5f) * 0.03f)
                           : Palette::furBlack.withMultipliedBrightness (0.6f + rng.nextFloat() * (rng.nextInt (14) == 0 ? 3.0f : 1.2f));

        const auto side = (x - spineX) / (box.getWidth() * 0.5f);
        const auto angle = side * 1.1f + (rng.nextFloat() - 0.5f) * 1.3f;
        const auto length = 4.0f + rng.nextFloat() * 6.0f;
        const juce::Point<float> start (x, y);
        const auto end = start.getPointOnCircumference (length, angle);

        g.setColour (colour.withAlpha (0.55f + rng.nextFloat() * 0.4f));
        g.drawLine ({ start, end }, 0.8f + rng.nextFloat() * 0.6f);

        if (rng.nextInt (4) == 0)
        {
            g.setColour (colour.brighter (0.6f).withAlpha (0.5f));
            g.fillRect (end.x - 0.5f, end.y - 0.5f, 1.0f, 1.0f);
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (body, juce::PathStrokeType (2.0f));
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (fur, 0, 0);

    // Title printed over the rust band.
    auto title = titleArea.toFloat();
    g.setColour (Palette::ink);
    g.setFont (juce::FontOptions (42.0f, juce::Font::bold).withHorizontalScale (0.92f));
    g.drawText ("WOOLLY BEAR", title.removeFromTop (46.0f), juce::Justification::centred);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("FUZZ  \xc2\xb7  SUSTAINER")),
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
    g.setColour (Palette::silkscreen.withAlpha (0.6f));
    g.drawEllipse (led.expanded (1.5f), 1.5f);

    // Maker's line at the foot.
    g.setColour (Palette::silkscreen.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ANIMAL TESTING  v" JucePlugin_VersionString, getLocalBounds().removeFromBottom (34),
                juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (28, 22);

    auto knobRow = area.removeFromTop (130);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &volume, &tone, &sustain })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (6, 0));

    titleArea = area.removeFromTop (76);
    area.removeFromTop (6);

    ledArea = juce::Rectangle<int> (14, 14).withCentre ({ area.getCentreX(), area.getY() + 8 });
    area.removeFromTop (24);
    footSwitch.setBounds (juce::Rectangle<int> (76, 76).withCentre ({ area.getCentreX(), area.getY() + 44 }));

    renderFur();
}
