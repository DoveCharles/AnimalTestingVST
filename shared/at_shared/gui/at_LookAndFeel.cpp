namespace at
{
    LookAndFeel::LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
        setColour (juce::Slider::rotarySliderFillColourId,    Palette::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, Palette::track);
        setColour (juce::Slider::thumbColourId,               Palette::text);
        setColour (juce::Slider::textBoxTextColourId,         Palette::text);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,    Palette::accent.withAlpha (0.4f));
        setColour (juce::Label::textColourId,                 Palette::text);
        setColour (juce::TextEditor::backgroundColourId,      Palette::panel);
        setColour (juce::TextEditor::textColourId,            Palette::text);
        setColour (juce::TextEditor::outlineColourId,         Palette::track);
        setColour (juce::ComboBox::backgroundColourId,        Palette::panel);
        setColour (juce::ComboBox::textColourId,              Palette::text);
        setColour (juce::ComboBox::outlineColourId,           Palette::track);
        setColour (juce::ComboBox::arrowColourId,             Palette::textDim);
        setColour (juce::PopupMenu::backgroundColourId,       Palette::panel);
        setColour (juce::PopupMenu::textColourId,             Palette::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
        setColour (juce::PopupMenu::highlightedTextColourId,  Palette::background);
        setColour (juce::TextButton::buttonColourId,          Palette::panel);
        setColour (juce::TextButton::textColourOffId,         Palette::text);
        setColour (juce::TextButton::textColourOnId,          Palette::accent);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto lineW  = juce::jmax (3.0f, radius * 0.12f);
        const auto arcR   = radius - lineW * 0.5f;
        const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

        auto arc = [&] (float from, float to)
        {
            juce::Path p;
            p.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, from, to, true);
            return p;
        };

        const juce::PathStrokeType stroke { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (arc (startAngle, endAngle), stroke);

        if (slider.isEnabled())
        {
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            g.strokePath (arc (startAngle, angle), stroke);
        }

        const auto knobR = arcR - lineW * 1.6f;
        g.setColour (Palette::panel);
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

        const auto tip  = centre.getPointOnCircumference (knobR * 0.8f, angle);
        const auto base = centre.getPointOnCircumference (knobR * 0.25f, angle);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.drawLine ({ base, tip }, juce::jmax (2.0f, lineW * 0.6f));
    }

    juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions (13.0f));
        label->setJustificationType (juce::Justification::centred);
        return label;
    }

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour,
                                            bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = colour;
        if (down)             fill = fill.brighter (0.2f);
        else if (highlighted) fill = fill.brighter (0.1f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (Palette::track);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    juce::Font LookAndFeel::getLabelFont (juce::Label& label)
    {
        return label.getFont();
    }

    juce::Rectangle<int> LookAndFeel::drawEditorChrome (juce::Graphics& g, juce::Rectangle<int> bounds,
                                                       const juce::String& title, const juce::String& version)
    {
        g.fillAll (Palette::background);

        auto header = bounds.removeFromTop (headerHeight);
        g.setColour (Palette::panel);
        g.fillRect (header);
        g.setColour (Palette::accent);
        g.fillRect (header.removeFromBottom (2));

        header = header.reduced (14, 0);
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), header, juce::Justification::centredLeft);

        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("ANIMAL TESTING  v" + version, header, juce::Justification::centredRight);

        return bounds;
    }
}
