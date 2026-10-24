#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double modeFadeSeconds = 0.015;

    float targetDelaySamples (int mode, float finePercent, double sampleRate)
    {
        const auto mode0 = Modes::isPitch (mode) ? Modes::delay500 : mode;
        return Modes::delayMs (mode0, finePercent / 100.0f) * 0.001f * (float) sampleRate;
    }

    float targetSemitones (int mode, float finePercent)
    {
        return Modes::isPitch (mode) ? Modes::semitones (mode, finePercent / 100.0f) : 0.0f;
    }

    /** The Balance pot crossfades with a flat centre: both paths at full level at noon. */
    float dryGain (float balance) { return juce::jmin (1.0f, 2.0f * (1.0f - balance)); }
    float wetGain (float balance) { return juce::jmin (1.0f, 2.0f * balance); }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      balanceParam  (at::params::raw (apvts, ParamIDs::balance)),
      feedbackParam (at::params::raw (apvts, ParamIDs::feedback)),
      fineParam     (at::params::raw (apvts, ParamIDs::fine)),
      modeParam     (at::params::raw (apvts, ParamIDs::mode)),
      bypassParam   (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makePercent (ParamIDs::balance,  "Balance", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::feedback, "Feedback", 30.0f));
    layout.add (at::params::makePercent (ParamIDs::fine,     "Fine-Manual", 50.0f));
    layout.add (at::params::makeChoice  (ParamIDs::mode,     "Mode", Modes::names, Modes::delay500));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

void PluginProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& engine : engines)
        engine.prepare (sampleRate);

    delaySamples.reset (sampleRate, 0.25);
    semitones.reset (sampleRate, 0.05);
    feedbackAmount.reset (sampleRate, 0.05);
    balanceAmount.reset (sampleRate, 0.05);
    engagedAmount.reset (sampleRate, 0.02);

    activeMode = (int) modeParam.load();
    switchingMode = false;
    modeFade = 1.0f;
    modeFadeStep = (float) (1.0 / (modeFadeSeconds * sampleRate));

    snapSmoothersToTargets();
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    const auto fine = fineParam.load();
    delaySamples.setCurrentAndTargetValue (targetDelaySamples (activeMode, fine, sampleRate));
    semitones.setCurrentAndTargetValue (targetSemitones (activeMode, fine));
    feedbackAmount.setCurrentAndTargetValue (feedbackParam.load() / 100.0f);
    balanceAmount.setCurrentAndTargetValue (balanceParam.load() / 100.0f);
    engagedAmount.setCurrentAndTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto requestedMode = (int) modeParam.load();
    if (requestedMode != activeMode)
        switchingMode = true;

    const auto fine = fineParam.load();
    delaySamples.setTargetValue (targetDelaySamples (activeMode, fine, sampleRate));
    semitones.setTargetValue (targetSemitones (activeMode, fine));
    feedbackAmount.setTargetValue (feedbackParam.load() / 100.0f);
    balanceAmount.setTargetValue (balanceParam.load() / 100.0f);
    engagedAmount.setTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);

    const auto numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const auto numSamples = buffer.getNumSamples();

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
                for (auto& engine : engines)
                    engine.reset();

                delaySamples.setCurrentAndTargetValue (targetDelaySamples (activeMode, fine, sampleRate));
                semitones.setCurrentAndTargetValue (targetSemitones (activeMode, fine));
            }
        }
        else if (modeFade < 1.0f)
        {
            modeFade = juce::jmin (1.0f, modeFade + modeFadeStep);
        }

        const auto delay    = delaySamples.getNextValue();
        const auto ratio    = std::exp2 (semitones.getNextValue() / 12.0f);
        const auto feedback = feedbackAmount.getNextValue();
        const auto balance  = balanceAmount.getNextValue();
        const auto engaged  = engagedAmount.getNextValue();
        const auto pitchMode = Modes::isPitch (activeMode);

        const auto dry = dryGain (balance);
        const auto wet = wetGain (balance) * modeFade;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& sample = buffer.getWritePointer (ch)[n];
            auto& engine = engines[(size_t) ch];
            const auto effect = pitchMode ? engine.processPitch (sample, ratio, feedback)
                                          : engine.processDelay (sample, delay, feedback);
            const auto mixed = dry * sample + wet * effect;
            sample += engaged * (mixed - sample);
        }
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
