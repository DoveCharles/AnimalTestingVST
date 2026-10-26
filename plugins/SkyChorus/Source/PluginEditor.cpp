#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;
}

void Treadle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto plate = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, down ? 1.5f : 0.0f);

    // Plate body: the same painted wash as the enclosure, shaded from above, with a dark hinge gap.
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path outline;
        outline.addRoundedRectangle (plate, 8.0f);
        g.reduceClipRegion (outline);
        g.drawImageAt (wash, (int) plate.getX(), (int) plate.getY());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), plate.getTopLeft(),
                                                 Palette::shadow.withAlpha (0.25f), plate.getBottomLeft(), false));
        g.fillRect (plate);
    }
    g.setColour (Palette::shadow.withAlpha (highlighted ? 0.9f : 0.6f));
    g.drawRoundedRectangle (plate, 8.0f, 2.0f);

    auto area = plate.reduced (18.0f, 12.0f);

    auto logoRow = area.removeFromTop (26.0f);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("ANIMAL TESTING", logoRow, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, logoRow, juce::Justification::centredRight);
    area.removeFromTop (8.0f);

    // Slate nameplate stripe across the plate, edged in the creature's mustard.
    auto stripe = juce::Rectangle<float> (plate.getX(), area.getY(), plate.getWidth(), 58.0f);
    g.setColour (Palette::nameplate);
    g.fillRect (stripe);
    g.setColour (Palette::mustard);
    g.fillRect (stripe.withHeight (2.0f));
    g.fillRect (stripe.withTop (stripe.getBottom() - 2.0f));
    stripe.reduce (18.0f, 0.0f);
    g.setColour (Palette::pink);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("SKY CHORUS", stripe, juce::Justification::centredLeft);
    auto caption = stripe.removeFromRight (150.0f);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Bi-Phase Stereo", caption.removeFromTop (caption.getHeight() * 0.5f),
                juce::Justification::bottomRight);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Analog Chorus", caption, juce::Justification::topRight);
    area.removeFromTop (58.0f + 14.0f);

    // Ribbed rubber pad where the foot lands.
    const auto pad = area.withTrimmedBottom (4.0f);
    g.setColour (Palette::rubber);
    g.fillRoundedRectangle (pad, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (auto y = pad.getY() + 8.0f; y < pad.getBottom() - 4.0f; y += 9.0f)
        g.drawLine (pad.getX() + 6.0f, y, pad.getRight() - 6.0f, y, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (pad, 6.0f, 1.0f);
}

void Treadle::resized()
{
    wash = PedalLookAndFeel::paintWash (getWidth(), getHeight(), 7);
}

StereoSwitch::StereoSwitch (juce::AudioProcessorValueTreeState& state)
    : attachment (*state.getParameter (ParamIDs::stereo),
                  [this] (float mode)
                  {
                      buttons[(size_t) juce::jlimit (0, 2, juce::roundToInt (mode))]
                          .setToggleState (true, juce::dontSendNotification);
                  })
{
    for (int i = 0; i < (int) buttons.size(); ++i)
    {
        auto& button = buttons[(size_t) i];
        button.setButtonText (StereoModes::switchLabels[i]);
        button.setTooltip (StereoModes::names[i]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (1);
        button.setColour (juce::TextButton::buttonColourId,   Palette::enclosureLight.withAlpha (0.45f));
        button.setColour (juce::TextButton::buttonOnColourId, Palette::nameplate);
        button.setColour (juce::TextButton::textColourOffId,  Palette::silkscreen);
        button.setColour (juce::TextButton::textColourOnId,   Palette::pink);
        button.onClick = [this, i]
        {
            if (buttons[(size_t) i].getToggleState())
                attachment.setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (button);
    }

    attachment.sendInitialUpdate();
}

void StereoSwitch::resized()
{
    auto area = getLocalBounds();
    const auto buttonWidth = area.getWidth() / (int) buttons.size();
    for (auto& button : buttons)
        button.setBounds (area.removeFromLeft (buttonWidth).reduced (1, 0));
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      chorusProcessor (p),
      speed     (p.apvts, ParamIDs::speed,     "Speed"),
      width     (p.apvts, ParamIDs::width,     "Width"),
      delayTime (p.apvts, ParamIDs::delayTime, "D.Time"),
      mix       (p.apvts, ParamIDs::mix,       "Mix"),
      stereoSwitch (p.apvts),
      treadleAttachment (p.apvts, ParamIDs::bypass, treadle)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &speed, &width, &delayTime, &mix })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        // Dark value text on the pale enclosure; set on the slider so the existing value box picks it up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::silkscreen);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    addAndMakeVisible (stereoSwitch);

    treadle.setTooltip ("Bypass");
    treadle.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (treadle);

    setSize (480, 560);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    // The check LED breathes with the sweep, so Speed can be read at a glance.
    const auto phase = chorusProcessor.getLfoPhase();
    const auto pulse = 0.7f + 0.3f * (1.0f - 2.0f * std::abs (phase - 0.5f));
    if (std::abs (pulse - ledPulse) > 0.01f)
    {
        ledPulse = pulse;
        repaint (ledArea.expanded (8));
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Painted enclosure with a faint top-down sheen.
    g.fillAll (Palette::silkscreen);
    const auto box = bounds.reduced (6.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path outline;
        outline.addRoundedRectangle (box, 12.0f);
        g.reduceClipRegion (outline);
        g.drawImageAt (wash, (int) box.getX(), (int) box.getY());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.1f), box.getTopLeft(),
                                                 Palette::shadow.withAlpha (0.15f), box.getBottomLeft(), false));
        g.fillRect (box);
    }
    g.setColour (Palette::shadow);
    g.drawRoundedRectangle (box, 12.0f, 2.0f);

    // Check LED, set in a cobalt and mustard bezel like the creature's eye: lit while engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = ! treadle.getToggleState();
    const auto ledColour = Palette::ledOff.interpolatedWith (Palette::ledOn, ledPulse);
    g.setColour (Palette::mustard);
    g.fillEllipse (led.expanded (4.5f));
    g.setColour (Palette::cobalt);
    g.fillEllipse (led.expanded (3.0f));
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.25f * ledPulse));
        g.fillEllipse (led.expanded (8.0f));
    }
    g.setColour (engaged ? ledColour : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (engaged ? 0.6f * ledPulse : 0.15f));
    g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-1.5f, -1.5f));

    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("CHECK", led.withWidth (60.0f).translated (led.getWidth() + 8.0f, 0.0f).expanded (0.0f, 4.0f),
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("STEREO", stereoSwitch.getBounds().toFloat().withX (stereoSwitch.getX() - 56.0f).withWidth (50.0f),
                juce::Justification::centredRight);
}

void PluginEditor::resized()
{
    wash = PedalLookAndFeel::paintWash (getWidth() - 12, getHeight() - 12, 3);

    auto area = getLocalBounds().reduced (24, 20);

    auto knobRow = area.removeFromTop (150);
    const auto knobWidth = knobRow.getWidth() / 4;
    for (auto* knob : { &speed, &width, &delayTime, &mix })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (4, 0));

    area.removeFromTop (12);
    auto ledRow = area.removeFromTop (22);
    ledArea = juce::Rectangle<int> (12, 12).withCentre ({ ledRow.getX() + 16, ledRow.getCentreY() });
    stereoSwitch.setBounds (ledRow.removeFromRight (216));

    area.removeFromTop (10);
    treadle.setBounds (area.withTrimmedLeft (-6).withTrimmedRight (-6).withTrimmedBottom (-4));
}
