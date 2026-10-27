#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Gain pot range across both stages together. At 0 it's a clean boost with a little edge,
    // around noon it breaks up when you dig in, and at full it's a thick crunch.
    constexpr float minGainDb = 2.0f;
    constexpr float maxGainDb = 44.0f;

    // Tone pot: the treble shelf is flat at noon.
    constexpr float minTrebleDb = -15.0f;
    constexpr float maxTrebleDb = 10.0f;

    float gainToDb (float percent) { return juce::jmap (percent / 100.0f, minGainDb, maxGainDb); }

    /** Each half of the dual-gang pot: the stages share the gain equally. */
    float gainToStageGain (float percent)
    {
        return juce::Decibels::decibelsToGain (0.5f * gainToDb (percent));
    }

    /** Output make-down that follows the Gain pot, fitted so a typical guitar level (-18 dBFS)
        comes out a little louder as Gain goes up (about +3 dB at full) instead of the real pedal's
        +18 dB. Level then sets the volume, and Gain mostly sets the dirt. */
    float levelToGain (float levelDb, float gainPercent)
    {
        const auto compensationDb = -18.0f * std::tanh ((gainToDb (gainPercent) - minGainDb) / 18.0f);
        return juce::Decibels::decibelsToGain (levelDb + compensationDb);
    }

    float toneToTrebleGain (float percent)
    {
        const auto t = percent / 100.0f;
        const auto db = t < 0.5f ? juce::jmap (t * 2.0f, minTrebleDb, 0.0f)
                                 : juce::jmap (t * 2.0f - 1.0f, 0.0f, maxTrebleDb);
        return juce::Decibels::decibelsToGain (db);
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      levelParam  (at::params::raw (apvts, ParamIDs::level)),
      toneParam   (at::params::raw (apvts, ParamIDs::tone)),
      gainParam   (at::params::raw (apvts, ParamIDs::gain)),
      customParam (at::params::raw (apvts, ParamIDs::custom)),
      bypassParam (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeDecibels (ParamIDs::level, "Level", -24.0f, 12.0f, 0.0f));
    layout.add (at::params::makePercent  (ParamIDs::tone,  "Tone", 50.0f));
    layout.add (at::params::makePercent  (ParamIDs::gain,  "Gain", 30.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        at::params::id (ParamIDs::custom), "Mode", false,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction ([] (bool custom, int) { return custom ? "Custom" : "Standard"; })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.trim().startsWithIgnoreCase ("c") || text.getIntValue() != 0;
            })));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

void PluginProcessor::prepareCircuits (bool custom)
{
    customMode = custom;
    for (auto& circuit : circuits)
        circuit.prepare (oversampledRate, custom);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
    oversampledRate = sampleRate * (double) oversampling.getOversamplingFactor();

    prepareCircuits (customParam.load() > 0.5f);
    for (auto& circuit : circuits)
        circuit.reset();

    stageGain.reset (oversampledRate, 0.05);
    stageGain.setCurrentAndTargetValue (gainToStageGain (gainParam.load()));
    trebleGain.reset (oversampledRate, 0.05);
    trebleGain.setCurrentAndTargetValue (toneToTrebleGain (toneParam.load()));
    levelGain.reset (sampleRate, 0.05);
    levelGain.setCurrentAndTargetValue (levelToGain (levelParam.load(), gainParam.load()));

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

    // The mode switch swaps capacitor values, so only the filter corners change; the state carries on.
    if (const auto custom = customParam.load() > 0.5f; custom != customMode)
        prepareCircuits (custom);

    stageGain.setTargetValue (gainToStageGain (gainParam.load()));
    trebleGain.setTargetValue (toneToTrebleGain (toneParam.load()));
    levelGain.setTargetValue (levelToGain (levelParam.load(), gainParam.load()));
    bypassMix.setWetMixProportion (bypassParam.load() > 0.5f ? 0.0f : 1.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    bypassMix.pushDrySamples (block);

    auto oversampled = oversampling.processSamplesUp (block);
    const auto numChannels = juce::jmin ((int) oversampled.getNumChannels(), maxChannels);
    const auto numOversampled = (int) oversampled.getNumSamples();

    for (int n = 0; n < numOversampled; ++n)
    {
        const auto gain = stageGain.getNextValue();
        const auto treble = trebleGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = oversampled.getChannelPointer ((size_t) ch);
            data[n] = circuits[(size_t) ch].process (data[n], gain, treble);
        }
    }

    oversampling.processSamplesDown (block);

    const auto numSamples = (int) block.getNumSamples();
    for (int n = 0; n < numSamples; ++n)
    {
        const auto gain = levelGain.getNextValue();
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
