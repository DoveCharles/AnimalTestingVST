#include "TapeLibrary.h"

namespace chilltron
{
    juce::StringArray tapeDisplayNames()
    {
        juce::StringArray names;
        for (const auto& t : catalogue)
            names.add (t.display);
        return names;
    }

    //==============================================================================
    namespace
    {
        // SoundFont 2.04 generator operators used here.
        enum Generator : int
        {
            instrumentGen = 41,
            keyRange      = 43,
            coarseTune    = 51,
            fineTune      = 52,
            sampleID      = 53,
            rootKey       = 58,
        };

        struct Chunk
        {
            const juce::uint8* data = nullptr;
            size_t size = 0;
        };

        juce::uint32 u32 (const juce::uint8* p) { return juce::ByteOrder::littleEndianInt (p); }
        juce::uint16 u16 (const juce::uint8* p) { return juce::ByteOrder::littleEndianShort (p); }

        /** Walks the sub-chunks of a RIFF LIST body, calling f (id, chunk) for each. */
        template <typename F>
        void forEachChunk (const juce::uint8* p, size_t size, F&& f)
        {
            size_t pos = 0;
            while (pos + 8 <= size)
            {
                const juce::String id (reinterpret_cast<const char*> (p + pos), 4);
                const auto length = (size_t) u32 (p + pos + 4);
                if (pos + 8 + length > size)
                    break;
                f (id, Chunk { p + pos + 8, length });
                pos += 8 + length + (length & 1);
            }
        }

        juce::String name20 (const juce::uint8* p)
        {
            size_t n = 0;
            while (n < 20 && p[n] != 0)
                ++n;
            return juce::String (reinterpret_cast<const char*> (p), n).trim();
        }

        /** The generators of one zone, with "not set" as std::nullopt. */
        struct ZoneGens
        {
            std::array<std::optional<juce::int16>, 64> values;

            bool has (int op) const { return values[(size_t) op].has_value(); }
            int get (int op, int fallback) const { return values[(size_t) op].value_or ((juce::int16) fallback); }
            void inherit (const ZoneGens& global)
            {
                for (size_t i = 0; i < values.size(); ++i)
                    if (! values[i])
                        values[i] = global.values[i];
            }
        };

        /** Generators for bags [first, last) of a ?bag/?gen pair. */
        std::vector<ZoneGens> readZones (const Chunk& bags, const Chunk& gens, int first, int last)
        {
            std::vector<ZoneGens> zones;
            const auto numBags = (int) (bags.size / 4);
            const auto numGens = (int) (gens.size / 4);

            for (int b = first; b < last && b + 1 < numBags; ++b)
            {
                ZoneGens z;
                const auto from = (int) u16 (bags.data + b * 4);
                const auto to   = juce::jmin ((int) u16 (bags.data + (b + 1) * 4), numGens);
                for (int g = from; g < to; ++g)
                {
                    const auto op = (int) u16 (gens.data + g * 4);
                    if (op < 64)
                        z.values[(size_t) op] = (juce::int16) u16 (gens.data + g * 4 + 2);
                }
                zones.push_back (z);
            }
            return zones;
        }
    }

