#pragma once

#include <at_shared/at_shared.h>

/** The house style, re-skinned as an amp head: black tolex, a brass-piped control panel, cream
    chicken-head knobs and grille-cloth windows. */
class CabLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour tolex      { 0xff151515 };
        static inline const juce::Colour panel      { 0xff24221f };
        static inline const juce::Colour piping     { 0xffc9a65a };
        static inline const juce::Colour cream      { 0xffeee4cc };
        static inline const juce::Colour silkscreen { 0xffd9cfb6 };
        static inline const juce::Colour grille     { 0xff3b3226 };
        static inline const juce::Colour grilleWeave{ 0xff5a4b36 };
        static inline const juce::Colour screen     { 0xff0f1311 };
        static inline const juce::Colour trace      { 0xffffb347 };
        static inline const juce::Colour jewelOn    { 0xffff3b2a };
        static inline const juce::Colour jewelOff   { 0xff4a1d19 };
    };

    CabLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;

    /** Fills an area with a woven grille cloth. */
    static void drawGrille (juce::Graphics&, juce::Rectangle<float> area, float cornerSize);

    /** Fills an area with a tolex texture. */
    static void drawTolex (juce::Graphics&, juce::Rectangle<float> area);
};
