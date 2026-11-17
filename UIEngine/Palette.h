#pragma once

#include "Engine/Theme/ThemeRegistry.h"

#include <juce_graphics/juce_graphics.h>

namespace mvt::ui
{
/** Colours and fonts resolved from a theme document. Rebuilt whenever the
    theme generation counter changes, never during paint. */
struct Palette
{
    juce::Colour background;
    juce::Colour surface;
    juce::Colour surfaceRaised;
    juce::Colour border;
    juce::Colour text;
    juce::Colour textMuted;
    juce::Colour accent;
    juce::Colour accentWarm;
    juce::Colour harmonic;
    juce::Colour percussive;

    juce::Font titleFont;
    juce::Font bodyFont;
    juce::Font monoFont;
};

[[nodiscard]] Palette makePalette (const theme::ThemeRegistry& registry);

[[nodiscard]] juce::Colour colourOr (const theme::ThemeRegistry& registry,
                                     std::string_view token,
                                     juce::Colour fallback);

[[nodiscard]] juce::Font fontForToken (const theme::ThemeRegistry& registry,
                                       std::string_view token,
                                       float fallbackHeight,
                                       bool boldFallback = false);

} // namespace mvt::ui
