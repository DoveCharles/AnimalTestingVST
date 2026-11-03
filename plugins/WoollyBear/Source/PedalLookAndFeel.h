#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a green Russian enclosure: black knurled knobs with a white
    pointer, white silkscreen text on army green. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure     { 0xff4f5d3a };
        static inline const juce::Colour enclosureDark { 0xff36402a };
        static inline const juce::Colour silkscreen    { 0xfff1eee2 };
        static inline const juce::Colour knob          { 0xff151515 };
        static inline const juce::Colour ledOn         { 0xffff3b2a };
        static inline const juce::Colour ledOff        { 0xff4a1c18 };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
