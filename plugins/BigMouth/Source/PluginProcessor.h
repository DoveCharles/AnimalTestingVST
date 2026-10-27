#pragma once

#include "HitDetector.h"

namespace ParamIDs
{
    inline constexpr auto threshold = "threshold";
    inline constexpr auto listen    = "listen";
    inline constexpr auto gap       = "gap";
    inline constexpr auto dynamics  = "dynamics";
    inline constexpr auto monitor   = "monitor";
    inline constexpr std::array<const char*, HitClassifier::numSlots> notes { "note1", "note2", "note3", "note4", "note5" };
}

namespace Slots
{
    inline const juce::StringArray names { "Kick", "Snare", "Hi-Hat", "Open Hat", "Tom" };

    // General MIDI drum notes, which Addictive Drums 2 and most drum samplers follow for these pieces.
    inline constexpr std::array<int, HitClassifier::numSlots> defaultNotes { 36, 38, 42, 46, 45 };
}

/** Beatbox to MIDI drums: an audio effect that listens to a mic, finds each hit, decides which
    pad it is (by brightness, or by comparing with sounds you trained) and sends a drum note.
    Notes go to the host's MIDI output and, on macOS, to a virtual MIDI port called
    "AT Big Mouth" that any track or app can take as its input. */
class PluginProcessor final : public at::ProcessorBase,
                              private juce::Timer,
                              private juce::ValueTree::Listener
{
public:
    explicit PluginProcessor (bool openVirtualPort = true);
    ~PluginProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    // Training, from the message thread.
    static constexpr int hitsPerTraining = 8;

    void startTraining (int slot);
    void stopTraining();
    void clearSlot (int slot);
    int getTrainingSlot() const noexcept       { return trainingSlot.load(); }
    int getTrainingProgress() const noexcept   { return trainingTaken; }
    int getExampleCount (int slot) const;
    bool isTrained() const;

    /** Moves hits recorded for training into the model; the timer calls this, offline tools may too. */
    void collectTraining();

    //==============================================================================
    // For the editor.
    std::atomic<int> lastSlot { -1 }, lastVelocity { 0 }, hitCount { 0 };
    std::atomic<float> inputLevel { 0.0f };
    juce::String getVirtualPortName() const { return virtualPortName; }

private:
    struct Pending { int note = 0, samplesLeft = 0; };
    struct TrainingHit { int slot = 0; HitClassifier::Example example; };
    class VirtualPort;

    void timerCallback() override { collectTraining(); }
    void valueTreeRedirected (juce::ValueTree&) override { loadTraining(); }

    void publishModel();
    void saveTraining();
    void loadTraining();
    void sendNote (juce::MidiBuffer&, int sampleOffset, int note, int velocity);

    std::atomic<float>& thresholdParam;
    std::atomic<float>& listenParam;
    std::atomic<float>& gapParam;
    std::atomic<float>& dynamicsParam;
    std::atomic<float>& monitorParam;
    std::array<std::atomic<float>*, HitClassifier::numSlots> noteParams {};

    HitDetector detector;
    juce::SmoothedValue<float> monitorGain;
    std::array<Pending, 32> pending {};
    int numPending = 0, noteOffSamples = 0;

    // The model the audio thread reads; replaced whole under the lock, never edited in place.
    juce::SpinLock modelLock;
    std::unique_ptr<HitClassifier> liveModel;

    // The message thread's copy, which training edits.
    mutable juce::CriticalSection trainingLock;
    HitClassifier model;

    std::atomic<int> trainingSlot { -1 };
    int trainingTaken = 0;
    juce::AbstractFifo trainingFifo { 64 };
    std::array<TrainingHit, 64> trainingHits;

    std::unique_ptr<VirtualPort> virtualPort;
    juce::String virtualPortName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
