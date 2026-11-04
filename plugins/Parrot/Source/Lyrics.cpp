#include "Lyrics.h"

#include <map>
#include <set>
#include <string>

namespace parrot
{
namespace
{
    struct Sound
    {
        Ph ph;
        int at;   // character index in the lyric text
    };

    using Sounds = std::vector<Sound>;

    struct Letters
    {
        std::string s;          // lower-case a-z only
        std::vector<int> at;    // where each letter came from
    };

    Ph phonemeFromName (const std::string& name)
    {
        for (int i = 1; i < (int) Ph::Count; ++i)
            if (name == phonemeName ((Ph) i))
                return (Ph) i;
        return Ph::None;
    }

    //==============================================================================
    // Common words English spelling rules get wrong, mostly ones that turn up in songs.
    const std::map<std::string, const char*>& irregularWords()
    {
        static const std::map<std::string, const char*> words {
            { "the", "DH AH" },      { "a", "AH" },            { "i", "AY" },            { "im", "AY M" },
            { "ive", "AY V" },       { "ill", "AY L" },        { "id", "AY D" },         { "you", "Y UW" },
            { "your", "Y AO R" },    { "youre", "Y AO R" },    { "to", "T UW" },         { "do", "D UW" },
            { "who", "H UW" },       { "of", "AH V" },         { "is", "IH Z" },         { "was", "W AH Z" },
            { "his", "H IH Z" },     { "has", "H AE Z" },      { "as", "AE Z" },         { "love", "L AH V" },
            { "come", "K AH M" },    { "some", "S AH M" },     { "one", "W AH N" },      { "done", "D AH N" },
            { "none", "N AH N" },    { "gone", "G AO N" },     { "are", "AA R" },        { "were", "W ER" },
            { "there", "DH EH R" },  { "where", "W EH R" },    { "here", "H IY R" },     { "heart", "H AA R T" },
            { "said", "S EH D" },    { "says", "S EH Z" },     { "know", "N OW" },       { "now", "N AW" },
            { "how", "H AW" },       { "wow", "W AW" },        { "oh", "OW" },           { "ooh", "UW" },
            { "ah", "AA" },          { "eyes", "AY Z" },       { "eye", "AY" },          { "give", "G IH V" },
            { "live", "L IH V" },    { "have", "H AE V" },     { "been", "B IH N" },     { "does", "D AH Z" },
            { "friend", "F R EH N D" }, { "through", "TH R UW" }, { "world", "W ER L D" }, { "only", "OW N L IY" },
            { "baby", "B EY B IY" }, { "every", "EH V R IY" }, { "ever", "EH V ER" },    { "never", "N EH V ER" },
            { "very", "V EH R IY" }, { "little", "L IH T AH L" }, { "people", "P IY P AH L" },
            { "would", "W UH D" },   { "could", "K UH D" },    { "should", "SH UH D" },  { "put", "P UH T" },
            { "good", "G UH D" },    { "look", "L UH K" },     { "book", "B UH K" },     { "want", "W AA N T" },
            { "what", "W AH T" },    { "walk", "W AO K" },     { "talk", "T AO K" },     { "together", "T AH G EH DH ER" },
            { "forever", "F ER EH V ER" }, { "alone", "AH L OW N" }, { "again", "AH G EH N" }, { "away", "AH W EY" },
            { "above", "AH B AH V" }, { "money", "M AH N IY" }, { "hello", "H AH L OW" }, { "music", "M Y UW Z IH K" },
            { "parrot", "P AE R AH T" }, { "fire", "F AY ER" }, { "tonight", "T AH N AY T" }, { "dont", "D OW N T" },
            { "cant", "K AE N T" },  { "wont", "W OW N T" },   { "its", "IH T S" },      { "says", "S EH Z" },
            { "they", "DH EY" },     { "their", "DH EH R" },   { "theyre", "DH EH R" },  { "great", "G R EY T" },
            { "break", "B R EY K" }, { "door", "D AO R" },     { "floor", "F L AO R" },  { "four", "F AO R" },
            { "pretty", "P R IH T IY" }, { "woman", "W UH M AH N" }, { "women", "W IH M IH N" }, { "only", "OW N L IY" },
            { "mother", "M AH DH ER" }, { "brother", "B R AH DH ER" }, { "other", "AH DH ER" }, { "nothing", "N AH TH IH NG" },
            { "something", "S AH M TH IH NG" }, { "everything", "EH V R IY TH IH NG" }, { "water", "W AO T ER" },
            { "body", "B AA D IY" }, { "cover", "K AH V ER" }, { "lover", "L AH V ER" }, { "honey", "H AH N IY" },
            { "family", "F AE M AH L IY" }, { "animal", "AE N AH M AH L" }, { "magic", "M AE JH IH K" },
            { "heaven", "H EH V AH N" }, { "seven", "S EH V AH N" }, { "river", "R IH V ER" }, { "city", "S IH T IY" },
            { "wonder", "W AH N D ER" }, { "travel", "T R AE V AH L" }, { "promise", "P R AA M IH S" },
            { "oven", "AH V AH N" }, { "planet", "P L AE N AH T" }, { "any", "EH N IY" }, { "many", "M EH N IY" },
        };
        return words;
    }

