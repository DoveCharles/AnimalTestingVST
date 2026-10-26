#include "PedalLookAndFeel.h"

PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Palette::silkscreen);
    setColour (juce::Slider::textBoxHighlightColourId,  Palette::pink.withAlpha (0.4f));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               Palette::silkscreen);
    setColour (juce::TextEditor::backgroundColourId,    Palette::enclosureLight);
    setColour (juce::TextEditor::highlightColourId,     Palette::pink.withAlpha (0.4f));
}

juce::Image PedalLookAndFeel::paintWash (int width, int height, juce::int64 seed)
{
    juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true);
    juce::Graphics g (image);
    juce::Random random (seed);
    const auto w = (float) width, h = (float) height;

    g.fillAll (Palette::enclosure);

    const juce::Colour washes[] { Palette::enclosureLight, Palette::enclosureDark, juce::Colour (0xff6b8790),
                                  juce::Colour (0xff8aa5a6), juce::Colour (0xff647a92) };

    // Soft clouds of lighter and darker paint for the mottling.
    for (int i = 0; i < (int) (w * h / 6000.0f); ++i)
    {
        const auto size = 60.0f + random.nextFloat() * 160.0f;
        const auto centre = juce::Point<float> (random.nextFloat() * w, random.nextFloat() * h);
        const auto colour = washes[random.nextInt (5)];
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.1f + random.nextFloat() * 0.12f), centre,
                                                 colour.withAlpha (0.0f), centre.translated (size * 0.5f, 0.0f), true));
        g.fillEllipse (juce::Rectangle<float> (size, size * (0.5f + random.nextFloat() * 0.5f)).withCentre (centre));
    }

    // Dry-brush strokes on top, mostly sweeping across, a few going every which way.
    for (int i = 0; i < (int) (w * h / 1500.0f); ++i)
    {
        const auto length = 20.0f + random.nextFloat() * 90.0f;
        const auto thickness = 3.0f + random.nextFloat() * 12.0f;
        const auto angle = random.nextInt (5) == 0 ? random.nextFloat() * juce::MathConstants<float>::pi
                                                   : -0.35f + random.nextFloat() * 0.7f;

        juce::Path stroke;
        stroke.addRoundedRectangle (-length * 0.5f, -thickness * 0.5f, length, thickness, thickness * 0.5f);
        stroke.applyTransform (juce::AffineTransform::rotation (angle)
                                   .translated (random.nextFloat() * w, random.nextFloat() * h));

        g.setColour (washes[random.nextInt (5)].withAlpha (0.08f + random.nextFloat() * 0.14f));
        g.fillPath (stroke);
    }

    // The cover's stray flecks: mostly mustard and cobalt, a little red and pink.
    const juce::Colour flecks[] { Palette::mustard, Palette::cobalt, Palette::fleck, Palette::pink, Palette::cobalt };
    for (int i = 0; i < (int) (w * h / 9000.0f); ++i)
    {
        const auto size = 1.5f + random.nextFloat() * 3.5f;
        g.setColour (flecks[random.nextInt (5)].withAlpha (0.35f + random.nextFloat() * 0.4f));
        g.fillEllipse (random.nextFloat() * w, random.nextFloat() * h, size * (1.0f + random.nextFloat()), size);
    }

    return image;
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
    g.setColour (Palette::shadow.darker (0.6f).withAlpha (0.4f));
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

    // Flat top with a soft highlight.
    const auto cap = knob.reduced (knobR * 0.16f);
    g.setGradientFill (juce::ColourGradient (Palette::shadow.brighter (0.35f), cap.getTopLeft(),
                                             Palette::knob, cap.getCentre(), true));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cap, 1.0f);

    // Pink pointer line, from the centre out to the skirt.
    const auto tip  = centre.getPointOnCircumference (knobR * 0.92f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.15f, angle);
    g.setColour (Palette::pink);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, knobR * 0.08f));
}
