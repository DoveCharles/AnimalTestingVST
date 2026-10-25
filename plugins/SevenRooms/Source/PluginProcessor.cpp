#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double typeFadeSeconds = 0.02;
    constexpr int controlInterval = 32;   // samples between reverb coefficient updates

    /** Level runs from no reverb, through unity just past two o'clock, to a little over +6 dB. */
    float levelGain (float levelPercent)
    {
        const auto l = levelPercent / 100.0f;
        return 2.2f * l * l;
    }

    /** Reverse has no dry signal on the original; every other type keeps the dry path at unity. */
    float dryGainFor (int type) { return type == ReverbTypes::reverse ? 0.0f : 1.0f; }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      levelParam      (at::params::raw (apvts, ParamIDs::level)),
      livelinessParam (at::params::raw (apvts, ParamIDs::liveliness)),
      decayParam      (at::params::raw (apvts, ParamIDs::decay)),
      typeParam       (at::params::raw (apvts, ParamIDs::type)),
      tailsParam      (at::params::raw (apvts, ParamIDs::tails)),
      bypassParam     (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makePercent (ParamIDs::level,      "Level", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::liveliness, "Liveliness", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::decay,      "Decay", 50.0f));
    layout.add (at::params::makeChoice  (ParamIDs::type,       "Reverb Type", ReverbTypes::names, ReverbTypes::hall));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::tails), "Tails", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in and out like the pedal, plus mono in to stereo out and plain mono.
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
    activeType = (int) typeParam.load();
    switchingType = false;
    typeFade = 1.0f;
    typeFadeStep = (float) (1.0 / (typeFadeSeconds * sampleRate));

    reverb.prepare (sampleRate);
    reverb.setType (activeType);

    decay.reset (sampleRate, 0.1);
    liveliness.reset (sampleRate, 0.05);
    wetGain.reset (sampleRate, 0.05);
    dryGain.reset (sampleRate, typeFadeSeconds);
    engagedAmount.reset (sampleRate, 0.02);
    wetEnabled.reset (sampleRate, 0.02);
    snapSmoothersToTargets();

    reverb.setControls (decay.getTargetValue(), liveliness.getTargetValue());
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    const auto engaged = bypassParam.load() < 0.5f;
    decay.setCurrentAndTargetValue (decayParam.load() / 100.0f);
    liveliness.setCurrentAndTargetValue (livelinessParam.load() / 100.0f);
    wetGain.setCurrentAndTargetValue (levelGain (levelParam.load()));
    dryGain.setCurrentAndTargetValue (dryGainFor (activeType));
    engagedAmount.setCurrentAndTargetValue (engaged ? 1.0f : 0.0f);
    wetEnabled.setCurrentAndTargetValue (engaged || tailsParam.load() > 0.5f ? 1.0f : 0.0f);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto requestedType = (int) typeParam.load();
    if (requestedType != activeType)
        switchingType = true;

    // Bypassed with Tails on, nothing new goes in but whatever is ringing is heard out.
    const auto engaged = bypassParam.load() < 0.5f;
    decay.setTargetValue (decayParam.load() / 100.0f);
    liveliness.setTargetValue (livelinessParam.load() / 100.0f);
    wetGain.setTargetValue (levelGain (levelParam.load()));
    dryGain.setTargetValue (dryGainFor (switchingType ? requestedType : activeType));
    engagedAmount.setTargetValue (engaged ? 1.0f : 0.0f);
    wetEnabled.setTargetValue (engaged || tailsParam.load() > 0.5f ? 1.0f : 0.0f);

    const auto numInputs  = getTotalNumInputChannels();
    const auto numOutputs = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    const auto numSamples = buffer.getNumSamples();
    auto* left  = buffer.getWritePointer (0);
    auto* right = numOutputs > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        if (switchingType)
        {
            typeFade -= typeFadeStep;
            if (typeFade <= 0.0f)
            {
                typeFade = 0.0f;
                switchingType = false;
                activeType = requestedType;
                reverb.reset();
                reverb.setType (activeType);
            }
        }
        else if (typeFade < 1.0f)
        {
            typeFade = juce::jmin (1.0f, typeFade + typeFadeStep);
        }

        const auto currentDecay = decay.getNextValue();
        const auto currentLiveliness = liveliness.getNextValue();
        if (n % controlInterval == 0)
            reverb.setControls (currentDecay, currentLiveliness);

        const auto inL = left[n];
        const auto inR = numInputs > 1 ? right[n] : inL;
        const auto engagedNow = engagedAmount.getNextValue();

        float wetL, wetR;
        reverb.process (engagedNow * inL, engagedNow * inR, wetL, wetR);

        const auto wet = wetGain.getNextValue() * typeFade * wetEnabled.getNextValue();
        const auto dry = 1.0f + engagedNow * (dryGain.getNextValue() - 1.0f);

        // A mono output gets both sides of the reverb.
        if (right == nullptr)
        {
            left[n] = dry * inL + wet * 0.5f * (wetL + wetR);
            continue;
        }

        left[n]  = dry * inL + wet * wetL;
        right[n] = dry * inR + wet * wetR;
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
