#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int maxEvents = 256;

    /** A note-on this soon after a note-off is sung legato: the old syllable closes straight into the new one. */
    constexpr double legatoWindowSeconds = 0.03;
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      volumeParam   (at::params::raw (apvts, ParamIDs::volume)),
      sizeParam     (at::params::raw (apvts, ParamIDs::size)),
      toneParam     (at::params::raw (apvts, ParamIDs::tone)),
      breathParam   (at::params::raw (apvts, ParamIDs::breath)),
      dictionParam  (at::params::raw (apvts, ParamIDs::diction)),
      vibratoParam  (at::params::raw (apvts, ParamIDs::vibrato)),
      vibRateParam  (at::params::raw (apvts, ParamIDs::vibRate)),
      glideParam    (at::params::raw (apvts, ParamIDs::glide)),
      releaseParam  (at::params::raw (apvts, ParamIDs::release)),
      lofiParam     (at::params::raw (apvts, ParamIDs::lofi)),
      spellingParam (at::params::raw (apvts, ParamIDs::spelling)),
      timingParam   (at::params::raw (apvts, ParamIDs::timing))
{
    apvts.state.setProperty (lyricsProperty, defaultLyrics, nullptr);
    events.reserve (maxEvents);
    held.reserve (128);
    refreshLyrics();
    startTimerHz (10);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeFloat (ParamIDs::size, "Size", { 85.0f, 140.0f, 0.1f }, 115.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::tone, "Tone", 55.0f));
    layout.add (at::params::makePercent (ParamIDs::breath, "Breath", 15.0f));
    layout.add (at::params::makeFloat (ParamIDs::diction, "Diction", { 0.0f, 200.0f, 0.1f }, 100.0f, "%", 0));
    layout.add (at::params::makeFloat (ParamIDs::vibrato, "Vibrato", { 0.0f, 100.0f, 0.1f }, 25.0f, "cents", 0));
    layout.add (at::params::makeFloat (ParamIDs::vibRate, "Vibrato Rate", { 3.0f, 8.0f, 0.01f }, 5.5f, "Hz", 1));
    layout.add (at::params::makeSeconds (ParamIDs::glide, "Glide", 0.005f, 0.5f, 0.06f));
    layout.add (at::params::makeSeconds (ParamIDs::release, "Release", 0.02f, 1.0f, 0.12f));
    layout.add (at::params::makePercent (ParamIDs::lofi, "Lo-fi", 0.0f));
    layout.add (at::params::makeDecibels (ParamIDs::volume, "Volume", -24.0f, 12.0f, 0.0f));
    layout.add (at::params::makeChoice (ParamIDs::spelling, "Spelling", { "English", "Romaji" }, 0));
    layout.add (at::params::makeChoice (ParamIDs::timing, "Timing", { "On the beat", "Live" }, 0));
    return layout;
}

//==============================================================================
juce::String PluginProcessor::getLyrics() const
{
    return apvts.state.getProperty (lyricsProperty, juce::String()).toString();
}

void PluginProcessor::setLyrics (const juce::String& text)
{
    apvts.state.setProperty (lyricsProperty, text, nullptr);
    refreshLyrics();
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    at::ProcessorBase::setStateInformation (data, sizeInBytes);
    if (! apvts.state.hasProperty (lyricsProperty))
        apvts.state.setProperty (lyricsProperty, defaultLyrics, nullptr);

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        refreshLyrics();   // otherwise the timer picks it up
}

void PluginProcessor::refreshLyrics()
{
    const auto text = getLyrics();
    const auto spelling = (int) spellingParam.load();
    if (text == parsedText && spelling == parsedSpelling)
        return;

    parsedText = text;
    parsedSpelling = spelling;
    syllablesForEditor = parrot::parseLyrics (text, spelling == 1 ? parrot::Spelling::romaji : parrot::Spelling::english);

    {
        const juce::SpinLock::ScopedLockType lock (incomingLock);
        incoming = syllablesForEditor;
        incomingReady = true;
    }

    // The editor follows lyrics restored with a set or preset, and the spelling switch.
    lyricsChanged.sendChangeMessage();
}

