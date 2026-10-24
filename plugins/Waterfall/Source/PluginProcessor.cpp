#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Voices alternate left/right and are detuned by a few cents for a wide, shimmering ring.
    constexpr std::array<float, Chords::numVoices> voicePan    { -0.7f, 0.7f, -0.35f, 0.35f, -0.9f, 0.9f };
    constexpr std::array<float, Chords::numVoices> voiceDetune { -4.0f, 4.0f,  -2.0f,  2.0f, -6.0f, 6.0f };

    constexpr float outputDrive = 0.6f;
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      keyParam        (at::params::raw (apvts, ParamIDs::key)),
      chordParam      (at::params::raw (apvts, ParamIDs::chord)),
      brightnessParam (at::params::raw (apvts, ParamIDs::brightness)),
      decayParam      (at::params::raw (apvts, ParamIDs::decay)),
      mixParam        (at::params::raw (apvts, ParamIDs::mix))
{
    for (size_t v = 0; v < voices.size(); ++v)
    {
        // Equal-power pan.
        const auto angle = (voicePan[v] + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        panLeft[v]  = std::cos (angle);
        panRight[v] = std::sin (angle);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice  (ParamIDs::key,   "Key",   Chords::keyNames, 0));
    layout.add (at::params::makeChoice  (ParamIDs::chord, "Chord", Chords::names(),  2));
    layout.add (at::params::makePercent (ParamIDs::brightness, "Brightness", 60.0f));
    layout.add (at::params::makeSeconds (ParamIDs::decay, "Decay", 0.05f, 10.0f, 1.5f));
    layout.add (at::params::makePercent (ParamIDs::mix,   "Mix", 70.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    // Lowest possible voice is C3 detuned down; leave headroom below it.
    const auto lowestHz = (float) juce::MidiMessage::getMidiNoteInHertz (Chords::rootMidiNote) * 0.9f;
    for (auto& voice : voices)
        voice.prepare (sampleRate, lowestHz);

    currentKey = currentChord = -1;
    updateTuning();

    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, numChannels });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
    dryWet.setWetMixProportion (mixParam.load() / 100.0f);
}

void PluginProcessor::updateTuning()
{
    const auto key   = juce::jlimit (0, 11, (int) keyParam.load());
    const auto chord = juce::jlimit (0, (int) Chords::all.size() - 1, (int) chordParam.load());

    if (key == currentKey && chord == currentChord)
        return;

    currentKey = key;
    currentChord = chord;

    const auto& intervals = Chords::all[(size_t) chord].intervals;
    for (size_t v = 0; v < voices.size(); ++v)
    {
        const auto note = (float) (Chords::rootMidiNote + key + intervals[v]) + voiceDetune[v] / 100.0f;
        voices[v].setFrequency (440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f));
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    updateTuning();

    const auto brightness = brightnessParam.load() / 100.0f;
    const auto decay = decayParam.load();
    for (auto& voice : voices)
    {
        voice.setBrightness (brightness);
        voice.setDecay (decay);
    }

    dryWet.setWetMixProportion (mixParam.load() / 100.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    dryWet.pushDrySamples (block);

    auto* left  = buffer.getWritePointer (0);
    auto* right = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto inputScale = 1.0f / (float) juce::jmax (1, numIn);

    for (int n = 0; n < numSamples; ++n)
    {
        float in = 0.0f;
        for (int ch = 0; ch < numIn; ++ch)
            in += buffer.getSample (ch, n);
        in *= inputScale;

        float outL = 0.0f, outR = 0.0f;
        for (size_t v = 0; v < voices.size(); ++v)
        {
            const auto y = voices[v].processSample (in);
            outL += y * panLeft[v];
            outR += y * panRight[v];
        }

        // Soft limiter: long decays on loud, tonal input can stack up a lot of energy.
        outL = std::tanh (outL * outputDrive);
        outR = std::tanh (outR * outputDrive);

        if (right != nullptr)
        {
            left[n]  = outL;
            right[n] = outR;
        }
        else
        {
            left[n] = 0.5f * (outL + outR);
        }
    }

    dryWet.mixWetSamples (block);

    // Feed the editor's spectrogram with the final mono output. Drops samples if the GUI isn't reading.
    const auto scope = scopeFifo.write (juce::jmin (numSamples, scopeFifo.getFreeSpace()));
    const auto outScale = 1.0f / (float) juce::jmax (1, numOut);
    auto pushScope = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numOut; ++ch)
                sum += buffer.getSample (ch, offset + i);
            scopeBuffer[(size_t) (start + i)] = sum * outScale;
        }
    };
    pushScope (scope.startIndex1, scope.blockSize1, 0);
    pushScope (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

int PluginProcessor::pullScopeSamples (float* dest, int maxSamples)
{
    const auto scope = scopeFifo.read (juce::jmin (maxSamples, scopeFifo.getNumReady()));
    std::copy_n (scopeBuffer.begin() + scope.startIndex1, scope.blockSize1, dest);
    std::copy_n (scopeBuffer.begin() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);
    return scope.blockSize1 + scope.blockSize2;
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
