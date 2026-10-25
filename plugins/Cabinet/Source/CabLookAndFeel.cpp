#include "CabLookAndFeel.h"

CabLookAndFeel::CabLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,        Palette::silkscreen);
    setColour (juce::Slider::textBoxHighlightColourId,   Palette::piping.withAlpha (0.3f));
    setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                Palette::silkscreen);
    setColour (juce::TextEditor::backgroundColourId,     Palette::panel.darker (0.4f));
    setColour (juce::TextEditor::highlightColourId,      Palette::piping.withAlpha (0.3f));

    setColour (juce::ComboBox::backgroundColourId,       Palette::screen);
    setColour (juce::ComboBox::textColourId,             Palette::cream);
    setColour (juce::ComboBox::outlineColourId,          Palette::piping.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId,            Palette::piping);
    setColour (juce::PopupMenu::backgroundColourId,      Palette::panel);
    setColour (juce::PopupMenu::textColourId,            Palette::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::piping.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::cream);

    setColour (juce::TextButton::buttonColourId,         Palette::screen);
    setColour (juce::TextButton::buttonOnColourId,       Palette::piping);
    setColour (juce::TextButton::textColourOffId,        Palette::cream);
    setColour (juce::TextButton::textColourOnId,         Palette::tolex);
}

void CabLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Numbered scale, 0 to 10, like an amp panel.
    g.setColour (Palette::silkscreen.withAlpha (0.8f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto inner = i % 5 == 0 ? radius * 0.80f : radius * 0.86f;
        g.drawLine ({ centre.getPointOnCircumference (inner, a), centre.getPointOnCircumference (radius * 0.94f, a) },
                    i % 5 == 0 ? 1.6f : 1.0f);
    }

    const auto knobR = radius * 0.62f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.12f).expanded (2.0f));

    // Black skirt, then a cream chicken-head pointer on top that turns with the value.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3835), knob.getTopLeft(),
                                             juce::Colour (0xff0d0d0d), knob.getBottomRight(), false));
    g.fillEllipse (knob);

    juce::Path head;
    head.startNewSubPath (0.0f, -knobR * 1.18f);
    head.cubicTo (knobR * 0.32f, -knobR * 0.9f, knobR * 0.46f, -knobR * 0.1f, knobR * 0.42f, knobR * 0.32f);
    head.cubicTo (knobR * 0.38f, knobR * 0.62f, -knobR * 0.38f, knobR * 0.62f, -knobR * 0.42f, knobR * 0.32f);
    head.cubicTo (-knobR * 0.46f, -knobR * 0.1f, -knobR * 0.32f, -knobR * 0.9f, 0.0f, -knobR * 1.18f);
    head.closeSubPath();
    head.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (head, juce::AffineTransform::translation (0.0f, knobR * 0.08f));
    g.setGradientFill (juce::ColourGradient (Palette::cream.brighter (0.15f), centre.translated (-knobR * 0.4f, -knobR * 0.6f),
                                             Palette::cream.darker (0.25f), centre.translated (knobR * 0.4f, knobR * 0.6f), false));
    g.fillPath (head);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.strokePath (head, juce::PathStrokeType (1.0f));

    const auto tip  = centre.getPointOnCircumference (knobR * 1.05f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.35f, angle);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.drawLine ({ base, tip }, juce::jmax (1.5f, knobR * 0.07f));
}

void CabLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId).withMultipliedAlpha (box.isMouseOver (true) ? 1.0f : 0.7f));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);

    const auto arrow = area.removeFromRight (22.0f).reduced (7.0f, (float) height * 0.38f);
    juce::Path p;
    p.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (p);
}

juce::Font CabLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (14.0f, juce::Font::bold);
}

void CabLookAndFeel::drawGrille (juce::Graphics& g, juce::Rectangle<float> area, float cornerSize)
{
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, cornerSize);
    g.reduceClipRegion (clip);

    g.setColour (Palette::grille);
    g.fillRect (area);

    // Diagonal basket weave, two directions.
    g.setColour (Palette::grilleWeave.withAlpha (0.55f));
    const auto span = area.getWidth() + area.getHeight();
    for (auto d = 0.0f; d < span; d += 4.0f)
    {
        g.drawLine (area.getX() + d, area.getY(), area.getX() + d - area.getHeight(), area.getBottom(), 1.2f);
        g.drawLine (area.getRight() - d, area.getY(), area.getRight() - d + area.getHeight(), area.getBottom(), 0.6f);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, area.getCentre(),
                                             juce::Colours::black.withAlpha (0.45f), area.getTopLeft(), true));
    g.fillRect (area);
}

void CabLookAndFeel::drawTolex (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (Palette::tolex);
    g.fillRect (area);

    // A fixed speckle pattern, so the texture doesn't crawl on repaint.
    juce::Random random (1957);
    for (int i = 0; i < (int) (area.getWidth() * area.getHeight() / 60.0f); ++i)
    {
        const auto px = area.getX() + random.nextFloat() * area.getWidth();
        const auto py = area.getY() + random.nextFloat() * area.getHeight();
        g.setColour (random.nextBool() ? juce::Colours::white.withAlpha (0.035f) : juce::Colours::black.withAlpha (0.25f));
        g.fillRect (px, py, 1.5f, 1.5f);
    }
}
