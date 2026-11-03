#include "DeckLookAndFeel.h"

namespace
{
    /** Pseudo-random 0 to 1 for a pair of numbers, the same every time. */
    float hash (int a, int b)
    {
        const auto h = std::sin ((float) (a * 127 + b * 311) * 12.9898f) * 43758.5453f;
        return h - std::floor (h);
    }

    juce::PathStrokeType roundedStroke (float width)
    {
        return { width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };
    }
}

DeckLookAndFeel::DeckLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId,         Palette::cherry);
    setColour (juce::Slider::textBoxTextColourId,              Palette::ink);
    setColour (juce::Slider::textBoxHighlightColourId,         Palette::mustard.withAlpha (0.5f));
    setColour (juce::Slider::textBoxOutlineColourId,           juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                      Palette::ink);
    setColour (juce::TextEditor::backgroundColourId,           Palette::paper);
    setColour (juce::TextEditor::textColourId,                 Palette::ink);
    setColour (juce::TextEditor::highlightColourId,            Palette::mustard.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId,              Palette::ink);
    setColour (juce::TextEditor::focusedOutlineColourId,       Palette::ink);
    setColour (juce::ComboBox::textColourId,                   Palette::ink);
    setColour (juce::ComboBox::arrowColourId,                  Palette::ink);
    setColour (juce::PopupMenu::backgroundColourId,            Palette::paper);
    setColour (juce::PopupMenu::textColourId,                  Palette::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::mustard);
    setColour (juce::PopupMenu::highlightedTextColourId,       Palette::ink);
    setColour (juce::TextButton::textColourOffId,              Palette::ink);
    setColour (juce::TextButton::textColourOnId,               Palette::ink);
}

juce::Font DeckLookAndFeel::hand (float size)
{
    // Picked once: the first of these the system has.
    static const auto choice = []
    {
        const auto installed = juce::Font::findAllTypefaceNames();
        for (auto* name : { "Chalkboard SE", "Segoe Print", "Comic Sans MS" })
            if (installed.contains (name))
                return juce::String (name);
        return juce::String();
    }();

    if (choice.isEmpty())
        return juce::FontOptions (size, juce::Font::bold);
    return juce::FontOptions (choice, size, juce::Font::bold);
}

juce::Image DeckLookAndFeel::gingham (int width, int height)
{
    juce::Image image (juce::Image::RGB, juce::jmax (1, width), juce::jmax (1, height), false);
    juce::Graphics g (image);
    g.fillAll (Palette::cloth);

    // Stripes both ways; where they cross the check is twice as strong.
    constexpr int band = 26, stripe = 13;
    g.setColour (Palette::check.withAlpha (0.42f));
    for (int x = 0; x < image.getWidth(); x += band)
        g.fillRect (x, 0, stripe, image.getHeight());
    for (int y = 0; y < image.getHeight(); y += band)
        g.fillRect (0, y, image.getWidth(), stripe);

    // The weave: faint threads along each stripe.
    g.setColour (Palette::check.withAlpha (0.12f));
    for (int x = 0; x < image.getWidth(); x += 3)
        g.drawVerticalLine (x, 0.0f, (float) image.getHeight());
    for (int y = 0; y < image.getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) image.getWidth());

    return image;
}

void DeckLookAndFeel::drawCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, int seed)
{
    constexpr auto corner = 10.0f;
    g.setColour (Palette::ink.withAlpha (0.16f));
    g.fillRoundedRectangle (r.translated (3.0f, 5.0f), corner);
    g.setColour (Palette::paper);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (r, corner, 2.5f);

    g.setColour (Palette::rule);
    g.drawLine (r.getX() + 10.0f, r.getY() + 29.0f, r.getRight() - 10.0f, r.getY() + 29.0f, 1.5f);

    g.setColour (Palette::ink);
    g.setFont (hand (17.0f));
    g.drawText (title, r.reduced (14.0f, 0.0f).withHeight (30.0f), juce::Justification::centredLeft);

    // A strip of tape over the top right corner, with torn ends.
    const auto tapeW = 60.0f, tapeH = 20.0f;
    juce::Path strip;
    strip.startNewSubPath (-tapeW * 0.5f, -tapeH * 0.5f);
    strip.lineTo (tapeW * 0.5f, -tapeH * 0.5f);
    for (int i = 1; i <= 4; ++i)
        strip.lineTo (tapeW * 0.5f + (i % 2 == 1 ? -2.5f : 0.0f), -tapeH * 0.5f + tapeH * (float) i / 4.0f);
    strip.lineTo (-tapeW * 0.5f, tapeH * 0.5f);
    for (int i = 1; i <= 4; ++i)
        strip.lineTo (-tapeW * 0.5f + (i % 2 == 1 ? 2.5f : 0.0f), tapeH * 0.5f - tapeH * (float) i / 4.0f);
    strip.closeSubPath();
    const auto tilt = juce::degreesToRadians (32.0f + (hash (seed, 3) - 0.5f) * 18.0f);
    strip.applyTransform (juce::AffineTransform::rotation (tilt).translated (r.getRight() - 14.0f, r.getY() + 6.0f));
    g.setColour (Palette::tape);
    g.fillPath (strip);
    g.setColour (Palette::ink.withAlpha (0.25f));
    g.strokePath (strip, juce::PathStrokeType (1.0f));
}

