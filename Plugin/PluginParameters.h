#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace mvt::params
{
/** Parameter identifiers. Short, stable, host-visible automation names. */
namespace id
{
inline constexpr auto inputGain = "inputGain";
inline constexpr auto smoothing = "smoothing";
inline constexpr auto flashLimit = "flashLimit";
inline constexpr auto skin = "skin";
} // namespace id

/** Indices into the Flash Limit choice parameter. */
namespace flashLimitIndex
{
inline constexpr int standard = 0;
inline constexpr int strict = 1;
} // namespace flashLimitIndex

/** Skin folder names, matching the folders under Assets/skins. */
namespace skinName
{
inline constexpr auto standard = "default";
inline constexpr auto showcase = "showcase";
} // namespace skinName

/** Indices into the Skin choice parameter, in skinChoices() order. */
namespace skinIndex
{
inline constexpr int standard = 0;
inline constexpr int showcase = 1;
} // namespace skinIndex

/** Human-readable skin names, in choice-parameter order. */
[[nodiscard]] inline juce::StringArray skinChoices()
{
    return { "Default", "Showcase" };
}

/**
 * The plugin's automatable parameters.

 * Deliberately small: everything here changes what the visualiser shows, never
 * the audio that passes through the plugin. The analyser stays bit-transparent,
 * which is what lets it sit on a master bus.
 */
[[nodiscard]] inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Visual sensitivity. Boosting it raises quiet material into view; it does
    // not change the audio, only how much of it the analysis resolves.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id::inputGain, 1 },
        "Input Gain",
        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // One-pole smoothing time constant for the spectral views. 0 is off.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id::smoothing, 1 },
        "Smoothing",
        juce::NormalisableRange<float> { 0.0f, 250.0f, 1.0f },
        60.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // The photosensitivity gate keeps final authority either way; this only
    // picks how conservative its flash budget is. There is no "off" setting.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id::flashLimit, 1 },
        "Flash Limit",
        juce::StringArray { "Standard", "Strict" },
        flashLimitIndex::standard));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id::skin, 1 },
        "Skin",
        skinChoices(),
        skinIndex::standard));

    return layout;
}
} // namespace mvt::params
