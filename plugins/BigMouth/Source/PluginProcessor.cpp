#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int midiChannel = 10;
    constexpr double noteLengthSeconds = 0.04;

    juce::String noteText (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " (" + juce::String (note) + ")";
    }
}

//==============================================================================
/** A CoreMIDI source other apps and tracks can listen to. The audio thread queues messages
    without locking; this thread sends them, polling every millisecond. */
class PluginProcessor::VirtualPort : private juce::Thread
{
public:
    explicit VirtualPort (std::unique_ptr<juce::MidiOutput> device)
        : juce::Thread ("Big Mouth MIDI"), output (std::move (device))
    {
        startThread (juce::Thread::Priority::high);
    }

    ~VirtualPort() override { stopThread (500); }

    /** Audio thread. Drops the message if the queue is full. */
    void push (const juce::MidiMessage& message) noexcept
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
            queue[(size_t) scope.startIndex1] = message;
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            const auto scope = fifo.read (fifo.getNumReady());
            scope.forEach ([this] (int index) { output->sendMessageNow (queue[(size_t) index]); });
            wait (1);
        }
    }

    std::unique_ptr<juce::MidiOutput> output;
    juce::AbstractFifo fifo { 256 };
    std::array<juce::MidiMessage, 256> queue;
};

//==============================================================================
PluginProcessor::PluginProcessor (bool openVirtualPort)
    : at::ProcessorBase (createParameterLayout()),
      thresholdParam (at::params::raw (apvts, ParamIDs::threshold)),
      listenParam    (at::params::raw (apvts, ParamIDs::listen)),
      gapParam       (at::params::raw (apvts, ParamIDs::gap)),
      dynamicsParam  (at::params::raw (apvts, ParamIDs::dynamics)),
      monitorParam   (at::params::raw (apvts, ParamIDs::monitor)),
      liveModel (std::make_unique<HitClassifier>())
{
    for (size_t slot = 0; slot < noteParams.size(); ++slot)
        noteParams[slot] = &at::params::raw (apvts, ParamIDs::notes[slot]);

   #if JUCE_MAC
    if (openVirtualPort)
    {
        // One port per instance; later instances get a number so they can be told apart.
        static std::atomic<int> instances { 0 };
        const auto number = ++instances;
        virtualPortName = number == 1 ? juce::String ("AT Big Mouth") : "AT Big Mouth " + juce::String (number);
        if (auto device = juce::MidiOutput::createNewDevice (virtualPortName))
            virtualPort = std::make_unique<VirtualPort> (std::move (device));
        else
            virtualPortName = {};
    }
   #else
    juce::ignoreUnused (openVirtualPort);
   #endif

    apvts.state.addListener (this);
    startTimerHz (20);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
    apvts.state.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (at::params::makeDecibels (ParamIDs::threshold, "Threshold", -60.0f, -6.0f, -36.0f));
    layout.add (at::params::makeFloat (ParamIDs::listen, "Listen", { 8.0f, 40.0f, 0.1f }, 16.0f, "ms", 0));
    layout.add (at::params::makeFloat (ParamIDs::gap, "Gap", { 30.0f, 250.0f, 1.0f }, 60.0f, "ms", 0));
    layout.add (at::params::makePercent (ParamIDs::dynamics, "Dynamics", 60.0f));
    layout.add (at::params::makePercent (ParamIDs::monitor, "Monitor", 0.0f));

    for (int slot = 0; slot < HitClassifier::numSlots; ++slot)
        layout.add (std::make_unique<juce::AudioParameterInt> (
            at::params::id (ParamIDs::notes[(size_t) slot]), Slots::names[slot] + " Note", 0, 127, Slots::defaultNotes[(size_t) slot],
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int note, int) { return noteText (note); })
                .withValueFromStringFunction ([] (const juce::String& text) { return text.retainCharacters ("0123456789").getIntValue(); })));

    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    detector.prepare (sampleRate);
    monitorGain.reset (sampleRate, 0.05);
    monitorGain.setCurrentAndTargetValue (monitorParam.load() * 0.01f);
    noteOffSamples = (int) (noteLengthSeconds * sampleRate);
    numPending = 0;
}

