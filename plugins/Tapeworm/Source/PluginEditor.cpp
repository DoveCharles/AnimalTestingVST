#include "PluginEditor.h"

namespace
{
    using Palette = DeckLookAndFeel::Palette;

    constexpr int sectionTitleHeight = 26;
    constexpr int switchHeight = 32;

    /** The attachment only fills the box from items that are already there. */
    juce::ComboBox& withChoices (juce::ComboBox& box, const juce::StringArray& names)
    {
        box.addItemList (names, 1);
        return box;
    }

    juce::TextButton& asToggle (juce::TextButton& button)
    {
        button.setClickingTogglesState (true);
        return button;
    }

    void scribble (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, float size,
                   juce::Justification justification)
    {
        g.setFont (DeckLookAndFeel::hand (size));
        g.setColour (Palette::ink);
        g.drawText (text, area, justification);
    }
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
      reels (p),
      speed      (p.apvts, ParamIDs::speed,      "Speed"),
      drive      (p.apvts, ParamIDs::drive,      "Drive"),
      tone       (p.apvts, ParamIDs::tone,       "Tone"),
      filter     (p.apvts, ParamIDs::filter,     "Filter"),
      output     (p.apvts, ParamIDs::output,     "Output"),
      wow        (p.apvts, ParamIDs::wow,        "Wow"),
      flutter    (p.apvts, ParamIDs::flutter,    "Flutter"),
      wear       (p.apvts, ParamIDs::wear,       "Wear"),
      noise      (p.apvts, ParamIDs::noise,      "Hiss"),
      mechanical (p.apvts, ParamIDs::mechanical, "Motor"),
      boostAttachment       (p.apvts, ParamIDs::boost,       asToggle (boost)),
      toneEnableAttachment  (p.apvts, ParamIDs::toneEnable,  asToggle (toneEnable)),
      stereoWidthAttachment (p.apvts, ParamIDs::stereoWidth, asToggle (stereoWidth)),
      tapeStopAttachment    (p.apvts, ParamIDs::tapeStop,    asToggle (tapeStop)),
      stopSpeedAttachment   (p.apvts, ParamIDs::stopSpeed, withChoices (stopSpeed, StopSpeeds::names)),
      catchUpAttachment     (p.apvts, ParamIDs::catchUp,   withChoices (catchUp, CatchUps::names))
{
    // Each knob is a different sweet jar.
    const std::pair<at::Knob*, juce::uint32> jars[] {
        { &speed, 0xff5a7de0 }, { &drive, 0xffe8473f }, { &tone, 0xffff9a3c }, { &filter, 0xfff2d43c },
        { &output, 0xff9a63d6 }, { &wow, 0xff5cc9a7 }, { &flutter, 0xffff7fae }, { &wear, 0xffc98a4b },
        { &noise, 0xff9bd34a }, { &mechanical, 0xff5ab8e8 } };
    for (auto [knob, colour] : jars)
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::ink);
        knob->getLabel().setFont (DeckLookAndFeel::hand (15.0f));

