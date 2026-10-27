// Offline checks for Big Mouth, without a DAW.
//
//   BigMouthRender test [--seed=1] [--<param>=<value> ...]
//       Plays a synthetic beatbox pattern (kick, snare and hi-hat sounds with random pitch, length,
//       level and background noise) through the processor, untrained and then trained on a
//       separate take, and prints how many hits were found, how many went to the right pad and
//       how late the notes come.
//
//   BigMouthRender render <input audio> [output.mid] [--<param>=<value> ...]
//       Prints every hit found in a recording (time, pad, velocity) and can save them as a MIDI file.
//
//   BigMouthRender snapshot <output.png> [--<param>=<value> ...]
//       Saves a picture of the editor.

#include "PluginProcessor.h"

namespace
{
    constexpr int blockSize = 256;
    constexpr double testRate = 48000.0;

    void applyParameters (PluginProcessor& processor, const juce::ArgumentList& args)
    {
        for (auto& arg : args.arguments)
        {
            if (! arg.isLongOption() || arg.text.startsWith ("--seed="))
                continue;

            const auto id = arg.text.fromFirstOccurrenceOf ("--", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto value = arg.text.fromFirstOccurrenceOf ("=", false, false);
            if (auto* param = processor.apvts.getParameter (id))
                param->setValueNotifyingHost (param->getValueForText (value));
            else
                std::cerr << "Unknown parameter: " << id << "\n";
        }
    }

    std::unique_ptr<PluginProcessor> makeProcessor (const juce::ArgumentList& args, double sampleRate)
    {
        auto processor = std::make_unique<PluginProcessor> (false);
        processor->setPlayConfigDetails (1, 1, sampleRate, blockSize);
        applyParameters (*processor, args);
        processor->prepareToPlay (sampleRate, blockSize);
        return processor;
    }

    struct Note { int sample = 0, note = 0, velocity = 0; };

    std::vector<Note> process (PluginProcessor& processor, const juce::AudioBuffer<float>& audio)
    {
        std::vector<Note> notes;
        juce::AudioBuffer<float> block (1, blockSize);
        juce::MidiBuffer midi;
        for (int start = 0; start < audio.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, audio.getNumSamples() - start);
            block.setSize (1, n, false, false, true);
            block.copyFrom (0, 0, audio, 0, start, n);
            midi.clear();
            processor.processBlock (block, midi);
            for (const auto event : midi)
                if (event.getMessage().isNoteOn())
                    notes.push_back ({ start + event.samplePosition, event.getMessage().getNoteNumber(), event.getMessage().getVelocity() });
        }
        return notes;
    }

    //==============================================================================
    // Synthetic beatbox: rough stand-ins for a lip "b" kick, a "pf"/"k" snare and a "ts" hat.

    struct Hit { int sample = 0, slot = 0; };

    void addSound (juce::AudioBuffer<float>& audio, int start, int slot, float gain, juce::Random& random)
    {
        auto* out = audio.getWritePointer (0);
        const auto rate = (float) testRate;
        const auto vary = [&] (float amount) { return 1.0f + amount * (random.nextFloat() * 2.0f - 1.0f); };

        juce::dsp::IIR::Filter<float> colour;
        if (slot == 1)      colour.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (testRate, 1800.0f * vary (0.3f), 0.8f);
        else if (slot == 2) colour.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (testRate, 6500.0f * vary (0.2f), 0.7f);
        else                colour.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (testRate, 900.0f, 0.7f);

        const auto length = slot == 0 ? 0.12f * vary (0.3f) : slot == 1 ? 0.14f * vary (0.3f) : 0.05f * vary (0.3f);
        const auto startHz = 160.0f * vary (0.25f), endHz = 55.0f * vary (0.15f), body = 190.0f * vary (0.2f);
        auto phase = 0.0f;

        for (int n = 0; n < (int) (length * rate * 2.0f) && start + n < audio.getNumSamples(); ++n)
        {
            const auto t = (float) n / rate;
            const auto envelope = std::exp (-t / (length * 0.35f)) * juce::jmin (1.0f, t / 0.0015f);
            const auto noise = random.nextFloat() * 2.0f - 1.0f;
            auto s = 0.0f;
            if (slot == 0)
            {
                // Pitch-swept thump with a short breathy click on the front.
                phase += juce::MathConstants<float>::twoPi * (endHz + (startHz - endHz) * std::exp (-t / 0.03f)) / rate;
                s = std::sin (phase) + colour.processSample (noise) * std::exp (-t / 0.004f) * 0.8f;
            }
            else if (slot == 1)
            {
                phase += juce::MathConstants<float>::twoPi * body / rate;
                s = colour.processSample (noise) * 2.0f + 0.35f * std::sin (phase) * std::exp (-t / 0.02f);
            }
            else
            {
                s = colour.processSample (noise) * 1.6f;
            }
            out[start + n] += gain * envelope * s;
        }
    }

