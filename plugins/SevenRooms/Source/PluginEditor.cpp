#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    constexpr float knobLabelHeight = 18.0f;   // matches at::Knob's caption row
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (18.0f, 6.0f);

    // Maker's logo, top left.
    auto logoRow = area.removeFromTop (26.0f);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("ANIMAL TESTING", logoRow, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, logoRow, juce::Justification::centredRight);
    area.removeFromTop (8.0f);

    // Black nameplate stripe, edge to edge, with the name and a teal caption.
    auto stripe = juce::Rectangle<float> (0.0f, area.getY(), (float) getWidth(), 58.0f);
    g.setColour (Palette::nameplate);
    g.fillRect (stripe);
    g.setColour (Palette::accent);
    g.fillRect (stripe.withHeight (3.0f));
    g.fillRect (stripe.withTrimmedTop (stripe.getHeight() - 3.0f));
    stripe.reduce (18.0f, 0.0f);
    g.setColour (Palette::nameText);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("SEVEN ROOMS", stripe, juce::Justification::centredLeft);
    auto caption = stripe.removeFromRight (150.0f);
    g.setColour (Palette::accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Stereo Reverb", caption.removeFromTop (caption.getHeight() * 0.5f),
                juce::Justification::bottomRight);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Seven Types", caption, juce::Justification::topRight);
    area.removeFromTop (58.0f);

    // Chrome stomp switch: hex nut, then the domed cap; it sinks a little while held.
    const auto centre = area.getCentre();
    const auto nutR = juce::jmin (area.getHeight() * 0.36f, 44.0f);
    juce::Path nut;
    nut.addPolygon (centre, 6, nutR, juce::MathConstants<float>::pi / 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (nut, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (Palette::chrome, centre.translated (-nutR, -nutR),
                                             Palette::chrome.darker (0.7f), centre.translated (nutR, nutR), false));
    g.fillPath (nut);

    const auto capR = nutR * 0.62f * (down ? 0.96f : 1.0f);
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cap.getTopLeft(),
                                             Palette::chrome.darker (down ? 0.8f : 0.5f), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.7f : 0.45f));
    g.drawEllipse (cap, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillEllipse (cap.reduced (capR * 0.55f).translated (-capR * 0.25f, -capR * 0.25f));
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      level      (p.apvts, ParamIDs::level,      "Level"),
      liveliness (p.apvts, ParamIDs::liveliness, "Liveliness"),
      decay      (p.apvts, ParamIDs::decay,      "Decay"),
      type       (p.apvts, ParamIDs::type,       "Reverb Type"),
      tailsAttachment (p.apvts, ParamIDs::tails, tails),
      footswitchAttachment (p.apvts, ParamIDs::bypass, footswitch)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &level, &liveliness, &decay, &type })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        // Pale value text on the dark enclosure; set on the slider so the existing value box picks it up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::silkscreen);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    // The selector's positions are printed around it, so it needs no value box.
    type.getSlider().setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    // Decay reads out what it means for the current type: a reverb time or a window length.
    decay.getSlider().textFromValueFunction = [this] (double v) { return decayText (v); };
    decay.getSlider().setTextBoxIsEditable (false);
    decay.getSlider().updateText();
    type.getSlider().onValueChange = [this]
    {
        decay.getSlider().updateText();
        repaint();   // lights the selected type's label
    };

    tails.setClickingTogglesState (true);
    tails.setTooltip ("Tails: let the reverb ring out after bypassing");
    tails.setColour (juce::TextButton::buttonColourId,   Palette::enclosureDark);
    tails.setColour (juce::TextButton::buttonOnColourId, Palette::accent.darker (0.3f));
    tails.setColour (juce::TextButton::textColourOffId,  Palette::silkscreen.withAlpha (0.6f));
    tails.setColour (juce::TextButton::textColourOnId,   Palette::nameText);
    addAndMakeVisible (tails);

    footswitch.setTooltip ("Bypass");
    footswitch.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (footswitch);

    setSize (480, 560);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

juce::String PluginEditor::decayText (double decayPercent)
{
    const auto t = (int) type.getSlider().getValue();
    const auto seconds = ReverbEngine::decayTime (t, (float) decayPercent / 100.0f);

    if (seconds < 1.0f)
        return juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
    return juce::String (seconds, 1) + " s";
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Charcoal die-cast enclosure with a faint top-down sheen.
    g.fillAll (juce::Colour (0xff0c0c0d));
    const auto box = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.15f), box.getTopLeft(),
                                             Palette::enclosure.darker (0.2f), box.getBottomLeft(), false));
    g.fillRoundedRectangle (box, 12.0f);
    g.setColour (Palette::enclosureDark);
    g.drawRoundedRectangle (box, 12.0f, 2.0f);

    // Type positions around the selector.
    const auto& slider = type.getSlider();
    const auto knobArea = slider.getBounds().translated (type.getX(), type.getY()).toFloat().reduced (4.0f);
    const auto centre = knobArea.getCentre();
    const auto radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto selected = (int) slider.getValue();

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int i = 0; i < ReverbTypes::dialLabels.size(); ++i)
    {
        const auto a = params.startAngleRadians
                     + (float) i / (float) (ReverbTypes::dialLabels.size() - 1) * (params.endAngleRadians - params.startAngleRadians);
        const auto at = centre.getPointOnCircumference (radius + 16.0f, a);
        g.setColour (i == selected ? Palette::accent : Palette::silkscreen);
        g.drawText (ReverbTypes::dialLabels[i], juce::Rectangle<float> (56.0f, 14.0f).withCentre (at),
                    juce::Justification::centred);
    }

    // Status LED: lit while the effect is engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = ! footswitch.getToggleState();
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.3f));
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
    g.drawText ("ON", led.withWidth (60.0f).translated (led.getWidth() + 8.0f, 0.0f).expanded (0.0f, 4.0f),
                juce::Justification::centredLeft);

    // Divider between the control deck and the footswitch half.
    g.setColour (Palette::enclosureDark.withAlpha (0.8f));
    g.drawHorizontalLine (footswitch.getY() - 2, box.getX() + 12.0f, box.getRight() - 12.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 20);

    // Knob row: three pots, then the Type selector in a wider column so its labels fit around it.
    auto knobRow = area.removeFromTop (160);
    auto typeColumn = knobRow.removeFromRight (180);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &level, &liveliness, &decay })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (4, 0).withTrimmedTop (10));

    // Same knob size as the pots: their slider area is the column minus caption and value box.
    type.setBounds (typeColumn.withSizeKeepingCentre (knobWidth - 8, 150)
                              .withTrimmedBottom ((int) knobLabelHeight));

    area.removeFromTop (14);
    auto ledRow = area.removeFromTop (22);
    ledArea = juce::Rectangle<int> (12, 12).withCentre ({ ledRow.getX() + 16, ledRow.getCentreY() });
    tails.setBounds (ledRow.removeFromRight (80));

    area.removeFromTop (14);
    footswitch.setBounds (area.withTrimmedLeft (-6).withTrimmedRight (-6).withTrimmedBottom (-4));
}
