#pragma once

#include <at_shared/at_shared.h>

/** Waterfall's take on the house style: glass ring knobs whose arcs glow in their own colour,
    thin widely spaced captions and small monospaced values with no boxes.

    A knob glows brighter while its slider's "glow" property (0..1) is high; the editor sets it
    from the sound. */
class WaterfallLookAndFeel final : public at::LookAndFeel
{
public:
    WaterfallLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    static juce::Font captionFont (float height);
    static juce::Font valueFont (float height);
    static juce::Font thinFont (float height);

    /** Draws white text with a soft dark halo, so it reads over the brightest parts of the visual. */
    static void drawHaloText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification,
                              juce::Colour);

    static constexpr auto glowProperty = "glow";
    static inline const juce::Colour shadow { 0xff04182e };
};
