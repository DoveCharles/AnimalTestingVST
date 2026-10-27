#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** The M400's tone control is a treble cut; fully open it passes everything the tapes hold. */
    float cutoffFor (float percent)
    {
        return 900.0f * std::pow (18000.0f / 900.0f, percent / 100.0f);
    }

    juce::dsp::Reverb::Parameters roomFor (float percent)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.55f + 0.3f * percent / 100.0f;
        p.damping  = 0.45f;
        p.width    = 1.0f;
        p.dryLevel = 0.5f;   // unity: juce::Reverb doubles it
        p.wetLevel = 0.16f * percent / 100.0f;
        return p;
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      tapeAParam    (at::params::raw (apvts, ParamIDs::tapeA)),
      tapeBParam    (at::params::raw (apvts, ParamIDs::tapeB)),
      blendParam    (at::params::raw (apvts, ParamIDs::blend)),
      pitchParam    (at::params::raw (apvts, ParamIDs::pitch)),
      wowParam      (at::params::raw (apvts, ParamIDs::wow)),
      clickParam    (at::params::raw (apvts, ParamIDs::click)),
      loopParam     (at::params::raw (apvts, ParamIDs::loop)),
      attackParam   (at::params::raw (apvts, ParamIDs::attack)),
      releaseParam  (at::params::raw (apvts, ParamIDs::release)),
      velocityParam (at::params::raw (apvts, ParamIDs::velocity)),
      toneParam     (at::params::raw (apvts, ParamIDs::tone)),
      spreadParam   (at::params::raw (apvts, ParamIDs::spread)),
      reverbParam   (at::params::raw (apvts, ParamIDs::reverb)),
      volumeParam   (at::params::raw (apvts, ParamIDs::volume))
{
    library->request ((int) tapeAParam.load());
    library->request ((int) tapeBParam.load());
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    const auto tapes = chilltron::tapeDisplayNames();

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::tapeA, "Tape A", tapes, 0));
    layout.add (at::params::makeChoice (ParamIDs::tapeB, "Tape B", tapes, 5));
    layout.add (at::params::makePercent (ParamIDs::blend, "Blend", 0.0f));
    layout.add (at::params::makeFloat (ParamIDs::pitch, "Pitch", { -100.0f, 100.0f, 0.1f }, 0.0f, "ct", 0));
    layout.add (at::params::makePercent (ParamIDs::wow, "Wow & Flutter", 35.0f));
    layout.add (at::params::makePercent (ParamIDs::click, "Click", 50.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::loop), "Loop", false));
    layout.add (at::params::makeSeconds (ParamIDs::attack, "Attack", 0.002f, 2.0f, 0.012f));
    layout.add (at::params::makeSeconds (ParamIDs::release, "Release", 0.03f, 3.0f, 0.12f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::velocity), "Velocity", false));
    layout.add (at::params::makePercent (ParamIDs::tone, "Tone", 70.0f));
    layout.add (at::params::makePercent (ParamIDs::spread, "Spread", 30.0f));
    layout.add (at::params::makePercent (ParamIDs::reverb, "Reverb", 20.0f));
    layout.add (at::params::makeDecibels (ParamIDs::volume, "Volume", -24.0f, 6.0f, 0.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    monoScratch.setSize (1, samplesPerBlock);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setResonance (0.65f);
    toneFilter.setCutoffFrequency (cutoffFor (toneParam.load()));

    room.prepare (spec);
    room.setParameters (roomFor (reverbParam.load()));
    room.reset();

    cutoff.reset (sampleRate, 0.05);
    cutoff.setCurrentAndTargetValue (cutoffFor (toneParam.load()));
    volume.reset (sampleRate, 0.05);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
    expression.reset (sampleRate, 0.02);
    expression.setCurrentAndTargetValue (1.0f);
}

void PluginProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.setSustain (m.isSustainPedalOn());
    else if (m.isPitchWheel())
        engine.setPitchBend (2.0f * (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType (1))
        engine.setModWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isControllerOfType (11))
        expression.setTargetValue ((float) m.getControllerValue() / 127.0f);   // the M400's volume pedal
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const auto a = (int) tapeAParam.load();
    const auto b = (int) tapeBParam.load();
    const auto blend = blendParam.load() / 100.0f;
    library->request (a);
    library->request (b);
    engine.setTapes (library->get (a), library->get (b), library->getClick());

    chilltron::Controls controls;
    controls.blend      = blend;
    controls.pitchCents = pitchParam.load();
    controls.wow        = wowParam.load() / 100.0f;
    controls.attack     = attackParam.load();
    controls.release    = releaseParam.load();
    controls.click      = clickParam.load() / 100.0f;
    controls.velocity   = velocityParam.load() >= 0.5f;
    controls.spread     = spreadParam.load() / 100.0f;
    controls.loop       = loopParam.load() >= 0.5f;
    engine.setControls (controls);

    // Rendered in stereo even for a mono output; folded down below.
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (right == nullptr)
    {
        monoScratch.setSize (1, numSamples, false, false, true);
        monoScratch.clear();
        right = monoScratch.getWritePointer (0);
    }

    int position = 0;
    for (const auto metadata : midi)
    {
        const auto end = juce::jmin (metadata.samplePosition, numSamples);
        if (end > position)
            engine.render (left + position, right + position, end - position);
        position = juce::jmax (position, end);
        handleMidi (metadata.getMessage());
    }
    if (numSamples > position)
        engine.render (left + position, right + position, numSamples - position);

    cutoff.setTargetValue (cutoffFor (toneParam.load()));
    volume.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));

    for (int i = 0; i < numSamples; ++i)
    {
        if (i % 32 == 0)
            toneFilter.setCutoffFrequency (cutoff.getNextValue());
        else
            cutoff.getNextValue();

        left[i]  = toneFilter.processSample (0, left[i]);
        right[i] = toneFilter.processSample (1, right[i]);
    }

    room.setParameters (roomFor (reverbParam.load()));
    if (buffer.getNumChannels() > 1)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
        room.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
    else
    {
        float* channels[] = { left, right };
        juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
        room.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = volume.getNextValue() * expression.getNextValue();
        if (buffer.getNumChannels() > 1)
        {
            left[i] *= gain;
            right[i] *= gain;
        }
        else
        {
            left[i] = 0.5f * gain * (left[i] + right[i]);
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
