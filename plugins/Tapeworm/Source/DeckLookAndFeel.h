#pragma once

#include <at_shared/at_shared.h>

/** The house style, rebuilt as a studio tape deck: a matte silver faceplate screwed to a
    black chassis, engraved black lettering, big knurled knobs with black caps, chunky push
    buttons with an amber lamp, and amber-on-black readouts for the choice boxes. */
class DeckLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour chassis    { 0xff141518 };
        static inline const juce::Colour metal      { 0xffd0d2d5 };
        static inline const juce::Colour metalLight { 0xffe8e9eb };
        static inline const juce::Colour metalDark  { 0xff8e9298 };
        static inline const juce::Colour ink        { 0xff1d1f23 };
        static inline const juce::Colour inkSoft    { 0xff4a4e55 };
        static inline const juce::Colour cap        { 0xff1b1c20 };
        static inline const juce::Colour window     { 0xff0d0e10 };
        static inline const juce::Colour amber      { 0xffffa43a };
        static inline const juce::Colour lampOff    { 0xff5a3a1c };
        static inline const juce::Colour pointer    { 0xffff7a3d };
    };

    DeckLookAndFeel();

    /** Matte silver over the given size, like a tape deck's bead-blasted faceplate: soft light
        from the top and a fine speckle. The same seed always gives the same plate. */
    static juce::Image matteMetal (int width, int height, juce::int64 seed);

    /** A raised plate edge: light on the top left, dark on the bottom right. */
    static void drawBevel (juce::Graphics&, juce::Rectangle<float>, float cornerSize, bool raised);

    /** A recessed panel cut into the faceplate. */
    static void drawRecess (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill);

    /** A slotted screw head. */
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};
