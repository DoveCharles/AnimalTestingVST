#pragma once

#include <at_shared/at_shared.h>

namespace StateKeys
{
    inline constexpr auto text       = "notesText";
    inline constexpr auto width      = "editorWidth";
    inline constexpr auto height     = "editorHeight";
    inline constexpr auto fontSize   = "fontSize";
}

/** A pass-through effect that holds a page of text in the set.

    The text lives here, not in the editor, so it is saved whether or not the window is open.
    Every edit marks the set as changed so the host asks to save it, and a copy is written to
    ~/Library/Application Support/Animal Testing/AT Notes/Backups a moment after typing stops,
    in case the set is never saved. */
class PluginProcessor final : public at::ProcessorBase,
                              public juce::ChangeBroadcaster,
                              private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double, int) override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::String getText() const;
    /** Called by the editor on every edit (message thread). */
    void setText (const juce::String&);

    static juce::File getBackupDirectory();
    juce::File getBackupFile() const;

    // Editor size and font, kept with the set.
    std::atomic<int> editorWidth { 520 }, editorHeight { 420 };
    std::atomic<float> fontSize { 16.0f };

private:
    void timerCallback() override;
    void writeBackup();

    mutable juce::CriticalSection textLock;
    juce::String text;
    // Each instance writes its own backup file, named when it is first edited, so duplicated
    // tracks and reopened sets never overwrite one another's backups.
    juce::File backupFile;
    bool backupPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
