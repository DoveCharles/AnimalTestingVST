#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      modeParam   (at::params::raw (apvts, ParamIDs::mode)),
      amountParam (at::params::raw (apvts, ParamIDs::amount)),
      outputParam (at::params::raw (apvts, ParamIDs::output))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::mode, "Mode", Modes::names, 0));
    layout.add (at::params::makePercent (ParamIDs::amount, "Amount", 50.0f));
    layout.add (at::params::makeDecibels (ParamIDs::output, "Output", -12.0f, 6.0f, 0.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maximizer.setSettings ((int) modeParam.load(), amountParam.load() / 100.0f, outputParam.load());
    maximizer.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (maximizer.getLatency());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    maximizer.setSettings ((int) modeParam.load(), amountParam.load() / 100.0f, outputParam.load());
    maximizer.process (buffer, getTotalNumOutputChannels());
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
