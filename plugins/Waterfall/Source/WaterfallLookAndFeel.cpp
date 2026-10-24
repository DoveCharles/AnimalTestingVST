#include "WaterfallLookAndFeel.h"

namespace
{
    juce::Font systemFont (float height, const juce::String& macName, const juce::String& macStyle,
                           const juce::String& winName, const juce::String& winStyle)
    {
       #if JUCE_MAC
        juce::ignoreUnused (winName, winStyle);
        return juce::Font (juce::FontOptions (macName, macStyle, height));
       #elif JUCE_WINDOWS
        juce::ignoreUnused (macName, macStyle);
        return juce::Font (juce::FontOptions (winName, winStyle, height));
       #else
        juce::ignoreUnused (macName, macStyle, winName, winStyle);
        return juce::Font (juce::FontOptions (height));
       #endif
    }
}

WaterfallLookAndFeel::WaterfallLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  juce::Colours::white.withAlpha (0.25f));
    setColour (juce::TextEditor::backgroundColourId,    shadow.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId,       juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::white.withAlpha (0.3f));
    setColour (juce::Label::textColourId,               juce::Colours::white.withAlpha (0.9f));
}

juce::Font WaterfallLookAndFeel::captionFont (float height)
{
    return systemFont (height, "Avenir Next", "Medium", "Segoe UI", "Semilight").withExtraKerningFactor (0.28f);
}

juce::Font WaterfallLookAndFeel::valueFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain))
               .withExtraKerningFactor (0.04f);
}

juce::Font WaterfallLookAndFeel::thinFont (float height)
{
    return systemFont (height, "Avenir Next", "Regular", "Segoe UI", "Light").withExtraKerningFactor (0.2f);
}

void WaterfallLookAndFeel::drawHaloText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                                         juce::Justification justification, juce::Colour colour)
{
    // A ring of faint offset copies reads as a soft glow of shadow around the letters.
    const auto alpha = colour.getFloatAlpha();
    g.setColour (shadow.withAlpha (0.16f * alpha));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f },
                           { -1.5f, 1.5f }, { 1.5f, 1.5f }, { 0.0f, 2.0f } })
        g.drawText (text, area.translated (dx, dy), justification, false);

    g.setColour (colour);
    g.drawText (text, area, justification, false);
}

void WaterfallLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineW  = juce::jmax (2.5f, radius * 0.09f);
    const auto arcR   = radius - lineW * 1.5f;   // room for the bloom
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto glow   = juce::jlimit (0.0f, 1.0f, (float) slider.getProperties()[glowProperty]);

    auto arc = [&] (float from, float to, float r)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, r, r, 0.0f, from, to, true);
        return p;
    };

    // Glass disc: faint fill lit from the top left, tinted by the knob's colour as it glows.
    const auto discR = arcR - lineW * 1.7f;
    const auto disc = juce::Rectangle<float> (discR * 2.0f, discR * 2.0f).withCentre (centre);
    // Dark glass behind the ring, so the light of the arc and rim reads over the bright visual.
    juce::ColourGradient lens (shadow.withAlpha (0.42f), centre, shadow.withAlpha (0.0f),
                               centre.translated (radius, 0.0f), true);
    lens.addColour (0.7, shadow.withAlpha (0.3f));
    g.setGradientFill (lens);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    juce::ColourGradient glass (juce::Colours::white.withAlpha (0.14f), centre.translated (-discR * 0.6f, -discR * 0.8f),
                                colour.withAlpha (0.06f + 0.18f * glow), centre.translated (discR * 0.5f, discR * 0.9f), true);
    g.setGradientFill (glass);
    g.fillEllipse (disc);

    // Rim, brightest at the top, like light catching the edge of a lens.
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.8f), centre.translated (0.0f, -discR),
                              juce::Colours::white.withAlpha (0.2f), centre.translated (0.0f, discR), false);
    g.setGradientFill (rim);
    g.drawEllipse (disc, 1.2f);

    // Track.
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.strokePath (arc (startAngle, endAngle, arcR), juce::PathStrokeType (lineW * 0.6f, juce::PathStrokeType::curved,
                                                                          juce::PathStrokeType::rounded));

    if (! slider.isEnabled() || angle <= startAngle + 0.001f)
        return;

    // Value arc as light: a wide soft bloom under a thin line, both fading in from the start so the
    // brightest light sits at the tip.
    const auto span = angle - startAngle;
    const auto segments = juce::jmax (2, (int) std::ceil (span / 0.06f));
    const juce::PathStrokeType bloom (lineW * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::butt);
    const juce::PathStrokeType core  (lineW * 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::butt);

    for (int i = 0; i < segments; ++i)
    {
        const auto a0 = startAngle + span * (float) i / (float) segments;
        const auto a1 = startAngle + span * (float) (i + 1) / (float) segments + 0.004f;
        const auto f = std::pow ((float) (i + 1) / (float) segments, 1.6f);

        g.setColour (colour.withAlpha ((0.1f + 0.2f * glow) * f));
        g.strokePath (arc (a0, a1, arcR), bloom);
        g.setColour (colour.brighter (0.15f + 0.3f * glow).withAlpha (0.45f + 0.55f * f));
        g.strokePath (arc (a0, a1, arcR), core);
    }

    // A soft point of light at the tip, and a matching mark on the glass.
    const auto tip = centre.getPointOnCircumference (arcR, angle);
    const auto haloR = lineW * (2.4f + 1.4f * glow);
    juce::ColourGradient halo (colour.brighter (0.4f).withAlpha (0.55f + 0.35f * glow), tip,
                               colour.withAlpha (0.0f), tip.translated (haloR, 0.0f), true);
    g.setGradientFill (halo);
    g.fillEllipse (juce::Rectangle<float> (haloR * 2.0f, haloR * 2.0f).withCentre (tip));
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.fillEllipse (juce::Rectangle<float> (lineW * 0.9f, lineW * 0.9f).withCentre (tip));

    const auto mark = centre.getPointOnCircumference (discR * 0.62f, angle);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre (mark));
}

juce::Label* WaterfallLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = at::LookAndFeel::createSliderTextBox (slider);
    label->setFont (valueFont (12.0f));
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundWhenEditingColourId, shadow.withAlpha (0.5f));
    return label;
}

void WaterfallLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.isBeingEdited())
    {
        g.fillAll (label.findColour (juce::Label::backgroundWhenEditingColourId));
        return;
    }

    const auto font = getLabelFont (label);
    const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
    const auto alpha = label.isEnabled() ? 1.0f : 0.5f;

    g.setFont (font);
    drawHaloText (g, label.getText(), area.toFloat(), label.getJustificationType(),
                  label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
}
