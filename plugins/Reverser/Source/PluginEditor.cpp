#include "PluginEditor.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;

    juce::RangedAudioParameter& parameter (PluginProcessor& p, const char* id)
    {
        auto* param = p.apvts.getParameter (id);
        jassert (param != nullptr);
        return *param;
    }
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (18.0f, 6.0f);

    // Maker's logo, top left.
    auto logoRow = area.removeFromTop (26.0f);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.drawText ("ANIMAL TESTING", logoRow, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, logoRow, juce::Justification::centredRight);
    area.removeFromTop (8.0f);

    // Black nameplate stripe, edge to edge, with the name, a rewind mark and the caption.
    auto stripe = juce::Rectangle<float> (0.0f, area.getY(), (float) getWidth(), 58.0f);
    g.setColour (Palette::nameplate);
    g.fillRect (stripe);
    g.setColour (Palette::accent);
    g.fillRect (stripe.withHeight (3.0f));
    g.fillRect (stripe.withTrimmedTop (stripe.getHeight() - 3.0f));
    stripe.reduce (18.0f, 0.0f);

    const juce::Font nameFont (juce::FontOptions (34.0f, juce::Font::bold).withHorizontalScale (0.9f));
    g.setColour (Palette::nameText);
    g.setFont (nameFont);
    g.drawText ("REVERSER", stripe, juce::Justification::centredLeft);

    const auto nameWidth = juce::GlyphArrangement::getStringWidth (nameFont, "REVERSER");
    const auto mark = juce::Rectangle<float> (26.0f, 14.0f).withPosition (stripe.getX() + nameWidth + 12.0f,
                                                                          stripe.getCentreY() - 7.0f);
    juce::Path rewind;
    for (auto x : { mark.getX(), mark.getCentreX() })
        rewind.addTriangle (x + 13.0f, mark.getY(), x + 13.0f, mark.getBottom(), x, mark.getCentreY());
    g.setColour (Palette::accent);
    g.fillPath (rewind);

    auto caption = stripe.removeFromRight (150.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Reverse Reverb", caption.removeFromTop (caption.getHeight() * 0.5f),
                juce::Justification::bottomRight);
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("& Delay", caption, juce::Justification::topRight);
    area.removeFromTop (58.0f);

    // Chrome stomp switch: hex nut, then the domed cap; it sinks a little while held.
    const auto centre = area.getCentre();
    const auto nutR = juce::jmin (area.getHeight() * 0.36f, 44.0f);
    juce::Path nut;
    nut.addPolygon (centre, 6, nutR, juce::MathConstants<float>::pi / 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (nut, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (Palette::chrome, centre.translated (-nutR, -nutR),
                                             Palette::chrome.darker (0.7f), centre.translated (nutR, nutR), false));
    g.fillPath (nut);

    const auto capR = nutR * 0.62f * (down ? 0.96f : 1.0f);
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cap.getTopLeft(),
                                             Palette::chrome.darker (down ? 0.8f : 0.5f), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.7f : 0.45f));
    g.drawEllipse (cap, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillEllipse (cap.reduced (capR * 0.55f).translated (-capR * 0.25f, -capR * 0.25f));
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      time     (p.apvts, ParamIDs::time,     "Time"),
      division (p.apvts, ParamIDs::division, "Time"),
      repeats  (p.apvts, ParamIDs::repeats,  "Repeats"),
      tone     (p.apvts, ParamIDs::tone,     "Tone"),
      mix      (p.apvts, ParamIDs::mix,      "Mix"),
      modeAttachment (parameter (p, ParamIDs::mode), [this] (float value)
      {
          for (int i = 0; i < (int) modeButtons.size(); ++i)
              modeButtons[(size_t) i].setToggleState (i == juce::roundToInt (value), juce::dontSendNotification);
      }),
      syncAttachment (parameter (p, ParamIDs::sync), [this] (float value)
      {
          const auto synced = value > 0.5f;
          sync.setToggleState (synced, juce::dontSendNotification);
          time.setVisible (! synced);
          division.setVisible (synced);
      }),
      tailsAttachment (p.apvts, ParamIDs::tails, tails),
      footswitchAttachment (p.apvts, ParamIDs::bypass, footswitch)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &time, &division, &repeats, &tone, &mix })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        // Pale value text on the dark enclosure; set on the slider so the existing value box picks it up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::silkscreen);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addChildComponent (*knob);
    }
    for (auto* knob : { &repeats, &tone, &mix })
        knob->setVisible (true);

    for (int i = 0; i < (int) modeButtons.size(); ++i)
    {
        auto& button = modeButtons[(size_t) i];
        button.setButtonText (Modes::names[i].toUpperCase());
        button.setRadioGroupId (1);
        button.onClick = [this, i] { modeAttachment.setValueAsCompleteGesture ((float) i); };
        styleSwitch (button);
    }
    modeButtons[ReverseModes::reverb].setTooltip ("Reverb: each note swells up backwards and cuts off, like the RV-7's Reverse");
    modeButtons[ReverseModes::delay].setTooltip ("Delay: each stretch of Time played backwards");
    modeButtons[ReverseModes::wash].setTooltip ("Wash: the reverse delay sent into a long hall");

    sync.setTooltip ("Sync: set Time in note lengths at the host tempo");
    sync.onClick = [this] { syncAttachment.setValueAsCompleteGesture (sync.getToggleState() ? 0.0f : 1.0f); };
    styleSwitch (sync);

    tails.setClickingTogglesState (true);
    tails.setTooltip ("Tails: let whatever is playing backwards finish after bypassing");
    styleSwitch (tails);

    footswitch.setTooltip ("Bypass");
    footswitch.onStateChange = [this] { repaint (ledArea.expanded (8)); };
    addAndMakeVisible (footswitch);

    modeAttachment.sendInitialUpdate();
    syncAttachment.sendInitialUpdate();

    setSize (480, 580);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::styleSwitch (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId,   Palette::enclosureDark);
    button.setColour (juce::TextButton::buttonOnColourId, Palette::accent.darker (0.3f));
    button.setColour (juce::TextButton::textColourOffId,  Palette::silkscreen.withAlpha (0.6f));
    button.setColour (juce::TextButton::textColourOnId,   Palette::nameText);
    addAndMakeVisible (button);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Charcoal die-cast enclosure with a faint top-down sheen.
    g.fillAll (juce::Colour (0xff0c0c0d));
    const auto box = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.15f), box.getTopLeft(),
                                             Palette::enclosure.darker (0.2f), box.getBottomLeft(), false));
    g.fillRoundedRectangle (box, 12.0f);
    g.setColour (Palette::enclosureDark);
    g.drawRoundedRectangle (box, 12.0f, 2.0f);

    // Silkscreened "MODE" caption beside the mode switches.
    const auto firstMode = modeButtons.front().getBounds().toFloat();
    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("MODE", juce::Rectangle<float>::leftTopRightBottom (box.getX() + 12.0f, firstMode.getY(),
                                                                    firstMode.getX() - 10.0f, firstMode.getBottom()),
                juce::Justification::centredRight);

    // Status LED: lit while the effect is engaged.
    const auto led = ledArea.toFloat();
    const auto engaged = ! footswitch.getToggleState();
    if (engaged)
    {
        g.setColour (Palette::ledOn.withAlpha (0.3f));
        g.fillEllipse (led.expanded (6.0f));
    }
    g.setColour (engaged ? Palette::ledOn : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (engaged ? 0.6f : 0.15f));
    g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-1.5f, -1.5f));
    g.setColour (juce::Colour (0xff8a8c86));
    g.drawEllipse (led.expanded (1.5f), 1.5f);

    g.setColour (Palette::silkscreen);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("ON", led.withWidth (60.0f).translated (led.getWidth() + 8.0f, 0.0f).expanded (0.0f, 4.0f),
                juce::Justification::centredLeft);

    // Divider between the control deck and the footswitch half.
    g.setColour (Palette::enclosureDark.withAlpha (0.8f));
    g.drawHorizontalLine (footswitch.getY() - 2, box.getX() + 12.0f, box.getRight() - 12.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 20);

    // Knob row: Time (or its synced Division), Repeats, Tone, Mix.
    auto knobRow = area.removeFromTop (160);
    const auto knobWidth = knobRow.getWidth() / 4;
    const auto timeBounds = knobRow.removeFromLeft (knobWidth).reduced (4, 0).withTrimmedTop (10);
    time.setBounds (timeBounds);
    division.setBounds (timeBounds);
    for (auto* knob : { &repeats, &tone, &mix })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (4, 0).withTrimmedTop (10));

    // Mode switches, a row of three lit buttons.
    area.removeFromTop (12);
    auto modeRow = area.removeFromTop (30).withSizeKeepingCentre (3 * 104, 30);
    for (auto& button : modeButtons)
        button.setBounds (modeRow.removeFromLeft (104).reduced (3, 0));

    area.removeFromTop (14);
    auto ledRow = area.removeFromTop (22);
    ledArea = juce::Rectangle<int> (12, 12).withCentre ({ ledRow.getX() + 16, ledRow.getCentreY() });
    tails.setBounds (ledRow.removeFromRight (80));
    ledRow.removeFromRight (8);
    sync.setBounds (ledRow.removeFromRight (80));

    area.removeFromTop (14);
    footswitch.setBounds (area.withTrimmedLeft (-6).withTrimmedRight (-6).withTrimmedBottom (-4));
}
