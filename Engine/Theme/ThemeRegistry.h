#pragma once

#include "Engine/Theme/TokenValue.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mvt::theme
{
struct ThemeError
{
    std::string path;
    std::string message;
    bool warning = false;
};

/**
 * Token registry for a single theme document.

 * Resolution order: runtime override -> variant override -> base token.
 * Aliases use "$token.id"; cycles are detected at load and at runtime.

 * Not thread-safe for writes: the designer mutates it on the message thread,
 * while the renderer reads an immutable resolved cache. The generation counter
 * is atomic so caches can cheaply detect invalidation.
 */
class ThemeRegistry
{
public:
    ThemeRegistry() = default;

    ThemeRegistry (const ThemeRegistry&) = delete;
    ThemeRegistry& operator= (const ThemeRegistry&) = delete;

    /** Registries are moved, not copied: the designer builds a validated
        candidate on the side and swaps it in. */
    ThemeRegistry (ThemeRegistry&& other) noexcept;
    ThemeRegistry& operator= (ThemeRegistry&& other) noexcept;

    [[nodiscard]] bool loadFromJson (const juce::var& document);

    [[nodiscard]] std::optional<TokenValue> resolve (std::string_view tokenId,
                                                     std::string_view variant = {},
                                                     std::string* error = nullptr) const;

    [[nodiscard]] std::optional<Dimension> resolveDimension (std::string_view tokenId,
                                                             std::string_view variant = {},
                                                             std::string* error = nullptr) const;

    [[nodiscard]] std::optional<Color> resolveColor (std::string_view tokenId,
                                                     std::string_view variant = {},
                                                     std::string* error = nullptr) const;

    void setOverride (std::string tokenId, TokenValue value);
    bool clearOverride (std::string_view tokenId);

    [[nodiscard]] bool hasToken (std::string_view tokenId, std::string_view variant = {}) const;
    [[nodiscard]] bool hasVariant (std::string_view variant) const;
    [[nodiscard]] std::vector<std::string> tokenIds() const;
    [[nodiscard]] std::vector<std::string> variants() const;
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_.load (std::memory_order_acquire); }
    [[nodiscard]] const std::vector<ThemeError>& errors() const noexcept { return errors_; }

private:
    using TokenMap = std::unordered_map<std::string, juce::var>;

    [[nodiscard]] const juce::var* findRaw (std::string_view tokenId, std::string_view variant) const;

    [[nodiscard]] std::optional<TokenValue> resolveInternal (std::string_view tokenId,
                                                             std::string_view variant,
                                                             std::vector<std::string>& chain,
                                                             std::string* error) const;

    [[nodiscard]] std::optional<TokenValue> convertValue (const juce::var& value,
                                                          std::string_view variant,
                                                          std::vector<std::string>& chain,
                                                          std::string* error) const;

    [[nodiscard]] std::optional<float> numberValue (const juce::var& value,
                                                    std::string_view variant,
                                                    std::vector<std::string>& chain,
                                                    std::string* error) const;

    TokenMap baseTokens_;
    std::unordered_map<std::string, TokenMap> variants_;
    std::unordered_map<std::string, TokenValue> overrides_;
    std::vector<ThemeError> errors_;
    std::atomic<std::uint64_t> generation_ { 0 };
};

} // namespace mvt::theme
