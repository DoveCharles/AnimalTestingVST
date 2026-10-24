#pragma once

namespace at
{
    /** The single house style for every Animal Testing plugin.
        Editors get it via juce::SharedResourcePointer<at::LookAndFeel>. */
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        struct Palette
        {
            static inline const juce::Colour background { 0xff16181d };
            static inline const juce::Colour panel      { 0xff20232a };
            static inline const juce::Colour track      { 0xff343843 };
            static inline const juce::Colour accent     { 0xffff7a3d };
            static inline const juce::Colour text       { 0xffe8e6e3 };
            static inline const juce::Colour textDim    { 0xff8a8f9c };
        };

        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

        juce::Label* createSliderTextBox (juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                   bool highlighted, bool down) override;

        juce::Font getLabelFont (juce::Label&) override;

        /** Fills the editor background and draws the plugin title + version header. Returns the area below it. */
        static juce::Rectangle<int> drawEditorChrome (juce::Graphics&, juce::Rectangle<int> bounds,
                                                     const juce::String& title, const juce::String& version);

        static constexpr int headerHeight = 44;
    };
}
