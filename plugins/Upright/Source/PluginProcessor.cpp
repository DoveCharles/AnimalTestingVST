#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** Bridge velocity (m/s) to output level: a pedalled stride passage at velocity 0.7 peaks
        around -9 dBFS, leaving room for fortissimo chords. */
    constexpr float outputScale = 0.7f;
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
      volumeParam   (at::params::raw (apvts, ParamIDs::volume))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::voicing, "Voicing", { "Studio", "Mills" }, 1));
    layout.add (at::params::makePercent (ParamIDs::hardness, "Hardness", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::unison, "Unison", 30.0f));
    layout.add (at::params::makeFloat (ParamIDs::decay, "Decay", { 25.0f, 200.0f, 1.0f }, 100.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::resonance, "Resonance", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::noise, "Noise", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::body, "Body", 85.0f));
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

        soundboards[(size_t) v].loadImpulseResponse (piano::makeSoundboard (sampleRate, voicing), sampleRate,
                                                     juce::dsp::Convolution::Stereo::yes,
                                                     juce::dsp::Convolution::Trim::no,
                                                     juce::dsp::Convolution::Normalise::no);
        soundboards[(size_t) v].prepare (spec);
        boardBuffers[(size_t) v].setSize (2, samplesPerBlock);
    }

    bridge.assign ((size_t) samplesPerBlock, 0.0f);

    dryHighPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0);
    dryHighPass.reset();

    body.reset (sampleRate, 0.05);
    body.setCurrentAndTargetValue (bodyParam.load() / 100.0f);
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

    const auto selected = (size_t) juce::jlimit (0, 1, (int) voicingParam.load());

    // Each engine goes through its own soundboard; the bridge signal of both is the dry path.
    std::fill (bridge.begin(), bridge.begin() + numSamples, 0.0f);

    for (size_t v = 0; v < engines.size(); ++v)
    {
        auto& board = boardBuffers[v];
        board.clear (0, 0, numSamples);

        int position = 0;
        auto render = [&] (int end)
        {
            if (end > position)
                engines[v].render (board.getWritePointer (0, position), end - position);
            position = end;
        };

        for (const auto metadata : midi)
        {
            render (juce::jmin (metadata.samplePosition, numSamples));
            handleMidi (metadata.getMessage(), engines[v], v == selected);
        }
        render (numSamples);

        juce::FloatVectorOperations::add (bridge.data(), board.getReadPointer (0), numSamples);
        board.copyFrom (1, 0, board, 0, 0, numSamples);

        juce::dsp::AudioBlock<float> block (board.getArrayOfWritePointers(), 2, (size_t) numSamples);
        soundboards[v].process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    body.setTargetValue (bodyParam.load() / 100.0f);
    volume.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));

    const auto outChannels = juce::jmin (2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i)
    {
        const auto wet = body.getNextValue();
        const auto gain = volume.getNextValue() * outputScale;
        const auto dry = dryHighPass.processSample (bridge[(size_t) i]) * (1.0f - wet);

        for (int ch = 0; ch < outChannels; ++ch)
        {
            const auto boardSample = boardBuffers[0].getSample (ch, i) + boardBuffers[1].getSample (ch, i);
            buffer.setSample (ch, i, gain * (dry + wet * boardSample));
        }
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
