#pragma once

#include "Engine/Layout/LayoutTypes.h"

#include <string_view>

namespace mvt::layout
{
class FlexSolver
{
public:
    /** Solves a parsed layout inside a definite viewport. */
    [[nodiscard]] static LayoutBox solve (const LayoutNode& root,
                                          Size available,
                                          float pixelScale = 0.0f);

    [[nodiscard]] static const LayoutBox* hitTest (const LayoutBox& root, Point point) noexcept;

    [[nodiscard]] static const LayoutBox* findById (const LayoutBox& root,
                                                    std::string_view id) noexcept;
};

} // namespace mvt::layout
