#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a late-2000s die-cast stompbox: black ribbed knobs with
    brushed-aluminium tops and a black pointer, pale silkscreen on a charcoal enclosure. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure     { 0xff2b2d30 };
        static inline const juce::Colour enclosureDark { 0xff141517 };
        static inline const juce::Colour silkscreen    { 0xffe4e4df };
        static inline const juce::Colour accent        { 0xff3fb8c8 };
        static inline const juce::Colour nameplate     { 0xff101113 };
        static inline const juce::Colour nameText      { 0xfff2f0ea };
        static inline const juce::Colour knob          { 0xff111111 };
        static inline const juce::Colour knobCap       { 0xffc9ccce };
        static inline const juce::Colour pointer       { 0xff141414 };
        static inline const juce::Colour chrome        { 0xffd6d8da };
        static inline const juce::Colour ledOn         { 0xff3aa8ff };
        static inline const juce::Colour ledOff        { 0xff1c2c3c };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
