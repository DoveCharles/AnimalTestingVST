#include "PluginEditor.h"
#include "CourtPainting.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    juce::RangedAudioParameter& parameter (PluginProcessor& p, const char* paramId)
    {
        auto* param = p.apvts.getParameter (paramId);
        jassert (param != nullptr);
        return *param;
    }

}

//==============================================================================
LeverSwitch::LeverSwitch (juce::RangedAudioParameter& parameter, const juce::StringArray& positionLabels)
    : labels (positionLabels),
      attachment (parameter, [this] (float value)
      {
          position = juce::jlimit (0, labels.size() - 1, juce::roundToInt (value));
          repaint();
          if (onChange != nullptr)
              onChange (position);
      })
{
    attachment.sendInitialUpdate();
}

void LeverSwitch::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto count = labels.size();
    const auto slotWidth = area.getWidth() / (float) count;

    auto labelRow = area.removeFromTop (16.0f);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (int i = 0; i < count; ++i)
    {
        g.setColour (Palette::silkscreen.withAlpha (i == position ? 1.0f : 0.55f));
        g.drawText (labels[i], labelRow.removeFromLeft (slotWidth), juce::Justification::centred);
    }

    // Chrome bushing plate with a dark slot the lever travels along.
    const auto plate = area.reduced (slotWidth * 0.25f, 4.0f);
    g.setGradientFill (juce::ColourGradient (Palette::chrome.brighter (0.3f), plate.getTopLeft(),
                                             Palette::chrome.darker (0.4f), plate.getBottomLeft(), false));
    g.fillRoundedRectangle (plate, plate.getHeight() * 0.5f);
    const auto slot = plate.reduced (plate.getHeight() * 0.3f);
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (slot, slot.getHeight() * 0.5f);

    // Lever tip, a chrome ball over the selected position.
    const auto x = slotWidth * ((float) position + 0.5f);
    const auto ballR = plate.getHeight() * 0.42f;
    const auto ball = juce::Rectangle<float> (ballR * 2.0f, ballR * 2.0f).withCentre ({ x, plate.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (ball.translated (0.0f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, ball.getTopLeft(),
                                             Palette::chrome.darker (0.5f), ball.getBottomRight(), false));
    g.fillEllipse (ball);
}

void LeverSwitch::mouseDown (const juce::MouseEvent& e)
{
    const auto count = labels.size();
    const auto index = juce::jlimit (0, count - 1, e.x * count / juce::jmax (1, getWidth()));
    attachment.setValueAsCompleteGesture ((float) index);
}

//==============================================================================
Stompswitch::Stompswitch (juce::RangedAudioParameter& bypass, std::atomic<float>& switchMode)
    : switchModeParam (switchMode),
      attachment (bypass, [this] (float value)
      {
          engaged = value < 0.5f;
          repaint();
          if (onChange != nullptr)
              onChange();
      })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Stompswitch::setEngaged (bool shouldBeEngaged)
{
    if (shouldBeEngaged != engaged)
        attachment.setValueAsCompleteGesture (shouldBeEngaged ? 0.0f : 1.0f);
}

void Stompswitch::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    setEngaged (isMomentary() ? true : ! engaged);
    repaint();
}

void Stompswitch::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    if (isMomentary())
        setEngaged (false);
    repaint();
}

