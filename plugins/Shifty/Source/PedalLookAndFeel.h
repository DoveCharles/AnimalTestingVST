#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned cute and glitchy: minty gumdrop knobs with a teal-to-pink value
    arc, on a white cloud floating over teal. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        // The marble, darkest to palest.
        static inline const juce::Colour abyss     { 0xff0a2a2c };
        static inline const juce::Colour deepTeal  { 0xff11605c };
        static inline const juce::Colour teal      { 0xff1db3a4 };
        static inline const juce::Colour aqua      { 0xff52e2cd };
        static inline const juce::Colour foam      { 0xffbdf4e4 };
        static inline const juce::Colour cream     { 0xfff4efdf };
        static inline const juce::Colour vein      { 0xff1b2f2f };
        static inline const juce::Colour gold      { 0xffe9c98a };

        // Glitch fringes and the cute bits.
        static inline const juce::Colour magenta   { 0xffff4fd8 };
        static inline const juce::Colour cyan      { 0xff3ff6ff };
        static inline const juce::Colour blush     { 0xffff8fb8 };
        static inline const juce::Colour hotPink   { 0xffff4f9a };
        static inline const juce::Colour mint      { 0xff7dffd4 };

        static inline const juce::Colour glass     { 0x8c071c1e };
        static inline const juce::Colour ink       { 0xff12201f };
    };

    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
