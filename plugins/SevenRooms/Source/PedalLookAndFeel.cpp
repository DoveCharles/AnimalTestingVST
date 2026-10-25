#include "PedalLookAndFeel.h"

PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Palette::silkscreen);
    setColour (juce::Slider::textBoxHighlightColourId,  Palette::silkscreen.withAlpha (0.3f));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               Palette::silkscreen);
    setColour (juce::TextEditor::backgroundColourId,    Palette::enclosureDark);
    setColour (juce::TextEditor::highlightColourId,     Palette::silkscreen.withAlpha (0.3f));
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Silkscreened scale: one tick per detent on stepped knobs, otherwise 11 dots.
    const auto range = slider.getRange();
    const auto stepped = slider.getInterval() >= 1.0 && range.getLength() < 12.0;
    const auto ticks = stepped ? (int) range.getLength() : 10;
    g.setColour (Palette::silkscreen.withAlpha (0.85f));
    for (int i = 0; i <= ticks; ++i)
    {
        const auto a = startAngle + (float) i / (float) ticks * (endAngle - startAngle);
        if (stepped)
        {
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.86f, a),
                          centre.getPointOnCircumference (radius, a) }, 2.0f);
        }
        else
        {
            const auto dotR = (i == 0 || i == 5 || i == 10) ? 2.0f : 1.3f;
            g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f)
                               .withCentre (centre.getPointOnCircumference (radius - 2.0f, a)));
        }
    }

    const auto knobR = radius * 0.74f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    // Drop shadow on the enclosure.
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.1f).expanded (1.5f));

    // Ribbed skirt: fine alternating ridges, rotating with the knob.
    constexpr int ridges = 48;
    juce::Path skirt;
    for (int i = 0; i < ridges * 2; ++i)
    {
        const auto a = angle + (float) i * juce::MathConstants<float>::pi / (float) ridges;
        const auto r = (i % 2 == 0) ? knobR : knobR * 0.96f;
        const auto p = centre.getPointOnCircumference (r, a);
        i == 0 ? skirt.startNewSubPath (p) : skirt.lineTo (p);
    }
    skirt.closeSubPath();
    g.setColour (Palette::knob);
    g.fillPath (skirt);

    // Brushed-aluminium top, lit from the upper left.
    const auto cap = knob.reduced (knobR * 0.18f);
    g.setGradientFill (juce::ColourGradient (Palette::knobCap.brighter (0.35f), cap.getTopLeft(),
                                             Palette::knobCap.darker (0.45f), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (auto r = cap.getWidth() * 0.12f; r < cap.getWidth() * 0.5f; r += 2.5f)
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 0.6f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);

    // Black pointer line across the cap, out to the skirt.
    const auto tip  = centre.getPointOnCircumference (knobR * 0.92f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.15f, angle);
    g.setColour (Palette::pointer);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, knobR * 0.08f));
}
