#include "PluginEditor.h"

namespace
{
    constexpr float meterRangeDb = 12.0f;
    const std::array<const char*, Maximizer::numBands + 1> meterCaptions { "LOW", "MID", "HIGH", "LIM" };
}

void PluginEditor::Meters::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (at::LookAndFeel::Palette::panel);
    g.fillRoundedRectangle (area, 6.0f);

    auto inner = area.reduced (10.0f, 8.0f);
    auto captions = inner.removeFromBottom (16.0f);
    const auto width = inner.getWidth() / (float) reduction.size();

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (size_t i = 0; i < reduction.size(); ++i)
    {
        const auto column = inner.withX (inner.getX() + width * (float) i).withWidth (width).reduced (6.0f, 0.0f);
        g.setColour (at::LookAndFeel::Palette::track);
        g.fillRoundedRectangle (column, 3.0f);

        const auto t = juce::jlimit (0.0f, 1.0f, reduction[i] / meterRangeDb);
        g.setColour (at::LookAndFeel::Palette::accent);
        g.fillRoundedRectangle (column.withHeight (t * column.getHeight()), 3.0f);

        g.setColour (at::LookAndFeel::Palette::textDim);
        g.drawText (meterCaptions[i], captions.withX (column.getX()).withWidth (column.getWidth()), juce::Justification::centred);
    }
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      goodBoy (p),
      presetBar (p.presets),
      amount (p.apvts, ParamIDs::amount, "Amount"),
      output (p.apvts, ParamIDs::output, "Output"),
      modeAttachment (*p.apvts.getParameter (ParamIDs::mode), [this] (float index) { showMode ((int) index); })
{
    setLookAndFeel (&sharedLookAndFeel.get());

    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        auto& button = modeButtons[i];
        button.setButtonText (Modes::names[(int) i]);
        button.setTooltip (Modes::descriptions[(int) i]);
        button.setColour (juce::TextButton::buttonOnColourId, at::LookAndFeel::Palette::accent);
        button.setColour (juce::TextButton::textColourOnId, at::LookAndFeel::Palette::background);
        button.onClick = [this, i] { modeAttachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (button);
    }
    modeAttachment.sendInitialUpdate();

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &amount, &output, &meters })
        addAndMakeVisible (c);

    setSize (460, 360);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::showMode (int index)
{
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i].setToggleState ((int) i == index, juce::dontSendNotification);
    repaint();
}

void PluginEditor::timerCallback()
{
    // Fast up, slower down.
    auto& m = goodBoy.maximizer;
    for (size_t i = 0; i < meters.reduction.size(); ++i)
    {
        const auto value = i < Maximizer::numBands ? m.bandReduction[i].load() : m.limiterReduction.load();
        auto& shown = meters.reduction[i];
        shown = value > shown ? value : shown + 0.2f * (value - shown);
    }
    meters.repaint();
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Good Boy", JucePlugin_VersionString);

    // The voicing's name under the mode buttons.
    const auto& first = modeButtons.front();
    const auto& last = modeButtons.back();
    const auto index = juce::jlimit (0, 3, (int) goodBoy.apvts.getRawParameterValue (ParamIDs::mode)->load());
    g.setColour (at::LookAndFeel::Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (Modes::descriptions[index].toUpperCase(),
                juce::Rectangle<int> (first.getX(), first.getBottom() + 4, last.getRight() - first.getX(), 16),
                juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (12);

    auto left = area.removeFromLeft (230);
    area.removeFromLeft (12);

    auto modes = left.removeFromBottom (48).removeFromTop (28);
    const auto buttonWidth = modes.getWidth() / (int) modeButtons.size();
    for (auto& button : modeButtons)
        button.setBounds (modes.removeFromLeft (buttonWidth).reduced (3, 0));
    amount.setBounds (left.withSizeKeepingCentre (200, left.getHeight()));

    output.setBounds (area.removeFromBottom (110).withSizeKeepingCentre (100, 110));
    area.removeFromBottom (8);
    meters.setBounds (area);
}
