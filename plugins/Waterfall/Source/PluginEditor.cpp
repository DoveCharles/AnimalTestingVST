#include "PluginEditor.h"

namespace
{
    // Each knob has its own colour from the trails' rainbow.
    const std::array<juce::Colour, 6> knobColours {
        juce::Colour::fromHSV (0.78f, 0.55f, 1.0f, 1.0f),   // Key: violet
        juce::Colour::fromHSV (0.68f, 0.55f, 1.0f, 1.0f),   // Octave: indigo
        juce::Colour::fromHSV (0.58f, 0.60f, 1.0f, 1.0f),   // Chord: blue
        juce::Colour::fromHSV (0.13f, 0.60f, 1.0f, 1.0f),   // Brightness: gold
        juce::Colour::fromHSV (0.46f, 0.60f, 1.0f, 1.0f),   // Decay: aqua
        juce::Colour::fromHSV (0.93f, 0.50f, 1.0f, 1.0f),   // Mix: rose
    };

    // RMS level to 0..1 over a 54 dB range.
    float toUnit (float rms)
    {
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (rms, -100.0f) + 54.0f) / 54.0f);
    }
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      waterfall (p),
      visual (p),
      key        (p.apvts, ParamIDs::key,        "Key"),
      octave     (p.apvts, ParamIDs::octave,     "Octave"),
      chord      (p.apvts, ParamIDs::chord,      "Chord"),
      brightness (p.apvts, ParamIDs::brightness, "Brightness"),
      decay      (p.apvts, ParamIDs::decay,      "Decay"),
      mix        (p.apvts, ParamIDs::mix,        "Mix")
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (visual);
    addAndMakeVisible (shadows);
    addAndMakeVisible (chordReadout);

    size_t index = 0;
    for (auto* knob : { &key, &octave, &chord, &brightness, &decay, &mix })
    {
        knob->getSlider().setColour (juce::Slider::rotarySliderFillColourId, knobColours[index++]);
        knob->getLabel().setFont (WaterfallLookAndFeel::captionFont (11.0f));
        knob->getLabel().setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (*knob);
        knob->getSlider().sendLookAndFeelChange();   // re-create the value box with this look and feel
    }

    visual.onTick = [this] (float time) { animate (time); };

    setSize (600, 360);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::animate (float time)
{
    const auto levels = waterfall.getLevels();
    const auto input = toUnit (levels.input);
    const auto ring  = toUnit (levels.ring);
    const auto high  = toUnit (levels.high * 4.0f);   // the high end is quieter; lift it to a similar range
    const auto wet   = (float) mix.getSlider().getValue() / 100.0f;

    // Key, Octave and Chord glow with the ringing resonators, Brightness with their high end, Decay with the
    // tail that rings on once the input drops, and Mix with whatever it lets through.
    const std::array<float, 6> targets {
        ring,
        ring,
        ring,
        high,
        juce::jlimit (0.0f, 1.0f, ring - 0.7f * input + 0.15f),
        juce::jmap (wet, input, ring),
    };

    size_t index = 0;
    for (auto* knob : { &key, &octave, &chord, &brightness, &decay, &mix })
    {
        auto& g = glow[index];
        const auto target = targets[index++];
        g += (target - g) * (target > g ? 0.2f : 0.04f);

        auto& slider = knob->getSlider();
        slider.getProperties().set (WaterfallLookAndFeel::glowProperty, g);
        slider.repaint();
    }

    chordReadout.setChord ((int) key.getSlider().getValue(), (int) chord.getSlider().getValue());
    chordReadout.setTime (time);
}

void PluginEditor::resized()
{
    // No header, preset bar or panel: the spectrogram is the whole window, with the chord's notes and
    // the knobs floating across the middle.
    auto area = getLocalBounds();
    visual.setBounds (area);
    shadows.setBounds (area);

    auto strip = area.reduced (22, 0).withSizeKeepingCentre (area.getWidth() - 44, 116);
    chordReadout.setBounds (area.withTop (strip.getY() - 62).withHeight (48));

    std::vector<juce::Rectangle<int>> knobBounds;
    const auto knobWidth = strip.getWidth() / 6;
    for (auto* knob : { &key, &octave, &chord, &brightness, &decay, &mix })
    {
        knob->setBounds (strip.removeFromLeft (knobWidth));
        knobBounds.push_back (knob->getBounds());
    }
    shadows.setKnobBounds (std::move (knobBounds));
}
