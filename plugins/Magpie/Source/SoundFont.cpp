#include "SoundFont.h"

namespace magpie
{
    const Preset* SoundFont::find (int bank, int program) const noexcept
    {
        const Preset* sameProgram = nullptr;
        for (const auto& p : presets)
        {
            if (p.program != program)
                continue;
            if (p.bank == bank)
                return &p;
            if (sameProgram == nullptr || (sameProgram->bank != 0 && p.bank == 0))
                sameProgram = &p;
        }

        if (sameProgram != nullptr)
            return sameProgram;
        return presets.empty() ? nullptr : &presets.front();
    }

    //==============================================================================
    namespace
    {
        juce::uint32 u32 (const juce::uint8* p) { return juce::ByteOrder::littleEndianInt (p); }
        juce::uint16 u16 (const juce::uint8* p) { return juce::ByteOrder::littleEndianShort (p); }

        juce::String fourCC (const juce::uint8* p) { return juce::String (reinterpret_cast<const char*> (p), 4); }

        juce::String fixedName (const juce::uint8* p, size_t maxLength)
        {
            size_t n = 0;
            while (n < maxLength && p[n] != 0)
                ++n;
            return juce::String (juce::CharPointer_UTF8 (reinterpret_cast<const char*> (p)), n).trim();
        }

        struct Chunk
        {
            const juce::uint8* data = nullptr;
            size_t size = 0;
        };

        /** Walks the sub-chunks of a RIFF LIST body, calling f (id, chunk) for each. */
        template <typename F>
        void forEachChunk (const juce::uint8* p, size_t size, F&& f)
        {
            size_t pos = 0;
            while (pos + 8 <= size)
            {
                const auto length = (size_t) u32 (p + pos + 4);
                if (pos + 8 + length > size)
                    break;
                f (fourCC (p + pos), Chunk { p + pos + 8, length });
                pos += 8 + length + (length & 1);
            }
        }

        /** The generators set in one zone. Ranges keep their raw lo/hi byte pair. */
        struct ZoneGens
        {
            std::array<std::optional<int>, numGens> values;

            bool has (Gen g) const { return values[(size_t) g].has_value(); }
            int get (Gen g, int fallback) const { return values[(size_t) g].value_or (fallback); }

            void inherit (const ZoneGens& global)
            {
                for (size_t i = 0; i < values.size(); ++i)
                    if (! values[i])
                        values[i] = global.values[i];
            }

            std::pair<int, int> range (Gen g) const
            {
                const auto v = get (g, 127 << 8);
                return { v & 0xff, (v >> 8) & 0xff };
            }
        };

        /** Zones for bags [first, last) of a ?bag/?gen pair. */
        std::vector<ZoneGens> readZones (const Chunk& bags, const Chunk& gens, int first, int last)
        {
            std::vector<ZoneGens> zones;
            const auto numBags = (int) (bags.size / 4);
            const auto numRecords = (int) (gens.size / 4);

            for (int b = first; b < last && b + 1 < numBags; ++b)
            {
                ZoneGens z;
                const auto from = (int) u16 (bags.data + b * 4);
                const auto to   = juce::jmin ((int) u16 (bags.data + (b + 1) * 4), numRecords);
                for (int g = from; g < to; ++g)
                {
                    const auto op = (int) u16 (gens.data + g * 4);
                    if (op >= numGens)
                        continue;
                    const auto raw = u16 (gens.data + g * 4 + 2);
                    z.values[(size_t) op] = (op == keyRange || op == velRange) ? (int) raw : (int) (juce::int16) raw;
                }
                zones.push_back (z);
            }
            return zones;
        }

        /** Splits off a global zone (the first one, if it lacks the terminal generator) and folds it into the rest. */
        void applyGlobalZone (std::vector<ZoneGens>& zones, Gen terminal)
        {
            if (zones.empty() || zones.front().has (terminal))
                return;

            const auto global = zones.front();
            zones.erase (zones.begin());
            for (auto& z : zones)
                z.inherit (global);
        }

        std::array<int, numGens> defaultGens()
        {
            std::array<int, numGens> g {};
            for (auto t : { delayModLFO, delayVibLFO, delayModEnv, attackModEnv, holdModEnv, decayModEnv, releaseModEnv,
                            delayVolEnv, attackVolEnv, holdVolEnv, decayVolEnv, releaseVolEnv })
                g[(size_t) t] = -12000;
            g[initialFilterFc]   = 13500;
            g[scaleTuning]       = 100;
            g[keynum]            = -1;
            g[velocity]          = -1;
            g[overridingRootKey] = -1;
            return g;
        }

