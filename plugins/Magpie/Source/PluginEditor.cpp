#include "PluginEditor.h"

using Palette = at::LookAndFeel::Palette;

namespace
{
    bool isSoundFont (const juce::String& path)
    {
        return juce::File (path).hasFileExtension ("sf2");
    }

    juce::String describeSize (juce::int64 bytes)
    {
        return bytes >= 1024 * 1024 ? juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB"
                                    : juce::String (bytes / 1024) + " KB";
    }
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      owner (p),
      presetBar (p.presets),
      volume    (p.apvts, ParamIDs::volume, "Volume"),
      transpose (p.apvts, ParamIDs::transpose, "Transpose"),
      tune      (p.apvts, ParamIDs::tune, "Tune"),
      bendRange (p.apvts, ParamIDs::bendRange, "Bend"),
      velocity  (p.apvts, ParamIDs::velocity, "Velocity"),
      release   (p.apvts, ParamIDs::release, "Release"),
      reverb    (p.apvts, ParamIDs::reverb, "Reverb"),
      keyboard  (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&sharedLookAndFeel.get());

    loadButton.onClick = [this] { chooseFile(); };
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    presetBox.setTextWhenNothingSelected ("No SoundFont loaded");
    presetBox.setTextWhenNoChoicesAvailable ("No presets");
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index < (int) info.presets.size())
        {
            const auto& entry = info.presets[(size_t) index];
            owner.selectPreset (entry.bank, entry.program);
        }
    };

    keyboard.setAvailableRange (0, 127);
    keyboard.setLowestVisibleKey (36);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.2f));

    addAndMakeVisible (presetBar);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (nextButton);
    for (auto* k : { &volume, &transpose, &tune, &bendRange, &velocity, &release, &reverb })
        addAndMakeVisible (*k);
    addAndMakeVisible (keyboard);

    owner.fonts.addChangeListener (this);
    refreshFont();

    setSize (720, 430);
    startTimerHz (10);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    owner.fonts.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void PluginEditor::refreshFont()
{
    info = owner.fonts.getInfo();

    presetBox.clear (juce::dontSendNotification);
    int lastBank = -1;
    for (size_t i = 0; i < info.presets.size(); ++i)
    {
        const auto& entry = info.presets[i];
        if (entry.bank != lastBank)
        {
            presetBox.addSectionHeading (entry.bank == 128 ? juce::String ("Percussion (bank 128)")
                                                           : "Bank " + juce::String (entry.bank));
            lastBank = entry.bank;
        }
        presetBox.addItem (juce::String (entry.program + 1).paddedLeft ('0', 3) + "  " + entry.name, (int) i + 1);
    }

    const auto hasPresets = ! info.presets.empty();
    presetBox.setEnabled (hasPresets);
    prevButton.setEnabled (hasPresets);
    nextButton.setEnabled (hasPresets);

    shownPreset = -2;
    showPlaying();
    repaint();
}

void PluginEditor::showPlaying()
{
    const auto playing = owner.getPlayingPreset();
    if (playing == shownPreset)
        return;

    shownPreset = playing;
    for (size_t i = 0; i < info.presets.size(); ++i)
    {
        const auto& entry = info.presets[i];
        if (PluginProcessor::packPreset (entry.bank, entry.program) == playing)
        {
            presetBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
            return;
        }
    }
    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void PluginEditor::timerCallback()
{
    showPlaying();

    const auto voices = owner.getActiveVoices();
    if (voices != shownVoices)
    {
        shownVoices = voices;
        repaint (fontArea);
    }
}

void PluginEditor::step (int delta)
{
    const auto count = presetBox.getNumItems();
    if (count == 0)
        return;

    const auto current = presetBox.getSelectedItemIndex();
    const auto next = current < 0 ? 0 : (current + delta + count) % count;
    presetBox.setSelectedItemIndex (next, juce::sendNotificationSync);
}

void PluginEditor::chooseFile()
{
    const auto current = owner.fonts.getFile();
    chooser = std::make_unique<juce::FileChooser> ("Load a SoundFont", current.existsAsFile() ? current : juce::File(), "*.sf2");
    juce::Component::SafePointer<PluginEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safe != nullptr && file.existsAsFile())
                                  safe->owner.loadSoundFont (file);
                          });
}

bool PluginEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), isSoundFont);
}

void PluginEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    for (const auto& path : files)
    {
        if (isSoundFont (path))
        {
            owner.loadSoundFont (juce::File (path));
            break;
        }
    }
    repaint();
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Magpie", JucePlugin_VersionString);

    // The font: its name, then where it came from and what it's doing.
    const auto box = fontArea.toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (box, 6.0f);
    if (dragging)
    {
        g.setColour (Palette::accent);
        g.drawRoundedRectangle (box.reduced (1.0f), 6.0f, 2.0f);
    }

    auto text = fontArea.reduced (14, 10);
    text.removeFromRight (loadButton.getWidth() + 10);

    juce::String title, detail;
    auto detailColour = Palette::textDim;
    if (info.loading)
    {
        title = info.file.getFileNameWithoutExtension();
        detail = "Loading...";
    }
    else if (info.error.isNotEmpty())
    {
        title = info.file.getFileName();
        detail = info.error;
        detailColour = juce::Colour (0xffff5c5c);
    }
    else if (info.file == juce::File())
    {
        title = "Drop a .sf2 here";
        detail = "or press Load SF2...";
    }
    else
    {
        title = info.name.isNotEmpty() ? info.name : info.file.getFileNameWithoutExtension();
        detail = info.file.getFileName() + "   " + juce::String ((int) info.presets.size()) + " presets   "
                 + describeSize (info.sampleBytes) + " of samples   "
                 + juce::String (juce::jmax (0, shownVoices)) + " voices";
    }

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawFittedText (title, text.removeFromTop (text.getHeight() / 2 + 2), juce::Justification::bottomLeft, 1);
    g.setColour (detailColour);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (detail, text, juce::Justification::topLeft, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (14, 10);

    presetBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);

    fontArea = area.removeFromTop (62);
    loadButton.setBounds (fontArea.reduced (14, 0).removeFromRight (110).withSizeKeepingCentre (110, 28));
    area.removeFromTop (10);

    auto presetRow = area.removeFromTop (28);
    prevButton.setBounds (presetRow.removeFromLeft (32));
    nextButton.setBounds (presetRow.removeFromRight (32));
    presetBox.setBounds (presetRow.reduced (6, 0));
    area.removeFromTop (10);

    keyboard.setBounds (area.removeFromBottom (96));
    area.removeFromBottom (8);

    juce::Component* knobs[] = { &volume, &transpose, &tune, &bendRange, &velocity, &release, &reverb };
    const auto width = area.getWidth() / (int) std::size (knobs);
    for (auto* k : knobs)
        k->setBounds (area.removeFromLeft (width).reduced (4, 0));
}