void DeckLookAndFeel::drawBoard (juce::Graphics& g, juce::Rectangle<float> r)
{
    constexpr auto corner = 18.0f;
    g.setColour (Palette::ink.withAlpha (0.2f));
    g.fillRoundedRectangle (r.translated (3.0f, 5.0f), corner);

    juce::Path board;
    board.addRoundedRectangle (r, corner);
    g.setGradientFill (juce::ColourGradient::vertical (Palette::wood.brighter (0.08f), r.getY(),
                                                       Palette::wood.darker (0.06f), r.getBottom()));
    g.fillPath (board);

    {
        // Grain: long wavy lines, and a knot or two the lines bend around.
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (board);
        const juce::Point<float> knots[] { { r.getX() + r.getWidth() * 0.13f, r.getY() + r.getHeight() * 0.78f },
                                           { r.getX() + r.getWidth() * 0.86f, r.getY() + r.getHeight() * 0.2f } };
        g.setColour (Palette::woodDark.withAlpha (0.35f));
        int line = 0;
        for (auto y = r.getY() + 6.0f; y < r.getBottom(); y += 11.0f + 6.0f * hash (line, 1), ++line)
        {
            juce::Path grain;
            const auto phase = hash (line, 2) * 6.0f, wave = 60.0f + 50.0f * hash (line, 4);
            for (auto x = r.getX(); x <= r.getRight(); x += 4.0f)
            {
                auto dy = 2.0f * std::sin (x / wave + phase);
                for (auto knot : knots)
                {
                    const auto d = juce::Point<float> (x, y).getDistanceFrom (knot);
                    dy += (y < knot.y ? -1.0f : 1.0f) * 14.0f * std::exp (-d * d / 900.0f);
                }
                x == r.getX() ? grain.startNewSubPath (x, y + dy) : grain.lineTo (x, y + dy);
            }
            g.strokePath (grain, juce::PathStrokeType (1.3f));
        }
        for (auto knot : knots)
            for (auto k : { 4.0f, 8.0f })
                g.drawEllipse (juce::Rectangle<float> (k * 2.2f, k * 1.4f).withCentre (knot), 1.3f);
    }

    // The juice groove round the edge, cut in with a lit lower lip.
    const auto groove = r.reduced (9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (groove.translated (0.0f, 1.5f), corner - 8.0f, 2.0f);
    g.setColour (Palette::woodDark.withAlpha (0.7f));
    g.drawRoundedRectangle (groove, corner - 8.0f, 2.0f);

    g.setColour (Palette::ink);
    g.strokePath (board, juce::PathStrokeType (3.0f));
}

void DeckLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // The scale: eleven dots, filled in up to the value.
    const auto dotR = juce::jmax (2.5f, radius * 0.055f);
    for (int i = 0; i <= 10; ++i)
    {
        const auto at = (float) i / 10.0f;
        const auto dot = juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f)
                             .withCentre (centre.getPointOnCircumference (radius - dotR - 1.0f,
                                                                          startAngle + at * (endAngle - startAngle)));
        g.setColour (at <= sliderPos + 0.001f ? colour : Palette::cream);
        g.fillEllipse (dot);
        g.setColour (Palette::ink);
        g.drawEllipse (dot, 1.5f);
    }

    // A jar lid: a ridged rim that turns with the knob, and a flat top.
    const auto lidR = radius * 0.74f;
    juce::Path rim;
    constexpr int ridges = 26;
    for (int i = 0; i <= 160; ++i)
    {
        const auto a = (float) i / 160.0f * juce::MathConstants<float>::twoPi;
        const auto r = lidR * (1.0f - 0.04f * (0.5f + 0.5f * std::cos ((float) ridges * (a - angle))));
        const auto p = centre.getPointOnCircumference (r, a);
        i == 0 ? rim.startNewSubPath (p) : rim.lineTo (p);
    }
    rim.closeSubPath();

    g.setColour (Palette::ink.withAlpha (0.22f));
    g.fillPath (rim, juce::AffineTransform::translation (2.0f, 4.0f));
    g.setColour (colour.darker (0.25f));
    g.fillPath (rim);
    g.setColour (Palette::ink);
    g.strokePath (rim, juce::PathStrokeType (2.5f));

    const auto topR = lidR * 0.78f;
    const auto top = juce::Rectangle<float> (topR * 2.0f, topR * 2.0f).withCentre (centre);
    g.setColour (colour);
    g.fillEllipse (top);
    g.setColour (Palette::ink);
    g.drawEllipse (top, 2.0f);

    // Cartoon shine, which stays put.
    juce::Path shine;
    shine.addCentredArc (centre.x, centre.y, topR * 0.75f, topR * 0.75f, 0.0f,
                         juce::degreesToRadians (-70.0f), juce::degreesToRadians (-25.0f), true);
    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.strokePath (shine, roundedStroke (juce::jmax (2.5f, topR * 0.11f)));

    juce::Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (topR * 0.15f, angle));
    pointer.lineTo (centre.getPointOnCircumference (topR * 0.8f, angle));
    g.setColour (Palette::ink);
    g.strokePath (pointer, roundedStroke (juce::jmax (3.0f, topR * 0.15f)));
}

