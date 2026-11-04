#pragma once

#include <array>
#include <cstdint>

namespace parrot
{
    /** The sounds Parrot can make. Vowels (and the diphthongs, which glide between two vowel
        shapes) come first; anything up to Nsyl can carry a note. */
    enum class Ph : std::uint8_t
    {
        None,

        // Vowels
        IY, IH, EH, AE, AA, AO, OH, UH, UW, AH, ER, UU,
        // Diphthongs
        AY, EY, OW, AW, OY,
        // Syllabic n (Japanese ん): a hummed nasal that takes a note of its own
        Nsyl,

        // Consonants
        P, B, T, D, K, G,
        F, V, TH, DH, S, Z, SH, ZH, H,
        CH, JH,
        M, N, NG,
        L, R, W, Y,
        RT,   // the Japanese r, a quick tap
        Q,    // a held stop between two syllables (Japanese small tsu, "kk", "tt")

        Count
    };

    inline constexpr bool isVowel (Ph p) noexcept     { return p >= Ph::IY && p <= Ph::OY; }
    inline constexpr bool isDiphthong (Ph p) noexcept { return p >= Ph::AY && p <= Ph::OY; }
    inline constexpr bool isNucleus (Ph p) noexcept   { return p >= Ph::IY && p <= Ph::Nsyl; }
    inline constexpr bool isConsonant (Ph p) noexcept { return p >= Ph::P && p < Ph::Count; }

    inline constexpr bool isStop (Ph p) noexcept
    {
        return p == Ph::P || p == Ph::B || p == Ph::T || p == Ph::D || p == Ph::K || p == Ph::G;
    }

    /** Voiced consonants that can be held as a note when a syllable has no vowel ("mm", "zz"). */
    inline constexpr bool isSonorant (Ph p) noexcept
    {
        return p == Ph::M || p == Ph::N || p == Ph::NG || p == Ph::L || p == Ph::R || p == Ph::W
            || p == Ph::Y || p == Ph::Z || p == Ph::V || p == Ph::ZH || p == Ph::DH || p == Ph::Nsyl;
    }

    inline const char* phonemeName (Ph p) noexcept
    {
        static constexpr std::array<const char*, (size_t) Ph::Count> names {
            "-",
            "IY", "IH", "EH", "AE", "AA", "AO", "OH", "UH", "UW", "AH", "ER", "UU",
            "AY", "EY", "OW", "AW", "OY",
            "N*",
            "P", "B", "T", "D", "K", "G",
            "F", "V", "TH", "DH", "S", "Z", "SH", "ZH", "H",
            "CH", "JH",
            "M", "N", "NG",
            "L", "R", "W", "Y",
            "r", "Q"
        };
        return names[(size_t) p];
    }

    /** One sung syllable: the consonants before the note, what's held, and the consonants that
        close it when the note ends. Plain data, so the audio thread can copy it freely. */
    struct Syllable
    {
        static constexpr int maxConsonants = 6;

        std::array<Ph, maxConsonants> onset {};
        std::array<Ph, maxConsonants> coda {};
        std::uint8_t numOnset = 0, numCoda = 0;
        Ph nucleus = Ph::AA;
        bool slur = false;              // "+": no new syllable, the last vowel carries on to this note
        int textStart = 0, textEnd = 0; // where it came from in the lyric text (characters)
    };
}
