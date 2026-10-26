#include "PluginEditor.h"

namespace
{
    constexpr int sectionTitleHeight = 22;

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
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
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
    setLookAndFeel (&sharedLookAndFeel.get());

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &drive, &tone, &filter, &output, &wow, &flutter,
                                                             &wear, &noise, &mechanical, &boost, &toneEnable, &stereoWidth,
                                                             &tapeStop, &stopSpeed, &catchUp })
        addAndMakeVisible (c);

    setSize (720, 470);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    g.setColour (at::LookAndFeel::Palette::panel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (at::LookAndFeel::Palette::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), area.reduced (10, 0).removeFromTop (sectionTitleHeight),
                juce::Justification::centredLeft);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Tapeworm", JucePlugin_VersionString);
    drawSection (g, driveArea, "Drive");
    drawSection (g, toneArea, "Tone");
    drawSection (g, outputArea, "Output");
    drawSection (g, motionArea, "Transport");
    drawSection (g, noiseArea, "Noise");
    drawSection (g, stopArea, "Tape Stop");
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);

    stopArea = area.removeFromBottom (56);
    area.removeFromBottom (10);

    const auto rowHeight = (area.getHeight() - 10) / 2;
    auto top = area.removeFromTop (rowHeight);
    area.removeFromTop (10);
    auto bottom = area;

    const auto unit = (top.getWidth() - 20) / 6;
    driveArea = top.removeFromLeft (unit * 2);
    top.removeFromLeft (10);
    toneArea = top.removeFromLeft (unit * 3);
    top.removeFromLeft (10);
    outputArea = top;

    motionArea = bottom.removeFromLeft ((bottom.getWidth() - 10) * 3 / 5);
    bottom.removeFromLeft (10);
    noiseArea = bottom;

    const auto layoutKnobs = [] (juce::Rectangle<int> r, std::initializer_list<at::Knob*> knobs)
    {
        const auto width = r.getWidth() / (int) knobs.size();
        for (auto* knob : knobs)
            knob->setBounds (r.removeFromLeft (width).reduced (2, 0));
    };
    const auto inner = [] (juce::Rectangle<int> r) { return r.reduced (8).withTrimmedTop (sectionTitleHeight - 6); };
    const auto switchRow = [] (juce::Rectangle<int>& r) { return r.removeFromBottom (24).withSizeKeepingCentre (76, 22); };

    auto driveInner = inner (driveArea);
    boost.setBounds (switchRow (driveInner));
    layoutKnobs (driveInner, { &drive });

    auto toneInner = inner (toneArea);
    auto toneSwitch = toneInner.removeFromBottom (24);
    toneEnable.setBounds (toneSwitch.removeFromLeft (toneSwitch.getWidth() / 2).withSizeKeepingCentre (76, 22));
    layoutKnobs (toneInner, { &tone, &filter });

    layoutKnobs (inner (outputArea).withTrimmedBottom (24), { &output });

    auto motionInner = inner (motionArea);
    stereoWidth.setBounds (switchRow (motionInner));
    layoutKnobs (motionInner, { &wow, &flutter, &wear });

    layoutKnobs (inner (noiseArea).withTrimmedBottom (24), { &noise, &mechanical });

    auto stop = stopArea.reduced (10, 0).withTrimmedTop (sectionTitleHeight - 4).withTrimmedBottom (8);
    stop.removeFromLeft (90);
    tapeStop.setBounds (stop.removeFromLeft (90));
    stop.removeFromLeft (16);
    stopSpeed.setBounds (stop.removeFromLeft (130));
    stop.removeFromLeft (10);
    catchUp.setBounds (stop.removeFromLeft (150));
}
