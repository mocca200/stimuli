#include "UIEngine/Palette.h"

#include "UIEngine/Fonts.h"

#include <algorithm>

namespace mvt::ui
{
namespace
{
juce::Colour toJuce (const theme::Color& colour)
{
    return juce::Colour (colour.packedARGB());
}
} // namespace

juce::Colour colourOr (const theme::ThemeRegistry& registry,
                       std::string_view token,
                       juce::Colour fallback)
{
    if (const auto colour = registry.resolveColor (token))
        return toJuce (*colour);

    return fallback;
}

juce::Font fontForToken (const theme::ThemeRegistry& registry,
                         std::string_view token,
                         float fallbackHeight,
                         bool boldFallback)
{
    // Family tokens are advisory: every face in the suite is Nunito, so a theme
    // can choose sizes and emphasis but not a different typeface.
    if (const auto value = registry.resolve (token))
    {
        if (const auto* typography = std::get_if<theme::Typography> (&*value))
        {
            const auto height = typography->sizePx > 0.0f ? typography->sizePx
                                                          : fallbackHeight;
            return fonts::forWeight (typography->weight, height);
        }
    }

    return boldFallback ? fonts::bold (fallbackHeight)
                        : fonts::normal (fallbackHeight);
}

Palette makePalette (const theme::ThemeRegistry& registry)
{
    Palette palette;
    palette.background = colourOr (registry, "color.bg", juce::Colour (0xff0b0d10));
    palette.surface = colourOr (registry, "color.surface", juce::Colour (0xff141a24));
    palette.surfaceRaised = colourOr (registry, "color.surfaceRaised", juce::Colour (0xff1b2230));
    palette.border = colourOr (registry, "color.border", juce::Colour (0xff283244));
    palette.text = colourOr (registry, "color.text", juce::Colour (0xffe8eef7));
    palette.textMuted = colourOr (registry, "color.textMuted", juce::Colour (0xff7a879b));
    palette.accent = colourOr (registry, "color.accent", juce::Colour (0xff5ee7ff));
    palette.accentWarm = colourOr (registry, "color.accentWarm", juce::Colour (0xffffb86b));
    palette.harmonic = colourOr (registry, "color.harmonic", juce::Colour (0xff7ea6ff));
    palette.percussive = colourOr (registry, "color.percussive", juce::Colour (0xffff7a90));

    palette.titleFont = fontForToken (registry, "type.title", 18.0f, true);
    palette.bodyFont = fontForToken (registry, "type.body", 14.0f);
    palette.monoFont = fontForToken (registry, "type.mono", 12.0f);
    return palette;
}

} // namespace mvt::ui
