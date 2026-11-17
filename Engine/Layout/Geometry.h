#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace mvt::layout
{
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Size
{
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    [[nodiscard]] float right() const noexcept { return x + width; }
    [[nodiscard]] float bottom() const noexcept { return y + height; }
    [[nodiscard]] Point centre() const noexcept { return { x + width * 0.5f, y + height * 0.5f }; }
    [[nodiscard]] bool contains (Point p) const noexcept
    {
        return p.x >= x && p.y >= y && p.x < right() && p.y < bottom();
    }

    [[nodiscard]] Rect inset (float top, float rightInset, float bottomInset, float left) const noexcept
    {
        return { x + left,
                 y + top,
                 std::max (0.0f, width - left - rightInset),
                 std::max (0.0f, height - top - bottomInset) };
    }
};

struct EdgeInsets
{
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    [[nodiscard]] float horizontal() const noexcept { return left + right; }
    [[nodiscard]] float vertical() const noexcept { return top + bottom; }
    [[nodiscard]] bool isZero() const noexcept
    {
        return top == 0.0f && right == 0.0f && bottom == 0.0f && left == 0.0f;
    }
};

enum class LengthKind
{
    autoValue,
    pixels,
    percent
};

struct Length
{
    LengthKind kind = LengthKind::autoValue;
    float value = 0.0f;

    [[nodiscard]] static Length px (float v) noexcept { return { LengthKind::pixels, v }; }
    [[nodiscard]] static Length percent (float v) noexcept { return { LengthKind::percent, v }; }
    [[nodiscard]] static Length automatic() noexcept { return {}; }
    [[nodiscard]] bool isDefinite() const noexcept { return kind != LengthKind::autoValue; }
};

inline float resolveLength (const Length& length, float reference, float fallback = 0.0f) noexcept
{
    switch (length.kind)
    {
        case LengthKind::pixels:   return length.value;
        case LengthKind::percent:  return std::isfinite (reference) ? reference * length.value / 100.0f : fallback;
        case LengthKind::autoValue: break;
    }

    return fallback;
}

inline float snapToPixel (float value, float scale) noexcept
{
    if (scale <= 0.0f)
        return value;

    return std::round (value * scale) / scale;
}

inline Rect snapRect (const Rect& r, float scale) noexcept
{
    const auto left   = snapToPixel (r.x, scale);
    const auto top    = snapToPixel (r.y, scale);
    const auto right  = snapToPixel (r.right(), scale);
    const auto bottom = snapToPixel (r.bottom(), scale);
    return { left, top, std::max (0.0f, right - left), std::max (0.0f, bottom - top) };
}

} // namespace mvt::layout
