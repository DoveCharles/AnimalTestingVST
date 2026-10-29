#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // SoundFonts are voiced for a GM module playing many parts at once, so one part at unity
    // is quiet next to the other plugins. This lines a three- or four-note chord up just under full scale.
    constexpr float makeupGain = 1.4f;

    juce::dsp::Reverb::Parameters roomFor (float percent)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.6f + 0.3f * percent / 100.0f;
        p.damping  = 0.4f;
        p.width    = 1.0f;
        p.dryLevel = 0.5f;   // unity: juce::Reverb doubles it
        p.wetLevel = 0.2f * percent / 100.0f;
        return p;
    }

    float controllerGain (int value)
    {
        const auto x = (float) value / 127.0f;
        return x * x;   // the spec's concave curve for CC7 and CC11
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      volumeParam    (at::params::raw (apvts, ParamIDs::volume)),
      transposeParam (at::params::raw (apvts, ParamIDs::transpose)),
      tuneParam      (at::params::raw (apvts, ParamIDs::tune)),
      bendRangeParam (at::params::raw (apvts, ParamIDs::bendRange)),
      velocityParam  (at::params::raw (apvts, ParamIDs::velocity)),
      releaseParam   (at::params::raw (apvts, ParamIDs::release)),
      reverbParam    (at::params::raw (apvts, ParamIDs::reverb))
{
    apvts.state.addListener (this);
    startTimerHz (4);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
    apvts.state.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::NormalisableRange<float> releaseRange { 0.25f, 4.0f, 0.01f };
    releaseRange.setSkewForCentre (1.0f);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeDecibels (ParamIDs::volume, "Volume", -36.0f, 12.0f, 0.0f));
    layout.add (at::params::makeFloat (ParamIDs::transpose, "Transpose", { -24.0f, 24.0f, 1.0f }, 0.0f, "st", 0));
    layout.add (at::params::makeFloat (ParamIDs::tune, "Tune", { -100.0f, 100.0f, 1.0f }, 0.0f, "ct", 0));
    layout.add (at::params::makeFloat (ParamIDs::bendRange, "Bend Range", { 0.0f, 24.0f, 1.0f }, 2.0f, "st", 0));
    layout.add (at::params::makePercent (ParamIDs::velocity, "Velocity", 100.0f));
    layout.add (at::params::makeFloat (ParamIDs::release, "Release", releaseRange, 1.0f, "x", 2));
    layout.add (at::params::makePercent (ParamIDs::reverb, "Reverb", 0.0f));
    return layout;
}

//==============================================================================
void PluginProcessor::loadSoundFont (const juce::File& file)
{
    apvts.state.setProperty (StateKeys::soundFont, file.getFullPathName(), nullptr);
    apvts.state.setProperty (StateKeys::bank, -1, nullptr);
    apvts.state.setProperty (StateKeys::program, -1, nullptr);
    selection = -1;
    fonts.load (file);
}

void PluginProcessor::selectPreset (int bank, int program)
{
    apvts.state.setProperty (StateKeys::bank, bank, nullptr);
    apvts.state.setProperty (StateKeys::program, program, nullptr);
    selection = packPreset (bank, program);
}

void PluginProcessor::restoreFromState()
{
    const juce::String path = apvts.state.getProperty (StateKeys::soundFont).toString();
    const juce::File file = path.isNotEmpty() ? juce::File (path) : juce::File();
    const int bank = apvts.state.getProperty (StateKeys::bank, -1);
    const int program = apvts.state.getProperty (StateKeys::program, -1);

    selection = bank < 0 || program < 0 ? -1 : packPreset (bank, program);
    if (file != fonts.getFile())
        fonts.load (file);
}

void PluginProcessor::timerCallback()
{
    // Keeps the saved preset in step with what's playing, after a MIDI program change or when
    // the font's first preset was picked for us. Only once the loaded font is the one playing.
    const auto current = playing.load();
    if (current < 0 || fonts.getLoadedSerial() != playingSerial.load())
        return;

    const auto bank = current >> 8, program = current & 0xff;
    if ((int) apvts.state.getProperty (StateKeys::bank, -1) != bank)
        apvts.state.setProperty (StateKeys::bank, bank, nullptr);
    if ((int) apvts.state.getProperty (StateKeys::program, -1) != program)
        apvts.state.setProperty (StateKeys::program, program, nullptr);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    monoScratch.setSize (1, samplesPerBlock);

    room.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    room.setParameters (roomFor (reverbParam.load()));
    room.reset();

    volume.reset (sampleRate, 0.05);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
    channelVolume.reset (sampleRate, 0.02);
    channelVolume.setCurrentAndTargetValue (controllerGain (100));
    expression.reset (sampleRate, 0.02);
    expression.setCurrentAndTargetValue (1.0f);
}

void PluginProcessor::resolvePreset() noexcept
{
    const auto* font = engine.getFont();
    if (font == nullptr)
    {
        playing = -1;
        return;
    }

    const auto wanted = selection.load();
    if (wanted == resolvedSelection)
        return;

    resolvedSelection = wanted;
    const auto* preset = wanted < 0 ? &font->presets.front() : font->find (wanted >> 8, wanted & 0xff);
    engine.setPreset (preset);
    playing = preset != nullptr ? packPreset (preset->bank, preset->program) : -1;
}

void PluginProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.setSustain (m.isSustainPedalOn());
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isProgramChange())
    {
        selection = packPreset (bankSelect, m.getProgramChangeNumber());
        resolvePreset();
    }
    else if (m.isController())
    {
        const auto value = m.getControllerValue();
        switch (m.getControllerNumber())
        {
            case 0:   bankSelect = value; break;
            case 1:   engine.setModWheel ((float) value / 127.0f); break;
            case 7:   channelVolume.setTargetValue (controllerGain (value)); break;
            case 10:  engine.setPan ((float) (value - 64) / 64.0f); break;
            case 11:  expression.setTargetValue (controllerGain (value)); break;
            case 120: engine.allSoundOff(); break;
            case 121:
                engine.setModWheel (0.0f);
                engine.setPitchBend (0.0f);
                engine.setSustain (false);
                expression.setTargetValue (1.0f);
                break;
            case 123: engine.allNotesOff(); break;
            default:  break;
        }
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Swap the engine onto a newly loaded font before anything else touches it: the old one is
    // freed by the loader soon after acquire() hands it back.
    int serial = 0;
    if (const auto* font = fonts.acquire (serial); font != engine.getFont())
    {
        engine.setFont (font);
        resolvedSelection = -2;
    }
    playingSerial = serial;
    resolvePreset();

    magpie::Controls controls;
    controls.transpose    = juce::roundToInt (transposeParam.load());
    controls.tuneCents    = tuneParam.load();
    controls.bendRange    = bendRangeParam.load();
    controls.velocity     = velocityParam.load() / 100.0f;
    controls.releaseScale = releaseParam.load();
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

    activeVoices = engine.getNumActiveVoices();

    room.setParameters (roomFor (reverbParam.load()));
    if (reverbParam.load() > 0.0f)
    {
        float* channels[] = { left, right };
        juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
        room.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    volume.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = makeupGain * volume.getNextValue() * channelVolume.getNextValue() * expression.getNextValue();
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