    std::vector<std::unique_ptr<Tape>> readSoundFont (const juce::File& file)
    {
        std::vector<std::unique_ptr<Tape>> result;

        juce::MemoryBlock block;
        if (! file.loadFileAsData (block) || block.getSize() < 12)
            return result;

        const auto* data = static_cast<const juce::uint8*> (block.getData());
        if (juce::String (reinterpret_cast<const char*> (data), 4) != "RIFF"
            || juce::String (reinterpret_cast<const char*> (data + 8), 4) != "sfbk")
            return result;

        Chunk smpl;
        std::map<juce::String, Chunk> pdta;

        forEachChunk (data + 12, juce::jmin ((size_t) u32 (data + 4), block.getSize() - 8) - 4, [&] (const juce::String& id, Chunk list)
        {
            if (id != "LIST" || list.size < 4)
                return;
            const juce::String type (reinterpret_cast<const char*> (list.data), 4);
            forEachChunk (list.data + 4, list.size - 4, [&] (const juce::String& sub, Chunk c)
            {
                if (type == "sdta" && sub == "smpl") smpl = c;
                if (type == "pdta")                  pdta[sub] = c;
            });
        });

        for (const auto* required : { "phdr", "pbag", "pgen", "inst", "ibag", "igen", "shdr" })
            if (pdta.count (required) == 0)
                return result;
        if (smpl.data == nullptr)
            return result;

        const auto& phdr = pdta["phdr"];
        const auto& inst = pdta["inst"];
        const auto& shdr = pdta["shdr"];
        const auto numPresets = (int) (phdr.size / 38) - 1;   // the last record is the terminal "EOP"
        const auto numInsts   = (int) (inst.size / 22) - 1;
        const auto numSamples = (int) (shdr.size / 46) - 1;
        const auto* pcm = reinterpret_cast<const juce::int16*> (smpl.data);
        const auto pcmLength = (juce::int64) (smpl.size / 2);

        for (int p = 0; p < numPresets; ++p)
        {
            const auto* rec = phdr.data + p * 38;
            auto tape = std::make_unique<Tape>();
            tape->name = name20 (rec);

            // Which instruments the preset uses. Preset-level ranges and tuning aren't used by these tapes.
            std::vector<int> instruments;
            for (const auto& z : readZones (pdta["pbag"], pdta["pgen"], u16 (rec + 24), u16 (rec + 38 + 24)))
                if (z.has (instrumentGen))
                    instruments.push_back (z.get (instrumentGen, 0));

            struct Mapped { int lo, hi; double root; int zone; };
            std::vector<Mapped> mapped;

            for (const auto i : instruments)
            {
                if (i < 0 || i >= numInsts)
                    continue;

                auto zones = readZones (pdta["ibag"], pdta["igen"], u16 (inst.data + i * 22 + 20), u16 (inst.data + (i + 1) * 22 + 20));
                if (! zones.empty() && ! zones.front().has (sampleID))
                {
                    const auto global = zones.front();
                    zones.erase (zones.begin());
                    for (auto& z : zones)
                        z.inherit (global);
                }

                for (const auto& z : zones)
                {
                    const auto s = z.get (sampleID, -1);
                    if (s < 0 || s >= numSamples)
                        continue;

                    const auto* sh = shdr.data + s * 46;
                    const auto start = (juce::int64) u32 (sh + 20);
                    const auto end   = juce::jmin ((juce::int64) u32 (sh + 24), pcmLength);
                    const auto rate  = (double) u32 (sh + 36);
                    if (end - start < 4 || rate <= 0.0)
                        continue;

                    const auto range = (juce::uint16) z.get (keyRange, 127 << 8);
                    const auto originalPitch = (int) sh[40];
                    const auto correction = (int) (juce::int8) sh[41];
                    const auto root = z.get (rootKey, originalPitch == 255 ? 60 : originalPitch);

                    // Tuning raises the pitch, which is the same as lowering the effective root key.
                    const auto effectiveRoot = root - z.get (coarseTune, 0) - (z.get (fineTune, 0) + correction) / 100.0;

                    Tape::Zone zone;
                    zone.offset = (int) tape->pcm.size();
                    zone.length = (int) (end - start);
                    zone.sampleRate = rate;
                    tape->pcm.insert (tape->pcm.end(), pcm + start, pcm + end);

                    mapped.push_back ({ range & 0xff, range >> 8, effectiveRoot, (int) tape->zones.size() });
                    tape->zones.push_back (zone);
                }
            }

            if (mapped.empty())
                continue;

            // Keys outside every zone borrow the nearest one, up to an octave away, like stretching
            // a tape set past the M400's 35 notes. Beyond that they stay silent.
            for (int key = 0; key < 128; ++key)
            {
                const Mapped* best = nullptr;
                int bestDistance = 13;
                for (const auto& m : mapped)
                {
                    const auto distance = key < m.lo ? m.lo - key : (key > m.hi ? key - m.hi : 0);
                    if (distance < bestDistance)
                    {
                        best = &m;
                        bestDistance = distance;
                    }
                }

                if (best != nullptr)
                    tape->keys[(size_t) key] = { best->zone, std::exp2 ((key - best->root) / 12.0) };
            }

            result.push_back (std::move (tape));
        }

        return result;
    }

    //==============================================================================
    namespace
    {
        juce::PropertiesFile::Options settingsOptions()
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "Chilltron";
            o.folderName = "Animal Testing/Chilltron";
            o.osxLibrarySubFolder = "Application Support";
            o.filenameSuffix = ".settings";
            return o;
        }

