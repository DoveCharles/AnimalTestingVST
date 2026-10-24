#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Sustain pot range. At 0 the pedal is a gritty overdrive, past noon it's the full wall of fuzz.
    constexpr float minSustainDb = -20.0f;
    constexpr float maxSustainDb = 24.0f;

    // Brings the circuit's output to roughly the input's level with Volume at 0 dB.
    constexpr float outputTrim = 0.16f;

    float sustainToGain (float percent)
    {
        return juce::Decibels::decibelsToGain (juce::jmap (percent / 100.0f, minSustainDb, maxSustainDb));
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      volumeParam  (at::params::raw (apvts, ParamIDs::volume)),
      toneParam    (at::params::raw (apvts, ParamIDs::tone)),
      sustainParam (at::params::raw (apvts, ParamIDs::sustain)),
      bypassParam  (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeDecibels (ParamIDs::volume,  "Volume", -36.0f, 12.0f, 0.0f));
    layout.add (at::params::makePercent  (ParamIDs::tone,    "Tone", 50.0f));
    layout.add (at::params::makePercent  (ParamIDs::sustain, "Sustain", 60.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
    const auto oversampledRate = sampleRate * (double) oversampling.getOversamplingFactor();

    for (auto& circuit : circuits)
        circuit.prepare (oversampledRate);

    sustainGain.reset (oversampledRate, 0.05);
    sustainGain.setCurrentAndTargetValue (sustainToGain (sustainParam.load()));
    toneAmount.reset (oversampledRate, 0.05);
    toneAmount.setCurrentAndTargetValue (toneParam.load() / 100.0f);
    volumeGain.reset (sampleRate, 0.05);
    volumeGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));

    const auto latency = oversampling.getLatencyInSamples();
    setLatencySamples ((int) latency);

    bypassMix.prepare (spec);
    bypassMix.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    bypassMix.setWetLatency (latency);
    bypassMix.setWetMixProportion (bypassParam.load() > 0.5f ? 0.0f : 1.0f);
    bypassMix.reset();
}

void PluginProcessor::releaseResources()
{
    oversampling.reset();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    sustainGain.setTargetValue (sustainToGain (sustainParam.load()));
    toneAmount.setTargetValue (toneParam.load() / 100.0f);
    volumeGain.setTargetValue (juce::Decibels::decibelsToGain (volumeParam.load()));
    bypassMix.setWetMixProportion (bypassParam.load() > 0.5f ? 0.0f : 1.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    bypassMix.pushDrySamples (block);

    auto oversampled = oversampling.processSamplesUp (block);
    const auto numChannels = juce::jmin ((int) oversampled.getNumChannels(), maxChannels);
    const auto numOversampled = (int) oversampled.getNumSamples();

    for (int n = 0; n < numOversampled; ++n)
    {
        const auto sustain = sustainGain.getNextValue();
        const auto tone = toneAmount.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = oversampled.getChannelPointer ((size_t) ch);
            data[n] = circuits[(size_t) ch].process (data[n], sustain, tone);
        }
    }

    oversampling.processSamplesDown (block);

    const auto numSamples = (int) block.getNumSamples();
    for (int n = 0; n < numSamples; ++n)
    {
        const auto gain = volumeGain.getNextValue() * outputTrim;
        for (int ch = 0; ch < (int) block.getNumChannels(); ++ch)
            block.getChannelPointer ((size_t) ch)[n] *= gain;
    }

    bypassMix.mixWetSamples (block);
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
