#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mvt
{
AudioProcessor::AudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , state (*this, nullptr, "PARAMETERS", params::createLayout())
{
    state.addParameterListener (params::id::inputGain, this);
    state.addParameterListener (params::id::smoothing, this);
    applyAnalysisParameters();
}

AudioProcessor::~AudioProcessor()
{
    state.removeParameterListener (params::id::inputGain, this);
    state.removeParameterListener (params::id::smoothing, this);
}

void AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // All DSP resources are allocated here, never in processBlock.
    pipeline.prepare (sampleRate, samplesPerBlock);

    // prepare() rebuilds the worker, so re-seed it with the live parameters.
    applyAnalysisParameters();
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
    if (const auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destination);
    else
        destination.setSize (0, false);
}

void AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);

    // Ignore state written by a different plugin or a broken document; keep the
    // current parameters rather than resetting the user's session to defaults.
    if (! tree.isValid() || ! tree.hasType (state.state.getType()))
        return;

    state.replaceState (tree);
    applyAnalysisParameters();
}

void AudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Called from whichever thread changed the parameter, including the audio
    // thread during automation. Only atomic stores and two pure maths calls
    // happen here: no allocation, no locks, no I/O.
    if (parameterID == params::id::inputGain)
        pipeline.setAnalysisGainDecibels (newValue);
    else if (parameterID == params::id::smoothing)
        pipeline.setSmoothingMilliseconds (newValue);
}

void AudioProcessor::applyAnalysisParameters()
{
    if (const auto* gain = state.getRawParameterValue (params::id::inputGain))
        pipeline.setAnalysisGainDecibels (gain->load());

    if (const auto* smoothing = state.getRawParameterValue (params::id::smoothing))
        pipeline.setSmoothingMilliseconds (smoothing->load());
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