void PluginProcessor::updateLatency()
{
    const auto beat = (int) timingParam.load() == 0;
    const auto samples = beat ? juce::roundToInt (lookaheadSeconds * getSampleRate()) : 0;
    if (getSampleRate() > 0.0 && samples != getLatencySamples())
        setLatencySamples (samples);
}

void PluginProcessor::timerCallback()
{
    refreshLyrics();
    updateLatency();
}

//==============================================================================
void PluginProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    voice.prepare (newSampleRate);
    voiceBuffer.allocate ((size_t) juce::jmax (1, samplesPerBlock), true);
    voiceBufferSize = juce::jmax (1, samplesPerBlock);
    events.clear();
    held.clear();
    clock = 0;
    lastNoteOnTime = -1;
    lofiPhase = lofiHeld = 0.0f;
    volume.reset (newSampleRate, 0.05);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));

    onTheBeat = (int) timingParam.load() == 0;
    latency = onTheBeat ? juce::roundToInt (lookaheadSeconds * newSampleRate) : 0;
    setLatencySamples (latency);
}

void PluginProcessor::advanceCursor (int delta)
{
    const auto count = (int) live.size();
    cursor = count > 0 ? ((cursor + delta) % count + count) % count : 0;
    nextSyllable = cursor;
}

void PluginProcessor::receive (const juce::MidiMessage& m, juce::int64 time)
{
    if (m.isNoteOn())
    {
        Event e;
        e.time = time;
        e.on = true;
        e.note = m.getNoteNumber();
        e.velocity = m.getFloatVelocity();

        if (! live.empty())
        {
            e.syllableIndex = juce::jlimit (0, (int) live.size() - 1, cursor);
            e.syllable = live[(size_t) e.syllableIndex];
            advanceCursor (1);
        }
        else
        {
            e.syllable.nucleus = parrot::Ph::AA;
        }

        // A note-off just before this one: close the old syllable straight into this one.
        for (auto& previous : events)
            if (! previous.on && ! previous.absorbed && previous.returnTo < 0
                && time - previous.time <= (juce::int64) (legatoWindowSeconds * sampleRate))
                previous.absorbed = true;

        held.erase (std::remove (held.begin(), held.end(), e.note), held.end());
        held.push_back (e.note);

        if ((int) events.size() < maxEvents)
            events.push_back (e);
    }
    else if (m.isNoteOff())
    {
        Event e;
        e.time = time;
        e.note = m.getNoteNumber();
        held.erase (std::remove (held.begin(), held.end(), e.note), held.end());
        e.returnTo = held.empty() ? -1 : held.back();
        if ((int) events.size() < maxEvents)
            events.push_back (e);
    }
    else if (m.isPitchWheel())
    {
        pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
    }
    else if (m.isControllerOfType (1))
    {
        modWheel = (float) m.getControllerValue() / 127.0f;
    }
    else if (m.isControllerOfType (50) && m.getControllerValue() >= 64)
    {
        advanceCursor (-1);
    }
    else if (m.isControllerOfType (51) && m.getControllerValue() >= 64)
    {
        advanceCursor (1);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        events.clear();
        held.clear();
        voice.stop();
    }
}

juce::int64 PluginProcessor::dueTime (const Event& e) const
{
    if (! onTheBeat)
        return e.time;

    const auto lead = [this] (float seconds) { return (juce::int64) std::llround (seconds * sampleRate); };
    juce::int64 due = e.time + latency;

    if (e.on)
    {
        // Start the consonants early, so the vowel lands on the note.
        due -= lead (voice.onsetSeconds (e.syllable));
        if (voice.isHolding() && ! e.syllable.slur)
            due -= lead (voice.codaSeconds (voice.getSyllable()));
    }
    else if (! e.absorbed && e.returnTo < 0 && voice.isHolding() && voice.getNote() == e.note)
    {
        // Finish the closing consonants by the end of the note.
        due -= lead (voice.codaSeconds (voice.getSyllable()));
    }

    return juce::jmax (e.time, due);
}

