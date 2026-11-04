#pragma once

#include "PluginProcessor.h"

/** Colours of a 1970s talking-toy case: red-orange plastic, a cream keypad with navy
    lettering, and a glowing display. */
struct Toy
{
    static inline const juce::Colour caseLight { 0xffe2532a };
    static inline const juce::Colour caseDark  { 0xffb63716 };
    static inline const juce::Colour caseDeep  { 0xff8c2810 };
    static inline const juce::Colour plate     { 0xfff0dfae };
    static inline const juce::Colour key       { 0xfffbf3dc };
    static inline const juce::Colour navy      { 0xff1d3266 };
    static inline const juce::Colour cream     { 0xfffff4dc };
    static inline const juce::Colour screen    { 0xff0b0f0e };
    static inline const juce::Colour glow      { 0xff6ff7d6 };
};

/** Rubber keys, cream knobs and dark slots, over the shared house style. */
class ToyLookAndFeel final : public at::LookAndFeel
{
public:
    ToyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

/** The glowing display: the syllable being sung, big, with the next one and its sounds. */
class SpellDisplay final : public juce::Component
{
public:
    void set (const juce::String& now, const juce::String& next, const juce::String& sounds);
    void paint (juce::Graphics&) override;

private:
    juce::String nowText, nextText, soundText;
};

/** The lyrics as the notes will sing them: one key per syllable. The sounding one is lit,
    the next one outlined; click a key to sing from there. */
class SyllableStrip final : public juce::Component
{
public:
    explicit SyllableStrip (PluginProcessor&);

    void refresh();                   // after the lyrics change
    void setPosition (int sung, int next);

    const juce::StringArray& getLabels() const noexcept { return labels; }
    const juce::StringArray& getSounds() const noexcept { return sounds; }

    void paint (juce::Graphics&) override;
    void resized() override { layout(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

private:
    void layout();
    void setHovered (int);

    PluginProcessor& owner;
    juce::StringArray labels, sounds;
    std::vector<juce::Rectangle<float>> chips;
    int sungIndex = -1, nextIndex = 0, hovered = -1;
    float scroll = 0.0f;
};

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateDisplay();

    PluginProcessor& owner;
    ToyLookAndFeel lookAndFeel;

    at::PresetBar presetBar;
    juce::Label spellingLabel { {}, "SPELLING" }, timingLabel { {}, "TIMING" };
    juce::ComboBox spellingBox, timingBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment spellingAttachment, timingAttachment;

    SpellDisplay display;
    juce::TextEditor lyrics;
    juce::TextButton pasteButton { "PASTE" };
    juce::TextButton restartButton { "|<" }, backButton { "<" }, forwardButton { ">" };
    juce::Label hint;
    SyllableStrip strip;

    at::Knob size, tone, breath, diction, vibrato, vibRate, glide, release, lofi, volume;
    juce::MidiKeyboardComponent keyboard;

    juce::Rectangle<int> grilleArea, plateArea, titleArea;
    int shownSung = -2, shownNext = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
