#pragma once

namespace at
{
    /** Rotary knob + caption, bound to an APVTS parameter. Value text comes from the parameter. */
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
            : attachment (state, paramId, slider)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
            slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            addAndMakeVisible (slider);

            label.setText (caption.toUpperCase(), juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, LookAndFeel::Palette::textDim);
            addAndMakeVisible (label);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (18));
            slider.setBounds (r);
        }

        juce::Slider& getSlider() noexcept { return slider; }

    private:
        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };
}
