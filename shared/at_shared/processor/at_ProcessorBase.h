#pragma once

namespace at
{
    /** Common AudioProcessor boilerplate: owns the APVTS and preset manager, handles state
        save/restore, programs and bus layouts. Plugins implement prepareToPlay, processBlock
        and createEditor. */
    class ProcessorBase : public juce::AudioProcessor
    {
    public:
        explicit ProcessorBase (juce::AudioProcessorValueTreeState::ParameterLayout layout,
                                BusesProperties buses = defaultBuses());

        static BusesProperties defaultBuses();

        juce::AudioProcessorValueTreeState apvts;
        PresetManager presets { apvts };

        //==============================================================================
        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override           { return JucePlugin_WantsMidiInput; }
        bool producesMidi() const override          { return JucePlugin_ProducesMidiOutput; }
        bool isMidiEffect() const override          { return JucePlugin_IsMidiEffect; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override                             { return 1; }
        int getCurrentProgram() override                          { return 0; }
        void setCurrentProgram (int) override                     {}
        const juce::String getProgramName (int) override          { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        bool hasEditor() const override { return true; }
        void releaseResources() override {}

        bool isBusesLayoutSupported (const BusesLayout&) const override;

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        using juce::AudioProcessor::processBlock;
    };
}
