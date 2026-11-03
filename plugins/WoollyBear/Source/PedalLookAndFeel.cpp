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
                                         float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Silkscreened scale: 11 dots, 0 to 10.
    g.setColour (Palette::silkscreen.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto dotR = (i == 0 || i == 5 || i == 10) ? 2.0f : 1.3f;
        g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f)
                           .withCentre (centre.getPointOnCircumference (radius - 2.0f, a)));
    }

    const auto knobR = radius * 0.78f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    // Drop shadow on the enclosure.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.08f).expanded (1.5f));

    // Knurled skirt: alternating ridges around the edge.
    constexpr int ridges = 36;
    juce::Path skirt;
    for (int i = 0; i < ridges * 2; ++i)
    {
        const auto a = angle + (float) i * juce::MathConstants<float>::pi / (float) ridges;
        const auto r = (i % 2 == 0) ? knobR : knobR * 0.95f;
        const auto p = centre.getPointOnCircumference (r, a);
        i == 0 ? skirt.startNewSubPath (p) : skirt.lineTo (p);
    }
    skirt.closeSubPath();
    g.setColour (Palette::knob);
    g.fillPath (skirt);

    // Cap with a soft top-left highlight.
    const auto cap = knob.reduced (knobR * 0.14f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), cap.getTopLeft(),
                                             Palette::knob, cap.getCentre(), true));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);

    // Pointer line, painted white like the originals.
    const auto tip  = centre.getPointOnCircumference (knobR * 0.9f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.35f, angle);
    g.setColour (Palette::silkscreen);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, knobR * 0.08f));
}