    /** Sixteenth notes at 100 BPM, a simple boom-tss-pf-tss groove with gaps, levels from -24 to -6 dB. */
    std::vector<Hit> makePattern (juce::AudioBuffer<float>& audio, juce::Random& random, int bars)
    {
        constexpr std::array<int, 16> groove { 0, 2, 2, 2, 1, 2, 0, 2, 2, 0, 2, 2, 1, 2, 2, -1 };
        const auto step = (int) (testRate * 60.0 / 100.0 / 4.0);
        audio.setSize (1, step * 16 * bars + (int) testRate, false, true);
        audio.clear();

        // Room noise around -60 dBFS.
        for (int n = 0; n < audio.getNumSamples(); ++n)
            audio.setSample (0, n, 0.001f * (random.nextFloat() * 2.0f - 1.0f));

        std::vector<Hit> hits;
        for (int i = 0; i < 16 * bars; ++i)
        {
            const auto slot = groove[(size_t) (i % 16)];
            if (slot < 0 || random.nextFloat() < 0.1f)
                continue;
            const auto start = (int) testRate / 4 + i * step + random.nextInt ({ -400, 400 });
            addSound (audio, start, slot, juce::Decibels::decibelsToGain (-24.0f + 18.0f * random.nextFloat()), random);
            hits.push_back ({ start, slot });
        }
        return hits;
    }

    void score (const std::vector<Hit>& hits, const std::vector<Note>& notes)
    {
        const auto& noteSlot = Slots::defaultNotes;

        int found = 0, right = 0, extra = 0;
        double latency = 0.0, worst = 0.0;
        std::array<std::array<int, 3>, 3> confusion {};
        std::vector<bool> used (notes.size(), false);

        for (auto& hit : hits)
        {
            // The first unused note within 60 ms after the onset belongs to this hit.
            for (size_t i = 0; i < notes.size(); ++i)
            {
                const auto delay = notes[i].sample - hit.sample;
                if (used[i] || delay < 0 || delay > (int) (0.06 * testRate))
                    continue;
                used[i] = true;
                ++found;
                const auto slot = (int) (std::find (noteSlot.begin(), noteSlot.end(), notes[i].note) - noteSlot.begin());
                right += slot == hit.slot ? 1 : 0;
                if (slot < 3)
                    ++confusion[(size_t) hit.slot][(size_t) slot];
                latency += delay * 1000.0 / testRate;
                worst = juce::jmax (worst, delay * 1000.0 / testRate);
                break;
            }
        }
        for (auto u : used)
            extra += u ? 0 : 1;

        std::cout << "  Found   " << found << " of " << hits.size() << " hits, " << extra << " extra notes\n"
                  << "  Correct " << right << " of " << found << " (" << juce::String (100.0 * right / juce::jmax (1, found), 1) << " %)\n"
                  << "  Latency " << juce::String (latency / juce::jmax (1, found), 1) << " ms average, " << juce::String (worst, 1) << " ms worst\n"
                  << "  Played as:    Kick Snare  Hat\n";
        for (int slot = 0; slot < 3; ++slot)
            std::cout << "    " << Slots::names[slot].paddedRight (' ', 10) << juce::String (confusion[(size_t) slot][0]).paddedLeft (' ', 5)
                      << juce::String (confusion[(size_t) slot][1]).paddedLeft (' ', 6) << juce::String (confusion[(size_t) slot][2]).paddedLeft (' ', 5) << "\n";
    }

