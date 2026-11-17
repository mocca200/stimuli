#pragma once

#include "Engine/Theme/Color.h"

#include <string>
#include <variant>

namespace mvt::theme
{
struct Dimension
{
    float pixels = 0.0f;
};

struct Shadow
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blur = 0.0f;
    Color color;
};

struct Typography
{
    std::string family = "Inter";
    float sizePx = 14.0f;
    int weight = 400;
    float lineHeightPx = 0.0f;
};

using TokenValue = std::variant<std::monostate, Color, Dimension, float, bool, std::string, Shadow, Typography>;

[[nodiscard]] std::string tokenValueToString (const TokenValue& value);

} // namespace mvt::theme
