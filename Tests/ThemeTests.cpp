#include "Engine/Theme/ThemeRegistry.h"

#include <catch2/catch_all.hpp>
#include <juce_core/juce_core.h>

using namespace mvt::theme;

namespace
{
juce::var parseJson (const char* json)
{
    return juce::JSON::parse (juce::String (json));
}
} // namespace

TEST_CASE ("Theme tokens resolve colours and dimensions", "[theme]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": {
            "color.bg": "#0B0D10",
            "color.accent": "#5EE7FF",
            "space.md": "12px",
            "radius.lg": "16px",
            "opacity.high": 0.9
        }
    })")));

    const auto background = theme.resolveColor ("color.bg");
    REQUIRE (background.has_value());
    REQUIRE (background->r == 0x0B);
    REQUIRE (background->g == 0x0D);
    REQUIRE (background->b == 0x10);

    const auto spacing = theme.resolveDimension ("space.md");
    REQUIRE (spacing.has_value());
    REQUIRE (spacing->pixels == 12.0f);

    const auto opacity = theme.resolve ("opacity.high");
    REQUIRE (opacity.has_value());
    REQUIRE (std::get<float> (*opacity) == Catch::Approx (0.9f));
}

TEST_CASE ("Theme aliases resolve through chains", "[theme][aliases]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": {
            "color.base": "#112233",
            "color.surface": "$color.base",
            "color.panel": "$color.surface",
            "space.sm": "8px",
            "space.card": "$space.sm"
        }
    })")));

    REQUIRE (theme.resolveColor ("color.panel")->toHex() == "#112233");
    REQUIRE (theme.resolveDimension ("space.card")->pixels == 8.0f);
}

TEST_CASE ("Theme alias cycles are rejected", "[theme][aliases]")
{
    ThemeRegistry theme;
    const auto loaded = theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": {
            "a": "$b",
            "b": "$c",
            "c": "$a"
        }
    })"));

    REQUIRE_FALSE (loaded);
    REQUIRE_FALSE (theme.errors().empty());
    REQUIRE (theme.errors().front().message.find ("cycle") != std::string::npos);
}

TEST_CASE ("Unknown aliases produce a useful error", "[theme][aliases]")
{
    ThemeRegistry theme;
    REQUIRE_FALSE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": { "color.a": "$color.missing" }
    })")));

    REQUIRE_FALSE (theme.errors().empty());
    REQUIRE (theme.errors().front().message.find ("unknown token") != std::string::npos);
}

TEST_CASE ("Variant overrides take precedence over base tokens", "[theme][variants]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": {
            "color.bg": "#101010",
            "color.accent": "#00FFAA",
            "space.md": "12px"
        },
        "variants": {
            "highContrast": {
                "color.bg": "#000000",
                "color.accent": "#FFFFFF"
            }
        }
    })")));

    REQUIRE (theme.resolveColor ("color.bg", "highContrast")->toHex() == "#000000");
    REQUIRE (theme.resolveColor ("color.accent", "highContrast")->toHex() == "#FFFFFF");
    REQUIRE (theme.resolveDimension ("space.md", "highContrast")->pixels == 12.0f);
    REQUIRE (theme.hasVariant ("highContrast"));
}

TEST_CASE ("Runtime override invalidates the generation counter", "[theme][overrides]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": { "color.bg": "#101010", "space.md": "12px" }
    })")));

    const auto before = theme.generation();
    theme.setOverride ("color.bg", Color { 255, 0, 0, 255 });
    REQUIRE (theme.generation() > before);
    REQUIRE (theme.resolveColor ("color.bg")->toHex() == "#FF0000");

    REQUIRE (theme.clearOverride ("color.bg"));
    REQUIRE (theme.resolveColor ("color.bg")->toHex() == "#101010");
    REQUIRE_FALSE (theme.clearOverride ("color.bg"));
}

TEST_CASE ("Shadow and typography composite tokens parse", "[theme][composites]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": {
            "color.shadow": "#00000080",
            "shadow.panel": { "x": 0, "y": 4, "blur": 12, "color": "$color.shadow" },
            "type.body": { "family": "Inter", "size": "14px", "weight": 450, "lineHeight": 20 }
        }
    })")));

    const auto shadow = theme.resolve ("shadow.panel");
    REQUIRE (shadow.has_value());
    const auto& value = std::get<Shadow> (*shadow);
    REQUIRE (value.offsetY == 4.0f);
    REQUIRE (value.blur == 12.0f);
    REQUIRE (value.color.a == 0x80);

    const auto type = theme.resolve ("type.body");
    REQUIRE (type.has_value());
    const auto& typography = std::get<Typography> (*type);
    REQUIRE (typography.family == "Inter");
    REQUIRE (typography.sizePx == 14.0f);
    REQUIRE (typography.weight == 450);
    REQUIRE (typography.lineHeightPx == 20.0f);
}

TEST_CASE ("Unknown tokens return nullopt without corrupting the registry", "[theme]")
{
    ThemeRegistry theme;
    REQUIRE (theme.loadFromJson (parseJson (R"({
        "$schema": "mvt.theme/1",
        "tokens": { "color.bg": "#000000" }
    })")));

    std::string error;
    REQUIRE_FALSE (theme.resolve ("color.nope", {}, &error).has_value());
    REQUIRE (error.find ("unknown token") != std::string::npos);
    REQUIRE (theme.resolveColor ("color.bg")->toHex() == "#000000");
}

TEST_CASE ("Relative luminance follows WCAG", "[theme][colour]")
{
    REQUIRE (Color { 255, 255, 255, 255 }.relativeLuminance() == Catch::Approx (1.0f).margin (0.0001f));
    REQUIRE (Color { 0, 0, 0, 255 }.relativeLuminance() == Catch::Approx (0.0f).margin (0.0001f));
    REQUIRE (Color { 255, 255, 255, 255 }.contrastRatio (Color { 0, 0, 0, 255 })
             == Catch::Approx (21.0f).margin (0.01f));
}
