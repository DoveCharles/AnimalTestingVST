#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> millisecondRange (float lo, float hi)
    {
        juce::NormalisableRange<float> range { lo, hi, 0.1f };
        range.setSkewForCentre (std::sqrt (lo * hi));
        return range;
    }

    const juce::StringArray weightNames { "100 Hz", "300 Hz", "1 kHz", "3 kHz", "8 kHz" };
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      depthParam       (at::params::raw (apvts, ParamIDs::depth)),
      sharpnessParam   (at::params::raw (apvts, ParamIDs::sharpness)),
      selectivityParam (at::params::raw (apvts, ParamIDs::selectivity)),
      attackParam      (at::params::raw (apvts, ParamIDs::attack)),
      releaseParam     (at::params::raw (apvts, ParamIDs::release)),
      stereoParam      (at::params::raw (apvts, ParamIDs::stereo)),
      mixParam         (at::params::raw (apvts, ParamIDs::mix)),
      deltaParam       (at::params::raw (apvts, ParamIDs::delta)),
      outputParam      (at::params::raw (apvts, ParamIDs::output))
{
    for (size_t i = 0; i < weightParams.size(); ++i)
        weightParams[i] = &at::params::raw (apvts, ParamIDs::weights[i]);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeFloat   (ParamIDs::depth, "Depth", { 0.0f, 20.0f, 0.01f }, 6.0f, "dB", 1));
    layout.add (at::params::makePercent (ParamIDs::sharpness, "Sharpness", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::selectivity, "Selectivity", 40.0f));
    layout.add (at::params::makeFloat   (ParamIDs::attack, "Attack", millisecondRange (0.5f, 100.0f), 5.0f, "ms", 1));
    layout.add (at::params::makeFloat   (ParamIDs::release, "Release", millisecondRange (10.0f, 1000.0f), 120.0f, "ms", 0));
    layout.add (at::params::makeChoice  (ParamIDs::stereo, "Stereo", StereoModes::names, StereoModes::stereo));
    for (size_t i = 0; i < ParamIDs::weights.size(); ++i)
        layout.add (at::params::makeFloat (ParamIDs::weights[i], "Weight " + weightNames[(int) i], { 0.0f, 200.0f, 0.1f }, 100.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::mix, "Mix", 100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::delta), "Delta", false));
    layout.add (at::params::makeDecibels (ParamIDs::output, "Output", -24.0f, 12.0f, 0.0f));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    suppressor.prepare (sampleRate);
    setLatencySamples (suppressor.getLatency());
    gain.reset (sampleRate, 0.05);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam.load()));
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ResonanceSuppressor::Settings settings;
    settings.depthDb = depthParam.load();
    settings.sharpness = sharpnessParam.load() / 100.0f;
    settings.selectivity = selectivityParam.load() / 100.0f;
    settings.attackSeconds = attackParam.load() * 0.001f;
    settings.releaseSeconds = releaseParam.load() * 0.001f;
    for (size_t i = 0; i < weightParams.size(); ++i)
        settings.weights[i] = weightParams[i]->load() / 100.0f;
    settings.mix = mixParam.load() / 100.0f;
    settings.delta = deltaParam.load() > 0.5f;
    suppressor.setSettings (settings);
    gain.setTargetValue (juce::Decibels::decibelsToGain (outputParam.load()));

    const auto numChannels = juce::jmin (maxChannels, buffer.getNumChannels());
    const auto midSide = numChannels == 2 && (int) stereoParam.load() == StereoModes::midSide;
    auto* const* data = buffer.getArrayOfWritePointers();

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        std::array<float, maxChannels> x {};
        for (int ch = 0; ch < numChannels; ++ch)
            x[(size_t) ch] = data[ch][n];

        if (midSide)
            x = { 0.5f * (x[0] + x[1]), 0.5f * (x[0] - x[1]) };

        suppressor.process (x.data(), numChannels);

        if (midSide)
            x = { x[0] + x[1], x[0] - x[1] };

        const auto g = gain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][n] = g * x[(size_t) ch];
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
