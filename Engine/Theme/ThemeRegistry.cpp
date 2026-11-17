#include "Engine/Theme/ThemeRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <utility>

namespace mvt::theme
{
namespace
{
std::string trimmed (std::string text)
{
    const auto notSpace = [] (unsigned char c) { return std::isspace (c) == 0; };

    auto begin = text.begin();
    while (begin != text.end() && ! notSpace (static_cast<unsigned char> (*begin)))
        ++begin;

    auto end = text.end();
    while (end != begin && ! notSpace (static_cast<unsigned char> (*(end - 1))))
        --end;

    return { begin, end };
}

std::string describePath (std::string_view id, std::string_view variant)
{
    if (variant.empty())
        return "tokens." + std::string (id);

    return "variants." + std::string (variant) + "." + std::string (id);
}

bool parseFloat (std::string_view text, float& result)
{
    try
    {
        const auto value = std::stof (std::string (text));
        if (! std::isfinite (value))
            return false;
        result = value;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace

ThemeRegistry::ThemeRegistry (ThemeRegistry&& other) noexcept
    : baseTokens_ (std::move (other.baseTokens_)),
      variants_ (std::move (other.variants_)),
      overrides_ (std::move (other.overrides_)),
      errors_ (std::move (other.errors_)),
      generation_ (other.generation_.load (std::memory_order_acquire))
{
}

ThemeRegistry& ThemeRegistry::operator= (ThemeRegistry&& other) noexcept
{
    if (this != &other)
    {
        baseTokens_ = std::move (other.baseTokens_);
        variants_ = std::move (other.variants_);
        overrides_ = std::move (other.overrides_);
        errors_ = std::move (other.errors_);
        generation_.store (other.generation_.load (std::memory_order_acquire),
                           std::memory_order_release);
    }

    return *this;
}

bool ThemeRegistry::loadFromJson (const juce::var& document)
{
    baseTokens_.clear();
    variants_.clear();
    overrides_.clear();
    errors_.clear();

    if (! document.isObject())
    {
        errors_.push_back ({ "document", "theme document must be a JSON object", false });
        return false;
    }

    const auto* root = document.getDynamicObject();
    jassert (root != nullptr);

    if (root->hasProperty ("$schema"))
    {
        const auto schema = document.getProperty ("$schema", {}).toString();
        if (schema.isNotEmpty() && schema != "mvt.theme/1")
            errors_.push_back ({ "$schema", "unsupported theme schema: " + schema.toStdString(), false });
    }

    if (! root->hasProperty ("tokens"))
    {
        errors_.push_back ({ "tokens", "theme requires a tokens object", false });
        return false;
    }

    const auto tokensValue = document.getProperty ("tokens", {});
    const auto* tokensObject = tokensValue.getDynamicObject();

    if (tokensObject == nullptr)
    {
        errors_.push_back ({ "tokens", "tokens must be a JSON object", false });
        return false;
    }

    for (const auto& property : tokensObject->getProperties())
        baseTokens_[property.name.toString().toStdString()] = property.value;

    if (root->hasProperty ("variants"))
    {
        const auto variantsValue = document.getProperty ("variants", {});
        const auto* variantsObject = variantsValue.getDynamicObject();

        if (variantsObject == nullptr)
        {
            errors_.push_back ({ "variants", "variants must be a JSON object", false });
        }
        else
        {
            for (const auto& variantProperty : variantsObject->getProperties())
            {
                const auto variantName = variantProperty.name.toString().toStdString();
                const auto* overrideObject = variantProperty.value.getDynamicObject();

                if (overrideObject == nullptr)
                {
                    errors_.push_back ({ "variants." + variantName,
                                         "variant must be a JSON object", false });
                    continue;
                }

                auto& map = variants_[variantName];
                for (const auto& tokenProperty : overrideObject->getProperties())
                    map[tokenProperty.name.toString().toStdString()] = tokenProperty.value;
            }
        }
    }

    auto validate = [this] (std::string_view id, std::string_view variant)
    {
        std::vector<std::string> chain;
        std::string error;
        if (! resolveInternal (id, variant, chain, &error).has_value())
            errors_.push_back ({ describePath (id, variant), error, false });
    };

    for (const auto& [id, unused] : baseTokens_)
    {
        juce::ignoreUnused (unused);
        validate (id, {});
    }

    for (const auto& [variantName, map] : variants_)
    {
        for (const auto& [id, unused] : map)
        {
            juce::ignoreUnused (unused);
            validate (id, variantName);
        }
    }

    generation_.fetch_add (1, std::memory_order_acq_rel);

    for (const auto& error : errors_)
        if (! error.warning)
            return false;

    return true;
}

const juce::var* ThemeRegistry::findRaw (std::string_view tokenId, std::string_view variant) const
{
    if (! variant.empty())
    {
        if (const auto variantIt = variants_.find (std::string (variant)); variantIt != variants_.end())
        {
            if (const auto it = variantIt->second.find (std::string (tokenId)); it != variantIt->second.end())
                return &it->second;
        }
    }

    if (const auto it = baseTokens_.find (std::string (tokenId)); it != baseTokens_.end())
        return &it->second;

    return nullptr;
}

std::optional<TokenValue> ThemeRegistry::resolve (std::string_view tokenId,
                                                  std::string_view variant,
                                                  std::string* error) const
{
    std::vector<std::string> chain;
    return resolveInternal (tokenId, variant, chain, error);
}

std::optional<TokenValue> ThemeRegistry::resolveInternal (std::string_view tokenId,
                                                          std::string_view variant,
                                                          std::vector<std::string>& chain,
                                                          std::string* error) const
{
    if (tokenId.empty())
    {
        if (error != nullptr)
            *error = "empty token id";
        return std::nullopt;
    }

    const auto id = std::string (tokenId);

    if (std::find (chain.begin(), chain.end(), id) != chain.end())
    {
        std::string cycle;
        for (const auto& element : chain)
            cycle += element + " -> ";
        cycle += id;

        if (error != nullptr)
            *error = "token alias cycle: " + cycle;

        return std::nullopt;
    }

    if (chain.size() > 32)
    {
        if (error != nullptr)
            *error = "token alias chain exceeds 32 entries";
        return std::nullopt;
    }

    if (const auto overrideIt = overrides_.find (id); overrideIt != overrides_.end())
        return overrideIt->second;

    const auto* raw = findRaw (id, variant);

    if (raw == nullptr)
    {
        if (error != nullptr)
            *error = "unknown token: " + id;
        return std::nullopt;
    }

    chain.push_back (id);
    auto result = convertValue (*raw, variant, chain, error);
    chain.pop_back();
    return result;
}

std::optional<TokenValue> ThemeRegistry::convertValue (const juce::var& value,
                                                       std::string_view variant,
                                                       std::vector<std::string>& chain,
                                                       std::string* error) const
{
    if (value.isString())
    {
        auto text = trimmed (value.toString().toStdString());

        if (! text.empty() && text.front() == '$')
        {
            const auto alias = std::string_view (text).substr (1);
            return resolveInternal (alias, variant, chain, error);
        }

        if (! text.empty() && text.front() == '#')
        {
            if (const auto color = Color::parse (text))
                return *color;

            if (error != nullptr)
                *error = "invalid colour literal: " + text;
            return std::nullopt;
        }

        constexpr std::string_view pxSuffix = "px";
        if (text.size() > pxSuffix.size()
            && text.compare (text.size() - pxSuffix.size(), pxSuffix.size(), pxSuffix) == 0)
        {
            float pixels = 0.0f;
            if (parseFloat (std::string_view (text).substr (0, text.size() - pxSuffix.size()), pixels))
                return Dimension { pixels };

            if (error != nullptr)
                *error = "invalid dimension literal: " + text;
            return std::nullopt;
        }

        return text;
    }

    if (value.isBool())
        return static_cast<bool> (value);

    if (value.isInt() || value.isInt64() || value.isDouble())
        return static_cast<float> (static_cast<double> (value));

    if (value.isObject())
    {
        const auto* object = value.getDynamicObject();
        jassert (object != nullptr);

        const auto hasShadowShape = object->hasProperty ("x") || object->hasProperty ("y")
                                 || object->hasProperty ("blur") || object->hasProperty ("color");
        const auto hasTypeShape = object->hasProperty ("family") || object->hasProperty ("size")
                               || object->hasProperty ("weight") || object->hasProperty ("lineHeight");

        if (hasShadowShape && ! hasTypeShape)
        {
            Shadow shadow;
            auto readNumber = [&] (const juce::Identifier& name, float& destination) -> bool
            {
                if (! object->hasProperty (name))
                    return true;

                if (const auto number = numberValue (object->getProperty (name), variant, chain, error))
                {
                    destination = *number;
                    return true;
                }

                return false;
            };

            if (! readNumber ("x", shadow.offsetX)
                || ! readNumber ("y", shadow.offsetY)
                || ! readNumber ("blur", shadow.blur))
                return std::nullopt;

            if (! object->hasProperty ("color"))
            {
                if (error != nullptr)
                    *error = "shadow token requires a color";
                return std::nullopt;
            }

            const auto colorValue = object->getProperty ("color");
            if (colorValue.isString())
            {
                const auto text = trimmed (colorValue.toString().toStdString());
                if (text.empty())
                {
                    if (error != nullptr)
                        *error = "shadow color is empty";
                    return std::nullopt;
                }

                if (text.front() == '$')
                {
                    const auto resolved = resolveInternal (std::string_view (text).substr (1),
                                                            variant, chain, error);
                    if (! resolved.has_value())
                        return std::nullopt;

                    if (const auto* color = std::get_if<Color> (&*resolved))
                    {
                        shadow.color = *color;
                    }
                    else
                    {
                        if (error != nullptr)
                            *error = "shadow color alias does not resolve to a colour";
                        return std::nullopt;
                    }
                }
                else if (const auto color = Color::parse (text))
                {
                    shadow.color = *color;
                }
                else
                {
                    if (error != nullptr)
                        *error = "invalid shadow colour: " + text;
                    return std::nullopt;
                }
            }
            else
            {
                if (error != nullptr)
                    *error = "shadow color must be a hex string or token alias";
                return std::nullopt;
            }

            return shadow;
        }

        if (hasTypeShape)
        {
            Typography type;
            if (object->hasProperty ("family"))
                type.family = object->getProperty ("family").toString().toStdString();

            if (object->hasProperty ("size"))
            {
                if (const auto size = numberValue (object->getProperty ("size"), variant, chain, error))
                    type.sizePx = *size;
                else
                    return std::nullopt;
            }

            if (object->hasProperty ("weight"))
            {
                if (const auto weight = numberValue (object->getProperty ("weight"), variant, chain, error))
                    type.weight = static_cast<int> (std::lround (*weight));
                else
                    return std::nullopt;
            }

            if (object->hasProperty ("lineHeight"))
            {
                if (const auto lineHeight = numberValue (object->getProperty ("lineHeight"), variant, chain, error))
                    type.lineHeightPx = *lineHeight;
                else
                    return std::nullopt;
            }

            return type;
        }

        if (error != nullptr)
            *error = "unsupported token object shape";
        return std::nullopt;
    }

    if (error != nullptr)
        *error = "unsupported token value type";
    return std::nullopt;
}

std::optional<float> ThemeRegistry::numberValue (const juce::var& value,
                                                 std::string_view variant,
                                                 std::vector<std::string>& chain,
                                                 std::string* error) const
{
    if (value.isInt() || value.isInt64() || value.isDouble())
        return static_cast<float> (static_cast<double> (value));

    if (! value.isString())
    {
        if (error != nullptr)
            *error = "expected a number or dimension token";
        return std::nullopt;
    }

    auto text = trimmed (value.toString().toStdString());

    if (! text.empty() && text.front() == '$')
    {
        const auto resolved = resolveInternal (std::string_view (text).substr (1), variant, chain, error);
        if (! resolved.has_value())
            return std::nullopt;

        if (const auto* dimension = std::get_if<Dimension> (&*resolved))
            return dimension->pixels;

        if (const auto* number = std::get_if<float> (&*resolved))
            return *number;

        if (error != nullptr)
            *error = "token does not resolve to a number";
        return std::nullopt;
    }

    constexpr std::string_view pxSuffix = "px";
    if (text.size() > pxSuffix.size()
        && text.compare (text.size() - pxSuffix.size(), pxSuffix.size(), pxSuffix) == 0)
        text = trimmed (text.substr (0, text.size() - pxSuffix.size()));

    float result = 0.0f;
    if (parseFloat (text, result))
        return result;

    if (error != nullptr)
        *error = "invalid number: " + text;
    return std::nullopt;
}

std::optional<Dimension> ThemeRegistry::resolveDimension (std::string_view tokenId,
                                                           std::string_view variant,
                                                           std::string* error) const
{
    const auto value = resolve (tokenId, variant, error);
    if (! value.has_value())
        return std::nullopt;

    if (const auto* dimension = std::get_if<Dimension> (&*value))
        return *dimension;

    if (const auto* number = std::get_if<float> (&*value))
        return Dimension { *number };

    if (error != nullptr)
        *error = "token is not a dimension: " + std::string (tokenId);
    return std::nullopt;
}

std::optional<Color> ThemeRegistry::resolveColor (std::string_view tokenId,
                                                   std::string_view variant,
                                                   std::string* error) const
{
    const auto value = resolve (tokenId, variant, error);
    if (! value.has_value())
        return std::nullopt;

    if (const auto* color = std::get_if<Color> (&*value))
        return *color;

    if (error != nullptr)
        *error = "token is not a colour: " + std::string (tokenId);
    return std::nullopt;
}

void ThemeRegistry::setOverride (std::string tokenId, TokenValue value)
{
    overrides_[std::move (tokenId)] = std::move (value);
    generation_.fetch_add (1, std::memory_order_acq_rel);
}

bool ThemeRegistry::clearOverride (std::string_view tokenId)
{
    const auto erased = overrides_.erase (std::string (tokenId)) > 0;
    if (erased)
        generation_.fetch_add (1, std::memory_order_acq_rel);
    return erased;
}

bool ThemeRegistry::hasToken (std::string_view tokenId, std::string_view variant) const
{
    if (! tokenId.empty() && overrides_.count (std::string (tokenId)) > 0)
        return true;

    return findRaw (tokenId, variant) != nullptr;
}

bool ThemeRegistry::hasVariant (std::string_view variant) const
{
    return variants_.count (std::string (variant)) > 0;
}

std::vector<std::string> ThemeRegistry::tokenIds() const
{
    std::set<std::string> ids;
    for (const auto& [id, unused] : baseTokens_)
    {
        juce::ignoreUnused (unused);
        ids.insert (id);
    }
    for (const auto& [variant, map] : variants_)
    {
        juce::ignoreUnused (variant);
        for (const auto& [id, unused] : map)
        {
            juce::ignoreUnused (unused);
            ids.insert (id);
        }
    }
    for (const auto& [id, unused] : overrides_)
    {
        juce::ignoreUnused (unused);
        ids.insert (id);
    }

    return { ids.begin(), ids.end() };
}

std::vector<std::string> ThemeRegistry::variants() const
{
    std::vector<std::string> names;
    names.reserve (variants_.size());
    for (const auto& [name, unused] : variants_)
    {
        juce::ignoreUnused (unused);
        names.push_back (name);
    }
    std::sort (names.begin(), names.end());
    return names;
}

} // namespace mvt::theme
