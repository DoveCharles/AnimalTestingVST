#pragma once

#include <at_shared/at_shared.h>

/** Builds the impulse response that Cabinet convolves with. Built-in cabs are not recordings: each
    is a magnitude response assembled from the parts a real mic'd cab adds, turned into a
    minimum-phase IR (so it rings like a speaker and adds no latency):

      speaker   box resonance and low roll-off (closed or open back), cone breakup peaks and dips,
                the steep top-end roll-off
      position  the dust cap is bright with a fizzy bump; towards the cone edge the highs fall away,
                an off-axis notch slides down and the low mids fill in
      mic       dynamic presence bump, ribbon darkness, condenser air
      distance  proximity bass fades, the breakup peaks smear as the other speakers join in, and a
                floor bounce arrives as a comb filter
      room      stereo early reflections and a short diffuse tail, so the IR becomes stereo

    A user IR file replaces the first three stages; distance and room are applied on top of it,
    measured from where it was recorded. */
namespace cab
{
    enum Model { vintage412, modern412, openBlue212, combo112, tweed410, practice18, userIr };

    inline const juce::StringArray modelNames { "4x12 Vintage 25W", "4x12 Modern 70W", "2x12 Open Alnico",
                                                "1x12 Open Combo", "4x10 Tweed", "1x8 Practice", "User IR" };

    enum MicType { dynamic, ribbon, condenser };

    inline const juce::StringArray micNames { "Dynamic", "Ribbon", "Condenser" };

    struct Settings
    {
        int model = vintage412;
        int mic = dynamic;
        float position = 0.35f;   // 0 = dust cap, 1 = cone edge
        float distance = 0.15f;   // 0 = on the grille, 1 = about a metre back
        float room = 0.0f;        // 0 to 1

        bool operator== (const Settings& other) const
        {
            return model == other.model && mic == other.mic
                && juce::exactlyEqual (position, other.position)
                && juce::exactlyEqual (distance, other.distance)
                && juce::exactlyEqual (room, other.room);
        }
    };

    /** An IR file as loaded, at its own sample rate (one or two channels). */
    struct UserImpulse
    {
        juce::AudioBuffer<float> samples;
        double sampleRate = 0.0;
        juce::String name;
    };

    /** Returns a stereo IR at sampleRate, levelled so the mids sit at roughly unity gain. If the
        model is userIr and userImpulse is empty, the first built-in cab is used instead. */
    juce::AudioBuffer<float> buildImpulse (const Settings&, double sampleRate, const UserImpulse* userImpulse);

    /** Reads an IR file: up to two channels, leading silence trimmed, at most two seconds long. */
    std::optional<UserImpulse> readImpulseFile (const juce::File&);

    /** Magnitude response of a built-in cab (no room), in dB, for drawing and tuning. */
    float responseDb (const Settings&, float hz);
}
