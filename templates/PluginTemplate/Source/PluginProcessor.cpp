#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      gainParam (at::params::raw (apvts, ParamIDs::gain))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeDecibels (ParamIDs::gain, "Gain", -24.0f, 24.0f, 0.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    gain.reset (sampleRate, 0.05);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam.load()));
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    gain.setTargetValue (juce::Decibels::decibelsToGain (gainParam.load()));
    gain.applyGain (buffer, buffer.getNumSamples());
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
