#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double fadeSeconds = 0.01;
    constexpr float glideThresholdSeconds = 0.005f;

    float upTarget (int direction)   { return direction == Directions::up ? 1.0f : (direction == Directions::dual ? 0.7071f : 0.0f); }
    float downTarget (int direction) { return direction == Directions::down ? 1.0f : (direction == Directions::dual ? 0.7071f : 0.0f); }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      shiftParam      (at::params::raw (apvts, ParamIDs::shift)),
      directionParam  (at::params::raw (apvts, ParamIDs::direction)),
      blendParam      (at::params::raw (apvts, ParamIDs::blend)),
      expressionParam (at::params::raw (apvts, ParamIDs::expression)),
      glideParam      (at::params::raw (apvts, ParamIDs::glide)),
      bypassParam     (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::NormalisableRange<float> glideRange { 0.0f, 2.0f, 0.001f };
    glideRange.setSkewForCentre (0.3f);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice  (ParamIDs::shift,      "Shift", Shifts::names, Shifts::octave));
    layout.add (at::params::makeChoice  (ParamIDs::direction,  "Direction", Directions::names, Directions::up));
    layout.add (at::params::makePercent (ParamIDs::blend,      "Blend", 50.0f));
    layout.add (at::params::makePercent (ParamIDs::expression, "Expression", 100.0f));
    layout.add (at::params::makeFloat   (ParamIDs::glide,      "Glide", glideRange, 0.0f, "s", 2));
    layout.add (at::params::makeChoice  (ParamIDs::switchMode, "Footswitch", SwitchModes::names, SwitchModes::latch));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

float PluginProcessor::targetSemitones() const noexcept
{
    return Shifts::semitones ((int) shiftParam.load()) * expressionParam.load() / 100.0f;
}

void PluginProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto* shifters : { &upShifters, &downShifters })
        for (auto& shifter : *shifters)
            shifter.prepare (sampleRate);

    semitones.reset (sampleRate, 0.03);
    upGain.reset (sampleRate, 0.03);
    downGain.reset (sampleRate, 0.03);
    blendAmount.reset (sampleRate, 0.05);

    snapSmoothersToTargets();
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    const auto direction = (int) directionParam.load();
    semitones.setCurrentAndTargetValue (targetSemitones());
    upGain.setCurrentAndTargetValue (upTarget (direction));
    downGain.setCurrentAndTargetValue (downTarget (direction));
    blendAmount.setCurrentAndTargetValue (blendParam.load() / 100.0f);
    engaged = bypassParam.load() > 0.5f ? 0.0f : 1.0f;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto direction = (int) directionParam.load();
    semitones.setTargetValue (targetSemitones());
    upGain.setTargetValue (upTarget (direction));
    downGain.setTargetValue (downTarget (direction));
    blendAmount.setTargetValue (blendParam.load() / 100.0f);

    // With Glide, engaging bends the pitch from unison up to the shift (and back down on release),
    // Whammy-style; the wet path only fades in and out over the first and last quarter of the bend.
    const auto glideSeconds = glideParam.load();
    const auto gliding = glideSeconds > glideThresholdSeconds;
    const auto engagedTarget = bypassParam.load() > 0.5f ? 0.0f : 1.0f;
    const auto engageStep = (float) (1.0 / (juce::jmax ((double) glideSeconds, fadeSeconds) * sampleRate));

    const auto numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int n = 0; n < numSamples; ++n)
    {
        engaged = engagedTarget > engaged ? juce::jmin (engagedTarget, engaged + engageStep)
                                          : juce::jmax (engagedTarget, engaged - engageStep);

        const auto shift = semitones.getNextValue() * (gliding ? engaged : 1.0f);
        const auto upRatio = std::exp2 (shift / 12.0f);
        const auto downRatio = 1.0f / upRatio;
        const auto up = upGain.getNextValue();
        const auto down = downGain.getNextValue();
        const auto blend = blendAmount.getNextValue();
        const auto engagedMix = gliding ? juce::jmin (1.0f, 4.0f * engaged) : engaged;

        // Equal-power Blend: dry only fully left, wet only fully right.
        const auto dry = std::cos (juce::MathConstants<float>::halfPi * blend);
        const auto wet = std::sin (juce::MathConstants<float>::halfPi * blend);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& sample = buffer.getWritePointer (ch)[n];
            const auto shifted = up   * upShifters[(size_t) ch].process (sample, upRatio)
                               + down * downShifters[(size_t) ch].process (sample, downRatio);
            const auto mixed = dry * sample + wet * shifted;
            sample += engagedMix * (mixed - sample);
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
