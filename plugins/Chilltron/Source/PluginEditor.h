#pragma once

#include "PluginProcessor.h"
#include "ChilltronLookAndFeel.h"

namespace chilltron::ui
{
    /** Fluted knob with its caption silkscreened beneath; the caption shows the value while the
        knob is hovered or turned, so the panel stays uncluttered. */
    class Knob final : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    private:
        juce::Slider slider;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
        juce::String caption;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };

    /** One line of the display: which tape a bank is playing. Click the name for the list, or
        step through with the arrows. */
    class TapeRow final : public juce::Component
    {
    public:
        TapeRow (PluginProcessor&, const juce::String& paramId, const juce::String& badge);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

        int getIndex() const noexcept { return index; }

    private:
        juce::Rectangle<float> leftArrow() const;
        juce::Rectangle<float> rightArrow() const;
        void step (int delta);
        void showMenu();

        PluginProcessor& processor;
        juce::String badge;
        int index = 0;
        juce::ParameterAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeRow)
    };

    /** The amber window: both tape banks, the A/B blend, and a strip per key showing how much of
        its tape has run. */
    class TapeDisplay final : public juce::Component
    {
    public:
        explicit TapeDisplay (PluginProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Rectangle<int> stripArea() const;
        void chooseFolder();

        PluginProcessor& processor;
        TapeRow rowA, rowB;
        juce::Slider blend;
        juce::AudioProcessorValueTreeState::SliderAttachment blendAttachment;
        juce::TextButton folderButton { "TAPES..." };
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeDisplay)
    };
}

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { display.repaint(); }

    static constexpr int cheekWidth = 30;

    juce::SharedResourcePointer<ChilltronLookAndFeel> lookAndFeel;

    at::PresetBar presetBar;
    chilltron::ui::TapeDisplay display;

    chilltron::ui::Knob pitch, wow, click;
    juce::ToggleButton loop { "Loop" };
    juce::AudioProcessorValueTreeState::ButtonAttachment loopAttachment;
    chilltron::ui::Knob attack, release;
    juce::ToggleButton velocity { "Velocity" };
    juce::AudioProcessorValueTreeState::ButtonAttachment velocityAttachment;
    chilltron::ui::Knob tone, spread, reverb, volume;

    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 700 };

    // Where each group of controls sits, for the silkscreened frames painted behind them.
    std::array<juce::Rectangle<int>, 3> groups;
    juce::Rectangle<int> feltArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
