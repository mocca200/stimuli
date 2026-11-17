#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mvt
{
AudioProcessor::AudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // All DSP resources are allocated here, never in processBlock.
    pipeline.prepare (sampleRate, samplesPerBlock);
}

void AudioProcessor::releaseResources()
{
    pipeline.stop();
}

void AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // The analyser is deliberately delay-neutral and bit-transparent: it must
    // be safe on a master bus. All feature extraction lives on a worker thread
    // fed by a lock-free capture queue.
    if (const auto numSamples = buffer.getNumSamples(); numSamples > 0 && numInputChannels > 0)
    {
        const auto* left = buffer.getReadPointer (0);
        const auto* right = numInputChannels > 1 ? buffer.getReadPointer (1) : nullptr;
        pipeline.pushAudio (left, right, numSamples);
    }
}

juce::AudioProcessorEditor* AudioProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void AudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    destination.setSize (0, false);
}

void AudioProcessor::setStateInformation (const void*, int)
{
}

bool AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
}

} // namespace mvt

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mvt::AudioProcessor();
}
