#pragma once

#include "Engine/Layout/LayoutTypes.h"

#include <functional>
#include <optional>
#include <string_view>

namespace juce
{
class var;
}

namespace mvt::layout
{
using DimensionTokenResolver = std::function<std::optional<float> (std::string_view)>;

/** Parses a "mvt.layout/1" document. Token resolution is injected so the
    layout module has no dependency on the theme module. */
[[nodiscard]] ParseResult parseLayout (const juce::var& document,
                                       const DimensionTokenResolver& resolveToken = {});

} // namespace mvt::layout
