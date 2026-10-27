#pragma once

#include <at_shared/at_shared.h>

namespace chilltron
{
    /** One instrument's worth of tape: 16-bit PCM for every key, as the SoundFont stores it. */
    struct Tape
    {
        struct Zone
        {
            int offset = 0, length = 0;   // into pcm, in samples
            double sampleRate = 22050.0;
        };

        /** How a MIDI key plays this tape: which zone, and how fast relative to the zone's own rate. */
        struct Key
        {
            int zone = -1;
            double ratio = 1.0;
        };

        juce::String name;
        std::vector<juce::int16> pcm;
        std::vector<Zone> zones;
        std::array<Key, 128> keys;
    };

    /** The tapes Chilltron knows about, in the order the Tape A / Tape B parameters list them.
        `preset` is the preset name inside the .sf2, which is how a tape is found on disk. */
    struct TapeInfo
    {
        const char* preset;
        const char* display;
    };

    inline constexpr std::array<TapeInfo, 11> catalogue { {
        { "M400 String Section", "M400 String Section" },
        { "MKII 3 Violins",      "MkII 3 Violins" },
        { "M300A Violins",       "M300 A Violins" },
        { "M300B Violin",        "M300 B Violin" },
        { "M400 Cello",          "M400 Cello" },
        { "MKII Flute",          "MkII Flute" },
        { "M400 Woodwind2",      "M400 Woodwind" },
        { "MKII Brass",          "MkII Brass" },
        { "Gc3 Brass",           "GC3 Brass" },
        { "M400 Cmb Choir",      "M400 Choir" },
        { "Gong",                "Gong" },
    } };

    inline constexpr int numTapes = (int) catalogue.size();

    juce::StringArray tapeDisplayNames();

    /** Reads every preset from a SoundFont 2 file. Only what a one-shot tape player needs is kept:
        key ranges, root keys, tuning and the sample data. Returns an empty list on a bad file. */
    std::vector<std::unique_ptr<Tape>> readSoundFont (const juce::File&);

    /** Finds the tapes on disk and loads them on demand on its own thread. One library is shared
        by every Chilltron instance in the process. Loaded tapes are never freed or changed, so the
        audio thread can hold plain pointers to them. */
    class TapeLibrary final : private juce::Thread
    {
    public:
        enum class Status { missing, loading, ready };

        TapeLibrary();
        ~TapeLibrary() override;

        /** nullptr until the tape has loaded. Safe on the audio thread. */
        const Tape* get (int index) const noexcept;
        /** The key-click tape (TapeKlik.sf2), or nullptr. Safe on the audio thread. */
        const Tape* getClick() const noexcept { return click.load (std::memory_order_acquire); }

        /** Asks for a tape to be loaded. Lock-free: safe on the audio thread. */
        void request (int index) noexcept;
        Status getStatus (int index) const noexcept;

        juce::File getFolder() const;
        /** Points the library at a new folder (searched recursively) and remembers it. */
        void setFolder (const juce::File&);

        static juce::File getDefaultFolder();

    private:
        void run() override;
        void scan();
        void load (int index);

        struct Source
        {
            juce::File file;
            bool dedicated = false;   // the file holds only this tape
        };

        std::unique_ptr<juce::PropertiesFile> settings;
        mutable juce::CriticalSection folderLock;
        juce::File folder;

        // Written by the loader thread only.
        std::array<Source, numTapes> sources;
        std::vector<std::unique_ptr<Tape>> owned;

        std::array<std::atomic<const Tape*>, numTapes> tapes {};
        std::array<std::atomic<bool>, numTapes> wanted {};
        std::array<std::atomic<bool>, numTapes> found {};
        std::atomic<const Tape*> click { nullptr };
        std::atomic<bool> rescan { true }, scanned { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeLibrary)
    };
}
