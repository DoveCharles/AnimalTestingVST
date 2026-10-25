#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    namespace StateKeys
    {
        inline const juce::Identifier irFile     { "irFile" };
        inline const juce::Identifier irName     { "irName" };
        inline const juce::Identifier irRate     { "irRate" };
        inline const juce::Identifier irChannels { "irChannels" };
        inline const juce::Identifier irData     { "irData" };
    }

    constexpr int chunkSize = 512;
    constexpr int filterUpdateInterval = 16;

    /** Smoothed magnitude (1/12 octave) of an IR at the given frequencies, in dB. */
    template <size_t N>
    std::array<float, N> magnitudeDb (const float* ir, int length, double sampleRate, const std::function<float (int)>& frequency)
    {
        const auto order = juce::jmax (14, (int) std::ceil (std::log2 ((double) juce::jmax (2, length))));
        juce::dsp::FFT fft (order);
        const auto n = fft.getSize();
        std::vector<float> data ((size_t) n * 2, 0.0f);
        std::copy (ir, ir + juce::jmin (length, n), data.begin());
        fft.performFrequencyOnlyForwardTransform (data.data(), true);

        std::array<float, N> result {};
        for (size_t i = 0; i < N; ++i)
        {
            const auto hz = frequency ((int) i);
            const auto lo = juce::jlimit (1, n / 2, (int) std::floor (hz * std::pow (2.0f, -1.0f / 24.0f) * n / sampleRate));
            const auto hi = juce::jlimit (lo, n / 2, (int) std::ceil (hz * std::pow (2.0f, 1.0f / 24.0f) * n / sampleRate));
            auto power = 0.0f;
            for (int k = lo; k <= hi; ++k)
                power += data[(size_t) k] * data[(size_t) k];
            result[i] = juce::Decibels::gainToDecibels (std::sqrt (power / (float) (hi - lo + 1)), -120.0f);
        }
        return result;
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      juce::Thread ("Cabinet IR builder"),
      modelParam    (at::params::raw (apvts, ParamIDs::model)),
      micParam      (at::params::raw (apvts, ParamIDs::mic)),
      positionParam (at::params::raw (apvts, ParamIDs::position)),
      distanceParam (at::params::raw (apvts, ParamIDs::distance)),
      roomParam     (at::params::raw (apvts, ParamIDs::room)),
      pushParam     (at::params::raw (apvts, ParamIDs::push)),
      lowCutParam   (at::params::raw (apvts, ParamIDs::lowCut)),
      highCutParam  (at::params::raw (apvts, ParamIDs::highCut)),
      outputParam   (at::params::raw (apvts, ParamIDs::output)),
      bypassParam   (at::params::raw (apvts, ParamIDs::bypass)),
      bypassParameter (apvts.getParameter (ParamIDs::bypass))
{
    response.fill (-120.0f);
}

PluginProcessor::~PluginProcessor()
{
    stopThread (2000);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice   (ParamIDs::model, "Cabinet", cab::modelNames, cab::vintage412));
    layout.add (at::params::makeChoice   (ParamIDs::mic, "Mic", cab::micNames, cab::dynamic));
    layout.add (at::params::makePercent  (ParamIDs::position, "Mic Position", 35.0f));
    layout.add (at::params::makePercent  (ParamIDs::distance, "Mic Distance", 15.0f));
    layout.add (at::params::makePercent  (ParamIDs::room, "Room", 0.0f));
    layout.add (at::params::makePercent  (ParamIDs::push, "Push", 20.0f));
    layout.add (at::params::makeFrequency (ParamIDs::lowCut, "Low Cut", 20.0f, 400.0f, 20.0f));
    layout.add (at::params::makeFrequency (ParamIDs::highCut, "High Cut", 2000.0f, 20000.0f, 20000.0f));
    layout.add (at::params::makeDecibels (ParamIDs::output, "Output", -24.0f, 12.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (at::params::id (ParamIDs::bypass), "Bypass", false));
    return layout;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono in to stereo out is how a guitar usually arrives; the room needs the stereo out.
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };

    return monoOrStereo (in) && monoOrStereo (out) && in.size() <= out.size();
}

//==============================================================================
cab::Settings PluginProcessor::currentSettings() const noexcept
{
    cab::Settings s;
    s.model    = (int) modelParam.load();
    s.mic      = (int) micParam.load();
    s.position = positionParam.load() / 100.0f;
    s.distance = distanceParam.load() / 100.0f;
    s.room     = roomParam.load() / 100.0f;
    return s;
}

