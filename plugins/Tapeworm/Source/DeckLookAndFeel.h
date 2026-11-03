#pragma once

#include <at_shared/at_shared.h>

/** The house style, laid out as a picnic: a gingham tablecloth, the controls on paper recipe
    cards taped down, knobs like the lids of sweet jars, chunky cartoon buttons, and the tape
    window on a chopping board. Everything gets the same dark ink outline and flat colours. */
class DeckLookAndFeel final : public at::LookAndFeel
{
public:
    struct Palette
    {
        static inline const juce::Colour ink       { 0xff2b1a1e };
        static inline const juce::Colour cloth     { 0xff85675d };
        static inline const juce::Colour check     { 0xff5c2b2a };
        static inline const juce::Colour paper     { 0xffecd9b0 };
        static inline const juce::Colour rule      { 0xffc9776a };
        static inline const juce::Colour tape      { 0xb8f7e7a6 };
        static inline const juce::Colour cream     { 0xffe6cf9f };
        static inline const juce::Colour mustard   { 0xffffc93c };
        static inline const juce::Colour cherry    { 0xffe8473f };
        static inline const juce::Colour wood      { 0xffd59a5c };
        static inline const juce::Colour woodDark  { 0xffa86a38 };
    };

    DeckLookAndFeel();

    /** A rounded, hand-lettered font, the closest the system has. */
    static juce::Font hand (float size);

    /** A dark red gingham tablecloth over the given size, for the cards to sit on. */
    static juce::Image gingham (int width, int height);

    /** A paper card with a soft shadow, a strip of tape over one corner and a ruled line under
        the title. The seed tilts the tape a little differently for each card. */
    static void drawCard (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, int seed);

    /** A wooden chopping board with its grain. */
    static void drawBoard (juce::Graphics&, juce::Rectangle<float>);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};
