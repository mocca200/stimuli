#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace mvt
{
/** Small bar along the bottom of the plugin editor holding the four parameters.

    The standalone app has no host UI to automate from, so without this the
    parameters are unreachable there. Every control is bound with an APVTS
    attachment, so the bar and the host automation always show the same value
    and neither has to poll the other.
*/
class ControlStrip final : public juce::Component
{
public:
    explicit ControlStrip (juce::AudioProcessorValueTreeState& state);
    ~ControlStrip() override;

    void paint (juce::Graphics& graphics) override;
    void resized() override;

    /** Height the editor reserves for the bar. */
    static constexpr int preferredHeight = 58;

private:
    void addChoiceItems (juce::AudioProcessorValueTreeState& state,
                         const juce::String& parameterID,
                         juce::ComboBox& box);
    void styleCaption (juce::Label& label, const juce::String& text);
    void styleSlider (juce::Slider& slider, const juce::String& suffix);

    juce::Label gainCaption;
    juce::Label smoothingCaption;
    juce::Label flashCaption;
    juce::Label skinCaption;
    juce::Slider gainSlider;
    juce::Slider smoothingSlider;
    juce::ComboBox flashBox;
    juce::ComboBox skinBox;

    // Declared after the controls so the attachments detach before the controls
    // they observe are destroyed.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> flashAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> skinAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlStrip)
};
} // namespace mvt