void PluginProcessor::rebuildImpulse (const cab::Settings& settings, double rate)
{
    std::shared_ptr<const cab::UserImpulse> file;
    {
        const juce::ScopedLock sl (impulseLock);
        file = userImpulse;
    }

    auto ir = cab::buildImpulse (settings, rate, file.get());
    const auto curve = magnitudeDb<numResponsePoints> (ir.getReadPointer (0), ir.getNumSamples(), rate, responseFrequency);

    convolution.loadImpulseResponse (std::move (ir), rate, juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    {
        const juce::ScopedLock sl (impulseLock);
        response = curve;
    }
    ++responseVersion;
}

void PluginProcessor::run()
{
    // Rebuilding takes a few milliseconds, so polling is cheap and naturally debounces knob drags.
    cab::Settings built = currentSettings();
    auto builtGeneration = userImpulseGeneration.load();
    auto builtRate = preparedSampleRate.load();

    while (! threadShouldExit())
    {
        wait (25);

        const auto settings = currentSettings();
        const auto generation = userImpulseGeneration.load();
        const auto rate = preparedSampleRate.load();

        if (rate > 0.0 && (settings != built || generation != builtGeneration || ! juce::exactlyEqual (rate, builtRate)))
        {
            rebuildImpulse (settings, rate);
            built = settings;
            builtGeneration = generation;
            builtRate = rate;
        }
    }
}

//==============================================================================
void PluginProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    stopThread (2000);

    sampleRate = newSampleRate;
    juce::ignoreUnused (samplesPerBlock);
    const auto blockSize = chunkSize;   // processBlock works in chunks of at most this
    const juce::dsp::ProcessSpec spec { newSampleRate, (juce::uint32) blockSize, 2 };

    // Load before prepare, so the right IR is already playing on the first block.
    rebuildImpulse (currentSettings(), newSampleRate);
    convolution.prepare (spec);

    for (auto& d : drive)
        d.prepare (newSampleRate);

    for (auto& f : lowCutFilter)
    {
        f.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        f.prepare ({ newSampleRate, (juce::uint32) blockSize, 1 });
    }
    for (auto& f : highCutFilter)
    {
        f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        f.prepare ({ newSampleRate, (juce::uint32) blockSize, 1 });
    }

    dryBuffer.setSize (2, blockSize);

    push.reset (newSampleRate, 0.05);
    engagedAmount.reset (newSampleRate, 0.02);
    lowCut.reset (newSampleRate, 0.05);
    highCut.reset (newSampleRate, 0.05);
    outputGain.reset (newSampleRate, 0.05);
    snapSmoothersToTargets();

    preparedSampleRate = newSampleRate;
    startThread (juce::Thread::Priority::low);
}

void PluginProcessor::snapSmoothersToTargets() noexcept
{
    push.setCurrentAndTargetValue (pushParam.load() / 100.0f);
    engagedAmount.setCurrentAndTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);
    lowCut.setCurrentAndTargetValue (lowCutParam.load());
    highCut.setCurrentAndTargetValue (juce::jmin (highCutParam.load(), (float) (0.45 * sampleRate)));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam.load()));

    for (int ch = 0; ch < 2; ++ch)
    {
        lowCutFilter[(size_t) ch].setCutoffFrequency (lowCut.getCurrentValue());
        highCutFilter[(size_t) ch].setCutoffFrequency (highCut.getCurrentValue());
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    push.setTargetValue (pushParam.load() / 100.0f);
    engagedAmount.setTargetValue (bypassParam.load() > 0.5f ? 0.0f : 1.0f);
    lowCut.setTargetValue (lowCutParam.load());
    highCut.setTargetValue (juce::jmin (highCutParam.load(), (float) (0.45 * sampleRate)));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam.load()));

    const auto numInputs  = getTotalNumInputChannels();
    const auto numOutputs = juce::jmin (2, buffer.getNumChannels(), getTotalNumOutputChannels());
    const auto numSamples = buffer.getNumSamples();

    // A mono guitar feeds both sides, so the room can spread it.
    if (numInputs == 1 && numOutputs > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    auto* left  = buffer.getWritePointer (0);
    auto* right = numOutputs > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int offset = 0; offset < numSamples; offset += chunkSize)
        processChunk (left + offset, right != nullptr ? right + offset : nullptr,
                      juce::jmin (chunkSize, numSamples - offset), numOutputs);
}

