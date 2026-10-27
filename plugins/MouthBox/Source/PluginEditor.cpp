#include "PluginEditor.h"

namespace
{
    using Palette = at::LookAndFeel::Palette;

    constexpr int sectionTitleHeight = 22;
    constexpr float meterFloorDb = -60.0f;

    void drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (Palette::accent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), area.reduced (10, 0).removeFromTop (sectionTitleHeight),
                    juce::Justification::centredLeft);
    }

    float meterPosition (float db) { return juce::jlimit (0.0f, 1.0f, (db - meterFloorDb) / -meterFloorDb); }
}

//==============================================================================
PluginEditor::Pad::Pad (PluginProcessor& p, int padSlot)
    : processor (p), slot (padSlot),
      noteAttachment (p.apvts, ParamIDs::notes[(size_t) padSlot], note)
{
    // The attachment fills the box from the parameter only if the items are added first.
    if (auto* param = dynamic_cast<juce::AudioParameterInt*> (p.apvts.getParameter (ParamIDs::notes[(size_t) padSlot])))
    {
        for (int n = 0; n <= 127; ++n)
            note.addItem (static_cast<juce::AudioProcessorParameter&> (*param).getText (param->convertTo0to1 ((float) n), 32), n + 1);
        note.setSelectedItemIndex (param->get(), juce::dontSendNotification);
    }
    note.setTooltip ("MIDI note this pad sends");
    addAndMakeVisible (note);

    train.onClick = [this]
    {
        if (processor.getTrainingSlot() == slot)
            processor.stopTraining();
        else
            processor.startTraining (slot);
    };
    clear.onClick = [this] { processor.clearSlot (slot); };
    addAndMakeVisible (train);
    addAndMakeVisible (clear);
}

void PluginEditor::Pad::refresh()
{
    const auto listening = processor.getTrainingSlot() == slot;
    train.setButtonText (listening ? "Hit it " + juce::String (processor.getTrainingProgress()) + "/" + juce::String (PluginProcessor::hitsPerTraining)
                                   : juce::String ("Train"));
    train.setColour (juce::TextButton::buttonColourId, listening ? Palette::accent.withAlpha (0.8f) : Palette::track);
    examples = processor.getExampleCount (slot);
    clear.setEnabled (examples > 0);
    repaint (face);
}

void PluginEditor::Pad::paint (juce::Graphics& g)
{
    const auto area = face.toFloat();
    g.setColour (Palette::track.interpolatedWith (Palette::accent, flash));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (processor.getTrainingSlot() == slot ? Palette::accent : Palette::track.brighter (0.15f));
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, processor.getTrainingSlot() == slot ? 2.0f : 1.0f);

    auto text = face.reduced (8, 6);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (Slots::names[slot].toUpperCase(), text.removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (flash > 0.5f ? Palette::background : Palette::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (examples > 0 ? juce::String (examples) + " trained" : juce::String ("untrained"),
                text.removeFromBottom (16), juce::Justification::centredLeft);

    if (velocity > 0)
    {
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold));
        g.setColour ((flash > 0.5f ? Palette::background : Palette::text).withAlpha (0.35f + 0.65f * flash));
        g.drawText (juce::String (velocity), text, juce::Justification::centred);
    }
}

void PluginEditor::Pad::resized()
{
    auto r = getLocalBounds();
    auto buttons = r.removeFromBottom (24);
    train.setBounds (buttons.removeFromLeft (buttons.getWidth() * 3 / 5).withTrimmedRight (3));
    clear.setBounds (buttons.withTrimmedLeft (3));
    r.removeFromBottom (6);
    note.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (6);
    face = r;
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      mouthBox (p),
      presetBar (p.presets),
      threshold (p.apvts, ParamIDs::threshold, "Threshold"),
      listen    (p.apvts, ParamIDs::listen,    "Listen"),
      gap       (p.apvts, ParamIDs::gap,       "Gap"),
      dynamics  (p.apvts, ParamIDs::dynamics,  "Dynamics"),
      monitor   (p.apvts, ParamIDs::monitor,   "Monitor")
{
    setLookAndFeel (&sharedLookAndFeel.get());

    addAndMakeVisible (presetBar);
    for (int slot = 0; slot < HitClassifier::numSlots; ++slot)
        addAndMakeVisible (*pads.emplace_back (std::make_unique<Pad> (p, slot)));
    for (auto* knob : { &threshold, &listen, &gap, &dynamics, &monitor })
        addAndMakeVisible (knob);

    seenHits = p.hitCount.load();
    for (auto& pad : pads)
        pad->refresh();

    setSize (760, 470);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    const auto hits = mouthBox.hitCount.load();
    const auto newHit = hits != seenHits;
    seenHits = hits;

    for (auto& pad : pads)
    {
        if (newHit && pad->slot == mouthBox.lastSlot.load())
        {
            pad->flash = 1.0f;
            pad->velocity = mouthBox.lastVelocity.load();
        }
        else
        {
            pad->flash *= 0.8f;
        }
        pad->refresh();
    }

    const auto level = juce::Decibels::gainToDecibels (mouthBox.inputLevel.load(), meterFloorDb);
    if (std::abs (level - meter) > 0.25f)
    {
        meter = level;
        repaint (meterArea);
    }
    repaint (statusArea);
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Mouth Box", JucePlugin_VersionString);

    drawSection (g, padsArea, "Pads");
    drawSection (g, knobsArea, "Detection");

    // Input meter with the threshold marked: hits only count when the bar crosses the line.
    const auto bar = meterArea.toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (Palette::accent.withAlpha (0.75f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * meterPosition (meter)), 4.0f);
    const auto thresholdX = bar.getX() + bar.getWidth() * meterPosition (mouthBox.apvts.getRawParameterValue (ParamIDs::threshold)->load());
    g.setColour (Palette::text);
    g.fillRect (juce::Rectangle<float> (thresholdX - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f));

    auto status = statusArea;
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (Palette::text);
    g.drawText (mouthBox.isTrained() ? "Trained: each hit goes to the closest trained pad"
                                     : "Untrained: low = Kick, mid = Snare, high = Hi-Hat. Train pads for your own sounds",
                status.removeFromTop (status.getHeight() / 2), juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    const auto port = mouthBox.getVirtualPortName();
    g.drawText (port.isNotEmpty() ? "MIDI out: this track, and the virtual port \"" + port + "\" (channel 10)"
                                  : juce::String ("MIDI out: this track's MIDI output (channel 10)"),
                status, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);

    auto top = area.removeFromTop (36);
    meterArea = top.removeFromLeft (200).withSizeKeepingCentre (200, 12);
    top.removeFromLeft (16);
    statusArea = top;
    area.removeFromTop (10);

    knobsArea = area.removeFromBottom (128);
    area.removeFromBottom (10);
    padsArea = area;

    auto padRow = padsArea.reduced (10).withTrimmedTop (sectionTitleHeight - 4);
    const auto padWidth = padRow.getWidth() / (int) pads.size();
    for (auto& pad : pads)
        pad->setBounds (padRow.removeFromLeft (padWidth).reduced (5, 0));

    auto knobRow = knobsArea.reduced (8).withTrimmedTop (sectionTitleHeight - 6);
    const auto knobWidth = knobRow.getWidth() / 5;
    for (auto* knob : { &threshold, &listen, &gap, &dynamics, &monitor })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (2, 0));
}
