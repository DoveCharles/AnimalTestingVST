#include "PluginEditor.h"

namespace
{
    using Palette = DeckLookAndFeel::Palette;

    constexpr int sectionTitleHeight = 22;
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

    void engrave (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, float size,
                  juce::Justification justification)
    {
        g.setFont (juce::FontOptions (size, juce::Font::bold).withKerningFactor (0.08f));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (text, area.translated (0, 1), justification);
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
    setLookAndFeel (&lookAndFeel);

    for (auto* knob : { &speed, &drive, &tone, &filter, &output, &wow, &flutter, &wear, &noise, &mechanical })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::ink);
        knob->getLabel().setFont (juce::FontOptions (12.0f, juce::Font::bold));

        // Set on the slider so the existing value box picks them up.
        auto& slider = knob->getSlider();
        slider.setColour (juce::Slider::textBoxTextColourId, Palette::ink);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &reels, &speed, &drive, &tone, &filter, &output,
                                                             &wow, &flutter, &wear, &noise, &mechanical, &boost, &toneEnable,
                                                             &stereoWidth, &tapeStop, &stopSpeed, &catchUp })
        addAndMakeVisible (c);

    setSize (840, 744);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    // An engraved border: a dark cut with a lit lower edge.
    const auto r = area.toFloat().reduced (1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f), 6.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    engrave (g, title.toUpperCase(), area.reduced (12, 0).removeFromTop (sectionTitleHeight + 4), 12.0f,
             juce::Justification::centredLeft);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::chassis);

    // The faceplate: matte silver, raised off the chassis, screwed down at the corners.
    const auto plateBounds = faceplate.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (plateBounds.translated (0.0f, 3.0f), 10.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (plateBounds, 10.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (plate, faceplate.getX(), faceplate.getY());
    }
    DeckLookAndFeel::drawBevel (g, plateBounds, 10.0f, true);

    const auto inset = 14.0f;
    const float slots[] { 0.5f, 1.9f, 1.1f, 2.6f };
    int corner = 0;
    for (auto x : { plateBounds.getX() + inset, plateBounds.getRight() - inset })
        for (auto y : { plateBounds.getY() + inset, plateBounds.getBottom() - inset })
            DeckLookAndFeel::drawScrew (g, { x, y }, 6.0f, slots[corner++]);

    // Nameplate: a black anodised badge with the name picked out in amber.
    const auto badge = header.withWidth (230).toFloat().reduced (0.0f, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (badge.translated (0.0f, 2.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xff2c2e33), badge.getY(),
                                                       juce::Colour (0xff121316), badge.getBottom()));
    g.fillRoundedRectangle (badge, 5.0f);
    DeckLookAndFeel::drawBevel (g, badge, 5.0f, true);
    g.setColour (Palette::amber);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold).withKerningFactor (0.18f));
    g.drawText ("TAPEWORM", badge.withTrimmedLeft (14.0f), juce::Justification::centredLeft);

    auto maker = getLocalBounds().withY (header.getY()).withHeight (header.getHeight()).withX (faceplate.getX() + 270);
    maker.setWidth (200);
    engrave (g, "ANIMAL TESTING", maker.removeFromTop (maker.getHeight() / 2).withTrimmedTop (6), 12.0f,
             juce::Justification::bottomLeft);
    engrave (g, "TAPE MACHINE  v" JucePlugin_VersionString, maker.withTrimmedBottom (6), 10.0f,
             juce::Justification::topLeft);

    drawSection (g, transportArea, "Transport");
    drawSection (g, driveArea, "Drive");
    drawSection (g, toneArea, "Tone");
    drawSection (g, outputArea, "Output");
    drawSection (g, motionArea, "Motion");
    drawSection (g, noiseArea, "Noise");

    // Labels over the two choice boxes.
    for (auto* box : { &stopSpeed, &catchUp })
        engrave (g, box == &stopSpeed ? "STOP TIME" : "CATCH-UP",
                 box->getBounds().withY (box->getY() - 17).withHeight (16), 10.0f, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    faceplate = getLocalBounds().reduced (8);
    plate = DeckLookAndFeel::matteMetal (faceplate.getWidth(), faceplate.getHeight(), 11);

    auto area = faceplate.reduced (24, 20);
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
