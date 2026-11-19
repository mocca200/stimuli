#pragma once

#include "Plugin/ControlStrip.h"
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

    /** Reads the automatable parameters and applies any change to the skin or
        the flash budget. Reads atomics only, so it is safe at 60 Hz. */
    void syncParameters();

    /** Loads the skin named by the Skin choice parameter, falling back to the
        built-in document when that skin folder is missing. */
    void loadSkin (int skinIndex);

    /** The area the visualisation owns: everything above the control strip. */
    [[nodiscard]] juce::Rectangle<int> visualisationBounds() const noexcept;

    ui::NunitoLookAndFeel nunitoLookAndFeel;
    AudioProcessor& processor;

    // Always visible: the standalone app has no host UI to reach the parameters
    // from. Sits on top of the visualisation at the bottom of the editor.
    ControlStrip controlStrip;

    ui::Skin skin;
    ui::RenderEngine renderEngine;
    ui::FrameSafetyGate safetyGate;
    dsp::AnalysisSnapshot snapshot;
    juce::String loadError;

    // -1 forces the first sync to apply both values, so a restored state is in
    // effect before the first frame is painted.
    int currentSkinIndex = -1;
    int currentFlashIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
} // namespace mvt