    int test (const juce::ArgumentList& args)
    {
        juce::Random random (args.containsOption ("--seed") ? args.getValueForOption ("--seed").getLargeIntValue() : 1);
        auto processor = makeProcessor (args, testRate);

        juce::AudioBuffer<float> audio;
        auto hits = makePattern (audio, random, 8);
        std::cout << "Untrained (by brightness)\n";
        score (hits, process (*processor, audio));

        // Training take: eight of each sound, one pad at a time, then a fresh take to score.
        for (int slot = 0; slot < 3; ++slot)
        {
            juce::AudioBuffer<float> take (1, (int) testRate * 4);
            take.clear();
            for (int i = 0; i < PluginProcessor::hitsPerTraining; ++i)
                addSound (take, (int) (testRate * (0.2 + 0.4 * i)), slot, juce::Decibels::decibelsToGain (-20.0f + 12.0f * random.nextFloat()), random);

            processor->startTraining (slot);
            process (*processor, take);
            processor->collectTraining();
            std::cout << "Trained " << Slots::names[slot] << " on " << processor->getExampleCount (slot) << " hits\n";
        }

        hits = makePattern (audio, random, 8);
        std::cout << "Trained\n";
        score (hits, process (*processor, audio));

        // The training must survive a save and reload.
        juce::MemoryBlock state;
        processor->getStateInformation (state);
        auto reloaded = makeProcessor (args, testRate);
        reloaded->setStateInformation (state.getData(), (int) state.getSize());
        std::cout << "Reloaded state: " << reloaded->getExampleCount (0) << " / " << reloaded->getExampleCount (1) << " / "
                  << reloaded->getExampleCount (2) << " examples\n";

        // CPU on the eight bars.
        const auto started = juce::Time::getMillisecondCounterHiRes();
        process (*reloaded, audio);
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - started;
        std::cout << "CPU " << juce::String (100.0 * elapsed / (1000.0 * audio.getNumSamples() / testRate), 3) << " % of one core\n";
        return 0;
    }

    int render (const juce::ArgumentList& args)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (args[1].resolveAsExistingFile()));
        if (reader == nullptr)
        {
            std::cerr << "Couldn't read " << args[1].text << "\n";
            return 1;
        }

        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
        if (audio.getNumChannels() > 1)
        {
            audio.addFrom (0, 0, audio, 1, 0, audio.getNumSamples());
            audio.applyGain (0, 0, audio.getNumSamples(), 0.5f);
        }

        auto processor = makeProcessor (args, reader->sampleRate);
        const auto notes = process (*processor, audio);

        juce::MidiMessageSequence sequence;
        for (auto& note : notes)
        {
            const auto seconds = note.sample / reader->sampleRate;
            const auto slot = (int) (std::find (Slots::defaultNotes.begin(), Slots::defaultNotes.end(), note.note) - Slots::defaultNotes.begin());
            std::cout << juce::String (seconds, 3).paddedLeft (' ', 8) << " s  "
                      << (slot < HitClassifier::numSlots ? Slots::names[slot] : juce::String (note.note)).paddedRight (' ', 9)
                      << " vel " << note.velocity << "\n";

            // 480 ticks per beat at 120 BPM: 960 ticks a second.
            sequence.addEvent (juce::MidiMessage::noteOn (10, note.note, (juce::uint8) note.velocity), seconds * 960.0);
            sequence.addEvent (juce::MidiMessage::noteOff (10, note.note), seconds * 960.0 + 40.0);
        }
        std::cout << notes.size() << " hits\n";

        if (args.size() >= 3 && ! args[2].isLongOption())
        {
            sequence.updateMatchedPairs();
            juce::MidiFile file;
            file.setTicksPerQuarterNote (480);
            file.addTrack (sequence);
            const auto out = args[2].resolveAsFile();
            out.deleteFile();
            juce::FileOutputStream stream (out);
            if (! stream.openedOk() || ! file.writeTo (stream))
                return 1;
        }
        return 0;
    }

    int snapshot (const juce::ArgumentList& args)
    {
        auto processor = makeProcessor (args, testRate);

        // A trained kick and a hit in progress, so the picture shows the pads in use.
        juce::Random random (3);
        juce::AudioBuffer<float> take (1, (int) testRate * 4);
        take.clear();
        for (int i = 0; i < PluginProcessor::hitsPerTraining; ++i)
            addSound (take, (int) (testRate * (0.2 + 0.4 * i)), 0, 0.3f, random);
        processor->startTraining (0);
        process (*processor, take);
        processor->collectTraining();
        processor->startTraining (1);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        processor->lastSlot = 0;
        processor->lastVelocity = 104;
        ++processor->hitCount;
        processor->inputLevel = 0.2f;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = args[1].resolveAsFile();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    const auto command = args.size() > 0 ? args[0].text : juce::String();
    if (command == "test")                         return test (args);
    if (command == "render" && args.size() >= 2)   return render (args);
    if (command == "snapshot" && args.size() >= 2) return snapshot (args);

    std::cerr << "Usage:\n"
                 "  BigMouthRender test [--seed=1] [--<param>=<value> ...]\n"
                 "  BigMouthRender render <input> [output.mid] [--<param>=<value> ...]\n"
                 "  BigMouthRender snapshot <output.png> [--<param>=<value> ...]\n";
    return 1;
}