    bool startsVoicedTh (const std::string& word)
    {
        static const std::set<std::string> words {
            "this", "that", "them", "then", "than", "these", "those", "though", "thus", "thee", "thy",
            "thou", "theirs", "with", "within", "without", "father", "weather", "whether", "either",
            "neither", "rather", "breathe", "smooth", "together"
        };
        return words.count (word) > 0;
    }

    bool isVowelLetter (char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

    //==============================================================================
    Sounds fromDictionary (const char* spec, int start, int end)
    {
        Sounds out;
        const auto names = juce::StringArray::fromTokens (spec, " ", "");
        const auto count = juce::jmax (1, names.size());
        int i = 0;
        for (const auto& name : names)
        {
            if (const auto ph = phonemeFromName (name.toStdString()); ph != Ph::None)
                out.push_back ({ ph, start + (end - start) * i / count });
            ++i;
        }
        return out;
    }

    Sounds englishToSounds (const Letters& word, bool wholeWord)
    {
        const auto& s = word.s;
        const int n = (int) s.size();
        Sounds out;
        if (n == 0)
            return out;

        if (wholeWord)
            if (const auto it = irregularWords().find (s); it != irregularWords().end())
                return fromDictionary (it->second, word.at.front(), word.at.back() + 1);

        auto at = [&] (int i) { return word.at[(size_t) juce::jlimit (0, n - 1, i)]; };
        auto letter = [&] (int i) { return i >= 0 && i < n ? s[(size_t) i] : '\0'; };
        auto from = [&] (int i, const char* text)
        {
            return s.compare ((size_t) i, std::strlen (text), text) == 0;
        };
        auto emit = [&] (Ph ph, int i) { out.push_back ({ ph, at (i) }); };

        auto hasVowelBefore = [&] (int i)
        {
            for (int j = 0; j < i; ++j)
                if (isVowelLetter (s[(size_t) j]) || (j > 0 && s[(size_t) j] == 'y'))
                    return true;
            return false;
        };

        // y is a vowel unless it starts the word or a vowel follows it.
        auto yIsVowel = [&] (int i) { return i > 0 && ! isVowelLetter (letter (i + 1)); };

        // A silent final e ("time", "dance") makes no sound of its own.
        const bool silentFinalE = n >= 2 && s.back() == 'e' && hasVowelBefore (n - 1)
                                  && ! isVowelLetter (s[(size_t) n - 2]);

        for (int i = 0; i < n;)
        {
            const char c = s[(size_t) i];
            const bool vowel = isVowelLetter (c) || (c == 'y' && yIsVowel (i));

            if (vowel)
            {
                struct Rule { const char* text; Ph ph; };
                static constexpr Rule teams[] = {
                    { "eigh", Ph::EY }, { "augh", Ph::AO }, { "ough", Ph::AO }, { "igh", Ph::AY },
                    { "ee", Ph::IY }, { "ea", Ph::IY }, { "ai", Ph::EY }, { "ay", Ph::EY }, { "ei", Ph::EY },
                    { "oa", Ph::OW }, { "oe", Ph::OW }, { "oo", Ph::UW }, { "ou", Ph::AW }, { "oi", Ph::OY },
                    { "oy", Ph::OY }, { "au", Ph::AO }, { "aw", Ph::AO }, { "ew", Ph::UW }, { "ue", Ph::UW },
                    { "ui", Ph::UW },
                };

                // Two-letter endings that depend on where they fall.
                if (from (i, "ey") && i + 2 == n) { emit (hasVowelBefore (i) ? Ph::IY : Ph::EY, i); i += 2; continue; }
                if (from (i, "ow"))               { emit (i + 2 == n ? Ph::OW : Ph::AW, i); i += 2; continue; }
                if (from (i, "ie"))               { emit (i + 2 == n && ! hasVowelBefore (i) ? Ph::AY : Ph::IY, i); i += 2; continue; }

                bool matched = false;
                for (const auto& rule : teams)
                {
                    if (from (i, rule.text))
                    {
                        emit (rule.ph, i);
                        i += (int) std::strlen (rule.text);
                        matched = true;
                        break;
                    }
                }
                if (matched)
                    continue;

                // The silent final e, and "-le" after a consonant ("little" -> "lit-tal").
                if (i == n - 1 && c == 'e' && silentFinalE)
                {
                    ++i;
                    continue;
                }

                const char next = letter (i + 1), afterNext = letter (i + 2);

                // r after a vowel colours it: "car", "for", "her", "bird", "turn".
                if (next == 'r' && ! isVowelLetter (afterNext) && afterNext != 'y')
                {
                    if (c == 'a')      { emit (Ph::AA, i); ++i; continue; }
                    if (c == 'o')      { emit (Ph::AO, i); ++i; continue; }
                    emit (Ph::ER, i);
                    i += 2;
                    continue;
                }

                // Vowel, r, silent e: "care", "here", "fire", "more", "pure".
                if (next == 'r' && afterNext == 'e' && i + 3 == n)
                {
                    switch (c)
                    {
                        case 'a': emit (Ph::EH, i); emit (Ph::R, i + 1); break;
                        case 'e': emit (Ph::IY, i); emit (Ph::R, i + 1); break;
                        case 'i': case 'y': emit (Ph::AY, i); emit (Ph::ER, i + 1); break;
                        case 'o': emit (Ph::AO, i); emit (Ph::R, i + 1); break;
                        default:  emit (Ph::Y, i); emit (Ph::UH, i); emit (Ph::R, i + 1); break;
                    }
                    i = n;
                    continue;
                }

                // Magic e: one consonant then a final e makes the vowel say its name ("time", "home").
                const bool magicE = silentFinalE && i + 2 == n - 1 && next != '\0' && ! isVowelLetter (next)
                                    && next != 'w' && next != 'x' && next != 'y';
                if (magicE)
                {
                    switch (c)
                    {
                        case 'a': emit (Ph::EY, i); break;
                        case 'e': emit (Ph::IY, i); break;
                        case 'i': case 'y': emit (Ph::AY, i); break;
                        case 'o': emit (Ph::OW, i); break;
                        default:  emit (Ph::UW, i); break;
                    }
                    ++i;
                    continue;
                }

                // A lone "a" is a schwa ("a-long"), and so is a leading "a" before one consonant and a vowel ("away", "ago").
                if (c == 'a' && i == 0 && (n == 1 || (! isVowelLetter (next) && next != 'r' && isVowelLetter (afterNext))))
                {
                    emit (Ph::AH, i);
                    ++i;
                    continue;
                }

                // A vowel that ends the word is long: "la", "me", "hi", "go", "flu", "my", "happy".
                if (i == n - 1)
                {
                    switch (c)
                    {
                        case 'a': emit (Ph::AA, i); break;
                        case 'e': emit (Ph::IY, i); break;
                        case 'i': emit (Ph::AY, i); break;
                        case 'o': emit (Ph::OW, i); break;
                        case 'u': emit (Ph::UW, i); break;
                        default:  emit (hasVowelBefore (i) ? Ph::IY : Ph::AY, i); break;
                    }
                    ++i;
                    continue;
                }

                // a, o or u before one consonant and another vowel is usually long: "lazy", "over", "music".
                if ((c == 'a' || c == 'o' || c == 'u') && wholeWord && ! isVowelLetter (next) && next != '\0'
                    && next != 'r' && next != 'w' && next != 'x' && next != 'y'
                    && (isVowelLetter (afterNext) || (afterNext == 'y' && i + 3 == n)))
                {
                    emit (c == 'a' ? Ph::EY : c == 'o' ? Ph::OW : Ph::UW, i);
                    ++i;
                    continue;
                }

                // A few patterns where a closed vowel is long anyway.
                if (c == 'a' && (from (i, "all") || from (i, "alk") || from (i, "alt"))) { emit (Ph::AO, i); ++i; continue; }
                if (c == 'o' && (from (i, "old") || from (i, "olt")))                    { emit (Ph::OW, i); ++i; continue; }
                if (c == 'i' && (from (i, "ind") || from (i, "ild")))                    { emit (Ph::AY, i); ++i; continue; }
                if (c == 'a' && letter (i - 1) == 'w')                                   { emit (Ph::AA, i); ++i; continue; }

                switch (c)
                {
                    case 'a': emit (Ph::AE, i); break;
                    case 'e': emit (Ph::EH, i); break;
                    case 'i': emit (Ph::IH, i); break;
                    case 'o': emit (Ph::AA, i); break;
                    case 'u': emit (Ph::AH, i); break;
                    default:  emit (Ph::IH, i); break;   // y inside a word: "gym", "rhythm"
                }
                ++i;
                continue;
            }

            // Consonants. A doubled letter sounds once ("hello").
            if (i > 0 && c == s[(size_t) i - 1] && c != 'e')
            {
                ++i;
                continue;
            }

            // Final "-le" after a consonant is a syllable of its own: "lit-tle", "ap-ple".
            if (c == 'l' && i == n - 2 && s.back() == 'e' && i > 0 && ! isVowelLetter (letter (i - 1)))
            {
                emit (Ph::AH, i);
                emit (Ph::L, i);
                i = n;
                continue;
            }

            struct Rule { const char* text; Ph first, second; bool atStartOnly, atEndOnly; };
            static constexpr Rule clusters[] = {
                { "tch", Ph::CH, Ph::None, false, false }, { "sch", Ph::S, Ph::K, false, false },
                { "ch", Ph::CH, Ph::None, false, false },  { "sh", Ph::SH, Ph::None, false, false },
                { "dh", Ph::DH, Ph::None, false, false },  { "zh", Ph::ZH, Ph::None, false, false },
                { "ph", Ph::F, Ph::None, false, false },   { "wh", Ph::W, Ph::None, false, false },
                { "nk", Ph::NG, Ph::K, false, false },     { "ng", Ph::NG, Ph::None, false, false },
                { "ck", Ph::K, Ph::None, false, false },   { "qu", Ph::K, Ph::W, false, false },
                { "kn", Ph::N, Ph::None, true, false },    { "wr", Ph::R, Ph::None, true, false },
                { "gn", Ph::N, Ph::None, false, true },    { "mb", Ph::M, Ph::None, false, true },
            };

            if (from (i, "th"))
            {
                emit (startsVoicedTh (s) || (i > 0 && i + 3 == n && from (i, "the")) ? Ph::DH : Ph::TH, i);
                i += 2;
                continue;
            }
            if (from (i, "gh"))
            {
                if (i == 0)
                    emit (Ph::G, i);
                i += 2;
                continue;
            }

            bool matched = false;
            for (const auto& rule : clusters)
            {
                const auto len = (int) std::strlen (rule.text);
                if (! from (i, rule.text) || (rule.atStartOnly && i != 0) || (rule.atEndOnly && i + len != n))
                    continue;
                emit (rule.first, i);
                if (rule.second != Ph::None)
                    emit (rule.second, i + 1);
                i += len;
                matched = true;
                break;
            }
            if (matched)
                continue;

            const char next = letter (i + 1);
            switch (c)
            {
                case 'b': emit (Ph::B, i); break;
                case 'c': emit (next == 'e' || next == 'i' || next == 'y' ? Ph::S : Ph::K, i); break;
                case 'd': emit (Ph::D, i); break;
                case 'f': emit (Ph::F, i); break;
                case 'g': emit (Ph::G, i); break;
                case 'h': if (isVowelLetter (next) || next == 'y') emit (Ph::H, i); break;
                case 'j': emit (Ph::JH, i); break;
                case 'k': emit (Ph::K, i); break;
                case 'l': emit (Ph::L, i); break;
                case 'm': emit (Ph::M, i); break;
                case 'n': emit (Ph::N, i); break;
                case 'p': emit (Ph::P, i); break;
                case 'q': emit (Ph::K, i); break;
                case 'r': emit (Ph::R, i); break;
                case 's':
                {
                    // Plural and verb endings after a voiced sound are buzzed: "days", "dreams", "goes".
                    const char prev = letter (i - 1);
                    const bool voiced = i == n - 1 && n > 2
                                        && (std::strchr ("bdglmnrvwy", prev) != nullptr || (prev == 'e' && n > 3));
                    emit (voiced ? Ph::Z : Ph::S, i);
                    break;
                }
                case 't': emit (Ph::T, i); break;
                case 'v': emit (Ph::V, i); break;
                case 'w': if (isVowelLetter (next) || next == 'h') emit (Ph::W, i); break;
                case 'x':
                    if (i == 0) emit (Ph::Z, i);
                    else { emit (Ph::K, i); emit (Ph::S, i); }
                    break;
                case 'y': emit (Ph::Y, i); break;
                case 'z': emit (Ph::Z, i); break;
                default: break;
            }
            ++i;
        }

        return out;
    }

    //==============================================================================
    Sounds romajiToSounds (const Letters& word)
    {
        const auto& s = word.s;
        const int n = (int) s.size();
        Sounds out;

        auto letter = [&] (int i) { return i >= 0 && i < n ? s[(size_t) i] : '\0'; };
        auto from = [&] (int i, const char* text) { return s.compare ((size_t) i, std::strlen (text), text) == 0; };
        auto emit = [&] (Ph ph, int i) { out.push_back ({ ph, word.at[(size_t) juce::jlimit (0, n - 1, i)] }); };

        for (int i = 0; i < n;)
        {
            const char c = s[(size_t) i];
            const char next = letter (i + 1);

            switch (c)
            {
                case 'a': emit (Ph::AA, i); ++i; continue;
                case 'i': emit (Ph::IY, i); ++i; continue;
                case 'u': emit (Ph::UU, i); ++i; continue;
                case 'e': emit (Ph::EH, i); ++i; continue;
                case 'o': emit (Ph::OH, i); ++i; continue;
                default: break;
            }

            // ん: an n with no vowel after it, and m before b/p/m ("sempai").
            if ((c == 'n' && ! isVowelLetter (next) && next != 'y')
                || (c == 'm' && (next == 'b' || next == 'p' || next == 'm')))
            {
                emit (Ph::Nsyl, i);
                ++i;
                continue;
            }

            // っ: a doubled consonant, or t before ch ("matcha").
            if ((next == c && c != 'n') || (c == 't' && next == 'c' && letter (i + 2) == 'h'))
            {
                emit (Ph::Q, i);
                ++i;
                continue;
            }

            if (from (i, "sh")) { emit (Ph::SH, i); i += 2; continue; }
            if (from (i, "ch")) { emit (Ph::CH, i); i += 2; continue; }
            if (from (i, "ts")) { emit (Ph::T, i); emit (Ph::S, i + 1); i += 2; continue; }

            switch (c)
            {
                case 'k': case 'c': case 'q': emit (Ph::K, i); break;
                case 'g': emit (Ph::G, i); break;
                case 's': emit (Ph::S, i); break;
                case 'z': emit (Ph::Z, i); break;
                case 't': emit (Ph::T, i); break;
                case 'd': emit (Ph::D, i); break;
                case 'n': emit (Ph::N, i); break;
                case 'h': emit (Ph::H, i); break;
                case 'b': emit (Ph::B, i); break;
                case 'p': emit (Ph::P, i); break;
                case 'm': emit (Ph::M, i); break;
                case 'y': emit (Ph::Y, i); break;
                case 'r': case 'l': emit (Ph::RT, i); break;
                case 'w': emit (Ph::W, i); break;
                case 'f': emit (Ph::F, i); break;
                case 'v': emit (Ph::V, i); break;
                case 'j': emit (Ph::JH, i); break;
                case 'x': emit (Ph::K, i); emit (Ph::S, i); break;
                default: break;
            }
            ++i;
        }
        return out;
    }

    //==============================================================================
    /** Whether two consonants can start a syllable together ("st", "pl", "tr", "my"). */
    bool canStartTogether (Ph first, Ph second)
    {
        if (second == Ph::Y)
            return first != Ph::Y;
        if (first == Ph::S)
            return second == Ph::P || second == Ph::T || second == Ph::K || second == Ph::M
                || second == Ph::N || second == Ph::L || second == Ph::W;
        const bool obstruent = isStop (first) || first == Ph::F || first == Ph::TH || first == Ph::SH;
        if (second == Ph::R || second == Ph::W)
            return obstruent;
        if (second == Ph::L)
            return obstruent && first != Ph::T && first != Ph::D && first != Ph::TH;
        return false;
    }

    void addConsonants (Syllable& syl, bool toOnset, const Sounds& sounds, int from, int to)
    {
        for (int i = from; i < to; ++i)
        {
            auto& count = toOnset ? syl.numOnset : syl.numCoda;
            auto& list = toOnset ? syl.onset : syl.coda;
            if (count < Syllable::maxConsonants)
                list[count++] = sounds[(size_t) i].ph;
        }
    }

    void setTextRange (Syllable& syl, const Sounds& sounds, int from, int to, int fallbackStart, int fallbackEnd)
    {
        syl.textStart = fallbackEnd;
        syl.textEnd = fallbackStart;
        for (int i = from; i < to; ++i)
        {
            syl.textStart = juce::jmin (syl.textStart, sounds[(size_t) i].at);
            syl.textEnd = juce::jmax (syl.textEnd, sounds[(size_t) i].at + 1);
        }
        if (syl.textEnd <= syl.textStart)
        {
            syl.textStart = fallbackStart;
            syl.textEnd = fallbackEnd;
        }
    }

    /** Splits a word's sounds into syllables: one per vowel, with the consonants between two
        vowels shared out so the next syllable starts with as many as English allows. */
    void syllabify (const Sounds& sounds, bool romaji, int chunkStart, int chunkEnd, std::vector<Syllable>& out)
    {
        const int n = (int) sounds.size();
        if (n == 0)
            return;

        std::vector<int> nuclei;
        for (int i = 0; i < n; ++i)
            if (isNucleus (sounds[(size_t) i].ph))
                nuclei.push_back (i);

        if (nuclei.empty())
        {
            // No vowel ("mm", "shh"): hold the last sound that can be held.
            int held = n - 1;
            for (int i = n - 1; i >= 0; --i)
                if (isSonorant (sounds[(size_t) i].ph)) { held = i; break; }
            nuclei.push_back (held);
        }

        // Where each syllable's onset begins.
        std::vector<int> onsetStart (nuclei.size(), 0);
        for (size_t k = 1; k < nuclei.size(); ++k)
        {
            const int prev = nuclei[k - 1], here = nuclei[k];
            int start = here;
            if (romaji)
            {
                // Every consonant leads into the next kana, except a held stop, which closes this one.
                start = prev + 1;
                while (start < here && sounds[(size_t) start].ph == Ph::Q)
                    ++start;
            }
            else if (here - prev - 1 >= 1)
            {
                start = here - 1;
                if (start - 1 > prev && canStartTogether (sounds[(size_t) start - 1].ph, sounds[(size_t) start].ph))
                {
                    --start;
                    if (start - 1 > prev && sounds[(size_t) start - 1].ph == Ph::S
                        && isStop (sounds[(size_t) start].ph))
                        --start;   // "str", "spl", "skr"
                }
            }
            onsetStart[k] = start;
        }

        for (size_t k = 0; k < nuclei.size(); ++k)
        {
            Syllable syl;
            const int nucleus = nuclei[k];
            const int from = onsetStart[k];
            const int to = k + 1 < nuclei.size() ? onsetStart[k + 1] : n;

            syl.nucleus = sounds[(size_t) nucleus].ph;
            addConsonants (syl, true, sounds, from, nucleus);
            addConsonants (syl, false, sounds, nucleus + 1, to);

            // The text runs from this syllable's first sound to the next one's, so silent and
            // multi-letter spellings ("sing", "time") stay with the syllable they belong to.
            syl.textStart = k == 0 ? chunkStart : juce::jlimit (chunkStart, chunkEnd, sounds[(size_t) from].at);
            syl.textEnd = k + 1 < nuclei.size() ? juce::jlimit (syl.textStart, chunkEnd, sounds[(size_t) onsetStart[k + 1]].at)
                                                : chunkEnd;
            if (syl.textEnd <= syl.textStart)
                setTextRange (syl, sounds, from, to, chunkStart, chunkEnd);
            out.push_back (syl);
        }
    }
}

//==============================================================================
std::vector<Syllable> parseLyrics (const juce::String& text, Spelling spelling)
{
    std::vector<Syllable> out;

    std::vector<juce::juce_wchar> chars;
    for (auto p = text.getCharPointer(); ! p.isEmpty();)
        chars.push_back (p.getAndAdvance());

    const int n = (int) chars.size();
    auto isWordChar = [] (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetter (c) || c == '\'' || c == 0x2019 || c == '-' || c == '+';
    };

    for (int i = 0; i < n;)
    {
        if (! isWordChar (chars[(size_t) i]))
        {
            ++i;
            continue;
        }

        const int wordStart = i;
        while (i < n && isWordChar (chars[(size_t) i]))
            ++i;
        const int wordEnd = i;

        // Count the hyphen-separated chunks, so a word with none can use the dictionary.
        bool hyphenated = false;
        for (int j = wordStart; j < wordEnd; ++j)
            hyphenated = hyphenated || chars[(size_t) j] == '-' || chars[(size_t) j] == '+';

        for (int j = wordStart; j < wordEnd;)
        {
            const auto c = chars[(size_t) j];
            if (c == '-')
            {
                ++j;
                continue;
            }
            if (c == '+')
            {
                Syllable slur;
                slur.slur = true;
                slur.textStart = j;
                slur.textEnd = j + 1;
                out.push_back (slur);
                ++j;
                continue;
            }

            const int chunkStart = j;
            Letters letters;
            while (j < wordEnd && chars[(size_t) j] != '-' && chars[(size_t) j] != '+')
            {
                const auto lower = juce::CharacterFunctions::toLowerCase (chars[(size_t) j]);
                if (lower >= 'a' && lower <= 'z')
                {
                    letters.s.push_back ((char) lower);
                    letters.at.push_back (j);
                }
                ++j;
            }

            if (letters.s.empty())
                continue;

            const auto sounds = spelling == Spelling::romaji ? romajiToSounds (letters)
                                                             : englishToSounds (letters, ! hyphenated);
            syllabify (sounds, spelling == Spelling::romaji, chunkStart, j, out);
        }
    }

    return out;
}

juce::String describe (const Syllable& syl)
{
    if (syl.slur)
        return "+";

    juce::StringArray parts;
    for (int i = 0; i < syl.numOnset; ++i)
        parts.add (juce::String (phonemeName (syl.onset[(size_t) i])).toLowerCase());
    parts.add (phonemeName (syl.nucleus));
    for (int i = 0; i < syl.numCoda; ++i)
        parts.add (juce::String (phonemeName (syl.coda[(size_t) i])).toLowerCase());
    return parts.joinIntoString (" ");
}
}
