namespace at
{
    ProcessorBase::ProcessorBase (juce::AudioProcessorValueTreeState::ParameterLayout layout,
                                  BusesProperties buses)
        : juce::AudioProcessor (std::move (buses)),
          apvts (*this, nullptr, "PARAMETERS", std::move (layout))
    {
    }

    juce::AudioProcessor::BusesProperties ProcessorBase::defaultBuses()
    {
        BusesProperties buses;
       #if ! JucePlugin_IsSynth
        buses = buses.withInput ("Input", juce::AudioChannelSet::stereo(), true);
       #endif
        return buses.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }

    bool ProcessorBase::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;

       #if ! JucePlugin_IsSynth
        if (layouts.getMainInputChannelSet() != out)
            return false;
       #endif

        return true;
    }

    void ProcessorBase::getStateInformation (juce::MemoryBlock& dest)
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void ProcessorBase::setStateInformation (const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}
