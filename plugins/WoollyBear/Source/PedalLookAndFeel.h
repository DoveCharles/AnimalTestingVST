#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a woolly bear caterpillar: black bristle at both ends, a rust
    band through the middle, and bone-coloured knobs with a black pointer. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour furBlack      { 0xff17120f };
        static inline const juce::Colour furRust       { 0xffa8551f };
        static inline const juce::Colour silkscreen    { 0xfff3e6cc };
        static inline const juce::Colour ink           { 0xff1b1209 };
        static inline const juce::Colour knob          { 0xffe6d6b4 };
        static inline const juce::Colour knobShade     { 0xffa8946c };
        static inline const juce::Colour ledOn         { 0xffffa52e };
        static inline const juce::Colour ledOff        { 0xff4a2a10 };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
