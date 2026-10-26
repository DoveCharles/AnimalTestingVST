#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** A knob that reads in dB along a measured curve (knob positions 0, .1, ... 1),
        interpolated linearly both ways. */
    juce::NormalisableRange<float> tableRange (std::array<float, 11> table)
    {
        return { table.front(), table.back(),
                 [table] (float, float, float proportion)
                 {
                     const auto pos = juce::jlimit (0.0f, 1.0f, proportion) * 10.0f;
                     const auto i = juce::jmin ((int) pos, 9);
                     return table[(size_t) i] + (pos - (float) i) * (table[(size_t) i + 1] - table[(size_t) i]);
                 },
                 [table] (float, float, float value)
                 {
                     value = juce::jlimit (table.front(), table.back(), value);
                     size_t i = 0;
                     while (i < 9 && value > table[i + 1])
                         ++i;
                     return ((float) i + (value - table[i]) / (table[i + 1] - table[i])) / 10.0f;
                 } };
    }

    constexpr std::array<float, 11> noiseDb { -70.0f, -57.4f, -46.6f, -37.3f, -29.3f, -22.5f, -16.5f, -11.4f, -7.03f, -3.25f, 0.0f };
    constexpr std::array<float, 11> outputDb { -70.0f, -55.7f, -43.3f, -32.7f, -23.5f, -15.7f, -8.89f, -3.06f, 1.96f, 6.28f, 10.0f };

    std::unique_ptr<juce::AudioParameterBool> makeSwitch (const char* paramId, const juce::String& name, bool on)
    {
        return std::make_unique<juce::AudioParameterBool> (at::params::id (paramId), name, on);
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      driveParam       (at::params::raw (apvts, ParamIDs::drive)),
      boostParam       (at::params::raw (apvts, ParamIDs::boost)),
      toneParam        (at::params::raw (apvts, ParamIDs::tone)),
      toneEnableParam  (at::params::raw (apvts, ParamIDs::toneEnable)),
      filterParam      (at::params::raw (apvts, ParamIDs::filter)),
      wowParam         (at::params::raw (apvts, ParamIDs::wow)),
      flutterParam     (at::params::raw (apvts, ParamIDs::flutter)),
      wearParam        (at::params::raw (apvts, ParamIDs::wear)),
      noiseParam       (at::params::raw (apvts, ParamIDs::noise)),
      mechanicalParam  (at::params::raw (apvts, ParamIDs::mechanical)),
      stereoWidthParam (at::params::raw (apvts, ParamIDs::stereoWidth)),
      outputParam      (at::params::raw (apvts, ParamIDs::output)),
      tapeStopParam    (at::params::raw (apvts, ParamIDs::tapeStop)),
      stopSpeedParam   (at::params::raw (apvts, ParamIDs::stopSpeed)),
      catchUpParam     (at::params::raw (apvts, ParamIDs::catchUp))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makePercent (ParamIDs::drive, "Drive", 0.0f));
    layout.add (makeSwitch (ParamIDs::boost, "Boost", false));
    layout.add (at::params::makePercent (ParamIDs::tone, "Tone", 50.0f));
    layout.add (makeSwitch (ParamIDs::toneEnable, "Tone Enable", true));
    layout.add (at::params::makeFloat (ParamIDs::filter, "Filter", { -100.0f, 100.0f, 1.0f }, 0.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::wow, "Wow", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::flutter, "Flutter", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::wear, "Wear", 0.0f));
    layout.add (at::params::makeFloat (ParamIDs::noise, "Noise", tableRange (noiseDb), -70.0f, "dB", 1));
    layout.add (at::params::makeFloat (ParamIDs::mechanical, "Mechanical Noise", tableRange (noiseDb), -70.0f, "dB", 1));
    layout.add (makeSwitch (ParamIDs::stereoWidth, "Stereo Width", false));
    layout.add (at::params::makeFloat (ParamIDs::output, "Output", tableRange (outputDb), 0.0f, "dB", 1));
    layout.add (makeSwitch (ParamIDs::tapeStop, "Tape Stop", false));
    layout.add (at::params::makeChoice (ParamIDs::stopSpeed, "Stop Speed", StopSpeeds::names, 3));
    layout.add (at::params::makeChoice (ParamIDs::catchUp, "Tape Catch-up", CatchUps::names, CatchUps::instant));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    tape.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (tape.getLatency());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
                bpm = *hostBpm;

    TapeMachine::Settings settings;
    settings.drive = driveParam.load() / 100.0f;
    settings.boost = boostParam.load() > 0.5f;
    settings.tone = toneParam.load() / 100.0f;
    settings.toneEnabled = toneEnableParam.load() > 0.5f;
    settings.filter = filterParam.load() / 100.0f;
    settings.wow = wowParam.load() / 100.0f;
    settings.flutter = flutterParam.load() / 100.0f;
    settings.wear = wearParam.load() / 100.0f;
    settings.noiseDb = noiseParam.load();
    settings.mechanicalDb = mechanicalParam.load();
    settings.stereoWidth = stereoWidthParam.load() > 0.5f;
    settings.outputDb = outputParam.load();
    settings.tapeStop = tapeStopParam.load() > 0.5f;
    // Bars of 4/4 at the host tempo.
    settings.stopSeconds = StopSpeeds::bars[(size_t) juce::jlimit (0, 5, (int) stopSpeedParam.load())] * 4.0 * 60.0 / bpm;
    settings.fastForward = (int) catchUpParam.load() == CatchUps::fastForward;
    tape.setSettings (settings);

    tape.process (buffer, getTotalNumOutputChannels());
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
