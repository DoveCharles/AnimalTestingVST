#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-dressed as a 1970s tape-replay console: walnut end cheeks, a cream
    fascia with dark silkscreen, black fluted Bakelite knobs, a maroon key felt, and an amber
    glass window for the tape names and strips. */
class ChilltronLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour walnutDark  { 0xff2e190e };
        static inline const juce::Colour walnut      { 0xff55301b };
        static inline const juce::Colour walnutLight { 0xff7a4727 };
        static inline const juce::Colour cream       { 0xffebe2cd };
        static inline const juce::Colour creamShade  { 0xffd6c9ac };
        static inline const juce::Colour ink         { 0xff2a2521 };
        static inline const juce::Colour inkDim      { 0xff7d7262 };
        static inline const juce::Colour red         { 0xffb8281f };
        static inline const juce::Colour glass       { 0xff120e0b };
        static inline const juce::Colour glassEdge   { 0xff2c241d };
        static inline const juce::Colour amber       { 0xffffb54a };
        static inline const juce::Colour amberDim    { 0xff5f4019 };
        static inline const juce::Colour bakelite    { 0xff1a1817 };
        static inline const juce::Colour felt        { 0xff5e1616 };
        static inline const juce::Colour topBar      { 0xff17130f };
    };

    ChilltronLookAndFeel();

    /** Walnut with a few seeded grain lines; the same seed always gives the same board. */
    static void paintWood (juce::Graphics&, juce::Rectangle<float>, int seed, bool vertical);

    /** A recessed glass panel, as the display window and the strips sit in. */
    static void paintGlass (juce::Graphics&, juce::Rectangle<float>);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
