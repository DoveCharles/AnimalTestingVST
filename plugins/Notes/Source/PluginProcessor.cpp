#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // How long typing has to pause before the backup file is rewritten.
    constexpr int backupDelayMs = 1500;
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase ({})
{
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
    if (backupPending)
        writeBackup();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

//==============================================================================
juce::String PluginProcessor::getText() const
{
    const juce::ScopedLock sl (textLock);
    return text;
}

void PluginProcessor::setText (const juce::String& newText)
{
    {
        const juce::ScopedLock sl (textLock);
        if (text == newText)
            return;
        text = newText;
    }

    // No parameter changes here, so tell the host directly that the set needs saving.
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));

    backupPending = true;
    startTimer (backupDelayMs);
}

void PluginProcessor::timerCallback()
{
    stopTimer();
    writeBackup();
}

juce::File PluginProcessor::getBackupDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
       #if JUCE_MAC
               .getChildFile ("Application Support")
       #endif
               .getChildFile (JucePlugin_Manufacturer)
               .getChildFile (JucePlugin_Name)
               .getChildFile ("Backups");
}

juce::File PluginProcessor::getBackupFile() const
{
    return backupFile;
}

void PluginProcessor::writeBackup()
{
    backupPending = false;

    const auto current = getText();
    if (current.trim().isEmpty() && ! backupFile.existsAsFile())
        return;

    if (backupFile == juce::File())
    {
        const auto dir = getBackupDirectory();
        dir.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S");
        backupFile = dir.getNonexistentChildFile (stamp, ".txt", false);
    }

    // replaceWithText writes a temporary file and swaps it in, so a crash mid-write leaves the old copy.
    backupFile.replaceWithText (current, false, false, "\n");
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty (StateKeys::text, getText(), nullptr);
    state.setProperty (StateKeys::width, editorWidth.load(), nullptr);
    state.setProperty (StateKeys::height, editorHeight.load(), nullptr);
    state.setProperty (StateKeys::fontSize, fontSize.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    {
        const juce::ScopedLock sl (textLock);
        text = state.getProperty (StateKeys::text).toString();
    }
    editorWidth  = (int) state.getProperty (StateKeys::width, editorWidth.load());
    editorHeight = (int) state.getProperty (StateKeys::height, editorHeight.load());
    fontSize     = (float) state.getProperty (StateKeys::fontSize, fontSize.load());

    // May arrive off the message thread; the editor picks the text up asynchronously.
    sendChangeMessage();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
