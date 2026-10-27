#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double modeFadeSeconds = 0.02;
    constexpr int controlInterval = 32;   // samples between engine coefficient updates

    /** Mix is a pedal-style blend: the dry stays at unity up to noon, where the wet reaches unity,
        then the dry fades out, leaving the wet alone at full (as on the RV-7's Reverse). */
    float dryGainFor (float mixPercent) { return juce::jmin (1.0f, 2.0f * (1.0f - mixPercent / 100.0f)); }
    float wetGainFor (float mixPercent) { return juce::jmin (1.0f, 2.0f * mixPercent / 100.0f); }

    juce::String timeText (float seconds)
    {
        if (seconds < 1.0f)
            return juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
        return juce::String (seconds, 2) + " s";
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      modeParam     (at::params::raw (apvts, ParamIDs::mode)),
      timeParam     (at::params::raw (apvts, ParamIDs::time)),
      syncParam     (at::params::raw (apvts, ParamIDs::sync)),
      divisionParam (at::params::raw (apvts, ParamIDs::division)),
      repeatsParam  (at::params::raw (apvts, ParamIDs::repeats)),
      toneParam     (at::params::raw (apvts, ParamIDs::tone)),
      mixParam      (at::params::raw (apvts, ParamIDs::mix)),
      tailsParam    (at::params::raw (apvts, ParamIDs::tails)),
      bypassParam   (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::NormalisableRange<float> timeRange { 0.05f, rv::maxWindowSeconds, 0.001f };
    timeRange.setSkewForCentre (0.4f);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::mode, "Mode", Modes::names, ReverseModes::reverb));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        at::params::id (ParamIDs::time), "Time", timeRange, 0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction ([] (float v, int) { return timeText (v); })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto value = text.getFloatValue();
                return text.containsIgnoreCase ("ms") ? value * 0.001f : value;
            })));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::sync), "Sync", false));
    layout.add (at::params::makeChoice (ParamIDs::division, "Division", Divisions::names, 8));
    layout.add (at::params::makePercent (ParamIDs::repeats, "Repeats", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::tone, "Tone", 60.0f));
    layout.add (at::params::makePercent (ParamIDs::mix, "Mix", 50.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::tails), "Tails", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in and out, plus mono in to stereo out and plain mono.
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };

    return monoOrStereo (in) && monoOrStereo (out) && in.size() <= out.size();
}

float PluginProcessor::windowSeconds (bool synced, float timeSeconds, int division, double tempo) noexcept
{
    if (! synced)
        return timeSeconds;

    const auto beats = Divisions::beats[(size_t) juce::jlimit (0, (int) Divisions::beats.size() - 1, division)];
    return juce::jlimit (0.02f, rv::maxWindowSeconds, (float) (beats * 60.0 / tempo));
}

float PluginProcessor::targetWindow() const noexcept
{
    return windowSeconds (syncParam.load() > 0.5f, timeParam.load(), (int) divisionParam.load(), bpm);
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    activeMode = (int) modeParam.load();
    switchingMode = false;
    modeFade = 1.0f;
    modeFadeStep = (float) (1.0 / (modeFadeSeconds * sampleRate));

    engine.prepare (sampleRate);
    engine.setMode (activeMode);

    // Time glides rather than jumps: the Reverb swell stretches smoothly; Delay takes the new length on its next pass.
    window.reset (sampleRate, 0.15);
    repeats.reset (sampleRate, 0.05);
    tone.reset (sampleRate, 0.05);
    wetGain.reset (sampleRate, 0.05);
    dryGain.reset (sampleRate, 0.05);
    engagedAmount.reset (sampleRate, 0.02);
    wetEnabled.reset (sampleRate, 0.02);
    snapSmoothersToTargets();

    engine.setControls (window.getTargetValue(), repeats.getTargetValue(), tone.getTargetValue());
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    const auto engaged = bypassParam.load() < 0.5f;
    window.setCurrentAndTargetValue (targetWindow());
    repeats.setCurrentAndTargetValue (repeatsParam.load() / 100.0f);
    tone.setCurrentAndTargetValue (toneParam.load() / 100.0f);
    wetGain.setCurrentAndTargetValue (wetGainFor (mixParam.load()));
    dryGain.setCurrentAndTargetValue (dryGainFor (mixParam.load()));
    engagedAmount.setCurrentAndTargetValue (engaged ? 1.0f : 0.0f);
    wetEnabled.setCurrentAndTargetValue (engaged || tailsParam.load() > 0.5f ? 1.0f : 0.0f);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
                bpm = *hostBpm;

    const auto requestedMode = (int) modeParam.load();
    if (requestedMode != activeMode)
        switchingMode = true;

    // Bypassed with Tails on, nothing new goes in but whatever is playing is heard out.
    const auto engaged = bypassParam.load() < 0.5f;
    window.setTargetValue (targetWindow());
    repeats.setTargetValue (repeatsParam.load() / 100.0f);
    tone.setTargetValue (toneParam.load() / 100.0f);
    wetGain.setTargetValue (wetGainFor (mixParam.load()));
    dryGain.setTargetValue (dryGainFor (mixParam.load()));
    engagedAmount.setTargetValue (engaged ? 1.0f : 0.0f);
    wetEnabled.setTargetValue (engaged || tailsParam.load() > 0.5f ? 1.0f : 0.0f);

    const auto numInputs  = getTotalNumInputChannels();
    const auto numOutputs = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    const auto numSamples = buffer.getNumSamples();
    auto* left  = buffer.getWritePointer (0);
    auto* right = numOutputs > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        if (switchingMode)
        {
            modeFade -= modeFadeStep;
            if (modeFade <= 0.0f)
            {
                modeFade = 0.0f;
                switchingMode = false;
                activeMode = requestedMode;
                engine.reset();
                engine.setMode (activeMode);
            }
        }
        else if (modeFade < 1.0f)
        {
            modeFade = juce::jmin (1.0f, modeFade + modeFadeStep);
        }

        const auto currentWindow = window.getNextValue();
        const auto currentRepeats = repeats.getNextValue();
        const auto currentTone = tone.getNextValue();
        if (n % controlInterval == 0)
            engine.setControls (currentWindow, currentRepeats, currentTone);

        const auto inL = left[n];
        const auto inR = numInputs > 1 && right != nullptr ? right[n] : inL;
        const auto engagedNow = engagedAmount.getNextValue();

        float wetL, wetR;
        engine.process (engagedNow * inL, engagedNow * inR, wetL, wetR);

        const auto wet = wetGain.getNextValue() * modeFade * wetEnabled.getNextValue();
        const auto dry = 1.0f + engagedNow * (dryGain.getNextValue() - 1.0f);

        // A mono output gets both sides of the wet signal.
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
