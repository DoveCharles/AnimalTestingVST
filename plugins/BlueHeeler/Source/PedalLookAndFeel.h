#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a blue overdrive enclosure: black knurled knobs with a white
    pointer, dark ink silkscreen on gloss blue. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure     { 0xff2f6fc4 };
        static inline const juce::Colour enclosureDark { 0xff1d4a8a };
        static inline const juce::Colour ink           { 0xff0d1626 };
        static inline const juce::Colour pointer       { 0xfff3f3ee };
        static inline const juce::Colour knob          { 0xff151515 };
        static inline const juce::Colour ledOn         { 0xffff3b2a };
        static inline const juce::Colour ledOff        { 0xff4a1c18 };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
