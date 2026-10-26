#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** Bridge velocity (m/s) to output level: a pedalled stride passage at velocity 0.7 peaks
        around -9 dBFS, leaving room for fortissimo chords. */
    constexpr float outputScale = 0.7f;

    /** The room is heard from close mics: even at full it sits well behind the piano. */
    float roomLevelFor (float percent) { return 0.5f * percent / 100.0f; }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      voicingParam  (at::params::raw (apvts, ParamIDs::voicing)),
      hardnessParam (at::params::raw (apvts, ParamIDs::hardness)),
      unisonParam   (at::params::raw (apvts, ParamIDs::unison)),
      decayParam    (at::params::raw (apvts, ParamIDs::decay)),
      resonanceParam (at::params::raw (apvts, ParamIDs::resonance)),
      noiseParam    (at::params::raw (apvts, ParamIDs::noise)),
      bodyParam     (at::params::raw (apvts, ParamIDs::body)),
      roomParam     (at::params::raw (apvts, ParamIDs::room)),
      volumeParam   (at::params::raw (apvts, ParamIDs::volume))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::voicing, "Voicing", { "Studio", "Mills", "House" }, 1));
    layout.add (at::params::makePercent (ParamIDs::hardness, "Hardness", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::unison, "Unison", 30.0f));
    layout.add (at::params::makeFloat (ParamIDs::decay, "Decay", { 25.0f, 200.0f, 1.0f }, 100.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::resonance, "Resonance", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::noise, "Noise", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::body, "Body", 85.0f));
    layout.add (at::params::makePercent (ParamIDs::room, "Room", 25.0f));
    layout.add (at::params::makeDecibels (ParamIDs::volume, "Volume", -24.0f, 12.0f, 0.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    for (int v = 0; v < 2; ++v)
    {
        const auto voicing = v == 0 ? piano::Voicing::studio : piano::Voicing::mills;
        engines[(size_t) v].prepare (sampleRate, voicing);

        for (int p = 0; p < piano::numDrivingPoints; ++p)
        {
            auto& board = soundboards[(size_t) v][(size_t) p];
            board.loadImpulseResponse (piano::makeSoundboard (sampleRate, voicing, (piano::DrivingPoint) p), sampleRate,
                                       juce::dsp::Convolution::Stereo::yes,
                                       juce::dsp::Convolution::Trim::no,
                                       juce::dsp::Convolution::Normalise::no);
            board.prepare (spec);
            boardBuffers[(size_t) v][(size_t) p].setSize (2, samplesPerBlock);
        }
    }

    house.prepare (sampleRate);
    houseBuffer.setSize (2, samplesPerBlock);

    room.loadImpulseResponse (piano::makeRoom (sampleRate), sampleRate,
                              juce::dsp::Convolution::Stereo::yes,
                              juce::dsp::Convolution::Trim::no,
                              juce::dsp::Convolution::Normalise::no);
    room.prepare (spec);
    roomBuffer.setSize (2, samplesPerBlock);
    dry.setSize (2, samplesPerBlock);
    mix.setSize (2, samplesPerBlock);

    for (auto& f : dryHighPass)
    {
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0);
        f.reset();
    }

    body.reset (sampleRate, 0.05);
    body.setCurrentAndTargetValue (bodyParam.load() / 100.0f);
    roomLevel.reset (sampleRate, 0.05);
    roomLevel.setCurrentAndTargetValue (roomLevelFor (roomParam.load()));
    volume.reset (sampleRate, 0.05);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
}

void PluginProcessor::handleMidi (const juce::MidiMessage& m, piano::Engine& engine, bool strikes)
{
    if (m.isNoteOn())
    {
        if (strikes)
            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        engine.noteOff (m.getNoteNumber());
    }
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
    {
        // Only the selected voicing's pedal makes a noise; the other just follows it.
        engine.setSustain (m.isSustainPedalOn(), strikes);
    }
    else if (m.isSoftPedalOn() || m.isSoftPedalOff())
    {
        engine.setSoftPedal (m.isSoftPedalOn());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff();
    }
}