void PluginProcessor::processChunk (float* left, float* right, int numSamples, int numChannels) noexcept
{
    float* channels[] { left, right };

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, channels[ch], numSamples);

    // The speaker is driven before it filters: breakup happens in the cone, not after the mic.
    for (int n = 0; n < numSamples; ++n)
    {
        const auto amount = push.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][n] = drive[(size_t) ch].process (channels[ch][n], amount);
    }

    juce::dsp::AudioBlock<float> block (channels, (size_t) numChannels, (size_t) numSamples);
    convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

    for (int n = 0; n < numSamples; ++n)
    {
        if (n % filterUpdateInterval == 0)
        {
            const auto low = lowCut.skip (juce::jmin (filterUpdateInterval, numSamples - n));
            const auto high = highCut.skip (juce::jmin (filterUpdateInterval, numSamples - n));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                lowCutFilter[(size_t) ch].setCutoffFrequency (low);
                highCutFilter[(size_t) ch].setCutoffFrequency (high);
            }
        }

        const auto gain = outputGain.getNextValue();
        const auto engaged = engagedAmount.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto y = lowCutFilter[(size_t) ch].processSample (0, channels[ch][n]);
            y = highCutFilter[(size_t) ch].processSample (0, y);
            const auto dry = dryBuffer.getSample (ch, n);
            channels[ch][n] = dry + engaged * (gain * y - dry);
        }
    }
}

//==============================================================================
bool PluginProcessor::loadImpulseFile (const juce::File& file)
{
    auto impulse = cab::readImpulseFile (file);
    if (! impulse.has_value())
        return false;

    // Keep a copy of the samples in the state, so a session still sounds right on another machine.
    juce::MemoryBlock data;
    for (int ch = 0; ch < impulse->samples.getNumChannels(); ++ch)
        data.append (impulse->samples.getReadPointer (ch), sizeof (float) * (size_t) impulse->samples.getNumSamples());

    apvts.state.setProperty (StateKeys::irFile, file.getFullPathName(), nullptr);
    apvts.state.setProperty (StateKeys::irName, impulse->name, nullptr);
    apvts.state.setProperty (StateKeys::irRate, impulse->sampleRate, nullptr);
    apvts.state.setProperty (StateKeys::irChannels, impulse->samples.getNumChannels(), nullptr);
    apvts.state.setProperty (StateKeys::irData, data.toBase64Encoding(), nullptr);

    {
        const juce::ScopedLock sl (impulseLock);
        userImpulse = std::make_shared<const cab::UserImpulse> (std::move (*impulse));
    }
    ++userImpulseGeneration;

    if (auto* parameter = apvts.getParameter (ParamIDs::model))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) cab::userIr));

    return true;
}

juce::String PluginProcessor::getUserImpulseName() const
{
    const juce::ScopedLock sl (impulseLock);
    return userImpulse != nullptr ? userImpulse->name : juce::String();
}

void PluginProcessor::reloadImpulseFromState()
{
    std::shared_ptr<const cab::UserImpulse> restored;

    const auto channels = (int) apvts.state.getProperty (StateKeys::irChannels, 0);
    const auto rate = (double) apvts.state.getProperty (StateKeys::irRate, 0.0);
    juce::MemoryBlock data;

    if (channels > 0 && rate > 0.0 && data.fromBase64Encoding (apvts.state.getProperty (StateKeys::irData).toString()))
    {
        const auto length = (int) (data.getSize() / (sizeof (float) * (size_t) channels));
        if (length > 0)
        {
            cab::UserImpulse impulse;
            impulse.samples.setSize (channels, length);
            for (int ch = 0; ch < channels; ++ch)
                impulse.samples.copyFrom (ch, 0, static_cast<const float*> (data.getData()) + ch * length, length);
            impulse.sampleRate = rate;
            impulse.name = apvts.state.getProperty (StateKeys::irName).toString();
            restored = std::make_shared<const cab::UserImpulse> (std::move (impulse));
        }
    }
    else if (const juce::File file { apvts.state.getProperty (StateKeys::irFile).toString() }; file.existsAsFile())
    {
        if (auto impulse = cab::readImpulseFile (file))
            restored = std::make_shared<const cab::UserImpulse> (std::move (*impulse));
    }

    {
        const juce::ScopedLock sl (impulseLock);
        userImpulse = std::move (restored);
    }
    ++userImpulseGeneration;
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    at::ProcessorBase::setStateInformation (data, sizeInBytes);
    reloadImpulseFromState();
}

//==============================================================================
float PluginProcessor::responseFrequency (int index)
{
    return 20.0f * std::pow (1000.0f, (float) index / (float) (numResponsePoints - 1));
}

std::array<float, PluginProcessor::numResponsePoints> PluginProcessor::getResponse() const
{
    const juce::ScopedLock sl (impulseLock);
    return response;
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
