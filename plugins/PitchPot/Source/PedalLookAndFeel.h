#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as a stompbox printed with a Ming court painting of pitch-pot:
    black knurled knobs with a cream pointer, ink lettering on aged silk in a brocade mount. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure     { 0xffe2cd9f };
        static inline const juce::Colour enclosureDark { 0xff3b2a1c };
        static inline const juce::Colour silkscreen    { 0xff2b1d14 };
        static inline const juce::Colour knob          { 0xff151515 };
        static inline const juce::Colour pointer       { 0xfff1e6cf };
        static inline const juce::Colour chrome        { 0xffc9ccd1 };
        static inline const juce::Colour display       { 0xff1a0d0e };
        static inline const juce::Colour displayText   { 0xffff6a3d };
        static inline const juce::Colour ledOn         { 0xffff3b2a };
        static inline const juce::Colour ledOff        { 0xff3a1210 };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
