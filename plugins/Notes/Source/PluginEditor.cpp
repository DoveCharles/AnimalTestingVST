#include "PluginEditor.h"

namespace
{
    constexpr float minFontSize = 10.0f, maxFontSize = 40.0f;
    constexpr int toolbarHeight = 26;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      notes (p)
{
    using Palette = at::LookAndFeel::Palette;
    setLookAndFeel (&sharedLookAndFeel.get());

    page.setMultiLine (true, true);
    page.setReturnKeyStartsNewLine (true);
    page.setTabKeyUsedAsCharacter (true);
    page.setScrollbarsShown (true);
    page.setIndents (10, 10);
    page.setColour (juce::TextEditor::backgroundColourId, Palette::panel);
    page.setColour (juce::TextEditor::textColourId, Palette::text);
    page.setColour (juce::TextEditor::highlightColourId, Palette::accent.withAlpha (0.35f));
    page.setColour (juce::CaretComponent::caretColourId, Palette::accent);
    page.setColour (juce::TextEditor::outlineColourId, Palette::track);
    page.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    page.setTextToShowWhenEmpty ("Lyrics, notes, anything. Saved with the set.", Palette::textDim);
    page.setFont (juce::FontOptions (notes.fontSize.load()));
    page.setText (notes.getText(), false);
    page.onTextChange = [this] { notes.setText (page.getText()); };

    // Hosts sometimes keep Cmd+C / Cmd+V for themselves, so these (and the right-click menu) always work.
    copy.setTooltip ("Copy the selection, or the whole page if nothing is selected");
    copy.onClick = [this]
    {
        const auto selected = page.getHighlightedText();
        juce::SystemClipboard::copyTextToClipboard (selected.isNotEmpty() ? selected : page.getText());
    };
    paste.setTooltip ("Paste at the cursor");
    paste.onClick = [this]
    {
        page.grabKeyboardFocus();
        page.paste();
    };

    smaller.onClick = [this] { setFontSize (notes.fontSize.load() - 2.0f); };
    larger.onClick  = [this] { setFontSize (notes.fontSize.load() + 2.0f); };
    backups.setTooltip ("Show the folder of backup copies");
    backups.onClick = [this]
    {
        if (const auto file = notes.getBackupFile(); file.existsAsFile())
            file.revealToUser();
        else if (const auto dir = PluginProcessor::getBackupDirectory(); dir.createDirectory())
            dir.revealToUser();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &page, &copy, &paste, &smaller, &larger, &backups })
        addAndMakeVisible (c);

    notes.addChangeListener (this);

    // Read the saved size first: setResizeLimits resizes the empty editor, and resized() stores it.
    const auto width = notes.editorWidth.load(), height = notes.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (320, 240, 2000, 1600);
    setSize (width, height);
}

PluginEditor::~PluginEditor()
{
    notes.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void PluginEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The host restored a state (a set loaded, or an undo).
    if (const auto text = notes.getText(); text != page.getText())
        page.setText (text, false);

    page.applyFontToAllText (juce::FontOptions (notes.fontSize.load()));
    setSize (notes.editorWidth.load(), notes.editorHeight.load());
}

void PluginEditor::setFontSize (float size)
{
    notes.fontSize = juce::jlimit (minFontSize, maxFontSize, size);
    page.applyFontToAllText (juce::FontOptions (notes.fontSize.load()));
}

void PluginEditor::paint (juce::Graphics& g)
{
    at::LookAndFeel::drawEditorChrome (g, getLocalBounds(), "Notes", JucePlugin_VersionString);
}

void PluginEditor::resized()
{
    notes.editorWidth  = getWidth();
    notes.editorHeight = getHeight();

    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight).reduced (12);

    auto toolbar = area.removeFromBottom (toolbarHeight);
    area.removeFromBottom (8);

    copy.setBounds (toolbar.removeFromLeft (60));
    toolbar.removeFromLeft (4);
    paste.setBounds (toolbar.removeFromLeft (60));

    backups.setBounds (toolbar.removeFromRight (80));
    toolbar.removeFromRight (6);
    larger.setBounds (toolbar.removeFromRight (36));
    toolbar.removeFromRight (4);
    smaller.setBounds (toolbar.removeFromRight (36));

    page.setBounds (area);
}
