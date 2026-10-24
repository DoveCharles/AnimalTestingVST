#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a mid-80s sky-blue compact pedal: black ribbed knobs with a white
    pointer, navy silkscreen on a powder-blue enclosure. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure     { 0xff9ccbe6 };
        static inline const juce::Colour enclosureDark { 0xff5f93b3 };
        static inline const juce::Colour silkscreen    { 0xff14263a };
        static inline const juce::Colour nameplate     { 0xff142235 };
        static inline const juce::Colour nameText      { 0xfff2f0ea };
        static inline const juce::Colour knob          { 0xff141414 };
        static inline const juce::Colour rubber        { 0xff1e1e1f };
        static inline const juce::Colour ledOn         { 0xffff3b2a };
        static inline const juce::Colour ledOff        { 0xff5a2420 };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
