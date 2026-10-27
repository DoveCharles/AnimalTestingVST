#include "PedalLookAndFeel.h"

PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Palette::cream);
    setColour (juce::Slider::textBoxHighlightColourId,  Palette::blush.withAlpha (0.4f));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               Palette::cream);
    setColour (juce::TextEditor::backgroundColourId,    Palette::ink);
    setColour (juce::TextEditor::highlightColourId,     Palette::blush.withAlpha (0.4f));
    setColour (juce::TooltipWindow::backgroundColourId, Palette::ink);
    setColour (juce::TooltipWindow::textColourId,       Palette::cream);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    const auto range = slider.getRange();
    const auto stepped = slider.getInterval() >= 1.0 && range.getLength() < 12.0;

    // Value arc: a dim track, then a mint-to-pink fill. Stepped knobs get a bead per detent instead.
    const auto arcR = radius - 3.0f;
    if (stepped)
    {
        const auto steps = (int) range.getLength();
        for (int i = 0; i <= steps; ++i)
        {
            const auto a = startAngle + (float) i / (float) steps * (endAngle - startAngle);
            const auto lit = i == juce::roundToInt (sliderPos * (float) steps);
            const auto beadR = lit ? 3.6f : 2.2f;
            g.setColour (lit ? Palette::hotPink : Palette::deepTeal.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (beadR * 2.0f, beadR * 2.0f)
                               .withCentre (centre.getPointOnCircumference (arcR, a)));
        }
    }
    else
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Palette::deepTeal.withAlpha (0.12f));
        g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path fill;
            fill.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setGradientFill (juce::ColourGradient (Palette::teal, bounds.getBottomLeft(),
                                                     Palette::hotPink, bounds.getBottomRight(), false));
            g.strokePath (fill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Gumdrop: a cream cap with a soft drop shadow, a little sheen and a squishy rim.
    const auto knobR = radius * 0.66f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    g.setColour (Palette::deepTeal.withAlpha (0.3f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.12f).expanded (1.0f));

    g.setGradientFill (juce::ColourGradient (Palette::cream, knob.getCentreX(), knob.getY(),
                                             Palette::aqua, knob.getCentreX(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (Palette::deepTeal.withAlpha (0.55f));
    g.drawEllipse (knob.reduced (0.5f), 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 0.55f, knobR * 0.32f)
                       .withCentre ({ centre.x - knobR * 0.32f, centre.y - knobR * 0.48f }));

    // Pointer: a round candy dot near the rim, turning with the knob.
    const auto dotR = juce::jmax (3.0f, knobR * 0.15f);
    const auto dot = centre.getPointOnCircumference (knobR * 0.62f, angle);
    g.setColour (Palette::ink);
    g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot));
    g.setColour (Palette::blush.withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (dotR, dotR).withCentre (dot.translated (-dotR * 0.2f, -dotR * 0.2f)));
}
