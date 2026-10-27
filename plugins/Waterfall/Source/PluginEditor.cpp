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

    // Windows XP's Luna blue, for the desk the river's window sits on.
    const juce::Colour deskTop    { 0xff3c84f2 };
    const juce::Colour deskBottom { 0xff1a4fc4 };
    const juce::Colour frameDark  { 0xff0a2a7a };

    constexpr float windowCorner = 7.0f;

    juce::ColourGradient deskGradient (juce::Rectangle<float> area)
    {
        return { deskTop, 0.0f, area.getY(), deskBottom, 0.0f, area.getBottom(), false };
    }

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

    setSize (600, 400);
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

void PluginEditor::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill (deskGradient (area));
    g.fillAll();

    // A soft gloss across the top, like the XP taskbar, and a shadow under the window.
    g.setGradientFill ({ juce::Colours::white.withAlpha (0.16f), 0.0f, 0.0f,
                         juce::Colours::white.withAlpha (0.0f), 0.0f, 60.0f, false });
    g.fillRect (area.withHeight (60.0f));

    juce::Path outline;
    outline.addRoundedRectangle (window.toFloat(), windowCorner);
    juce::DropShadow (frameDark.withAlpha (0.55f), 16, { 0, 5 }).drawForPath (g, outline);
}

void PluginEditor::paintOverChildren (juce::Graphics& g)
{
    // Round the window's corners by covering them with the desk, then frame it: a dark edge with a
    // thin highlight just inside, brightest along the top.
    const auto r = window.toFloat();
    juce::Path corners;
    corners.addRectangle (r.expanded (1.0f));
    corners.addRoundedRectangle (r, windowCorner);
    corners.setUsingNonZeroWinding (false);
    g.setGradientFill (deskGradient (getLocalBounds().toFloat()));
    g.fillPath (corners);

    g.setColour (frameDark.withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), windowCorner, 1.5f);
    g.setGradientFill ({ juce::Colours::white.withAlpha (0.45f), 0.0f, r.getY(),
                         juce::Colours::white.withAlpha (0.08f), 0.0f, r.getBottom(), false });
    g.drawRoundedRectangle (r.reduced (2.0f), windowCorner - 1.5f, 1.0f);
}

void PluginEditor::resized()
{
    // The river runs in a window across the top, with the chord's notes floating low in it, and the
    // knobs sit on the blue below.
    auto area = getLocalBounds();
    window = area.reduced (16).withHeight (236);
    visual.setBounds (window);
    shadows.setBounds (area);
    chordReadout.setBounds (window.withTop (window.getBottom() - 60).withHeight (48));

    auto strip = area.withTop (window.getBottom() + 10).withHeight (116).reduced (22, 0);

    std::vector<juce::Rectangle<int>> knobBounds;
    const auto knobWidth = strip.getWidth() / 6;
    for (auto* knob : { &key, &octave, &chord, &brightness, &decay, &mix })
    {
        knob->setBounds (strip.removeFromLeft (knobWidth));
        knobBounds.push_back (knob->getBounds());
    }
    shadows.setKnobBounds (std::move (knobBounds));
}
