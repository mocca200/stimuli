#include "Engine/Theme/TokenValue.h"

#include <iomanip>
#include <sstream>

namespace mvt::theme
{
std::string tokenValueToString (const TokenValue& value)
{
    if (const auto* color = std::get_if<Color> (&value))
        return color->toHex (true);

    if (const auto* dimension = std::get_if<Dimension> (&value))
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision (2) << dimension->pixels << "px";
        return stream.str();
    }

    if (const auto* number = std::get_if<float> (&value))
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision (2) << *number;
        return stream.str();
    }

    if (const auto* flag = std::get_if<bool> (&value))
        return *flag ? "true" : "false";

    if (const auto* text = std::get_if<std::string> (&value))
        return *text;

    if (const auto* shadow = std::get_if<Shadow> (&value))
    {
        std::ostringstream stream;
        stream << "shadow(" << shadow->offsetX << ", " << shadow->offsetY
               << ", " << shadow->blur << ", " << shadow->color.toHex (true) << ")";
        return stream.str();
    }

    if (const auto* type = std::get_if<Typography> (&value))
    {
        std::ostringstream stream;
        stream << "type(" << type->family << ", " << type->sizePx << "px, "
               << type->weight << ", lh=" << type->lineHeightPx << ")";
        return stream.str();
    }

    return "null";
}

} // namespace mvt::theme
