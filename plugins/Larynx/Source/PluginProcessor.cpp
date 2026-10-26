#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float vibratoFadeSeconds = 0.25f;

    // What full Gender (+100) adds. Female formants sit roughly 15-20 % (about 3 semitones) above
    // male ones, and female voices are usually a fourth or so higher rather than a full octave.
    constexpr float genderSemitones = 5.0f;
    constexpr float genderFormant = 3.0f;
    constexpr float genderChest = 0.7f;
    constexpr float genderAir = 0.5f;
    constexpr float genderBreath = 0.3f;

    float onePole (double sampleRate, double seconds)
    {
        return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }

    float midiFromPeriod (double sampleRate, float period)
    {
        return 69.0f + 12.0f * std::log2 ((float) sampleRate / period / 440.0f);
    }
}

PluginProcessor::PluginProcessor()
    : at::ProcessorBase (createParameterLayout()),
      modeParam     (at::params::raw (apvts, ParamIDs::mode)),
      pitchParam    (at::params::raw (apvts, ParamIDs::pitch)),
      formantParam  (at::params::raw (apvts, ParamIDs::formant)),
      keyParam      (at::params::raw (apvts, ParamIDs::key)),
      scaleParam    (at::params::raw (apvts, ParamIDs::scale)),
      retuneParam   (at::params::raw (apvts, ParamIDs::retune)),
      genderParam   (at::params::raw (apvts, ParamIDs::gender)),
      chestParam    (at::params::raw (apvts, ParamIDs::chest)),
      airParam      (at::params::raw (apvts, ParamIDs::air)),
      breathParam   (at::params::raw (apvts, ParamIDs::breath)),
      vibRateParam  (at::params::raw (apvts, ParamIDs::vibRate)),
      vibDepthParam (at::params::raw (apvts, ParamIDs::vibDepth)),
      vibDelayParam (at::params::raw (apvts, ParamIDs::vibDelay)),
      driveParam    (at::params::raw (apvts, ParamIDs::drive)),
      mixParam      (at::params::raw (apvts, ParamIDs::mix)),
      outputParam   (at::params::raw (apvts, ParamIDs::output))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (at::params::makeChoice (ParamIDs::mode,    "Mode", Modes::names, Modes::transpose));
    layout.add (at::params::makeFloat  (ParamIDs::pitch,   "Pitch",   { -12.0f, 12.0f, 0.01f }, 0.0f, "st", 2));
    layout.add (at::params::makeFloat  (ParamIDs::formant, "Formant", { -12.0f, 12.0f, 0.01f }, 0.0f, "st", 2));
    layout.add (at::params::makeChoice (ParamIDs::key,     "Key", Scales::keys, 0));
    layout.add (at::params::makeChoice (ParamIDs::scale,   "Scale", Scales::names, 0));
    layout.add (at::params::makeFloat  (ParamIDs::retune,  "Retune", { 0.0f, 400.0f, 1.0f }, 40.0f, "ms", 0));
    layout.add (at::params::makeFloat  (ParamIDs::gender,  "Gender", { -100.0f, 100.0f, 1.0f }, 0.0f, "%", 0));
    layout.add (at::params::makePercent (ParamIDs::chest,  "Chest Cut", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::air,    "Air", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::breath, "Breath", 0.0f));
    layout.add (at::params::makeFloat  (ParamIDs::vibRate,  "Vibrato Rate",  { 2.0f, 9.0f, 0.01f }, 5.5f, "Hz", 2));
    layout.add (at::params::makeFloat  (ParamIDs::vibDepth, "Vibrato Depth", { 0.0f, 100.0f, 0.1f }, 0.0f, "ct", 0));
    layout.add (at::params::makeFloat  (ParamIDs::vibDelay, "Vibrato Delay", { 0.0f, 1.5f, 0.001f, 0.5f }, 0.3f, "s", 2));
    layout.add (at::params::makePercent (ParamIDs::drive,  "Drive", 0.0f));
    layout.add (at::params::makePercent (ParamIDs::mix,    "Mix", 100.0f));
    layout.add (at::params::makeDecibels (ParamIDs::output, "Output", -24.0f, 12.0f, 0.0f));
    return layout;
}

