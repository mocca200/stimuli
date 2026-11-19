#pragma once

#include "Engine/DSP/DspPipeline.h"
#include "Plugin/PluginParameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace mvt
{
class AudioProcessor final : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    AudioProcessor();
    ~AudioProcessor() override;

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

    /** All parameter state lives here, so the editor reads the same values the
        host automates and one XML blob round-trips the whole plugin. */
    juce::AudioProcessorValueTreeState state;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /** Pushes the current parameter values into the analysis pipeline. Called
        after construction, after prepare(), and after a state restore. */
    void applyAnalysisParameters();

    mvt::dsp::DspPipeline pipeline;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessor)
};
} // namespace mvt
