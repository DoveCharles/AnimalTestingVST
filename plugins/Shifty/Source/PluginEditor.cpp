#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    constexpr float knobLabelHeight = 18.0f;   // matches at::Knob's caption row
    constexpr int frameRate = 30;

    /** Twinkly specks over the background, as fractions of the editor size. */
    constexpr std::array<std::array<float, 3>, 7> sparkles {{
        { 0.47f, 0.10f, 0.0f }, { 0.62f, 0.22f, 1.7f }, { 0.08f, 0.93f, 3.1f }, { 0.97f, 0.55f, 4.4f },
        { 0.40f, 0.95f, 2.2f }, { 0.73f, 0.07f, 5.3f }, { 0.985f, 0.94f, 0.9f },
    }};

    void drawSparkle (juce::Graphics& g, juce::Point<float> at, float size, juce::Colour colour)
    {
        juce::Path star;
        star.startNewSubPath (at.x, at.y - size);
        star.quadraticTo (at, { at.x + size, at.y });
        star.quadraticTo (at, { at.x, at.y + size });
        star.quadraticTo (at, { at.x - size, at.y });
        star.quadraticTo (at, { at.x, at.y - size });
        star.closeSubPath();
        g.setColour (colour);
        g.fillPath (star);
    }
}

void Blob::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto engaged = ! getToggleState();
    auto body = getLocalBounds().toFloat().reduced (8.0f, 9.0f).withTrimmedRight (6.0f);

    // Squishes when stomped.
    if (down)
        body = body.withSizeKeepingCentre (body.getWidth() * 1.08f, body.getHeight() * 0.84f).translated (0.0f, 2.0f);

    const auto corner = body.getHeight() * 0.5f;

    if (engaged)
    {
        g.setColour (Palette::blush.withAlpha (0.3f));
        g.fillRoundedRectangle (body.expanded (5.0f), corner + 5.0f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (body.translated (0.0f, 2.5f), corner);

    const auto top    = engaged ? Palette::blush.brighter (0.25f) : juce::Colour (0xff86a3a0);
    const auto bottom = engaged ? Palette::magenta.withMultipliedSaturation (0.8f) : juce::Colour (0xff4e6866);
    g.setGradientFill (juce::ColourGradient (highlighted ? top.brighter (0.15f) : top, body.getCentreX(), body.getY(),
                                             bottom, body.getCentreX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, corner);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (body, corner, 1.6f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (body.getWidth() * 0.22f, body.getHeight() * 0.18f)
                       .withPosition (body.getX() + corner * 0.55f, body.getY() + body.getHeight() * 0.14f));

    g.setColour (Palette::ink);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (engaged ? "ON" : "OFF", body.translated (0.0f, 1.0f), juce::Justification::centred);
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      balance  (p.apvts, ParamIDs::balance,  "Balance"),
      feedback (p.apvts, ParamIDs::feedback, "Feedback"),
      fine     (p.apvts, ParamIDs::fine,     "Fine-Manual"),
      mode     (p.apvts, ParamIDs::mode,     "Mode"),
      blobAttachment (p.apvts, ParamIDs::bypass, blob)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &balance, &feedback, &fine, &mode })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::deepTeal);
        knob->getLabel().setFont (juce::FontOptions (11.5f, juce::Font::bold));

        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::ink);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    // The selector's positions are printed around it, grouped DELAY / PITCH, so it needs no caption or value box.
    mode.getLabel().setText ({}, juce::dontSendNotification);
    mode.getSlider().setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    // Fine-Manual reads out what it means in the current mode: a delay time or a pitch.
    fine.getSlider().textFromValueFunction = [this] (double v) { return fineText (v); };
    fine.getSlider().setTextBoxIsEditable (false);
    fine.getSlider().updateText();
    mode.getSlider().onValueChange = [this] { fine.getSlider().updateText(); };

    blob.setTooltip ("Bypass");
    blob.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (blob);

    awake = blob.getToggleState() ? 0.0f : 1.0f;

    setSize (548, 262);
    startTimerHz (frameRate);
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

