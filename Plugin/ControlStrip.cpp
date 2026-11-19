#include "ControlStrip.h"
#include "PluginParameters.h"

#include "UIEngine/Fonts.h"

namespace mvt
{
namespace
{
constexpr auto kBarBackground = 0xff0b0f14;
constexpr auto kBarBorder = 0xff283244;
constexpr auto kCaptionText = 0xff8b98a8;
constexpr auto kControlBackground = 0xff141a24;
constexpr auto kControlText = 0xffe8eef7;
constexpr auto kAccent = 0xff5ee7ff;
} // namespace

ControlStrip::ControlStrip (juce::AudioProcessorValueTreeState& state)
{
    setOpaque (true);

    styleCaption (gainCaption, "Input gain");
    styleCaption (smoothingCaption, "Smoothing");
    styleCaption (flashCaption, "Flash limit");
    styleCaption (skinCaption, "Skin");

    styleSlider (gainSlider, " dB");
    styleSlider (smoothingSlider, " ms");

    for (auto* box : { &flashBox, &skinBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, juce::Colour (kControlBackground));
        box->setColour (juce::ComboBox::textColourId, juce::Colour (kControlText));
        box->setColour (juce::ComboBox::outlineColourId, juce::Colour (kBarBorder));
        box->setColour (juce::ComboBox::arrowColourId, juce::Colour (kCaptionText));
    }

    // Items must exist before the attachment runs, so it can select the id that
    // matches the current parameter index.
    addChoiceItems (state, params::id::flashLimit, flashBox);
    addChoiceItems (state, params::id::skin, skinBox);

    addAndMakeVisible (gainCaption);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (smoothingCaption);
    addAndMakeVisible (smoothingSlider);
    addAndMakeVisible (flashCaption);
    addAndMakeVisible (flashBox);
    addAndMakeVisible (skinCaption);
    addAndMakeVisible (skinBox);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::id::inputGain, gainSlider);
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::id::smoothing, smoothingSlider);
    flashAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::id::flashLimit, flashBox);
    skinAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::id::skin, skinBox);
}

ControlStrip::~ControlStrip()
{
    // Destroy the attachments while the controls they point at are still alive.
    gainAttachment.reset();
    smoothingAttachment.reset();
    flashAttachment.reset();
    skinAttachment.reset();
}

void ControlStrip::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (kBarBackground));
    graphics.setColour (juce::Colour (kBarBorder));
    graphics.fillRect (0, 0, getWidth(), 1);
}

void ControlStrip::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    const auto columnWidth = area.getWidth() / 4;

    const auto place = [&area, columnWidth] (juce::Label& caption, juce::Component& control)
    {
        auto column = area.removeFromLeft (columnWidth).reduced (6, 0);
        caption.setBounds (column.removeFromTop (14));
        control.setBounds (column.removeFromTop (24));
    };

    place (gainCaption, gainSlider);
    place (smoothingCaption, smoothingSlider);
    place (flashCaption, flashBox);
    place (skinCaption, skinBox);
}

void ControlStrip::addChoiceItems (juce::AudioProcessorValueTreeState& state,
                                   const juce::String& parameterID,
                                   juce::ComboBox& box)
{
    const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (state.getParameter (parameterID));

    if (choice == nullptr)
        return;

    for (int index = 0; index < choice->choices.size(); ++index)
        box.addItem (choice->choices[index], index + 1);
}

void ControlStrip::styleCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (ui::fonts::normal (12.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (kCaptionText));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
}

void ControlStrip::styleSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (1);
    slider.setScrollWheelEnabled (false);
    slider.setColour (juce::Slider::trackColourId, juce::Colour (kAccent));
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (kControlBackground));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (kControlText));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (kControlText));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}
} // namespace mvt
