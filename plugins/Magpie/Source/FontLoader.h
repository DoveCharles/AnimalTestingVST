#pragma once

#include "SoundFont.h"

namespace magpie
{
    /** What the editor shows about the loaded font. */
    struct FontInfo
    {
        struct Entry
        {
            juce::String name;
            int bank = 0, program = 0;
        };

        juce::File file;
        juce::String name, error;
        bool loading = false;
        int serial = 0;              // counts loads; the audio thread tags what it plays with it
        juce::int64 sampleBytes = 0;
        std::vector<Entry> presets;
    };

    /** Reads SoundFonts on its own thread and hands them to the audio thread.

        The audio thread picks up a new font with a try-lock and never frees one: the font it
        replaces is parked in `retired` and freed here on the next pass. */
    class FontLoader final : private juce::Thread,
                             public juce::ChangeBroadcaster
    {
    public:
        FontLoader();
        ~FontLoader() override;

        /** Starts loading a file; an empty File unloads. Any thread. */
        void load (const juce::File&);

        /** Audio thread: the font to play, after picking up a newly loaded one if there is one.
            The previous font stays valid until the caller's next call, so swap it out of the
            engine straight away. */
        const SoundFont* acquire (int& serial) noexcept;

        FontInfo getInfo() const;
        juce::File getFile() const;
        /** The serial of the latest finished load, or -1 while one is in progress. */
        int getLoadedSerial() const;

    private:
        void run() override;

        mutable juce::CriticalSection infoLock;
        FontInfo info;
        juce::File requested;
        bool hasRequest = false;

        juce::SpinLock swapLock;
        std::unique_ptr<SoundFont> active, pending, retired;
        int activeSerial = 0, pendingSerial = 0;
        bool hasPending = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FontLoader)
    };
}
