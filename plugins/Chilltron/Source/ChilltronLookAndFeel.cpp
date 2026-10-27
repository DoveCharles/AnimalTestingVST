#include "ChilltronLookAndFeel.h"

ChilltronLookAndFeel::ChilltronLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::cream);
    setColour (juce::Slider::rotarySliderFillColourId,    Palette::red);
    setColour (juce::Slider::rotarySliderOutlineColourId, Palette::creamShade);
    setColour (juce::Slider::thumbColourId,               Palette::cream);
    setColour (juce::Label::textColourId,                 Palette::ink);
    setColour (juce::PopupMenu::backgroundColourId,       Palette::glass);
    setColour (juce::PopupMenu::textColourId,             Palette::amber);
    setColour (juce::PopupMenu::headerTextColourId,       Palette::amber.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::amber);
    setColour (juce::PopupMenu::highlightedTextColourId,  Palette::glass);
}

void ChilltronLookAndFeel::paintWood (juce::Graphics& g, juce::Rectangle<float> r, int seed, bool vertical)
{
    const auto along  = vertical ? r.getHeight() : r.getWidth();
    const auto across = vertical ? r.getWidth() : r.getHeight();

    // Lighter in the middle of the board, as a varnished edge catches the light.
    juce::ColourGradient base (Palette::walnutDark, r.getX(), r.getY(),
                               Palette::walnutDark,
                               vertical ? r.getRight() : r.getX(),
                               vertical ? r.getY() : r.getBottom(), false);
    base.addColour (0.45, Palette::walnut);
    base.addColour (0.6, Palette::walnutLight.interpolatedWith (Palette::walnut, 0.5f));
    g.setGradientFill (base);
    g.fillRect (r);

    juce::Random random (seed);
    for (int i = 0; i < (int) (across / 2.5f); ++i)
    {
        const auto offset = random.nextFloat() * across;
        const auto wave = 1.0f + random.nextFloat() * 3.0f;
        const auto period = 80.0f + random.nextFloat() * 220.0f;
        const auto phase = random.nextFloat() * juce::MathConstants<float>::twoPi;

        juce::Path grain;
        for (float t = 0.0f; t <= along; t += 6.0f)
        {
            const auto d = offset + wave * std::sin (phase + t / period * juce::MathConstants<float>::twoPi);
            const auto p = vertical ? juce::Point<float> (r.getX() + d, r.getY() + t)
                                    : juce::Point<float> (r.getX() + t, r.getY() + d);
            if (t == 0.0f) grain.startNewSubPath (p);
            else           grain.lineTo (p);
        }

        const auto dark = random.nextBool();
        g.setColour ((dark ? Palette::walnutDark : Palette::walnutLight).withAlpha (0.12f + random.nextFloat() * 0.2f));
        g.strokePath (grain, juce::PathStrokeType (0.6f + random.nextFloat() * 0.9f));
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRect (r, 1.0f);
}

void ChilltronLookAndFeel::paintGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Palette::glassEdge);
    g.fillRoundedRectangle (r, 6.0f);

    const auto inner = r.reduced (3.0f);
    g.setColour (Palette::glass);
    g.fillRoundedRectangle (inner, 4.0f);

    // Faint reflection across the top of the window.
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), inner.getX(), inner.getY(),
                                juce::Colours::transparentWhite, inner.getX(), inner.getY() + inner.getHeight() * 0.45f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (inner, 4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (inner, 4.0f, 1.0f);
}

void ChilltronLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Scale printed on the fascia: eleven ticks, 0 to 10, the ends heavier.
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto outer = centre.getPointOnCircumference (radius, a);
        const auto inner = centre.getPointOnCircumference (radius * (i % 5 == 0 ? 0.80f : 0.86f), a);
        g.setColour (Palette::ink.withAlpha (i % 5 == 0 ? 0.85f : 0.45f));
        g.drawLine ({ inner, outer }, i % 5 == 0 ? 1.6f : 1.0f);
    }

    // The red arc of how far it's turned, just inside the ticks.
    {
        const auto arcR = radius * 0.74f;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? Palette::red : Palette::inkDim);
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto knobR = radius * 0.64f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    // Drop shadow on the panel.
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (knob.translated (0.0f, knobR * 0.12f).expanded (1.0f));

    // Fluted skirt: a ring of bumps, turning with the knob.
    {
        juce::Path flutes;
        constexpr int count = 18;
        for (int i = 0; i < count; ++i)
        {
            const auto a = angle + (float) i / (float) count * juce::MathConstants<float>::twoPi;
            const auto c = centre.getPointOnCircumference (knobR * 0.9f, a);
            flutes.addEllipse (juce::Rectangle<float> (knobR * 0.28f, knobR * 0.28f).withCentre (c));
        }
        flutes.addEllipse (knob.reduced (knobR * 0.1f));
        g.setColour (Palette::bakelite);
        g.fillPath (flutes);
    }

    // Domed cap.
    const auto cap = knob.reduced (knobR * 0.2f);
    juce::ColourGradient dome (Palette::bakelite.brighter (0.55f), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.2f,
                               Palette::bakelite, cap.getCentreX(), cap.getBottom(), true);
    g.setGradientFill (dome);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cap, 0.8f);

    // Cream pointer line inlaid in the cap.
    const auto tip  = centre.getPointOnCircumference (knobR * 0.95f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.35f, angle);
    g.setColour (slider.isMouseOverOrDragging() ? juce::Colours::white : Palette::cream);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, knobR * 0.1f));
}

void ChilltronLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        at::LookAndFeel::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    // A fader in a slot, on the glass: amber fill grows from A towards B.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto slot = bounds.withSizeKeepingCentre (bounds.getWidth(), 4.0f);

    g.setColour (Palette::amberDim.withAlpha (0.6f));
    g.fillRoundedRectangle (slot, 2.0f);

    g.setColour (Palette::amber.withAlpha (0.85f));
    g.fillRoundedRectangle (slot.withRight (sliderPos), 2.0f);

    // Centre detent mark.
    g.setColour (Palette::amber.withAlpha (0.4f));
    g.fillRect (juce::Rectangle<float> (1.0f, 10.0f).withCentre ({ slot.getCentreX(), slot.getCentreY() }));

    // The cap: a little black block with a cream line.
    const auto cap = juce::Rectangle<float> (14.0f, juce::jmin (22.0f, bounds.getHeight())).withCentre ({ sliderPos, bounds.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.5f);
    juce::ColourGradient body (Palette::bakelite.brighter (0.4f), cap.getX(), cap.getY(),
                               Palette::bakelite, cap.getX(), cap.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (slider.isMouseOverOrDragging() ? juce::Colours::white : Palette::cream);
    g.fillRect (juce::Rectangle<float> (2.0f, cap.getHeight() - 8.0f).withCentre (cap.getCentre()));
}

void ChilltronLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // A rocker switch with a red lamp above it.
    const auto bounds = button.getLocalBounds().toFloat();
    const auto on = button.getToggleState();

    auto area = bounds;
    const auto lampArea = area.removeFromTop (14.0f);
    const auto textArea = area.removeFromBottom (18.0f);

    const auto lamp = juce::Rectangle<float> (8.0f, 8.0f).withCentre (lampArea.getCentre());
    if (on)
    {
        g.setColour (Palette::red.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (4.0f));
    }
    g.setColour (on ? Palette::red.brighter (0.4f) : Palette::red.darker (0.7f));
    g.fillEllipse (lamp);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (lamp, 0.8f);

    const auto bezel = area.withSizeKeepingCentre (26.0f, juce::jmin (40.0f, area.getHeight() - 4.0f));
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (bezel, 3.0f);

    const auto rocker = bezel.reduced (3.0f);
    const auto half = rocker.getHeight() * 0.5f;
    auto up = rocker.withHeight (half);
    auto down = rocker.withTrimmedTop (half);

    // The pressed half sits lower and darker.
    g.setColour (Palette::bakelite.brighter (on ? 0.1f : 0.35f));
    g.fillRoundedRectangle (up, 2.0f);
    g.setColour (Palette::bakelite.brighter (on ? 0.35f : 0.1f));
    g.fillRoundedRectangle (down, 2.0f);
    g.setColour (Palette::cream.withAlpha (highlighted ? 1.0f : 0.8f));
    g.fillRect (juce::Rectangle<float> (8.0f, 1.5f).withCentre ((on ? up : down).getCentre()));

    g.setColour (Palette::ink);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (button.getButtonText().toUpperCase(), textArea, juce::Justification::centred);
}

void ChilltronLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Palette::glass);
    g.setColour (Palette::amberDim);
    g.drawRect (0, 0, width, height, 1);
}
