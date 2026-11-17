#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mvt::theme
{
struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    [[nodiscard]] constexpr std::uint32_t packedARGB() const noexcept
    {
        return (static_cast<std::uint32_t> (a) << 24)
             | (static_cast<std::uint32_t> (r) << 16)
             | (static_cast<std::uint32_t> (g) << 8)
             | static_cast<std::uint32_t> (b);
    }

    [[nodiscard]] constexpr bool operator== (const Color& other) const noexcept
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    [[nodiscard]] constexpr bool operator!= (const Color& other) const noexcept
    {
        return ! (*this == other);
    }

    /** WCAG relative luminance in the sRGB colour space. */
    [[nodiscard]] float relativeLuminance() const noexcept
    {
        const auto channel = [] (std::uint8_t value) noexcept
        {
            const auto c = static_cast<float> (value) / 255.0f;
            return c <= 0.04045f ? c / 12.92f : std::pow ((c + 0.055f) / 1.055f, 2.4f);
        };

        return 0.2126f * channel (r) + 0.7152f * channel (g) + 0.0722f * channel (b);
    }

    [[nodiscard]] float contrastRatio (const Color& other) const noexcept
    {
        const auto first = relativeLuminance();
        const auto second = other.relativeLuminance();
        const auto lighter = std::max (first, second);
        const auto darker = std::min (first, second);
        return (lighter + 0.05f) / (darker + 0.05f);
    }

    [[nodiscard]] static std::optional<Color> parse (std::string_view text) noexcept
    {
        if (text.empty() || text.front() != '#')
            return std::nullopt;

        text.remove_prefix (1);

        auto nibble = [] (char c) noexcept -> int
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        auto byteAt = [&] (std::size_t index) noexcept -> int
        {
            const auto high = nibble (text[index]);
            const auto low = nibble (text[index + 1]);
            return (high < 0 || low < 0) ? -1 : high * 16 + low;
        };

        auto expand = [&] (char c) noexcept -> int
        {
            const auto v = nibble (c);
            return v < 0 ? -1 : v * 17;
        };

        if (text.size() == 3 || text.size() == 4)
        {
            const auto r = expand (text[0]);
            const auto g = expand (text[1]);
            const auto b = expand (text[2]);
            const auto a = text.size() == 4 ? expand (text[3]) : 255;
            if (r < 0 || g < 0 || b < 0 || a < 0)
                return std::nullopt;
            return Color { static_cast<std::uint8_t> (r), static_cast<std::uint8_t> (g),
                           static_cast<std::uint8_t> (b), static_cast<std::uint8_t> (a) };
        }

        if (text.size() == 6 || text.size() == 8)
        {
            const auto r = byteAt (0);
            const auto g = byteAt (2);
            const auto b = byteAt (4);
            const auto a = text.size() == 8 ? byteAt (6) : 255;
            if (r < 0 || g < 0 || b < 0 || a < 0)
                return std::nullopt;
            return Color { static_cast<std::uint8_t> (r), static_cast<std::uint8_t> (g),
                           static_cast<std::uint8_t> (b), static_cast<std::uint8_t> (a) };
        }

        return std::nullopt;
    }

    [[nodiscard]] std::string toHex (bool includeAlpha = false) const
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string result = "#";
        for (const auto value : { r, g, b })
        {
            result.push_back (digits[(value >> 4) & 0x0f]);
            result.push_back (digits[value & 0x0f]);
        }
        if (includeAlpha)
        {
            result.push_back (digits[(a >> 4) & 0x0f]);
            result.push_back (digits[a & 0x0f]);
        }
        return result;
    }
};

} // namespace mvt::theme