void Stompswitch::paint (juce::Graphics& g)
{
    const auto size = (float) juce::jmin (getWidth(), getHeight());
    const auto centre = getLocalBounds().toFloat().getCentre();

    // Hex nut on the enclosure.
    juce::Path nut;
    const auto nutR = size * 0.48f;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = centre.getPointOnCircumference (nutR, juce::MathConstants<float>::pi / 6.0f
                                                             + (float) i * juce::MathConstants<float>::pi / 3.0f);
        i == 0 ? nut.startNewSubPath (p) : nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (nut, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (Palette::chrome.brighter (0.2f), centre.translated (0.0f, -nutR),
                                             Palette::chrome.darker (0.5f), centre.translated (0.0f, nutR), false));
    g.fillPath (nut);

    // Plunger cap, pushed in a little while held.
    const auto capR = size * (pressed ? 0.3f : 0.32f);
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cap.getTopLeft(),
                                             Palette::chrome.darker (0.6f), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cap, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (cap.reduced (capR * 0.25f), 1.0f);
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      directionParam (at::params::raw (p.apvts, ParamIDs::direction)),
      shift      (p.apvts, ParamIDs::shift,      "Shift"),
      blend      (p.apvts, ParamIDs::blend,      "Blend"),
      expression (p.apvts, ParamIDs::expression, "Expression"),
      glide      (p.apvts, ParamIDs::glide,      "Glide"),
      direction  (parameter (p, ParamIDs::direction),  { "DOWN", "DUAL", "UP" }),
      switchMode (parameter (p, ParamIDs::switchMode), { "LATCH", "MOM" }),
      stompswitch (parameter (p, ParamIDs::bypass), at::params::raw (p.apvts, ParamIDs::switchMode))
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &shift, &blend, &expression, &glide })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::silkscreen);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    // The selector's positions are printed around it and the display reads out the shift.
    shift.getSlider().setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    shift.getLabel().setVisible (false);
    shift.getSlider().onValueChange      = [this] { repaint (displayArea); };
    expression.getSlider().onValueChange = [this] { repaint (displayArea); };
    direction.onChange = [this] (int) { repaint (displayArea); };

    // A momentary switch rests off: the effect only sounds while it is held.
    switchMode.onChange = [this] (int mode)
    {
        if (mode == SwitchModes::momentary)
            stompswitch.setEngaged (false);
    };

    stompswitch.onChange = [this] { repaint (ledArea.expanded (8)); };

    for (auto* c : std::initializer_list<juce::Component*> { &direction, &switchMode, &stompswitch })
        addAndMakeVisible (c);

    setSize (460, 640);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // The painting is drawn once per display scale and cached: it is a lot of paths, and the
    // display and LED repaint whenever a control moves.
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! painting.isValid() || ! juce::approximatelyEqual (paintingScale, scale))
    {
        painting = juce::Image (juce::Image::ARGB, juce::roundToInt (bounds.getWidth() * scale),
                                juce::roundToInt (bounds.getHeight() * scale), true);
        juce::Graphics pg (painting);
        pg.addTransform (juce::AffineTransform::scale (scale));
        CourtPainting::paint (pg, bounds);
        paintingScale = scale;
    }
    g.drawImageTransformed (painting, juce::AffineTransform::scale (1.0f / scale));

    paintShiftLabels (g);
    paintDisplay (g);

    // Status LED: lit while the effect is engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = stompswitch.isEngaged();
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.3f));
        g.fillEllipse (led.expanded (6.0f));
    }
    g.setColour (engaged ? Palette::ledOn : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (engaged ? 0.6f : 0.15f));
    g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-1.5f, -1.5f));
    g.setColour (Palette::chrome.darker (0.3f));
    g.drawEllipse (led.expanded (1.5f), 1.5f);

    // Nameplate along the bottom, either side of the footswitch.
    auto name = nameArea.toFloat();
    CourtPainting::paintSeal (g, juce::Rectangle<float> (34.0f, 34.0f).withPosition (name.getRight() - 40.0f, name.getY() + 6.0f));
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold).withHorizontalScale (0.85f));
    g.drawText ("PITCH-POT", name.removeFromTop (44.0f), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("POLYPHONIC PITCH SHIFTER", name.removeFromTop (16.0f), juce::Justification::centredLeft);
    g.setColour (Palette::silkscreen.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ANIMAL TESTING  v" JucePlugin_VersionString, name.removeFromTop (16.0f), juce::Justification::centredLeft);
}

void PluginEditor::paintShiftLabels (juce::Graphics& g)
{
    const auto& slider = shift.getSlider();
    const auto knobArea = slider.getBounds().translated (shift.getX(), shift.getY()).toFloat().reduced (4.0f);
    const auto centre = knobArea.getCentre();
    const auto radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto count = Shifts::dialLabels.size();

    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (int i = 0; i < count; ++i)
    {
        const auto a = params.startAngleRadians
                     + (float) i / (float) (count - 1) * (params.endAngleRadians - params.startAngleRadians);
        const auto at = centre.getPointOnCircumference (radius + 20.0f, a);
        g.drawText (Shifts::dialLabels[i], juce::Rectangle<float> (40.0f, 14.0f).withCentre (at),
                    juce::Justification::centred);
    }
}

void PluginEditor::paintDisplay (juce::Graphics& g)
{
    const auto area = displayArea.toFloat();
    g.setColour (Palette::display);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (area, 5.0f, 1.5f);

    const auto shiftIndex = (int) shift.getSlider().getValue();
    const auto amount = Shifts::semitones (shiftIndex) * (float) expression.getSlider().getValue() / 100.0f;
    const auto dir = (int) directionParam.load();
    const juce::String sign = dir == Directions::up ? "+" : (dir == Directions::down ? "-" : juce::String::fromUTF8 ("\xc2\xb1"));

    juce::String readout;
    if (shiftIndex == Shifts::detune)
        readout = sign + juce::String (juce::roundToInt (amount * 100.0f)) + " ct";
    else
        readout = sign + (std::abs (amount - std::round (amount)) < 0.05f ? juce::String (juce::roundToInt (amount))
                                                                           : juce::String (amount, 1)) + " st";

    auto text = area.reduced (10.0f, 4.0f);
    g.setColour (Palette::displayText);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (readout, text.removeFromTop (text.getHeight() * 0.62f), juce::Justification::centred);
    g.setColour (Palette::displayText.withAlpha (0.65f));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (Shifts::names[shiftIndex].toUpperCase() + "  " + Directions::names[dir].toUpperCase(),
                text, juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (26, 22);

    // The big Shift selector, with room for its labels all round.
    auto top = area.removeFromTop (220);
    shift.setBounds (top.withSizeKeepingCentre (170, 190).translated (0, 6));

    displayArea = area.removeFromTop (54).withSizeKeepingCentre (200, 50);
    area.removeFromTop (14);

    auto knobRow = area.removeFromTop (130);
    const auto knobWidth = knobRow.getWidth() / 3;
    for (auto* knob : { &blend, &expression, &glide })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (8, 0));

    area.removeFromTop (12);
    auto switchRow = area.removeFromTop (44);
    direction.setBounds (switchRow.removeFromLeft (150));
    switchMode.setBounds (switchRow.removeFromRight (110));
    ledArea = juce::Rectangle<int> (14, 14).withCentre (switchRow.getCentre().translated (0, 6));

    area.removeFromTop (14);
    stompswitch.setBounds (area.removeFromRight (area.getHeight()).reduced (4));
    nameArea = area.withSizeKeepingCentre (area.getWidth(), 76);
}