PluginProcessor::Settings PluginProcessor::readSettings() const noexcept
{
    const auto g = genderParam.load() / 100.0f;
    const auto up = juce::jmax (0.0f, g);
    return {
        pitchParam.load() + genderSemitones * g,
        std::exp2 ((formantParam.load() + genderFormant * g) / 12.0f),
        juce::jlimit (0.0f, 1.0f, chestParam.load() / 100.0f + genderChest * up),
        juce::jlimit (0.0f, 1.0f, airParam.load() / 100.0f + genderAir * up),
        juce::jlimit (0.0f, 1.0f, breathParam.load() / 100.0f + genderBreath * up),
        driveParam.load() / 100.0f,
        mixParam.load() / 100.0f,
        juce::Decibels::decibelsToGain (outputParam.load())
    };
}

void PluginProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    detector.prepare (sampleRate);
    shifter.prepare (sampleRate);
    setLatencySamples (shifter.getLatency());

    for (auto& line : dryDelay)
        line.assign ((size_t) shifter.getLatency(), 0.0f);
    dryPos = 0;

    const auto settings = readSettings();
    for (auto* smoother : { &semitones, &formant, &mix, &gain, &drive, &breath, &air })
        smoother->reset (sampleRate, 0.05);
    semitones.setCurrentAndTargetValue (settings.semitones);
    formant.setCurrentAndTargetValue (settings.formant);
    mix.setCurrentAndTargetValue (settings.mix);
    gain.setCurrentAndTargetValue (settings.gain);
    drive.setCurrentAndTargetValue (settings.drive);
    breath.setCurrentAndTargetValue (settings.breath);
    air.setCurrentAndTargetValue (settings.air);

    correction = 0.0f;
    vibPhase = 0.0f;
    voicedSeconds = 0.0f;
    voicedLevel = 0.0f;
    envelope = {};

    const juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
    for (int ch = 0; ch < maxChannels; ++ch)
        for (auto* filter : { &chestHighPass[(size_t) ch], &chestDip[(size_t) ch], &airShelf[(size_t) ch],
                              &exciterBand[(size_t) ch], &breathLow[(size_t) ch], &breathHigh[(size_t) ch] })
        {
            filter->prepare (spec);
            filter->reset();
        }

    lastChest = lastAir = lastFormant = -1.0f;
    updateFilters (settings);
}

void PluginProcessor::updateFilters (const Settings& settings)
{
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    const auto nyquistSafe = (float) sampleRate * 0.45f;

    if (std::abs (settings.chest - lastChest) > 1.0e-4f)
    {
        lastChest = settings.chest;
        const auto highPass = Coefficients::makeHighPass (sampleRate, 40.0f * std::pow (5.0f, settings.chest), 0.707f);
        const auto dip = Coefficients::makePeakFilter (sampleRate, 300.0f, 0.8f,
                                                       juce::Decibels::decibelsToGain (-7.0f * settings.chest));
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            *chestHighPass[(size_t) ch].coefficients = *highPass;
            *chestDip[(size_t) ch].coefficients = *dip;
        }
    }

    if (std::abs (settings.air - lastAir) > 1.0e-4f)
    {
        lastAir = settings.air;
        const auto shelf = Coefficients::makeHighShelf (sampleRate, juce::jmin (9000.0f, nyquistSafe), 0.707f,
                                                        juce::Decibels::decibelsToGain (8.0f * settings.air));
        const auto band = Coefficients::makeHighPass (sampleRate, juce::jmin (5000.0f, nyquistSafe), 0.707f);
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            *airShelf[(size_t) ch].coefficients = *shelf;
            *exciterBand[(size_t) ch].coefficients = *band;
        }
    }

    // Breath noise is shaped like the shifted vocal tract, so it moves with Formant.
    if (std::abs (settings.formant - lastFormant) > 0.005f)
    {
        lastFormant = settings.formant;
        const auto low = Coefficients::makeBandPass (sampleRate, juce::jmin (1800.0f * settings.formant, nyquistSafe), 0.9f);
        const auto high = Coefficients::makeBandPass (sampleRate, juce::jmin (4500.0f * settings.formant, nyquistSafe), 0.7f);
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            *breathLow[(size_t) ch].coefficients = *low;
            *breathHigh[(size_t) ch].coefficients = *high;
        }
    }
}