        // Set on the slider so the existing value box picks them up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (colour));
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::ink);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &reels, &speed, &drive, &tone, &filter, &output,
                                                             &wow, &flutter, &wear, &noise, &mechanical, &boost, &toneEnable,
                                                             &stereoWidth, &tapeStop, &stopSpeed, &catchUp })
        addAndMakeVisible (c);

    // After the children are in, so the knobs remake their value boxes in this style.
    setLookAndFeel (&lookAndFeel);

    setSize (840, 744);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    DeckLookAndFeel::drawCard (g, area.toFloat().reduced (1.5f), title, area.getX() + area.getY());
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (cloth, 0, 0);

    // The name painted in red on a parchment lid.
    const auto box = header.withWidth (232).toFloat().reduced (0.0f, 3.0f);

    g.setColour (Palette::ink.withAlpha (0.18f));
    g.fillRoundedRectangle (box.translated (3.0f, 5.0f), 10.0f);
    g.setColour (Palette::paper.darker (0.35f));
    g.fillRoundedRectangle (box, 10.0f);
    const auto lid = box.withTrimmedBottom (5.0f);
    g.setColour (Palette::paper);
    g.fillRoundedRectangle (lid, 10.0f);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (box, 10.0f, 2.5f);
    g.drawRoundedRectangle (lid, 10.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawLine (lid.getX() + 12.0f, lid.getY() + 5.0f, lid.getX() + 60.0f, lid.getY() + 5.0f, 2.5f);
    g.setFont (DeckLookAndFeel::hand (27.0f));
    g.setColour (Palette::ink);
    g.drawText ("TAPEWORM", lid.translated (1.5f, 2.0f), juce::Justification::centred);
    g.setColour (Palette::cherry);
    g.drawText ("TAPEWORM", lid, juce::Justification::centred);

    auto maker = header.withX (header.getX() + 252).withWidth (200);
    scribble (g, "Animal Testing", maker.removeFromTop (maker.getHeight() / 2).withTrimmedTop (2), 16.0f,
              juce::Justification::bottomLeft);
    scribble (g, "tape machine  v" JucePlugin_VersionString, maker.withTrimmedBottom (2), 13.0f,
              juce::Justification::topLeft);

    drawSection (g, transportArea, "Transport");
    drawSection (g, driveArea, "Drive");
    drawSection (g, toneArea, "Tone");
    drawSection (g, outputArea, "Output");
    drawSection (g, motionArea, "Motion");
    drawSection (g, noiseArea, "Noise");

    // Labels over the two choice boxes.
    for (auto* box : { &stopSpeed, &catchUp })
        scribble (g, box == &stopSpeed ? "Stop time" : "Catch-up",
                  box->getBounds().withY (box->getY() - 19).withHeight (18), 13.0f, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    cloth = DeckLookAndFeel::gingham (getWidth(), getHeight());

    auto area = getLocalBounds().reduced (32, 28);
    header = area.removeFromTop (48);
    presetBar.setBounds (header.withTrimmedLeft (header.getWidth() - 340).withSizeKeepingCentre (340, 28));
    area.removeFromTop (14);

    // Deck row: the tape window, and the transport controls beside it.
    auto deck = area.removeFromTop (240);
    transportArea = deck.removeFromRight (276);
    deck.removeFromRight (14);
    reels.setBounds (deck);
    area.removeFromTop (14);

    auto transport = transportArea.reduced (12, 10).withTrimmedTop (sectionTitleHeight - 4);
    speed.setBounds (transport.removeFromLeft (124));
    transport.removeFromLeft (8);
    auto column = transport.withSizeKeepingCentre (transport.getWidth(), 152);
    tapeStop.setBounds (column.removeFromTop (34));
    column.removeFromTop (22);
    stopSpeed.setBounds (column.removeFromTop (26));
    column.removeFromTop (22);
    catchUp.setBounds (column.removeFromTop (26));

    const auto rowHeight = (area.getHeight() - 14) / 2;
    auto top = area.removeFromTop (rowHeight);
    area.removeFromTop (14);
    auto bottom = area;

    const auto unit = (top.getWidth() - 28) / 6;
    driveArea = top.removeFromLeft (unit * 2);
    top.removeFromLeft (14);
    toneArea = top.removeFromLeft (unit * 3);
    top.removeFromLeft (14);
    outputArea = top;

    motionArea = bottom.removeFromLeft ((bottom.getWidth() - 14) * 3 / 5);
    bottom.removeFromLeft (14);
    noiseArea = bottom;

    // Every knob is the same size; switches sit in a row underneath.
    const auto layoutKnobs = [] (juce::Rectangle<int> r, std::initializer_list<at::Knob*> knobs)
    {
        const auto width = r.getWidth() / (int) knobs.size();
        for (auto* knob : knobs)
            knob->setBounds (r.removeFromLeft (width).reduced (4, 0));
    };
    const auto inner = [] (juce::Rectangle<int> r)
    {
        return r.reduced (10, 8).withTrimmedTop (sectionTitleHeight - 2).withTrimmedBottom (switchHeight + 4);
    };
    const auto switchUnder = [] (juce::Rectangle<int> knobs, int x)
    {
        return juce::Rectangle<int> (92, switchHeight).withCentre ({ x, knobs.getBottom() + 4 + switchHeight / 2 });
    };

    const auto driveInner = inner (driveArea);
    layoutKnobs (driveInner, { &drive });
    boost.setBounds (switchUnder (driveInner, driveInner.getCentreX()));

    const auto toneInner = inner (toneArea);
    layoutKnobs (toneInner, { &tone, &filter });
    toneEnable.setBounds (switchUnder (toneInner, tone.getBounds().getCentreX()));

    layoutKnobs (inner (outputArea), { &output });

    const auto motionInner = inner (motionArea);
    layoutKnobs (motionInner, { &wow, &flutter, &wear });
    stereoWidth.setBounds (switchUnder (motionInner, motionInner.getCentreX()));

    layoutKnobs (inner (noiseArea), { &noise, &mechanical });
}
