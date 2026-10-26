#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned after the cover of Blue Smiley's "Return": a murky blue-green
    wash for the enclosure, the cover's deep slate shadow for the silkscreen and nameplate, and
    the painted creature's pink, mustard and cobalt for the accents. Slate knobs, pink pointers. */
class PedalLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour enclosure      { 0xff7d979b };
        static inline const juce::Colour enclosureLight { 0xffa3b7b4 };
        static inline const juce::Colour enclosureDark  { 0xff52687a };
        static inline const juce::Colour shadow         { 0xff2e3a52 };
        static inline const juce::Colour silkscreen     { 0xff1d2639 };
        static inline const juce::Colour nameplate      { 0xff2e3a52 };
        static inline const juce::Colour pink           { 0xffeaa79c };
        static inline const juce::Colour mustard        { 0xffd8a748 };
        static inline const juce::Colour cobalt         { 0xff3253a0 };
        static inline const juce::Colour fleck          { 0xffc4473d };
        static inline const juce::Colour cream          { 0xffefe6dc };
        static inline const juce::Colour knob           { 0xff1f2737 };
        static inline const juce::Colour rubber         { 0xff252c3a };
        static inline const juce::Colour ledOn          { 0xffff6a5c };
        static inline const juce::Colour ledOff         { 0xff5b3138 };
    };

    PedalLookAndFeel();

    /** A loose, scumbled paint wash over the given size, like the cover's background: overlapping
        translucent brush strokes in the blue-greys with the odd fleck of the creature's colours.
        The same seed always gives the same painting. */
    static juce::Image paintWash (int width, int height, juce::int64 seed);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
