#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float wetGain = 0.85f;   // the original's fixed blend: wet a touch below dry

    /** Mix crossfades with a flat centre: at noon, both paths at the original's levels. */
    float dryGain (float mix) { return juce::jmin (1.0f, 2.0f * (1.0f - mix)); }
    float wetMixGain (float mix) { return wetGain * juce::jmin (1.0f, 2.0f * mix); }

    struct OutputGains { float dryLeft, dryRight, wetLeft, wetRight; };

    OutputGains outputGains (int stereoMode, float mixPercent)
    {
        const auto dry = dryGain (mixPercent / 100.0f);
        const auto wet = wetMixGain (mixPercent / 100.0f);

        switch (stereoMode)
        {
            case StereoModes::split:    return { dry, 0.0f, 0.0f, wet };
            case StereoModes::monoSafe: return { dry, dry, wet, wet };
            default:                    return { dry, dry, wet, -wet };
        }
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float interval)
    {
        juce::NormalisableRange<float> range { lo, hi, interval };
        range.setSkewForCentre (std::sqrt (lo * hi));
        return range;
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      speedParam     (at::params::raw (apvts, ParamIDs::speed)),
      widthParam     (at::params::raw (apvts, ParamIDs::width)),
      delayTimeParam (at::params::raw (apvts, ParamIDs::delayTime)),
      mixParam       (at::params::raw (apvts, ParamIDs::mix)),
      stereoParam    (at::params::raw (apvts, ParamIDs::stereo)),
      bypassParam    (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeFloat   (ParamIDs::speed, "Speed", skewed (0.1f, 10.0f, 0.01f), 0.8f, "Hz", 2));
    layout.add (at::params::makePercent (ParamIDs::width, "Width", 50.0f));
    layout.add (at::params::makeFloat   (ParamIDs::delayTime, "Delay Time", skewed (2.0f, 15.0f, 0.1f), 6.0f, "ms", 1));
    layout.add (at::params::makePercent (ParamIDs::mix, "Mix", 50.0f));
    layout.add (at::params::makeChoice  (ParamIDs::stereo, "Stereo", StereoModes::names, StereoModes::biPhase));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Like the pedal: mono in to stereo out is the main use, but mono/mono and stereo/stereo work too.
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };

    return monoOrStereo (in) && monoOrStereo (out) && in.size() <= out.size();
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    chorus.prepare (sampleRate);

    rate.reset (sampleRate, 0.05);
    width.reset (sampleRate, 0.05);
    delayMs.reset (sampleRate, 0.1);
    engagedAmount.reset (sampleRate, 0.02);
    for (auto* gain : { &dryLeft, &dryRight, &wetLeft, &wetRight })
        gain->reset (sampleRate, 0.03);
    snapSmoothersToTargets();
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    rate.setCurrentAndTargetValue (speedParam.load());
    width.setCurrentAndTargetValue (widthParam.load() / 100.0f);
    delayMs.setCurrentAndTargetValue (delayTimeParam.load());
    engagedAmount.setCurrentAndTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);

    const auto gains = outputGains ((int) stereoParam.load(), mixParam.load());
    dryLeft.setCurrentAndTargetValue (gains.dryLeft);
    dryRight.setCurrentAndTargetValue (gains.dryRight);
    wetLeft.setCurrentAndTargetValue (gains.wetLeft);
    wetRight.setCurrentAndTargetValue (gains.wetRight);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    rate.setTargetValue (speedParam.load());
    width.setTargetValue (widthParam.load() / 100.0f);
    delayMs.setTargetValue (delayTimeParam.load());
    engagedAmount.setTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);

    // A mono output has nothing to spread across, so it always takes the plain dry + wet blend.
    const auto numOutputs = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    const auto gains = outputGains (numOutputs > 1 ? (int) stereoParam.load() : StereoModes::monoSafe,
                                    mixParam.load());
    dryLeft.setTargetValue (gains.dryLeft);
    dryRight.setTargetValue (gains.dryRight);
    wetLeft.setTargetValue (gains.wetLeft);
    wetRight.setTargetValue (gains.wetRight);

    const auto numInputs  = getTotalNumInputChannels();
    const auto numSamples = buffer.getNumSamples();
    auto* left  = buffer.getWritePointer (0);
    auto* right = numOutputs > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        // A mono input feeds both sides; a stereo input keeps its own dry image over a mono wet path.
        const auto inL = left[n];
        const auto inR = numInputs > 1 ? right[n] : inL;
        const auto wet = chorus.process (0.5f * (inL + inR), rate.getNextValue(),
                                         width.getNextValue(), delayMs.getNextValue());
        const auto engaged = engagedAmount.getNextValue();

        const auto outL = dryLeft.getNextValue() * inL + wetLeft.getNextValue() * wet;
        const auto outR = dryRight.getNextValue() * inR + wetRight.getNextValue() * wet;

        left[n] = inL + engaged * (outL - inL);
        if (right != nullptr)
            right[n] = inR + engaged * (outR - inR);
    }

    lfoPhase.store (chorus.getLfoPhase(), std::memory_order_relaxed);
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