void PluginProcessor::sendNote (juce::MidiBuffer& midi, int sampleOffset, int note, int velocity)
{
    const auto message = velocity > 0 ? juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) velocity)
                                      : juce::MidiMessage::noteOff (midiChannel, note);
    midi.addEvent (message, sampleOffset);
    if (virtualPort != nullptr)
        virtualPort->push (message);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numInputs = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    for (auto ch = numInputs; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    detector.setThresholdDb (thresholdParam.load());
    detector.setListenSeconds (listenParam.load() * 0.001f);
    detector.setGapSeconds (gapParam.load() * 0.001f);
    const auto dynamics = dynamicsParam.load() * 0.01f;

    // Note-offs left over from earlier blocks.
    for (int i = 0; i < numPending;)
    {
        auto& p = pending[(size_t) i];
        if (p.samplesLeft < numSamples)
        {
            sendNote (midi, juce::jmax (0, p.samplesLeft), p.note, 0);
            p = pending[(size_t) --numPending];
        }
        else
        {
            p.samplesLeft -= numSamples;
            ++i;
        }
    }

    const juce::SpinLock::ScopedTryLockType modelScope (modelLock);
    const auto* classifier = modelScope.isLocked() ? liveModel.get() : nullptr;
    const auto training = trainingSlot.load();

    HitDetector::Hit hit;
    for (int n = 0; n < numSamples; ++n)
    {
        auto mono = 0.0f;
        for (int ch = 0; ch < numInputs; ++ch)
            mono += buffer.getSample (ch, n);
        mono /= (float) juce::jmax (1, numInputs);

        if (! detector.push (mono, hit))
            continue;

        auto slot = training;
        if (slot < 0)
            slot = classifier != nullptr ? classifier->classify (hit) : HitClassifier::classifyByBrightness (hit);
        if (slot < 0)
            continue;

        // Velocity: how loud this hit is against the pad's own usual loudness (3 steps per dB at 100%).
        const auto reference = classifier != nullptr ? classifier->referenceDb (slot) : -18.0f;
        const auto velocity = juce::jlimit (1, 127, juce::roundToInt (100.0f + dynamics * 3.0f * (hit.levelDb - reference)));
        const auto note = (int) noteParams[(size_t) slot]->load();

        sendNote (midi, n, note, velocity);
        if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { note, n + noteOffSamples - numSamples };

        if (training >= 0)
        {
            const auto scope = trainingFifo.write (1);
            if (scope.blockSize1 > 0)
                trainingHits[(size_t) scope.startIndex1] = { training, { hit.features, hit.levelDb } };
        }

        lastSlot = slot;
        lastVelocity = velocity;
        ++hitCount;
    }

    // Note-offs due later in this same block.
    for (int i = 0; i < numPending;)
    {
        auto& p = pending[(size_t) i];
        if (p.samplesLeft < 0)
        {
            sendNote (midi, juce::jlimit (0, numSamples - 1, numSamples + p.samplesLeft), p.note, 0);
            p = pending[(size_t) --numPending];
        }
        else
        {
            ++i;
        }
    }

    inputLevel = detector.getLevel();

    monitorGain.setTargetValue (monitorParam.load() * 0.01f);
    if (monitorGain.isSmoothing() || monitorGain.getTargetValue() < 1.0f)
        monitorGain.applyGain (buffer, numSamples);
}

//==============================================================================
void PluginProcessor::startTraining (int slot)
{
    trainingTaken = 0;
    trainingSlot = slot;
}

void PluginProcessor::stopTraining()
{
    trainingSlot = -1;
}

void PluginProcessor::clearSlot (int slot)
{
    {
        const juce::ScopedLock scope (trainingLock);
        model.examples[(size_t) slot].clear();
    }
    publishModel();
    saveTraining();
}

int PluginProcessor::getExampleCount (int slot) const
{
    const juce::ScopedLock scope (trainingLock);
    return (int) model.examples[(size_t) slot].size();
}

bool PluginProcessor::isTrained() const
{
    const juce::ScopedLock scope (trainingLock);
    return model.isTrained();
}

void PluginProcessor::collectTraining()
{
    const auto ready = trainingFifo.getNumReady();
    if (ready == 0)
        return;

    {
        const juce::ScopedLock scope (trainingLock);
        trainingFifo.read (ready).forEach ([this] (int index)
        {
            const auto& item = trainingHits[(size_t) index];
            auto& list = model.examples[(size_t) item.slot];
            if ((int) list.size() >= HitClassifier::maxExamples)
                list.erase (list.begin());
            list.push_back (item.example);
            ++trainingTaken;
        });
    }

    if (trainingTaken >= hitsPerTraining)
        stopTraining();

    publishModel();
    saveTraining();
}

void PluginProcessor::publishModel()
{
    std::unique_ptr<HitClassifier> fresh;
    {
        const juce::ScopedLock scope (trainingLock);
        fresh = std::make_unique<HitClassifier> (model);
    }
    {
        const juce::SpinLock::ScopedLockType scope (modelLock);
        std::swap (liveModel, fresh);
    }
    // The old model is freed here, outside the lock.
}

//==============================================================================
// Training is kept as a TRAINING child of the parameter state, so the host saves it with the
// set and presets carry it: <TRAINING><SLOT index="0"><HIT level="-14.2" features="..."/>...

void PluginProcessor::saveTraining()
{
    juce::ValueTree tree ("TRAINING");
    {
        const juce::ScopedLock scope (trainingLock);
        for (int slot = 0; slot < HitClassifier::numSlots; ++slot)
        {
            juce::ValueTree slotTree ("SLOT");
            slotTree.setProperty ("index", slot, nullptr);
            for (auto& e : model.examples[(size_t) slot])
            {
                juce::StringArray values;
                for (auto f : e.features)
                    values.add (juce::String (f, 4));
                slotTree.appendChild (juce::ValueTree ("HIT", { { "level", e.levelDb }, { "features", values.joinIntoString (" ") } }), nullptr);
            }
            tree.appendChild (slotTree, nullptr);
        }
    }

    auto& state = apvts.state;
    if (auto old = state.getChildWithName ("TRAINING"); old.isValid())
        state.removeChild (old, nullptr);
    state.appendChild (tree, nullptr);
}

void PluginProcessor::loadTraining()
{
    HitClassifier loaded;
    for (auto slotTree : apvts.state.getChildWithName ("TRAINING"))
    {
        const auto slot = (int) slotTree.getProperty ("index", -1);
        if (! juce::isPositiveAndBelow (slot, HitClassifier::numSlots))
            continue;

        for (auto hitTree : slotTree)
        {
            const auto values = juce::StringArray::fromTokens (hitTree.getProperty ("features").toString(), " ", {});
            if (values.size() != HitDetector::numFeatures)
                continue;

            HitClassifier::Example e;
            e.levelDb = hitTree.getProperty ("level", -18.0f);
            for (int i = 0; i < HitDetector::numFeatures; ++i)
                e.features[(size_t) i] = values[i].getFloatValue();
            loaded.examples[(size_t) slot].push_back (e);
        }
    }

    {
        const juce::ScopedLock scope (trainingLock);
        model = std::move (loaded);
    }
    publishModel();
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
