#pragma once

#include "Engine/Layout/Geometry.h"

#include <optional>
#include <string>
#include <vector>

namespace mvt::layout
{
enum class Direction
{
    row,
    column
};

enum class JustifyContent
{
    start,
    centre,
    end,
    spaceBetween,
    spaceAround,
    spaceEvenly
};

enum class AlignItems
{
    start,
    centre,
    end,
    stretch
};

enum class Position
{
    relative,
    absolute
};

enum class Display
{
    flex,
    none
};

struct ResolvedEdges
{
    Length top;
    Length right;
    Length bottom;
    Length left;

    [[nodiscard]] bool anyDefinite() const noexcept
    {
        return top.isDefinite() || right.isDefinite() || bottom.isDefinite() || left.isDefinite();
    }
};

struct Style
{
    std::optional<Length> width;
    std::optional<Length> height;
    std::optional<Length> minWidth;
    std::optional<Length> minHeight;
    std::optional<Length> maxWidth;
    std::optional<Length> maxHeight;
    std::optional<Length> flexBasis;

    ResolvedEdges margin;
    ResolvedEdges padding;

    float flexGrow = 0.0f;
    float flexShrink = 1.0f;

    Direction direction = Direction::row;
    JustifyContent justify = JustifyContent::start;
    AlignItems align = AlignItems::stretch;
    std::optional<AlignItems> alignSelf;

    Position position = Position::relative;
    ResolvedEdges inset;
    Display display = Display::flex;

    Length gap;
    std::optional<float> aspectRatio;
};

struct LayoutNode
{
    std::string id;
    std::string type = "container";
    std::string text;
    Style style;
    std::vector<LayoutNode> children;

    [[nodiscard]] const LayoutNode* findById (std::string_view wantedId) const noexcept;
};

struct LayoutError
{
    std::string path;
    std::string message;
    bool warning = false;
};

struct ParseResult
{
    std::optional<LayoutNode> root;
    std::vector<LayoutError> errors;

    [[nodiscard]] bool ok() const noexcept
    {
        for (const auto& error : errors)
            if (! error.warning)
                return false;

        return root.has_value();
    }
};

struct LayoutBox
{
    std::string id;
    std::string type;
    std::string text;
    Rect frame;
    Rect content;
    std::vector<LayoutBox> children;
};

} // namespace mvt::layout