        constexpr auto folderKey = "tapeFolder";
        constexpr auto clickPreset = "TapeKlik";
    }

    TapeLibrary::TapeLibrary() : juce::Thread ("Chilltron tape loader")
    {
        settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
        const auto saved = settings->getValue (folderKey);
        folder = saved.isNotEmpty() ? juce::File (saved) : getDefaultFolder();
        startThread (juce::Thread::Priority::low);
    }

    TapeLibrary::~TapeLibrary()
    {
        stopThread (4000);
    }

    juce::File TapeLibrary::getDefaultFolder()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/Animal Testing/Chilltron/Tapes");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Animal Testing/Chilltron/Tapes");
       #endif
    }

    const Tape* TapeLibrary::get (int index) const noexcept
    {
        return juce::isPositiveAndBelow (index, numTapes) ? tapes[(size_t) index].load (std::memory_order_acquire) : nullptr;
    }

    void TapeLibrary::request (int index) noexcept
    {
        if (juce::isPositiveAndBelow (index, numTapes))
            wanted[(size_t) index].store (true, std::memory_order_relaxed);
    }

    TapeLibrary::Status TapeLibrary::getStatus (int index) const noexcept
    {
        if (get (index) != nullptr)
            return Status::ready;
        if (scanned.load() && ! found[(size_t) index].load())
            return Status::missing;
        return Status::loading;
    }

    juce::File TapeLibrary::getFolder() const
    {
        const juce::ScopedLock sl (folderLock);
        return folder;
    }

    void TapeLibrary::setFolder (const juce::File& newFolder)
    {
        {
            const juce::ScopedLock sl (folderLock);
            folder = newFolder;
        }
        settings->setValue (folderKey, newFolder.getFullPathName());
        settings->saveIfNeeded();
        rescan = true;
        notify();
    }

    void TapeLibrary::run()
    {
        while (! threadShouldExit())
        {
            if (rescan.exchange (false))
                scan();

            for (int i = 0; i < numTapes && ! threadShouldExit(); ++i)
                if (wanted[(size_t) i].load() && get (i) == nullptr && found[(size_t) i].load())
                    load (i);

            wait (50);
        }
    }

    void TapeLibrary::scan()
    {
        const auto dir = getFolder();
        auto files = dir.findChildFiles (juce::File::findFiles, true, "*.sf2;*.SF2");
        files.sort();

        // Reading every preset name means parsing whole files, so match on the file name first:
        // each tape normally lives in "<preset>.sf2". Shared sets like Default_Set.sf2 are the fallback.
        std::array<Source, numTapes> next;
        juce::File clickFile;

        for (const auto& f : files)
        {
            const auto stem = f.getFileNameWithoutExtension();
            if (stem.equalsIgnoreCase (clickPreset))
                clickFile = f;
            for (int i = 0; i < numTapes; ++i)
                if (stem.equalsIgnoreCase (catalogue[(size_t) i].preset))
                    next[(size_t) i] = { f, true };
        }

        for (const auto& f : files)
        {
            if (threadShouldExit())
                return;

            const auto missingAny = std::any_of (next.begin(), next.end(), [] (const Source& s) { return s.file == juce::File(); });
            if (! missingAny)
                break;

            const auto stem = f.getFileNameWithoutExtension();
            const auto isDedicated = std::any_of (next.begin(), next.end(), [&] (const Source& s) { return s.file == f; })
                                     || stem.equalsIgnoreCase (clickPreset);
            if (isDedicated)
                continue;

            for (const auto& t : readSoundFont (f))
                for (int i = 0; i < numTapes; ++i)
                    if (next[(size_t) i].file == juce::File() && t->name.equalsIgnoreCase (catalogue[(size_t) i].preset))
                        next[(size_t) i] = { f, false };
        }

        sources = next;
        for (int i = 0; i < numTapes; ++i)
            found[(size_t) i] = get (i) != nullptr || sources[(size_t) i].file != juce::File();

        if (click.load() == nullptr && clickFile != juce::File())
        {
            auto loaded = readSoundFont (clickFile);
            if (! loaded.empty())
            {
                click.store (loaded.front().get(), std::memory_order_release);
                owned.push_back (std::move (loaded.front()));
            }
        }

        scanned = true;
    }

    void TapeLibrary::load (int index)
    {
        const auto& source = sources[(size_t) index];
        for (auto& t : readSoundFont (source.file))
        {
            if (t->name.equalsIgnoreCase (catalogue[(size_t) index].preset) || source.dedicated)
            {
                tapes[(size_t) index].store (t.get(), std::memory_order_release);
                owned.push_back (std::move (t));
                return;
            }
        }

        // The file no longer has it: report it missing rather than retrying forever.
        found[(size_t) index] = false;
    }
}
