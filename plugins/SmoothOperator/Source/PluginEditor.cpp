#include "PluginEditor.h"

namespace
{
    constexpr int sectionTitleHeight = 22;
    constexpr float floorDb = -90.0f, ceilingDb = 0.0f, cutRangeDb = 20.0f;
    const std::array<const char*, ResonanceSuppressor::numWeights> weightCaptions { "100", "300", "1k", "3k", "8k" };

    /** The attachment only fills the box from items that are already there. */
    juce::ComboBox& withStereoChoices (juce::ComboBox& box)
    {
        box.addItemList (StereoModes::names, 1);
        return box;
    }
}

void PluginEditor::Display::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (at::LookAndFeel::Palette::panel);
    g.fillRoundedRectangle (area, 6.0f);

    const auto inner = area.reduced (8.0f, 6.0f);
    const auto xFor = [&] (float hz) { return inner.getX() + inner.getWidth() * std::log2 (hz / 20.0f) / std::log2 (1000.0f); };

    // Octave-ish grid with labels.
    g.setFont (juce::FontOptions (10.0f));
    for (auto hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xFor (hz);
        g.setColour (at::LookAndFeel::Palette::track.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), inner.getY(), inner.getBottom());
        g.setColour (at::LookAndFeel::Palette::textDim);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String (hz, 0),
                    juce::Rectangle<float> (x + 3.0f, inner.getBottom() - 14.0f, 30.0f, 14.0f), juce::Justification::left);
    }

    const auto bands = ResonanceSuppressor::displayBands;
    const auto bandX = [&] (int b) { return xFor (ResonanceSuppressor::displayBandHz (b)); };

    // Input spectrum, filled from the bottom.
    juce::Path level;
    level.startNewSubPath (bandX (0), inner.getBottom());
    for (int b = 0; b < bands; ++b)
    {
        const auto t = juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) b] - floorDb) / (ceilingDb - floorDb));
        level.lineTo (bandX (b), inner.getBottom() - t * inner.getHeight());
    }
    level.lineTo (bandX (bands - 1), inner.getBottom());
    level.closeSubPath();
    g.setColour (at::LookAndFeel::Palette::textDim.withAlpha (0.25f));
    g.fillPath (level);

    // The cut, hanging from the top: 0 dB at the top edge, cutRangeDb at the bottom.
    juce::Path cut;
    cut.startNewSubPath (bandX (0), inner.getY());
    for (int b = 0; b < bands; ++b)
        cut.lineTo (bandX (b), inner.getY() + juce::jlimit (0.0f, 1.0f, reduction[(size_t) b] / cutRangeDb) * inner.getHeight());
    cut.lineTo (bandX (bands - 1), inner.getY());
    cut.closeSubPath();
    g.setColour (at::LookAndFeel::Palette::accent.withAlpha (0.35f));
    g.fillPath (cut);
    g.setColour (at::LookAndFeel::Palette::accent);
    g.strokePath (cut, juce::PathStrokeType (1.5f));
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      smoothOperator (p),
      presetBar (p.presets),
      depth       (p.apvts, ParamIDs::depth,       "Depth"),
      sharpness   (p.apvts, ParamIDs::sharpness,   "Sharpness"),
      selectivity (p.apvts, ParamIDs::selectivity, "Selectivity"),
      attack      (p.apvts, ParamIDs::attack,      "Attack"),
      release     (p.apvts, ParamIDs::release,     "Release"),
      mix         (p.apvts, ParamIDs::mix,         "Mix"),
      output      (p.apvts, ParamIDs::output,      "Output"),
      stereoAttachment (p.apvts, ParamIDs::stereo, withStereoChoices (stereo)),
      deltaAttachment (p.apvts, ParamIDs::delta, delta)
{
    setLookAndFeel (&sharedLookAndFeel.get());

    for (size_t i = 0; i < weights.size(); ++i)
        weights[i] = std::make_unique<at::Knob> (p.apvts, ParamIDs::weights[i], weightCaptions[i]);

    delta.setClickingTogglesState (true);
    delta.setColour (juce::TextButton::buttonOnColourId, at::LookAndFeel::Palette::accent);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &display, &depth, &sharpness, &selectivity,
                                                             &attack, &release, &mix, &output, &stereo, &delta })
        addAndMakeVisible (c);
    for (auto& knob : weights)
        addAndMakeVisible (*knob);

    setSize (760, 540);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    // Fast up, slower down, so the cut reads like a meter rather than flicker.
    auto& s = smoothOperator.suppressor;
    for (size_t b = 0; b < display.spectrum.size(); ++b)
    {
        const auto level = s.spectrum[b].load(), cut = s.reduction[b].load();
        auto& shownLevel = display.spectrum[b];
        auto& shownCut = display.reduction[b];
        shownLevel = level > shownLevel ? level : shownLevel + 0.3f * (level - shownLevel);
        shownCut = cut > shownCut ? cut : shownCut + 0.25f * (cut - shownCut);
    }
    display.repaint();
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
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Smooth Operator", JucePlugin_VersionString);
    drawSection (g, detectArea, "Detection");
    drawSection (g, weightArea, "Weighting");
    drawSection (g, outputArea, "Output");
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);
    display.setBounds (area.removeFromTop (170));
    area.removeFromTop (10);

    const auto rowHeight = (area.getHeight() - 10) / 2;
    detectArea = area.removeFromTop (rowHeight);
    area.removeFromTop (10);
    weightArea = area.removeFromLeft (area.getWidth() * 3 / 5);
    area.removeFromLeft (10);
    outputArea = area;

    const auto layoutKnobs = [] (juce::Rectangle<int> r, std::initializer_list<at::Knob*> knobs)
    {
        const auto width = r.getWidth() / (int) knobs.size();
        for (auto* knob : knobs)
            knob->setBounds (r.removeFromLeft (width).reduced (2, 0));
    };
    const auto inner = [] (juce::Rectangle<int> r) { return r.reduced (8).withTrimmedTop (sectionTitleHeight - 6); };

    layoutKnobs (inner (detectArea), { &depth, &sharpness, &selectivity, &attack, &release });
    layoutKnobs (inner (weightArea), { weights[0].get(), weights[1].get(), weights[2].get(), weights[3].get(), weights[4].get() });

    auto out = inner (outputArea);
    auto switches = out.removeFromLeft (90).reduced (0, 6);
    const auto half = switches.getHeight() / 2;
    stereo.setBounds (switches.removeFromTop (half).reduced (0, 6));
    delta.setBounds (switches.reduced (0, 6));
    out.removeFromLeft (6);
    layoutKnobs (out, { &mix, &output });
}
