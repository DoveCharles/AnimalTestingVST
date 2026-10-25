#pragma once

#include "CabModel.h"
#include "SpeakerDrive.h"

namespace ParamIDs
{
    inline constexpr auto model    = "model";
    inline constexpr auto mic      = "mic";
    inline constexpr auto position = "position";
    inline constexpr auto distance = "distance";
    inline constexpr auto room     = "room";
    inline constexpr auto push     = "push";
    inline constexpr auto lowCut   = "lowCut";
    inline constexpr auto highCut  = "highCut";
    inline constexpr auto output   = "output";
    inline constexpr auto bypass   = "bypass";
}

/** Guitar cab simulator. Everything the cab does to the frequency response (speaker, mic, mic
    position, distance, room, or a loaded IR file) is folded into one impulse response, rebuilt on a
    background thread whenever a control moves and swapped in with a short crossfade; the audio thread
    only runs a zero-latency convolution. Push adds what an IR can't: the speaker compressing and
    breaking up when it's driven hard. */
class PluginProcessor final : public at::ProcessorBase,
                              private juce::Thread
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    double getTailLengthSeconds() const override { return 0.5; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    /** Loads an IR file and switches the model to User IR. Call on the message thread.
        Returns false (and changes nothing) if the file can't be read. */
    bool loadImpulseFile (const juce::File&);

    /** The loaded IR's name, or empty if none. */
    juce::String getUserImpulseName() const;

    /** Magnitude of the IR currently playing, in dB, at responseFrequencies. Bumps
        getResponseVersion() each time it changes. */
    static constexpr int numResponsePoints = 160;
    static float responseFrequency (int index);
    std::array<float, numResponsePoints> getResponse() const;
    int getResponseVersion() const noexcept { return responseVersion.load(); }

private:
    void run() override;
    cab::Settings currentSettings() const noexcept;
    void rebuildImpulse (const cab::Settings&, double sampleRate);
    void reloadImpulseFromState();
    void snapSmoothersToTargets() noexcept;
    void processChunk (float* left, float* right, int numSamples, int numChannels) noexcept;

    std::atomic<float>& modelParam;
    std::atomic<float>& micParam;
    std::atomic<float>& positionParam;
    std::atomic<float>& distanceParam;
    std::atomic<float>& roomParam;
    std::atomic<float>& pushParam;
    std::atomic<float>& lowCutParam;
    std::atomic<float>& highCutParam;
    std::atomic<float>& outputParam;
    std::atomic<float>& bypassParam;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    // The IR builder thread watches these; the audio thread never touches them.
    mutable juce::CriticalSection impulseLock;
    std::shared_ptr<const cab::UserImpulse> userImpulse;
    std::atomic<int> userImpulseGeneration { 0 };
    std::atomic<double> preparedSampleRate { 0.0 };
    std::array<float, numResponsePoints> response {};
    std::atomic<int> responseVersion { 0 };

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };
    std::array<SpeakerDrive, 2> drive;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowCutFilter, highCutFilter;
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float> push, engagedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCut, highCut, outputGain;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
