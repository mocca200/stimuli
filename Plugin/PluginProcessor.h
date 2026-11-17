#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/DSP/DspPipeline.h"

namespace mvt
{
class AudioProcessor final : public juce::AudioProcessor
{
public:
    AudioProcessor();
    ~AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
    [[nodiscard]] bool hasEditor() const override { return true; }

    [[nodiscard]] const juce::String getName() const override { return JucePlugin_Name; }
    [[nodiscard]] bool acceptsMidi() const override { return false; }
    [[nodiscard]] bool producesMidi() const override { return false; }
    [[nodiscard]] bool isMidiEffect() const override { return false; }
    [[nodiscard]] double getTailLengthSeconds() const override { return 0.0; }

    [[nodiscard]] int getNumPrograms() override { return 1; }
    [[nodiscard]] int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    [[nodiscard]] const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    [[nodiscard]] bool readSnapshot (dsp::AnalysisSnapshot& destination) noexcept
    {
        return pipeline.readSnapshot (destination);
    }

    [[nodiscard]] bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

private:
    mvt::dsp::DspPipeline pipeline;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessor)
};
} // namespace mvt
