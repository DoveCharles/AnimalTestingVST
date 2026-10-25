#pragma once

#include "PluginProcessor.h"
#include "CabLookAndFeel.h"

/** Front view of the speaker with the mic over it: drag sideways to move from the dust cap to the
    cone edge, up and down to pull the mic back. Bound to Mic Position and Mic Distance. */
class MicPad final : public juce::Component
{
public:
    explicit MicPad (juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> speakerArea() const;
    juce::Point<float> micPoint() const;
    void moveTo (juce::Point<float>);

    juce::RangedAudioParameter& positionParameter;
    juce::RangedAudioParameter& distanceParameter;
    juce::ParameterAttachment positionAttachment, distanceAttachment;
    float position = 0.35f, distance = 0.15f;
};

/** The frequency response of the IR that's playing, redrawn whenever it's rebuilt. */
class ResponseView final : public juce::Component
{
public:
    void setResponse (const std::array<float, PluginProcessor::numResponsePoints>&);
    void paint (juce::Graphics&) override;

private:
    std::array<float, PluginProcessor::numResponsePoints> response {};
    bool hasResponse = false;
};

/** Red jewel lamp that doubles as the on/off switch; its toggle state is "bypassed". */
class JewelSwitch final : public juce::Button
{
public:
    JewelSwitch() : juce::Button ("Bypass") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void chooseImpulseFile();
    void loadImpulse (const juce::File&);
    void updateImpulseLabel();

    PluginProcessor& cabProcessor;
    CabLookAndFeel lookAndFeel;

    juce::ComboBox modelBox, micBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment, micAttachment;
    juce::TextButton loadButton { "LOAD IR" };
    juce::Label impulseLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    MicPad micPad;
    ResponseView responseView;

    at::Knob position, distance, room, push, lowCut, highCut, output;
    JewelSwitch jewel;
    juce::AudioProcessorValueTreeState::ButtonAttachment jewelAttachment;

    juce::Rectangle<int> panelArea;
    int lastResponseVersion = -1;
    int lastModel = -1;
    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
