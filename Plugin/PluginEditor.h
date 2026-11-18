#pragma once

#include "UIEngine/FrameSafetyGate.h"
#include "UIEngine/Fonts.h"
#include "UIEngine/RenderEngine.h"
#include "UIEngine/Skin.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace mvt
{
class AudioProcessor;

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (AudioProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;

    ui::NunitoLookAndFeel nunitoLookAndFeel;
    AudioProcessor& processor;
    ui::Skin skin;
    ui::RenderEngine renderEngine;
    ui::FrameSafetyGate safetyGate;
    dsp::AnalysisSnapshot snapshot;
    juce::String loadError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
} // namespace mvt