namespace
{
    constexpr auto buttonDepth = 4.0f;

    /** The face of a chunky button: it sits up on its base, and drops onto it when pressed. */
    juce::Rectangle<float> buttonFace (juce::Button& button, bool down)
    {
        const auto r = button.getLocalBounds().toFloat().reduced (1.5f);
        const auto pressed = down || button.getToggleState();
        return r.withTrimmedBottom (buttonDepth).translated (0.0f, pressed ? buttonDepth - 1.5f : 0.0f);
    }
}

void DeckLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.5f);
    const auto face = buttonFace (button, down);
    const auto corner = juce::jmin (9.0f, face.getHeight() * 0.4f);
    auto fill = button.getToggleState() ? Palette::mustard : Palette::cream;
    if (highlighted && ! down)
        fill = fill.brighter (0.06f);
    if (! button.isEnabled())
        fill = fill.withMultipliedSaturation (0.3f);

    g.setColour (fill.darker (0.45f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (r, corner, 2.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (face, corner);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (face, corner, 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawLine (face.getX() + corner, face.getY() + 4.0f, face.getX() + juce::jmax (corner + 4.0f, face.getWidth() * 0.3f),
                face.getY() + 4.0f, 2.0f);

    // Switches get a cherry on the left that lights up.
    if (button.getClickingTogglesState())
    {
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ face.getX() + 13.0f, face.getCentreY() });
        g.setColour (button.getToggleState() ? Palette::cherry : Palette::cream.darker (0.2f));
        g.fillEllipse (lamp);
        if (button.getToggleState())
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (lamp.getCentre().translated (-1.5f, -1.5f)));
        }
        g.setColour (Palette::ink);
        g.drawEllipse (lamp, 1.5f);
    }
}

void DeckLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    auto area = buttonFace (button, down).toNearestInt().reduced (4, 0);
    if (button.getClickingTogglesState())
        area.removeFromLeft (16);

    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (hand (15.0f));
    g.drawFittedText (button.getButtonText().toUpperCase(), area, juce::Justification::centred, 1);
}

void DeckLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat().reduced (1.0f);
    g.setColour (Palette::cream);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (r, 8.0f, 2.0f);

    const auto arrow = juce::Rectangle<float> (9.0f, 5.0f).withCentre ({ r.getRight() - 13.0f, r.getCentreY() });
    juce::Path chevron;
    chevron.startNewSubPath (arrow.getTopLeft());
    chevron.lineTo (arrow.getCentreX(), arrow.getBottom());
    chevron.lineTo (arrow.getTopRight());
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, roundedStroke (2.0f));
}

void DeckLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font DeckLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return hand (14.0f);
}

juce::Font DeckLookAndFeel::getPopupMenuFont()
{
    return hand (15.0f);
}

juce::Label* DeckLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = at::LookAndFeel::createSliderTextBox (slider);
    label->setFont (hand (15.0f));
    return label;
}
