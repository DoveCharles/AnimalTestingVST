#pragma once

#include <at_shared/at_shared.h>
#include "FormantVoice.h"
#include "Lyrics.h"

namespace ParamIDs
{
    inline constexpr auto volume   = "volume";
    inline constexpr auto size     = "size";
    inline constexpr auto tone     = "tone";
    inline constexpr auto breath   = "breath";
    inline constexpr auto diction  = "diction";
    inline constexpr auto vibrato  = "vibrato";
    inline constexpr auto vibRate  = "vibRate";
    inline constexpr auto glide    = "glide";
    inline constexpr auto release  = "release";
    inline constexpr auto lofi     = "lofi";
    inline constexpr auto spelling = "spelling";
    inline constexpr auto timing   = "timing";
}

/** A monophonic singing synth: type lyrics, and each note sings the next syllable. */
class PluginProcessor final : public at::ProcessorBase,
                              private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 0.6; }
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;

    static inline const juce::Identifier lyricsProperty { "lyrics" };
    static inline const juce::String defaultLyrics { "hel-lo, I am a par-rot, sing a-long with me" };

    /** Lookahead in "On the beat" timing, so consonants can start before the note. */
    static constexpr double lookaheadSeconds = 0.15;

    //==============================================================================
    // Message thread
    juce::String getLyrics() const;
    void setLyrics (const juce::String&);
    const std::vector<parrot::Syllable>& getSyllables() const noexcept { return syllablesForEditor; }

    /** The syllable sounding now (-1 for none) and the one the next note will sing. */
    int getSungSyllable() const noexcept { return sungSyllable.load(); }
    int getNextSyllable() const noexcept { return nextSyllable.load(); }

    void jumpToSyllable (int index) { requestedJump = index; }
    void stepSyllable (int delta)   { requestedStep += delta; }

    /** Tells the editor the lyrics changed underneath it (a preset or saved set loaded). */
    juce::ChangeBroadcaster lyricsChanged;

    juce::MidiKeyboardState keyboardState;

private:
    void timerCallback() override;
    void refreshLyrics();
    void updateLatency();

    struct Event
    {
        juce::int64 time = 0;       // when it arrived, in samples since prepare
        int note = 0;
        float velocity = 0.0f;
        bool on = false;
        bool absorbed = false;      // a note-off that a legato note-on replaces
        int returnTo = -1;          // note-off: the note still held to go back to
        int syllableIndex = -1;
        parrot::Syllable syllable;
    };

    void receive (const juce::MidiMessage&, juce::int64 time);
    juce::int64 dueTime (const Event&) const;
    void apply (const Event&);
    void advanceCursor (int delta);

    std::atomic<float>& volumeParam;
    std::atomic<float>& sizeParam;
    std::atomic<float>& toneParam;
    std::atomic<float>& breathParam;
    std::atomic<float>& dictionParam;
    std::atomic<float>& vibratoParam;
    std::atomic<float>& vibRateParam;
    std::atomic<float>& glideParam;
    std::atomic<float>& releaseParam;
    std::atomic<float>& lofiParam;
    std::atomic<float>& spellingParam;
    std::atomic<float>& timingParam;

    // Lyrics: parsed on the message thread, handed over with a swap the audio thread only try-locks for.
    juce::String parsedText;
    int parsedSpelling = -1;
    std::vector<parrot::Syllable> syllablesForEditor, incoming, live;
    juce::SpinLock incomingLock;
    bool incomingReady = false;

    std::atomic<int> sungSyllable { -1 }, nextSyllable { 0 };
    std::atomic<int> requestedJump { -1 }, requestedStep { 0 };
    int cursor = 0;

    parrot::FormantVoice voice;
    float pitchBend = 0.0f, modWheel = 0.0f;

    std::vector<Event> events;
    std::vector<int> held;   // notes down, oldest first
    juce::int64 clock = 0;
    juce::int64 lastNoteOnTime = -1;   // arrival time of the note-on last applied
    int latency = 0;
    bool onTheBeat = true;
    bool wasPlaying = false;
    double sampleRate = 44100.0;

    juce::HeapBlock<float> voiceBuffer;
    int voiceBufferSize = 0;
    juce::SmoothedValue<float> volume;
    float lofiPhase = 0.0f, lofiHeld = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