void PluginProcessor::handleMidi (const juce::MidiMessage& m, piano::HousePiano& house, bool strikes)
{
    if (m.isNoteOn())
    {
        if (strikes)
            house.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        house.noteOff (m.getNoteNumber());
    }
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
    {
        house.setSustain (m.isSustainPedalOn());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        house.allNotesOff();
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const piano::Controls controls { hardnessParam.load() / 100.0f, unisonParam.load() / 100.0f, decayParam.load() / 100.0f,
                                     resonanceParam.load() / 100.0f, noiseParam.load() / 100.0f };
    for (auto& e : engines)
        e.setControls (controls);
    house.setControls (controls);

    const auto selected = (size_t) juce::jlimit (0, 2, (int) voicingParam.load());

    // Each engine drives its own boards; the strings as the close mics hear them are shared.
    dry.clear (0, 0, numSamples);
    dry.clear (1, 0, numSamples);

    for (size_t v = 0; v < engines.size(); ++v)
    {
        auto& boards = boardBuffers[v];
        for (auto& b : boards)
            b.clear (0, 0, numSamples);

        int position = 0;
        auto render = [&] (int end)
        {
            if (end > position)
                engines[v].render ({ boards[0].getWritePointer (0, position), boards[1].getWritePointer (0, position),
                                     boards[2].getWritePointer (0, position), dry.getWritePointer (0, position),
                                     dry.getWritePointer (1, position) },
                                   end - position);
            position = end;
        };

        for (const auto metadata : midi)
        {
            render (juce::jmin (metadata.samplePosition, numSamples));
            handleMidi (metadata.getMessage(), engines[v], v == selected);
        }
        render (numSamples);

        for (size_t p = 0; p < boards.size(); ++p)
        {
            boards[p].copyFrom (1, 0, boards[p], 0, 0, numSamples);
            juce::dsp::AudioBlock<float> block (boards[p].getArrayOfWritePointers(), 2, (size_t) numSamples);
            soundboards[v][p].process (juce::dsp::ProcessContextReplacing<float> (block));
        }
    }

    houseBuffer.clear (0, 0, numSamples);
    houseBuffer.clear (1, 0, numSamples);
    {
        int position = 0;
        auto render = [&] (int end)
        {
            if (end > position)
                house.render (houseBuffer.getWritePointer (0, position), houseBuffer.getWritePointer (1, position), end - position);
            position = end;
        };

        for (const auto metadata : midi)
        {
            render (juce::jmin (metadata.samplePosition, numSamples));
            handleMidi (metadata.getMessage(), house, selected == 2);
        }
        render (numSamples);
    }

    body.setTargetValue (bodyParam.load() / 100.0f);
    roomLevel.setTargetValue (roomLevelFor (roomParam.load()));
    volume.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = dry.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = dryHighPass[(size_t) ch].processSample (d[i]);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wet = body.getNextValue();
        for (int ch = 0; ch < 2; ++ch)
        {
            float boardSample = 0.0f;
            for (const auto& voicingBoards : boardBuffers)
                for (const auto& b : voicingBoards)
                    boardSample += b.getSample (ch, i);
            mix.setSample (ch, i, dry.getSample (ch, i) * (1.0f - wet) + wet * boardSample + houseBuffer.getSample (ch, i));
        }
    }

    // The room hears the whole piano.
    for (int ch = 0; ch < 2; ++ch)
        roomBuffer.copyFrom (ch, 0, mix, ch, 0, numSamples);
    juce::dsp::AudioBlock<float> roomBlock (roomBuffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
    room.process (juce::dsp::ProcessContextReplacing<float> (roomBlock));

    const auto outChannels = juce::jmin (2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = volume.getNextValue() * outputScale;
        const auto roomGain = roomLevel.getNextValue();
        for (int ch = 0; ch < outChannels; ++ch)
            buffer.setSample (ch, i, gain * (mix.getSample (ch, i) + roomGain * roomBuffer.getSample (ch, i)));
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
