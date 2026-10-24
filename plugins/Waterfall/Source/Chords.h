#pragma once

#include <array>

/** Chord voicings for the resonator bank, in semitones above the root (C3 + Key).
    Every chord fills all voices, spreading tones over about two octaves. */
namespace Chords
{
    inline constexpr int numVoices = 6;

    struct Chord
    {
        const char* name;
        std::array<int, numVoices> intervals;
    };

    inline constexpr std::array<Chord, 12> all {{
        { "Unison", { 0, 12, 24,  0, 12, 24 } },
        { "Fifth",  { 0,  7, 12, 19, 24, 31 } },
        { "Major",  { 0,  4,  7, 12, 16, 19 } },
        { "Minor",  { 0,  3,  7, 12, 15, 19 } },
        { "Sus2",   { 0,  2,  7, 12, 14, 19 } },
        { "Sus4",   { 0,  5,  7, 12, 17, 19 } },
        { "Maj7",   { 0,  4,  7, 11, 12, 16 } },
        { "Min7",   { 0,  3,  7, 10, 12, 15 } },
        { "Dom7",   { 0,  4,  7, 10, 12, 16 } },
        { "Add9",   { 0,  4,  7, 12, 14, 19 } },
        { "Min9",   { 0,  3,  7, 10, 14, 19 } },
        { "Dim",    { 0,  3,  6, 12, 15, 18 } },
    }};

    inline juce::StringArray names()
    {
        juce::StringArray result;
        for (const auto& c : all)
            result.add (c.name);
        return result;
    }

    inline const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    inline constexpr int rootMidiNote = 48;   // C3
}
