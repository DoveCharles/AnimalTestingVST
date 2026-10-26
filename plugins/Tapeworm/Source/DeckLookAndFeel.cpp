#include "DeckLookAndFeel.h"

DeckLookAndFeel::DeckLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,      Palette::ink);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::amber.withAlpha (0.4f));
    setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,              Palette::ink);
    setColour (juce::TextEditor::backgroundColourId,   Palette::metalLight);
    setColour (juce::TextEditor::textColourId,         Palette::ink);
    setColour (juce::TextEditor::highlightColourId,    Palette::amber.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId,           Palette::amber);
    setColour (juce::ComboBox::arrowColourId,          Palette::amber);
    setColour (juce::TextButton::textColourOffId,      Palette::metalLight);
    setColour (juce::TextButton::textColourOnId,       Palette::amber);
}

juce::Image DeckLookAndFeel::matteMetal (int width, int height, juce::int64 seed)
{
    juce::Image image (juce::Image::RGB, juce::jmax (1, width), juce::jmax (1, height), false);
    juce::Random random (seed);

    {
        // A gentle fall-off from the top light, nothing shiny.
        juce::Graphics g (image);
        juce::ColourGradient light (Palette::metalLight, 0.0f, 0.0f, Palette::metal.darker (0.06f),
                                    0.0f, (float) image.getHeight(), false);
        light.addColour (0.35, Palette::metal.brighter (0.08f));
        g.setGradientFill (light);
        g.fillAll();
    }

    // Bead-blasted: a fine, even speckle in every pixel.
    juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto colour = pixels.getPixelColour (x, y);
            const auto shift = (random.nextFloat() - 0.5f) * 0.045f;
            pixels.setPixelColour (x, y, shift > 0.0f ? colour.brighter (shift) : colour.darker (-shift));
        }

    return image;
}

void DeckLookAndFeel::drawBevel (juce::Graphics& g, juce::Rectangle<float> r, float cornerSize, bool raised)
{
    const auto light = juce::Colours::white.withAlpha (0.75f);
    const auto dark  = juce::Colours::black.withAlpha (0.5f);
    g.setGradientFill (juce::ColourGradient (raised ? light : dark, r.getTopLeft(),
                                             raised ? dark : light, r.getBottomRight(), false));
    g.drawRoundedRectangle (r.reduced (0.75f), cornerSize, 1.5f);
}

void DeckLookAndFeel::drawRecess (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    constexpr auto corner = 6.0f;
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);

    {
        // The lip of the cut throws a shadow down and to the right.
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.45f), r.getY(),
                                                           juce::Colours::transparentBlack, r.getY() + 10.0f));
        g.fillRect (r);
        g.setGradientFill (juce::ColourGradient::horizontal (juce::Colours::black.withAlpha (0.25f), r.getX(),
                                                             juce::Colours::transparentBlack, r.getX() + 7.0f));
        g.fillRect (r);
    }

    drawBevel (g, r.expanded (1.5f), corner + 1.5f, false);
}

void DeckLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius, float slotAngle)
{
    const auto head = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (head.translated (0.6f, 1.2f).expanded (0.8f));

    g.setGradientFill (juce::ColourGradient (Palette::metalLight.brighter (0.3f), head.getTopLeft(),
                                             Palette::metalDark.darker (0.3f), head.getBottomRight(), false));
    g.fillEllipse (head);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (head, 0.8f);

    const auto a = centre.getPointOnCircumference (radius * 0.75f, slotAngle);
    const auto b = centre.getPointOnCircumference (radius * 0.75f, slotAngle + juce::MathConstants<float>::pi);
    const auto slotWidth = juce::jmax (1.2f, radius * 0.28f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine ({ a.translated (0.0f, 0.8f), b.translated (0.0f, 0.8f) }, slotWidth);
    g.setColour (Palette::ink.withAlpha (0.85f));
    g.drawLine ({ a, b }, slotWidth);
}

void DeckLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Engraved scale: eleven ticks, longer at the ends and the middle.
    g.setColour (Palette::ink.withAlpha (0.8f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto major = i == 0 || i == 5 || i == 10;
        g.drawLine ({ centre.getPointOnCircumference (radius * (major ? 0.83f : 0.88f), a),
                      centre.getPointOnCircumference (radius, a) }, major ? 2.0f : 1.2f);
    }

    const auto knobR = radius * 0.78f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    // Shadow on the faceplate, a soft wide one under a tight dark one.
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillEllipse (knob.translated (knobR * 0.06f, knobR * 0.16f).expanded (knobR * 0.06f));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.07f));

    // Knurled aluminium skirt: the ridges turn with the knob, the light stays put.
    constexpr int ridges = 40;
    juce::Path skirt;
    for (int i = 0; i < ridges * 2; ++i)
    {
        const auto a = angle + (float) i * juce::MathConstants<float>::pi / (float) ridges;
        const auto p = centre.getPointOnCircumference (i % 2 == 0 ? knobR : knobR * 0.95f, a);
        i == 0 ? skirt.startNewSubPath (p) : skirt.lineTo (p);
    }
    skirt.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::metalLight.brighter (0.2f), knob.getTopLeft(),
                                             Palette::metalDark.darker (0.35f), knob.getBottomRight(), false));
    g.fillPath (skirt);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (skirt, juce::PathStrokeType (0.8f));

    // Black cap, slightly domed.
    const auto capR = knobR * 0.74f;
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cap.expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d55), cap.getX() + capR * 0.5f, cap.getY() + capR * 0.3f,
                                             Palette::cap, centre.x + capR * 0.3f, centre.y + capR * 0.6f, true));
    g.fillEllipse (cap);
    g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.16f), cap.getY(),
                                                       juce::Colours::transparentWhite, centre.y));
    g.fillEllipse (cap.reduced (capR * 0.12f).withTrimmedBottom (capR * 0.9f));

    // Orange pointer from near the middle out to the edge of the cap.
    const auto tip  = centre.getPointOnCircumference (capR * 0.88f, angle);
    const auto base = centre.getPointOnCircumference (capR * 0.3f, angle);
    juce::Path pointer;
    pointer.startNewSubPath (base);
    pointer.lineTo (tip);
    g.setColour (Palette::pointer);
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (2.5f, capR * 0.13f), juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void DeckLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);

    // Bezel cut into the plate, then a black cap that drops into it when pressed.
    g.setColour (Palette::chassis);
    g.fillRoundedRectangle (r, 5.0f);
    drawBevel (g, r.expanded (1.0f), 6.0f, false);

    auto cap = r.reduced (2.5f);
    const auto pressed = down || button.getToggleState();
    if (pressed)
        cap = cap.translated (0.0f, 1.0f).withTrimmedBottom (1.0f);

    g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (pressed ? 0xff2b2d32 : 0xff464950), cap.getY(),
                                                       juce::Colour (pressed ? 0xff18191c : 0xff222429), cap.getBottom()));
    g.fillRoundedRectangle (cap, 3.5f);
    if (! pressed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
    }
    if (highlighted && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (cap, 3.5f);
    }

    // Switches get a lamp on the left.
    if (button.getClickingTogglesState())
    {
        const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ cap.getX() + 11.0f, cap.getCentreY() });
        if (button.getToggleState())
        {
            g.setGradientFill (juce::ColourGradient (Palette::amber.withAlpha (0.55f), lamp.getCentre(),
                                                     Palette::amber.withAlpha (0.0f), lamp.getCentre().translated (9.0f, 0.0f), true));
            g.fillEllipse (lamp.expanded (6.0f));
            g.setColour (Palette::amber);
            g.fillEllipse (lamp);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillEllipse (lamp.reduced (2.2f).translated (-0.6f, -0.6f));
        }
        else
        {
            g.setColour (Palette::lampOff);
            g.fillEllipse (lamp);
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (lamp, 0.8f);
    }
}

void DeckLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    auto area = button.getLocalBounds().reduced (4, 2);
    if (button.getClickingTogglesState())
        area.removeFromLeft (14);
    if (down || button.getToggleState())
        area.translate (0, 1);

    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (button.getButtonText().toUpperCase(), area, juce::Justification::centred, 1);
}

void DeckLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    // An amber readout behind glass.
    const auto r = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (Palette::window);
    g.fillRoundedRectangle (r, 4.0f);
    g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.08f), r.getY(),
                                                       juce::Colours::transparentWhite, r.getCentreY()));
    g.fillRoundedRectangle (r.reduced (2.0f), 3.0f);
    drawBevel (g, r, 4.0f, false);

    const auto arrow = juce::Rectangle<float> (8.0f, 5.0f).withCentre ({ r.getRight() - 12.0f, r.getCentreY() });
    juce::Path triangle;
    triangle.addTriangle (arrow.getTopLeft(), arrow.getTopRight(), arrow.getCentre().withY (arrow.getBottom()));
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (triangle);
}

void DeckLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font DeckLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (13.0f, juce::Font::bold);
}