void PluginEditor::timerCallback()
{
    const auto engaged = ! blob.getToggleState();
    awake += ((engaged ? 1.0f : 0.0f) - awake) * 0.08f;

    clock += 1.0f / (float) frameRate;
    repaint();
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll (Palette::teal);

    for (const auto& s : sparkles)
    {
        const auto twinkle = 0.5f + 0.5f * std::sin (clock * 2.2f + s[2]);
        drawSparkle (g, { bounds.getWidth() * s[0], bounds.getHeight() * s[1] }, 2.0f + 3.5f * twinkle,
                     Palette::cream.withAlpha (0.25f + 0.65f * twinkle * (0.4f + 0.6f * awake)));
    }

    // White cloud the controls sit on: a card ringed with puffy rounded squares, with a soft shadow.
    const auto card = cardArea.toFloat();
    juce::Path cloud;
    cloud.addRoundedRectangle (card, 10.0f);
    auto puff = 0;
    const auto addPuffs = [&] (juce::Point<float> from, juce::Point<float> to)
    {
        // Puffs sit partly inside the card so only a rounded bump shows, with soft dips between them.
        const auto steps = juce::jmax (1, juce::roundToInt (from.getDistanceFrom (to) / 46.0f));
        const auto inward = (card.getCentre() - (from + to) * 0.5f);
        const auto normal = inward / inward.getDistanceFromOrigin();
        for (int i = 0; i < steps; ++i, ++puff)
        {
            const auto size = 40.0f + 4.0f * std::sin ((float) puff * 2.3f);
            const auto at = from + (to - from) * ((float) i / (float) steps) + normal * (size * 0.18f);
            cloud.addRoundedRectangle (juce::Rectangle<float> (size, size).withCentre (at), size * 0.42f);
        }
    };
    addPuffs (card.getTopLeft(), card.getTopRight());
    addPuffs (card.getTopRight(), card.getBottomRight());
    addPuffs (card.getBottomRight(), card.getBottomLeft());
    addPuffs (card.getBottomLeft(), card.getTopLeft());

    g.setColour (Palette::deepTeal.withAlpha (0.35f));
    g.fillPath (cloud, juce::AffineTransform::translation (0.0f, 4.0f));
    g.setColour (juce::Colours::white);
    g.fillPath (cloud);

    drawTitle (g);

    // Mode positions around the selector, grouped DELAY / PITCH underneath.
    const auto& slider = mode.getSlider();
    const auto knobArea = slider.getBounds().translated (mode.getX(), mode.getY()).toFloat().reduced (3.0f);
    const auto centre = knobArea.getCentre();
    const auto radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto selected = (int) slider.getValue();

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int i = 0; i < Modes::dialLabels.size(); ++i)
    {
        const auto a = params.startAngleRadians
                     + (float) i / (float) (Modes::dialLabels.size() - 1) * (params.endAngleRadians - params.startAngleRadians);
        const auto at = centre.getPointOnCircumference (radius + 17.0f, a);
        g.setColour (i == selected ? Palette::hotPink : Palette::deepTeal.withAlpha (0.8f));
        g.drawText (Modes::dialLabels[i], juce::Rectangle<float> (48.0f, 14.0f).withCentre (at),
                    juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.setColour (Palette::teal);
    const auto groupY = centre.y + radius + 14.0f;
    g.drawText ("DELAY", juce::Rectangle<float> (60.0f, 12.0f).withCentre ({ centre.x - radius - 12.0f, groupY }),
                juce::Justification::centred);
    g.drawText ("PITCH", juce::Rectangle<float> (60.0f, 12.0f).withCentre ({ centre.x + radius + 12.0f, groupY }),
                juce::Justification::centred);
}

void PluginEditor::drawTitle (juce::Graphics& g)
{
    // "SHIFTY" with a magenta/cyan colour split.
    const juce::String title ("SHIFTY");
    const auto font = juce::Font (juce::FontOptions (32.0f, juce::Font::bold)).withHorizontalScale (0.95f);
    constexpr auto split = 1.5f;

    g.setFont (font);
    const auto baseY = (float) titleArea.getY() - 2.0f;

    for (const auto& pass : { std::pair { Palette::magenta.withAlpha (0.85f), -split },
                              std::pair { Palette::cyan.withAlpha (0.85f), split },
                              std::pair { Palette::cream, 0.0f } })
    {
        auto x = (float) titleArea.getX() + pass.second;
        for (int i = 0; i < title.length(); ++i)
        {
            const auto letter = juce::String::charToString (title[i]);
            const auto width = juce::GlyphArrangement::getStringWidth (font, letter);
            g.setColour (pass.first);
            g.drawText (letter, juce::Rectangle<float> (x, baseY + pass.second * 0.3f, width + 2.0f, 36.0f),
                        juce::Justification::centredLeft, false);
            x += width + 1.0f;
        }
    }

    const auto captionArea = titleArea.toFloat().withTrimmedLeft (150.0f).translated (0.0f, 4.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (Palette::ink.withAlpha (0.7f));
    g.drawText ("pitch shifter ~ delay   v" JucePlugin_VersionString, captionArea.translated (1.0f, 1.0f),
                juce::Justification::centredLeft);
    g.setColour (Palette::cream);
    g.drawText ("pitch shifter ~ delay   v" JucePlugin_VersionString, captionArea, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 10);

    auto header = area.removeFromTop (48);
    blob.setBounds (header.removeFromRight (92).withSizeKeepingCentre (92, 52));
    titleArea = header.withSizeKeepingCentre (header.getWidth(), 34).withTrimmedLeft (4);

    area.removeFromTop (6);
    cardArea = area.reduced (14, 18);   // leaves room for the cloud's puffs

    // Knob row: three pots, then the Mode selector in a wider column so its labels fit around it.
    auto knobRow = cardArea.reduced (2, 4);
    auto modeColumn = knobRow.removeFromRight (180);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &balance, &feedback, &fine })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (4, 0));

    // Same dial size as the pots: their slider area is the column minus caption and value box.
    mode.setBounds (modeColumn.withSizeKeepingCentre (knobWidth - 8, knobRow.getHeight())
                              .withTrimmedBottom ((int) knobLabelHeight));
}