float PluginProcessor::correctionTarget (int mode, float midiIn, float shift) const noexcept
{
    const auto want = midiIn + shift;
    const auto root = (int) keyParam.load();

    if (mode == Modes::robot)
        return (float) (48 + root) + shift - want;

    const auto mask = Scales::masks[(size_t) juce::jlimit (0, (int) Scales::masks.size() - 1, (int) scaleParam.load())];
    const auto nearest = (int) std::round (want);
    for (int distance = 0; distance <= 6; ++distance)
    {
        // Of two equally near scale notes, take the one closer to where the voice actually is.
        const auto below = nearest - distance, above = nearest + distance;
        const auto inScale = [&] (int note) { return ((mask >> (((note - root) % 12 + 12) % 12)) & 1) != 0; };
        const auto belowOk = inScale (below), aboveOk = inScale (above);
        if (belowOk && aboveOk)
            return (std::abs ((float) below - want) <= std::abs ((float) above - want) ? (float) below : (float) above) - want;
        if (belowOk)
            return (float) below - want;
        if (aboveOk)
            return (float) above - want;
    }
    return 0.0f;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto settings = readSettings();
    updateFilters (settings);
    semitones.setTargetValue (settings.semitones);
    formant.setTargetValue (settings.formant);
    mix.setTargetValue (settings.mix);
    gain.setTargetValue (settings.gain);
    drive.setTargetValue (settings.drive);
    breath.setTargetValue (settings.breath);
    air.setTargetValue (settings.air);

    const auto mode = (int) modeParam.load();
    const auto retuneCoef = onePole (sampleRate, retuneParam.load() * 0.001);
    const auto vibStep = vibRateParam.load() / (float) sampleRate;
    const auto vibDepth = vibDepthParam.load() / 100.0f;
    const auto vibDelay = vibDelayParam.load();
    const auto voicedCoef = onePole (sampleRate, 0.02);
    const auto attack = onePole (sampleRate, 0.005), release = onePole (sampleRate, 0.06);

    const auto numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const auto numSamples = buffer.getNumSamples();
    const auto latency = shifter.getLatency();

    PitchDetector::Estimate estimate;
    float ratio = 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        std::array<float, maxChannels> in {}, wet {};
        auto mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            in[(size_t) ch] = buffer.getReadPointer (ch)[n];
            mono += in[(size_t) ch];
        }
        detector.push (mono / (float) juce::jmax (1, numChannels));
        estimate = detector.at (shifter.getTime() - latency);

        // Vibrato fades in once a note has been held for the Delay time.
        voicedSeconds = estimate.voiced ? voicedSeconds + 1.0f / (float) sampleRate : 0.0f;
        const auto onset = juce::jlimit (0.0f, 1.0f, (voicedSeconds - vibDelay) / vibratoFadeSeconds);
        vibPhase += vibStep;
        vibPhase -= std::floor (vibPhase);
        const auto vibrato = vibDepth * onset * std::sin (juce::MathConstants<float>::twoPi * vibPhase);

        const auto shift = semitones.getNextValue();
        if (mode == Modes::transpose)
            correction = 0.0f;
        else if (estimate.voiced)
            correction += retuneCoef * (correctionTarget (mode, midiFromPeriod (sampleRate, estimate.period), shift) - correction);

        ratio = std::exp2 ((shift + correction + vibrato) / 12.0f);
        const auto formantRatio = formant.getNextValue();
        shifter.process (in.data(), wet.data(), numChannels, { estimate.period, estimate.voiced, ratio, formantRatio });

        voicedLevel += voicedCoef * ((estimate.voiced ? 1.0f : 0.0f) - voicedLevel);
        const auto mixNow = mix.getNextValue(), gainNow = gain.getNextValue();
        const auto driveNow = drive.getNextValue(), breathNow = breath.getNextValue(), airNow = air.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = (size_t) ch;
            auto x = chestDip[c].processSample (chestHighPass[c].processSample (wet[c]));

            x = airShelf[c].processSample (x);
            x += airNow * 0.25f * std::tanh (4.0f * exciterBand[c].processSample (x));

            const auto level = std::abs (x);
            envelope[c] += (level > envelope[c] ? attack : release) * (level - envelope[c]);
            const auto hiss = envelope[c] * (2.0f * noise.nextFloat() - 1.0f);
            x += 1.5f * breathNow * voicedLevel * (breathLow[c].processSample (hiss) + breathHigh[c].processSample (hiss));

            if (driveNow > 0.0f)
            {
                const auto pre = 1.0f + 9.0f * driveNow;
                x += driveNow * (std::tanh (pre * x) / std::sqrt (pre) - x);
            }

            auto& delayed = dryDelay[c][(size_t) dryPos];
            const auto dry = delayed;
            delayed = in[c];

            buffer.getWritePointer (ch)[n] = gainNow * (dry + mixNow * (x - dry));
        }
        dryPos = (dryPos + 1) % latency;
    }

    const auto latest = detector.latest();
    const auto hz = latest.voiced ? (float) sampleRate / latest.period : 0.0f;
    inputHz.store (hz);
    outputHz.store (hz * ratio);
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