        /** Generators that only make sense on an instrument zone; a preset zone may not offset them. */
        bool isInstrumentOnly (int op)
        {
            switch (op)
            {
                case startAddrsOffset: case endAddrsOffset: case startloopAddrsOffset: case endloopAddrsOffset:
                case startAddrsCoarseOffset: case endAddrsCoarseOffset: case startloopAddrsCoarseOffset:
                case endloopAddrsCoarseOffset: case keynum: case velocity: case sampleModes: case exclusiveClass:
                case overridingRootKey: case sampleID: case instrument: case keyRange: case velRange:
                    return true;
                default:
                    return false;
            }
        }
    }

    std::unique_ptr<SoundFont> readSoundFont (const juce::File& file, juce::String& error)
    {
        juce::FileInputStream in (file);
        if (! in.openedOk())
        {
            error = "Can't open " + file.getFileName();
            return nullptr;
        }

        juce::uint8 header[12];
        if (in.read (header, 12) != 12 || fourCC (header) != "RIFF" || fourCC (header + 8) != "sfbk")
        {
            error = file.getFileName() + " isn't a SoundFont 2 file";
            return nullptr;
        }

        auto font = std::make_unique<SoundFont>();
        font->file = file;

        // Walk the top level by seeking, so the (possibly huge) sample data is read only once, straight into place.
        juce::int64 smplPosition = -1, smplSize = 0;
        juce::MemoryBlock pdta;
        const auto riffEnd = juce::jmin (in.getTotalLength(), (juce::int64) u32 (header + 4) + 8);

        while (in.getPosition() + 12 <= riffEnd)
        {
            juce::uint8 chunk[12];
            const auto chunkStart = in.getPosition();
            if (in.read (chunk, 12) != 12)
                break;

            const auto size = (juce::int64) u32 (chunk + 4);
            const auto next = chunkStart + 8 + size + (size & 1);
            if (fourCC (chunk) == "LIST")
            {
                const auto type = fourCC (chunk + 8);
                const auto bodySize = (size_t) juce::jmax ((juce::int64) 0, juce::jmin (size, riffEnd - chunkStart - 8) - 4);

                if (type == "INFO" || type == "pdta")
                {
                    juce::MemoryBlock body;
                    in.readIntoMemoryBlock (body, (juce::pointer_sized_int) bodySize);
                    if (type == "pdta")
                        pdta = std::move (body);
                    else
                        forEachChunk (static_cast<const juce::uint8*> (body.getData()), body.getSize(), [&] (const juce::String& id, Chunk c)
                        {
                            if (id == "INAM")
                                font->name = fixedName (c.data, c.size);
                        });
                }
                else if (type == "sdta")
                {
                    auto pos = chunkStart + 12;
                    const auto end = chunkStart + 12 + (juce::int64) bodySize;
                    while (pos + 8 <= end && in.setPosition (pos))
                    {
                        juce::uint8 sub[8];
                        if (in.read (sub, 8) != 8)
                            break;
                        const auto subSize = (juce::int64) u32 (sub + 4);
                        if (fourCC (sub) == "smpl")
                        {
                            smplPosition = pos + 8;
                            smplSize = juce::jmin (subSize, end - smplPosition);
                        }
                        pos += 8 + subSize + (subSize & 1);
                    }
                }
            }

            if (! in.setPosition (next))
                break;
        }

        std::map<juce::String, Chunk> chunks;
        forEachChunk (static_cast<const juce::uint8*> (pdta.getData()), pdta.getSize(), [&] (const juce::String& id, Chunk c) { chunks[id] = c; });

        for (const auto* required : { "phdr", "pbag", "pgen", "inst", "ibag", "igen", "shdr" })
        {
            if (chunks.count (required) == 0)
            {
                error = file.getFileName() + " has no preset data";
                return nullptr;
            }
        }
        if (smplPosition < 0)
        {
            error = file.getFileName() + " has no sample data";
            return nullptr;
        }

        // 16-bit sample data. The 24-bit extension (sm24) only adds the low byte, so it's left out.
        font->pcm.resize ((size_t) (smplSize / 2));
        in.setPosition (smplPosition);
        const auto bytes = (int) juce::jmin ((juce::int64) std::numeric_limits<int>::max(), (juce::int64) font->pcm.size() * 2);
        if (in.read (font->pcm.data(), bytes) != bytes)
        {
            error = file.getFileName() + " is cut short";
            return nullptr;
        }
        if constexpr (juce::ByteOrder::isBigEndian())
            for (auto& s : font->pcm)
                s = (juce::int16) juce::ByteOrder::swap ((juce::uint16) s);

        const auto& phdr = chunks["phdr"];
        const auto& inst = chunks["inst"];
        const auto& shdr = chunks["shdr"];
        const auto numPresets = (int) (phdr.size / 38) - 1;   // each list ends with a terminal record
        const auto numInsts   = (int) (inst.size / 22) - 1;
        const auto numSamples = (int) (shdr.size / 46) - 1;
        const auto pcmLength  = (juce::int64) font->pcm.size();

        for (int s = 0; s < numSamples; ++s)
        {
            const auto* sh = shdr.data + s * 46;
            Sample sample;
            sample.start      = juce::jlimit ((juce::int64) 0, pcmLength, (juce::int64) u32 (sh + 20));
            sample.end        = juce::jlimit (sample.start, pcmLength, (juce::int64) u32 (sh + 24));
            sample.loopStart  = u32 (sh + 28);
            sample.loopEnd    = u32 (sh + 32);
            sample.sampleRate = juce::jmax (1u, u32 (sh + 36));
            sample.originalPitch = sh[40] > 127 ? 60 : (int) sh[40];
            sample.correction = (int) (juce::int8) sh[41];
            if ((u16 (sh + 44) & 0x8000) != 0)   // ROM samples aren't in the file
                sample.end = sample.start;
            font->samples.push_back (sample);
        }

        // Instruments, each as its list of zones with the global zone folded in.
        std::vector<std::vector<ZoneGens>> instruments;
        for (int i = 0; i < numInsts; ++i)
        {
            auto zones = readZones (chunks["ibag"], chunks["igen"], u16 (inst.data + i * 22 + 20), u16 (inst.data + (i + 1) * 22 + 20));
            applyGlobalZone (zones, sampleID);
            instruments.push_back (std::move (zones));
        }

        const auto defaults = defaultGens();

        for (int p = 0; p < numPresets; ++p)
        {
            const auto* rec = phdr.data + p * 38;
            Preset preset;
            preset.name    = fixedName (rec, 20);
            preset.program = u16 (rec + 20);
            preset.bank    = u16 (rec + 22);

            auto presetZones = readZones (chunks["pbag"], chunks["pgen"], u16 (rec + 24), u16 (rec + 38 + 24));
            applyGlobalZone (presetZones, instrument);

            for (const auto& pz : presetZones)
            {
                const auto i = pz.get (instrument, -1);
                if (! juce::isPositiveAndBelow (i, (int) instruments.size()))
                    continue;

                const auto [pKeyLo, pKeyHi] = pz.range (keyRange);
                const auto [pVelLo, pVelHi] = pz.range (velRange);

                for (const auto& iz : instruments[(size_t) i])
                {
                    const auto s = iz.get (sampleID, -1);
                    if (! juce::isPositiveAndBelow (s, numSamples))
                        continue;
                    const auto& sample = font->samples[(size_t) s];
                    if (sample.end - sample.start < 4)
                        continue;

                    const auto [iKeyLo, iKeyHi] = iz.range (keyRange);
                    const auto [iVelLo, iVelHi] = iz.range (velRange);

                    Region r;
                    r.keyLo = juce::jmax (pKeyLo, iKeyLo);
                    r.keyHi = juce::jmin (pKeyHi, iKeyHi);
                    r.velLo = juce::jmax (pVelLo, iVelLo);
                    r.velHi = juce::jmin (pVelHi, iVelHi);
                    if (r.keyLo > r.keyHi || r.velLo > r.velHi)
                        continue;

                    r.sample = s;
                    r.gens = defaults;
                    for (int op = 0; op < numGens; ++op)
                        if (iz.values[(size_t) op])
                            r.gens[(size_t) op] = *iz.values[(size_t) op];

                    // Preset-level generators are offsets added on top of the instrument's values.
                    for (int op = 0; op < numGens; ++op)
                        if (pz.values[(size_t) op] && ! isInstrumentOnly (op))
                            r.gens[(size_t) op] += *pz.values[(size_t) op];

                    preset.keyLo = juce::jmin (preset.keyLo, r.keyLo);
                    preset.keyHi = juce::jmax (preset.keyHi, r.keyHi);
                    preset.regions.push_back (r);
                }
            }

            if (! preset.regions.empty())
                font->presets.push_back (std::move (preset));
        }

        if (font->presets.empty())
        {
            error = file.getFileName() + " has no playable presets";
            return nullptr;
        }

        std::stable_sort (font->presets.begin(), font->presets.end(), [] (const Preset& a, const Preset& b)
        {
            return a.bank != b.bank ? a.bank < b.bank : a.program < b.program;
        });

        if (font->name.isEmpty())
            font->name = file.getFileNameWithoutExtension();
        return font;
    }
}
