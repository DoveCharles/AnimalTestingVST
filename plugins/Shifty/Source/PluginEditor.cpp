#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    constexpr float knobLabelHeight = 18.0f;   // matches at::Knob's caption row
}

void Treadle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto plate = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, down ? 1.5f : 0.0f);

    // Plate body: same powder coat as the enclosure, lit from above, with a dark hinge gap.
    g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.08f), plate.getTopLeft(),
                                             Palette::enclosure.darker (0.12f), plate.getBottomLeft(), false));
    g.fillRoundedRectangle (plate, 8.0f);
    g.setColour (Palette::enclosureDark.darker (highlighted ? 0.4f : 0.2f));
    g.drawRoundedRectangle (plate, 8.0f, 2.0f);

    auto area = plate.reduced (18.0f, 12.0f);

    // Maker's logo, top left, the way the originals carry theirs.
    auto logoRow = area.removeFromTop (26.0f);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("ANIMAL TESTING", logoRow, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, logoRow, juce::Justification::centredRight);
    area.removeFromTop (8.0f);

    // Black nameplate stripe across the plate.
    auto stripe = juce::Rectangle<float> (plate.getX(), area.getY(), plate.getWidth(), 58.0f);
    g.setColour (Palette::nameplate);
    g.fillRect (stripe);
    stripe.reduce (18.0f, 0.0f);
    g.setColour (Palette::nameText);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("SHIFTY", stripe, juce::Justification::centredLeft);
    auto caption = stripe.removeFromRight (190.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Digital Pitch Shifter", caption.removeFromTop (caption.getHeight() * 0.5f),
                juce::Justification::bottomRight);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("/ Delay", caption, juce::Justification::topRight);
    area.removeFromTop (58.0f + 14.0f);

    // Ribbed rubber pad where the foot lands.
    const auto pad = area.withTrimmedBottom (4.0f);
    g.setColour (Palette::rubber);
    g.fillRoundedRectangle (pad, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (auto y = pad.getY() + 8.0f; y < pad.getBottom() - 4.0f; y += 9.0f)
        g.drawLine (pad.getX() + 6.0f, y, pad.getRight() - 6.0f, y, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (pad, 6.0f, 1.0f);
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      balance  (p.apvts, ParamIDs::balance,  "Balance"),
      feedback (p.apvts, ParamIDs::feedback, "Feedback"),
      fine     (p.apvts, ParamIDs::fine,     "Fine-Manual"),
      mode     (p.apvts, ParamIDs::mode,     "Mode"),
      treadleAttachment (p.apvts, ParamIDs::bypass, treadle)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &balance, &feedback, &fine, &mode })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        // Dark value text on the pale enclosure; set on the slider so the existing value box picks it up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::silkscreen);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    // The selector's positions are printed around it, so it needs no value box.
    mode.getSlider().setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    // Fine-Manual reads out what it means in the current mode: a delay time or a pitch.
    fine.getSlider().textFromValueFunction = [this] (double v) { return fineText (v); };
    fine.getSlider().setTextBoxIsEditable (false);
    fine.getSlider().updateText();
    mode.getSlider().onValueChange = [this] { fine.getSlider().updateText(); };

    treadle.setTooltip ("Bypass");
    treadle.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (treadle);

    setSize (480, 560);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

juce::String PluginEditor::fineText (double finePercent)
{
    const auto m = (int) mode.getSlider().getValue();
    const auto f = (float) finePercent / 100.0f;

    if (! Modes::isPitch (m))
    {
        const auto ms = Modes::delayMs (m, f);
        return ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
    }

    const auto st = Modes::semitones (m, f);
    if (m == Modes::pitchManual)
        return (st >= 0.0f ? "+" : "") + juce::String (st, 1) + " st";

    const auto cents = juce::roundToInt ((st - std::round (st)) * 100.0f);
    return (cents >= 0 ? "+" : "") + juce::String (cents) + " ct";
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Pale enclosure with a faint top-down sheen.
    g.fillAll (juce::Colour (0xff1a1b1d));
    const auto box = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.1f), box.getTopLeft(),
                                             Palette::enclosure.darker (0.1f), box.getBottomLeft(), false));
    g.fillRoundedRectangle (box, 12.0f);
    g.setColour (Palette::enclosureDark);
    g.drawRoundedRectangle (box, 12.0f, 2.0f);

    // Mode positions around the selector, grouped DELAY / PITCH underneath.
    const auto& slider = mode.getSlider();
    const auto knobArea = slider.getBounds().translated (mode.getX(), mode.getY()).toFloat().reduced (4.0f);
    const auto centre = knobArea.getCentre();
    const auto radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto params = slider.getRotaryParameters();

    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (int i = 0; i < Modes::dialLabels.size(); ++i)
    {
        const auto a = params.startAngleRadians
                     + (float) i / (float) (Modes::dialLabels.size() - 1) * (params.endAngleRadians - params.startAngleRadians);
        const auto at = centre.getPointOnCircumference (radius + 20.0f, a);
        g.drawText (Modes::dialLabels[i], juce::Rectangle<float> (48.0f, 14.0f).withCentre (at),
                    juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.setColour (Palette::silkscreen.withAlpha (0.7f));
    const auto groupY = centre.y + radius + 18.0f;
    g.drawText ("DELAY", juce::Rectangle<float> (60.0f, 12.0f).withCentre ({ centre.x - radius - 10.0f, groupY }),
                juce::Justification::centred);
    g.drawText ("PITCH", juce::Rectangle<float> (60.0f, 12.0f).withCentre ({ centre.x + radius + 10.0f, groupY }),
                juce::Justification::centred);

    // Check LED: lit while the effect is engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = ! treadle.getToggleState();
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.25f));
        g.fillEllipse (led.expanded (6.0f));
    }
    g.setColour (engaged ? Palette::ledOn : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (engaged ? 0.6f : 0.15f));
    g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-1.5f, -1.5f));
    g.setColour (juce::Colour (0xff8a8c86));
    g.drawEllipse (led.expanded (1.5f), 1.5f);

    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("CHECK", led.withWidth (60.0f).translated (led.getWidth() + 8.0f, 0.0f).expanded (0.0f, 4.0f),
                juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 20);

    // Knob row: three pots, then the Mode selector in a wider column so its labels fit around it.
    auto knobRow = area.removeFromTop (150);
    auto modeColumn = knobRow.removeFromRight (170);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &balance, &feedback, &fine })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (4, 0));

    // Same knob size as the pots: their slider area is the column minus caption and value box.
    mode.setBounds (modeColumn.withSizeKeepingCentre (knobWidth - 8, 150)
                              .withTrimmedBottom ((int) knobLabelHeight));

    area.removeFromTop (14);
    auto ledRow = area.removeFromTop (20);
    ledArea = juce::Rectangle<int> (12, 12).withCentre ({ ledRow.getX() + 16, ledRow.getCentreY() });

    area.removeFromTop (12);
    treadle.setBounds (area.withTrimmedLeft (-6).withTrimmedRight (-6).withTrimmedBottom (-4));
}
