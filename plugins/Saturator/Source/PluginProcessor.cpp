#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      driveParam (at::params::raw (apvts, ParamIDs::drive)),
      toneParam  (at::params::raw (apvts, ParamIDs::tone)),
      mixParam   (at::params::raw (apvts, ParamIDs::mix))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeDecibels  (ParamIDs::drive, "Drive", 0.0f, 36.0f, 6.0f));
    layout.add (at::params::makeFrequency (ParamIDs::tone,  "Tone", 1000.0f, 20000.0f, 18000.0f));
    layout.add (at::params::makePercent   (ParamIDs::mix,   "Mix", 100.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    driveGain.reset (sampleRate, 0.05);
    driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (driveParam.load()));

    toneCutoff.reset (sampleRate, 0.05);
    toneCutoff.setCurrentAndTargetValue (toneParam.load());

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);
    toneFilter.setCutoffFrequency (toneCutoff.getCurrentValue());

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetMixProportion (mixParam.load() / 100.0f);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    driveGain.setTargetValue (juce::Decibels::decibelsToGain (driveParam.load()));
    toneCutoff.setTargetValue (toneParam.load());
    dryWet.setWetMixProportion (mixParam.load() / 100.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    dryWet.pushDrySamples (block);

    const auto numChannels = (int) block.getNumChannels();
    const auto numSamples  = (int) block.getNumSamples();

    for (int n = 0; n < numSamples; ++n)
    {
        const auto gain = driveGain.getNextValue();
        // Half compensation: unity-ish peaks at low drive, loudness rises gently as drive increases.
        const auto makeup = 1.0f / std::sqrt (gain);

        if (toneCutoff.isSmoothing())
            toneFilter.setCutoffFrequency (toneCutoff.getNextValue());

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = block.getChannelPointer ((size_t) ch);
            const auto shaped = at::dsp::Waveshapers::tanhClip (data[n] * gain) * makeup;
            data[n] = toneFilter.processSample (ch, shaped);
        }
    }

    toneFilter.snapToZero();
    dryWet.mixWetSamples (block);
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
