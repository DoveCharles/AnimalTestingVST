// Offline checks for Notes, without a DAW.
//
//   NotesCheck check
//       Types into the editor, uses the Copy and Paste buttons, then saves the state and loads it
//       into a fresh instance (with and without the editor open) and checks the text survives.
//       Also checks the backup file is written. Prints PASS/FAIL per check.
//
//   NotesCheck snapshot <output.png>
//       Saves a picture of the editor with some text in it.

#include "PluginProcessor.h"

namespace
{
    int failures = 0;

    void expect (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "PASS  " : "FAIL  ") << what << "\n";
        if (! ok)
            ++failures;
    }

    void pump (int ms = 50)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    template <typename Type>
    Type* findChild (juce::Component& parent, std::function<bool (Type&)> match = {})
    {
        for (auto* c : parent.getChildren())
            if (auto* t = dynamic_cast<Type*> (c); t != nullptr && (! match || match (*t)))
                return t;
        return nullptr;
    }

    juce::TextButton* findButton (juce::Component& parent, const juce::String& name)
    {
        return findChild<juce::TextButton> (parent, [&] (juce::TextButton& b) { return b.getButtonText() == name; });
    }

    juce::MemoryBlock saveState (PluginProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    int check()
    {
        // Every instance below that gets text writes a backup; remove the ones this run makes.
        const auto backupDir = PluginProcessor::getBackupDirectory();
        const auto existingBackups = backupDir.findChildFiles (juce::File::findFiles, false, "*.txt");

        const juce::String lyrics = juce::String::fromUTF8 (
            "Verse 1\n\tI walked down to the river <slowly> & \"quietly\"\n"
            "Ch\xc3\xb3rus: \xe2\x99\xab la la \xf0\x9f\x90\x95\n\nBPM 92, key of F# minor\n");

        auto processor = std::make_unique<PluginProcessor>();
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        expect (editor->getWidth() == 520 && editor->getHeight() == 420, "editor opens at its default size");
        auto* page = findChild<juce::TextEditor> (*editor);
        expect (page != nullptr, "editor has a text page");
        if (page == nullptr)
            return 1;

        // Typing goes straight through to the processor, with the window still open.
        page->insertTextAtCaret (lyrics);
        pump();
        expect (processor->getText() == lyrics, "typed text reaches the processor");

        // Copy with nothing selected copies the whole page.
        page->setHighlightedRegion ({});
        findButton (*editor, "Copy")->triggerClick();
        pump();
        expect (juce::SystemClipboard::getTextFromClipboard() == lyrics, "Copy with no selection copies everything");

        // Copy with a selection copies just that.
        page->setHighlightedRegion ({ 0, 7 });
        findButton (*editor, "Copy")->triggerClick();
        pump();
        expect (juce::SystemClipboard::getTextFromClipboard() == "Verse 1", "Copy copies the selection");

        // Paste inserts at the cursor and is saved like typing.
        juce::SystemClipboard::copyTextToClipboard ("PASTED ");
        page->setCaretPosition (0);
        findButton (*editor, "Paste")->triggerClick();
        pump();
        const auto withPaste = "PASTED " + lyrics;
        expect (processor->getText() == withPaste, "Paste inserts at the cursor and reaches the processor");

        // Save while the editor is open, load into a fresh instance with no editor.
        {
            const auto state = saveState (*processor);
            PluginProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (restored.getText() == withPaste, "state round-trips with the editor open");
        }

        // Close the window, then save: the text must not depend on the editor.
        processor->fontSize = 22.0f;
        editor->setSize (700, 500);
        editor.reset();
        const auto closedState = saveState (*processor);
        {
            PluginProcessor restored;
            restored.setStateInformation (closedState.getData(), (int) closedState.getSize());
            expect (restored.getText() == withPaste, "state round-trips after the editor is closed");
            std::unique_ptr<juce::AudioProcessorEditor> reopened (restored.createEditor());
            expect (reopened->getWidth() == 700 && reopened->getHeight() == 500, "editor reopens at its saved size");
            expect (juce::exactlyEqual (restored.fontSize.load(), 22.0f), "font size is restored");
        }

        // Loading a set while the window is open updates the page.
        {
            PluginProcessor other;
            std::unique_ptr<juce::AudioProcessorEditor> otherEditor (other.createEditor());
            other.setStateInformation (closedState.getData(), (int) closedState.getSize());
            pump();
            expect (findChild<juce::TextEditor> (*otherEditor)->getText() == withPaste, "loading a state updates an open editor");
        }

        // A long document (about 1 MB).
        {
            juce::String big;
            for (int i = 0; i < 20000; ++i)
                big << "Line " << i << " of a very long set of notes, lyrics and reminders.\n";
            PluginProcessor p;
            p.setText (big);
            const auto state = saveState (p);
            PluginProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (restored.getText() == big, "1 MB of text round-trips");
        }

        // Garbage state is ignored, not crashed on.
        {
            PluginProcessor p;
            p.setText ("keep me");
            const char junk[] = "not a state";
            p.setStateInformation (junk, (int) sizeof (junk));
            expect (p.getText() == "keep me", "garbage state is ignored");
        }

        // The backup copy is written once typing pauses.
        {
            pump (2000);
            const auto backup = processor->getBackupFile();
            expect (backup.existsAsFile() && backup.loadFileAsString() == withPaste, "backup file is written: " + backup.getFullPathName());
        }
        processor.reset();

        for (const auto& f : backupDir.findChildFiles (juce::File::findFiles, false, "*.txt"))
            if (! existingBackups.contains (f))
                f.deleteFile();

        std::cout << (failures == 0 ? "All checks passed\n" : juce::String (failures) + " check(s) failed\n");
        return failures == 0 ? 0 : 1;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        PluginProcessor processor;
        processor.setText ("Verse 1\nI walked down to the river slowly\nand the water didn't move\n\n"
                           "Chorus\nla la la\n\n- vocal take 3 is the keeper\n- BPM 92, F# minor");
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        pump (200);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = args[1].resolveAsFile();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    const auto command = args.size() > 0 ? args[0].text : juce::String();
    if (command == "check")                        return check();
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  NotesCheck check\n"
                 "  NotesCheck snapshot <output.png>\n";
    return 1;
}
