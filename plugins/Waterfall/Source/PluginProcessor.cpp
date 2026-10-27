#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Pan and detune by each tone's place in the chord, lowest first, for a wide, shimmering ring. Each
    // side gets an even share of low and high tones and its detunes cancel, so panning the track (a
    // balance control on stereo tracks in most DAWs) doesn't make it brighter or sharper on one side.
    // No tone is more than about 8 dB louder on one side, so panning thins the chord rather than losing notes;
    // most of the width comes from the detune between partners.
    constexpr std::array<float, Chords::numVoices> rankPan    { -0.4f, 0.5f,  0.4f, -0.5f, -0.2f,  0.2f };
    constexpr std::array<float, Chords::numVoices> rankDetune { -4.0f, 4.0f, -6.0f,  6.0f, -2.0f,  2.0f };

    constexpr float outputDrive = 0.6f;
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      keyParam        (at::params::raw (apvts, ParamIDs::key)),
      octaveParam     (at::params::raw (apvts, ParamIDs::octave)),
      chordParam      (at::params::raw (apvts, ParamIDs::chord)),
      brightnessParam (at::params::raw (apvts, ParamIDs::brightness)),
      decayParam      (at::params::raw (apvts, ParamIDs::decay)),
      mixParam        (at::params::raw (apvts, ParamIDs::mix))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice  (ParamIDs::key,   "Key",   Chords::keyNames, 0));
    layout.add (at::params::makeChoice  (ParamIDs::octave, "Octave", Chords::octaveNames, Chords::defaultOctaveIndex));
    layout.add (at::params::makeChoice  (ParamIDs::chord, "Chord", Chords::names(),  2));
    layout.add (at::params::makePercent (ParamIDs::brightness, "Brightness", 60.0f));
    layout.add (at::params::makeSeconds (ParamIDs::decay, "Decay", 0.05f, 10.0f, 1.5f));
    layout.add (at::params::makePercent (ParamIDs::mix,   "Mix", 70.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    // Lowest possible voice is C-1 detuned down; leave headroom below it.
    const auto lowestHz = (float) juce::MidiMessage::getMidiNoteInHertz (Chords::lowestRootMidiNote) * 0.9f;
    for (auto& voice : voices)
        voice.prepare (sampleRate, lowestHz);

    for (size_t v = 0; v < voices.size(); ++v)
    {
        panLeft[v].reset (sampleRate, 0.05);
        panRight[v].reset (sampleRate, 0.05);
    }

    currentKey = currentOctave = currentChord = -1;
    updateTuning();

    for (size_t v = 0; v < voices.size(); ++v)
    {
        panLeft[v].setCurrentAndTargetValue (panLeft[v].getTargetValue());
        panRight[v].setCurrentAndTargetValue (panRight[v].getTargetValue());
    }

    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, numChannels });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
    dryWet.setWetMixProportion (mixParam.load() / 100.0f);
}

void PluginProcessor::updateTuning()
{
    const auto key    = juce::jlimit (0, 11, (int) keyParam.load());
    const auto octave = juce::jlimit (0, Chords::octaveNames.size() - 1, (int) octaveParam.load());
    const auto chord  = juce::jlimit (0, (int) Chords::all.size() - 1, (int) chordParam.load());

    if (key == currentKey && octave == currentOctave && chord == currentChord)
        return;

    currentKey = key;
    currentOctave = octave;
    currentChord = chord;

    const auto root = Chords::rootMidiNote (key, octave);

    // The resonators can't ring much above a fifth of the sample rate (their loop gets too short), so
    // in the top octaves fold the chord's upper tones down an octave at a time to keep them in tune.
    const auto highestHz = (float) getSampleRate() * 0.2f;

    const auto& intervals = Chords::all[(size_t) chord].intervals;
    std::array<size_t, Chords::numVoices> order {};
    std::iota (order.begin(), order.end(), size_t { 0 });
    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return intervals[a] < intervals[b]; });

    for (size_t rank = 0; rank < order.size(); ++rank)
    {
        const auto v = order[rank];
        const auto note = (float) (root + intervals[v]) + rankDetune[rank] / 100.0f;
        auto hz = 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
        while (highestHz > 0.0f && hz > highestHz)
            hz *= 0.5f;
        voices[v].setFrequency (hz);

        // Equal-power pan.
        const auto angle = (rankPan[rank] + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        panLeft[v].setTargetValue (std::cos (angle));
        panRight[v].setTargetValue (std::sin (angle));
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
    float inputSum = 0.0f, ringSum = 0.0f, highSum = 0.0f;

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
            outL += y * panLeft[v].getNextValue();
            outR += y * panRight[v].getNextValue();
        }

        // Soft limiter: long decays on loud, tonal input can stack up a lot of energy.
        outL = std::tanh (outL * outputDrive);
        outR = std::tanh (outR * outputDrive);

        const auto ring = 0.5f * (outL + outR);
        inputSum += in * in;
        ringSum  += ring * ring;
        highSum  += (ring - lastRing) * (ring - lastRing);
        lastRing = ring;

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

    if (numSamples > 0)
    {
        const auto norm = 1.0f / (float) numSamples;
        inputLevel = std::sqrt (inputSum * norm);
        ringLevel  = std::sqrt (ringSum * norm);
        highLevel  = std::sqrt (highSum * norm);
    }

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
