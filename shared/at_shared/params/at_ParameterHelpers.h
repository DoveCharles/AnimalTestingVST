#pragma once

namespace at::params
{
    /** Bump this when adding parameters to an existing plugin, so hosts keep automation
        working (see juce::ParameterID). New parameters get the new hint; old ones keep theirs. */
    inline constexpr int initialVersionHint = 1;

    inline juce::ParameterID id (const char* paramId, int versionHint = initialVersionHint)
    {
        return { paramId, versionHint };
    }

    inline auto makeFloat (const char* paramId, const juce::String& name,
                           juce::NormalisableRange<float> range, float defaultValue,
                           const juce::String& unit = {}, int decimals = 1)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            id (paramId), name, range, defaultValue,
            juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([decimals, unit] (float v, int)
                {
                    return juce::String (v, decimals) + (unit.isEmpty() ? "" : " " + unit);
                }));
    }

    inline auto makeDecibels (const char* paramId, const juce::String& name,
                              float minDb, float maxDb, float defaultDb)
    {
        return makeFloat (paramId, name, { minDb, maxDb, 0.01f }, defaultDb, "dB", 1);
    }

    inline auto makePercent (const char* paramId, const juce::String& name, float defaultPercent)
    {
        return makeFloat (paramId, name, { 0.0f, 100.0f, 0.1f }, defaultPercent, "%", 0);
    }

    /** Frequency range with a perceptually even (log-like) skew around the geometric centre. */
    inline auto makeFrequency (const char* paramId, const juce::String& name,
                               float minHz, float maxHz, float defaultHz)
    {
        juce::NormalisableRange<float> range { minHz, maxHz, 1.0f };
        range.setSkewForCentre (std::sqrt (minHz * maxHz));
        return makeFloat (paramId, name, range, defaultHz, "Hz", 0);
    }

    /** Typed, non-owning handle to an APVTS raw parameter value; read it on the audio thread. */
    inline std::atomic<float>& raw (juce::AudioProcessorValueTreeState& state, const char* paramId)
    {
        auto* value = state.getRawParameterValue (paramId);
        jassert (value != nullptr);   // unknown parameter ID
        return *value;
    }
}
