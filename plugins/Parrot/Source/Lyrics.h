#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "Phonemes.h"

namespace parrot
{
    enum class Spelling { english, romaji };

    /** Turns lyric text into one syllable per note.

        Words split into syllables on their own ("parrot" sings as two notes); a hyphen sets the
        split by hand ("par-rot"), and "+" on its own carries the last vowel over to another note.
        English is read with spelling rules and a short list of common irregular words, so spell
        unusual words the way they sound ("bay-bee"). Romaji gives one syllable per kana. */
    std::vector<Syllable> parseLyrics (const juce::String& text, Spelling spelling);

    /** "h EH l" style description, for the render tool and tests. */
    juce::String describe (const Syllable&);
}