void PluginProcessor::apply (const Event& e)
{
    if (e.on)
    {
        voice.noteOn (e.syllable, e.note, e.velocity);
        lastNoteOnTime = e.time;
        if (e.syllableIndex >= 0)
            sungSyllable = e.syllableIndex;
        return;
    }

    // A note-on that came later has already started (its consonants run ahead of the beat), so
    // this note-off belongs to the note before it: the two are sung straight through.
    if (e.absorbed || e.time <= lastNoteOnTime || ! voice.isHolding() || voice.getNote() != e.note)
        return;

    if (e.returnTo >= 0)
        voice.glideTo (e.returnTo);
    else
        voice.noteOff();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // New lyrics from the message thread.
    if (const juce::SpinLock::ScopedTryLockType lock (incomingLock); lock.isLocked() && incomingReady)
    {
        std::swap (live, incoming);
        incomingReady = false;
        if (cursor >= (int) live.size())
            cursor = 0;
        nextSyllable = cursor;
        sungSyllable = -1;
    }

    if (const auto jump = requestedJump.exchange (-1); jump >= 0)
    {
        cursor = 0;
        advanceCursor (jump);
    }
    if (const auto step = requestedStep.exchange (0); step != 0)
        advanceCursor (step);

    // Back to the first syllable whenever the transport starts.
    bool playing = false;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
            playing = position->getIsPlaying();
    if (playing && ! wasPlaying)
    {
        cursor = 0;
        nextSyllable = 0;
    }
    wasPlaying = playing;

    const auto beat = (int) timingParam.load() == 0;
    if (beat != onTheBeat)
    {
        onTheBeat = beat;
        latency = beat ? juce::roundToInt (lookaheadSeconds * sampleRate) : 0;
    }

    parrot::VoiceSettings settings;
    settings.size = sizeParam.load() / 100.0f;
    settings.tone = toneParam.load() / 100.0f;
    settings.breath = breathParam.load() / 100.0f;
    settings.diction = dictionParam.load() / 100.0f;
    settings.vibratoCents = vibratoParam.load();
    settings.vibratoRate = vibRateParam.load();
    settings.glideSeconds = glideParam.load();
    settings.releaseSeconds = releaseParam.load();

    for (const auto metadata : midi)
        receive (metadata.getMessage(), clock + juce::jlimit (0, numSamples - 1, metadata.samplePosition));

    settings.pitchBend = pitchBend;
    settings.modWheel = modWheel;
    voice.setSettings (settings);

    // Render, applying each event when it falls due.
    if (numSamples > voiceBufferSize)
    {
        voiceBuffer.allocate ((size_t) numSamples, true);   // only if the host broke its promise
        voiceBufferSize = numSamples;
    }

    int position = 0;
    while (position < numSamples)
    {
        // Earliest due first; on a tie, the one that arrived first.
        int next = -1;
        juce::int64 nextDue = std::numeric_limits<juce::int64>::max();
        for (int i = 0; i < (int) events.size(); ++i)
        {
            if (const auto due = dueTime (events[(size_t) i]); due < nextDue)
            {
                nextDue = due;
                next = i;
            }
        }
        if (nextDue >= clock + numSamples)
            next = -1;

        const auto until = next < 0 ? numSamples : (int) juce::jlimit ((juce::int64) position, (juce::int64) numSamples, nextDue - clock);
        if (until > position)
            voice.render (voiceBuffer.get() + position, until - position);
        position = until;

        if (next >= 0)
        {
            const auto e = events[(size_t) next];
            events.erase (events.begin() + next);
            apply (e);
        }
    }

    // Lo-fi: a lower sample rate and fewer bits.
    if (const auto amount = lofiParam.load() / 100.0f; amount > 0.0f)
    {
        const auto rate = juce::jmap (amount, 48000.0f, 8000.0f) / (float) sampleRate;
        const auto steps = std::exp2 (juce::jmap (amount, 16.0f, 6.0f) - 1.0f);
        auto* v = voiceBuffer.get();
        for (int i = 0; i < numSamples; ++i)
        {
            lofiPhase += rate;
            if (lofiPhase >= 1.0f)
            {
                lofiPhase -= std::floor (lofiPhase);
                lofiHeld = std::round (v[i] * steps) / steps;
            }
            v[i] = lofiHeld;
        }
    }

    volume.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
    auto* out0 = buffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        out0[i] = voiceBuffer[i] * volume.getNextValue();
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    clock += numSamples;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
