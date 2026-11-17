#include "Engine/Layout/LayoutParser.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

namespace mvt::layout
{
namespace
{
constexpr int kMaxDepth = 64;

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

std::string pathJoin (const std::string& base, std::string_view key)
{
    return base.empty() ? std::string (key) : base + "." + std::string (key);
}

class Parser
{
public:
    Parser (ParseResult& resultIn, const DimensionTokenResolver& resolverIn)
        : result (resultIn), resolver (resolverIn)
    {
    }

    std::optional<LayoutNode> parseRoot (const juce::var& document)
    {
        if (! document.isObject())
        {
            error ("", "layout document must be a JSON object");
            return std::nullopt;
        }

        const auto* object = document.getDynamicObject();
        jassert (object != nullptr);

        if (object->hasProperty ("$schema"))
        {
            const auto schema = document.getProperty ("$schema", {}).toString();
            if (schema.isNotEmpty() && schema != "mvt.layout/1")
                error ("$schema", "unsupported layout schema: " + schema.toStdString());
        }
        else
        {
            warning ("$schema", "missing $schema; assuming mvt.layout/1");
        }

        const auto rootValue = object->hasProperty ("root")
                                 ? document.getProperty ("root", {})
                                 : document;

        return parseNode (rootValue, "root", 0);
    }

    std::optional<LayoutNode> parseNode (const juce::var& value, const std::string& path, int depth)
    {
        if (! value.isObject())
        {
            error (path, "node must be a JSON object");
            return std::nullopt;
        }

        if (depth > kMaxDepth)
        {
            error (path, "layout tree exceeds maximum depth");
            return std::nullopt;
        }

        const auto* object = value.getDynamicObject();
        jassert (object != nullptr);

        LayoutNode node;

        if (object->hasProperty ("id"))
            node.id = value.getProperty ("id", {}).toString().toStdString();

        if (node.id.empty())
        {
            node.id = "node-" + std::to_string (autoIdCounter++);
            warning (pathJoin (path, "id"), "missing id; generated " + node.id);
        }

        if (! ids.insert (node.id).second)
            error (pathJoin (path, "id"), "duplicate node id: " + node.id);

        if (object->hasProperty ("type"))
            node.type = value.getProperty ("type", {}).toString().toStdString();

        if (object->hasProperty ("text"))
            node.text = value.getProperty ("text", {}).toString().toStdString();

        if (node.type.empty())
        {
            node.type = "container";
            warning (pathJoin (path, "type"), "missing type; using container");
        }

        const auto styleValue = object->getProperty ("style");
        if (! styleValue.isVoid())
            parseStyle (styleValue, node, pathJoin (path, "style"));
        else
            parseStyle (value, node, path); // allow style properties inline

        if (object->hasProperty ("children"))
        {
            const auto childrenValue = object->getProperty ("children");
            const auto* children = childrenValue.getArray();

            if (children == nullptr)
            {
                error (pathJoin (path, "children"), "children must be an array");
            }
            else
            {
                node.children.reserve (static_cast<std::size_t> (children->size()));

                for (int i = 0; i < children->size(); ++i)
                {
                    const auto childPath = pathJoin (pathJoin (path, "children"), std::to_string (i));
                    if (auto child = parseNode ((*children)[i], childPath, depth + 1))
                        node.children.push_back (std::move (*child));
                }
            }
        }

        return node;
    }

private:
    void error (const std::string& path, const std::string& message)
    {
        result.errors.push_back ({ path, message, false });
    }

    void warning (const std::string& path, const std::string& message)
    {
        result.errors.push_back ({ path, message, true });
    }

    Length parseLength (const juce::var& value, const std::string& path, bool* valid)
    {
        *valid = true;

        if (value.isInt() || value.isDouble())
        {
            const auto v = static_cast<float> (static_cast<double> (value));
            if (! std::isfinite (v) || v < 0.0f)
            {
                error (path, "length must be finite and non-negative");
                *valid = false;
                return Length::automatic();
            }
            return Length::px (v);
        }

        if (value.isVoid())
            return Length::automatic();

        if (! value.isString())
        {
            error (path, "length must be a number, px string, percent string, or token");
            *valid = false;
            return Length::automatic();
        }

        auto text = trimmed (value.toString().toStdString());

        if (text == "auto")
            return Length::automatic();

        if (! text.empty() && text.front() == '$')
        {
            if (! resolver)
            {
                error (path, "token reference used but no token resolver was supplied: " + text);
                *valid = false;
                return Length::automatic();
            }

            const auto resolved = resolver (std::string_view (text).substr (1));
            if (! resolved.has_value())
            {
                error (path, "unknown dimension token: " + text);
                *valid = false;
                return Length::automatic();
            }

            return Length::px (*resolved);
        }

        auto parseNumber = [&] (std::string_view suffix) -> std::optional<float>
        {
            if (text.size() <= suffix.size()
                || text.compare (text.size() - suffix.size(), suffix.size(), suffix) != 0)
                return std::nullopt;

            const auto numberText = trimmed (text.substr (0, text.size() - suffix.size()));
            if (numberText.empty())
                return std::nullopt;

            try
            {
                const auto v = std::stof (numberText);
                return std::isfinite (v) ? std::optional<float> (v) : std::nullopt;
            }
            catch (...)
            {
                return std::nullopt;
            }
        };

        if (const auto percent = parseNumber ("%"))
            return Length::percent (*percent);

        if (const auto pixels = parseNumber ("px"))
            return Length::px (*pixels);

        // Bare numeric string is treated as pixels, which is friendlier for
        // hand-edited documents than failing on "12".
        try
        {
            const auto v = std::stof (text);
            if (std::isfinite (v))
                return Length::px (v);
        }
        catch (...)
        {
        }

        error (path, "invalid length: " + text);
        *valid = false;
        return Length::automatic();
    }

    std::optional<Length> parseOptionalLength (const juce::var& value, const std::string& path)
    {
        bool valid = true;
        auto length = parseLength (value, path, &valid);

        if (! valid || ! length.isDefinite())
            return std::nullopt;

        return length;
    }

    ResolvedEdges parseEdges (const juce::var& value, const std::string& path)
    {
        ResolvedEdges edges;
        bool valid = false;

        if (value.isInt() || value.isDouble() || value.isString())
        {
            const auto length = parseLength (value, path, &valid);
            if (valid)
                edges.top = edges.right = edges.bottom = edges.left = length;
            return edges;
        }

        if (const auto* array = value.getArray())
        {
            const auto n = array->size();
            std::array<Length, 4> parsed { Length::automatic(), Length::automatic(),
                                          Length::automatic(), Length::automatic() };

            if (n == 1)
            {
                parsed[0] = parseLength ((*array)[0], path, &valid);
                parsed[1] = parsed[2] = parsed[3] = parsed[0];
            }
            else if (n == 2)
            {
                parsed[0] = parsed[2] = parseLength ((*array)[0], path, &valid);
                parsed[1] = parsed[3] = parseLength ((*array)[1], path, &valid);
            }
            else if (n == 3)
            {
                parsed[0] = parseLength ((*array)[0], path, &valid);
                parsed[1] = parsed[3] = parseLength ((*array)[1], path, &valid);
                parsed[2] = parseLength ((*array)[2], path, &valid);
            }
            else if (n == 4)
            {
                for (int i = 0; i < 4; ++i)
                    parsed[static_cast<std::size_t> (i)] =
                        parseLength ((*array)[i], path + "[" + std::to_string (i) + "]", &valid);
            }
            else
            {
                error (path, "edge shorthand must contain 1, 2, 3, or 4 values");
                valid = false;
            }

            if (valid)
            {
                edges.top = parsed[0];
                edges.right = parsed[1];
                edges.bottom = parsed[2];
                edges.left = parsed[3];
            }

            return edges;
        }

        if (value.isObject())
        {
            const auto* object = value.getDynamicObject();
            jassert (object != nullptr);

            for (const auto& property : object->getProperties())
            {
                const auto key = property.name.toString().toStdString();
                if (key == "top" || key == "right" || key == "bottom" || key == "left")
                {
                    const auto length = parseLength (property.value, pathJoin (path, key), &valid);
                    if (! valid)
                        continue;

                    if (key == "top")    edges.top = length;
                    if (key == "right")  edges.right = length;
                    if (key == "bottom") edges.bottom = length;
                    if (key == "left")   edges.left = length;
                }
                else
                {
                    warning (pathJoin (path, key), "unknown edge property ignored");
                }
            }

            return edges;
        }

        error (path, "edges must be a number, string, array, or object");
        return edges;
    }

    void parseStyle (const juce::var& value, LayoutNode& node, const std::string& path)
    {
        if (! value.isObject())
        {
            error (path, "style must be a JSON object");
            return;
        }

        const auto* object = value.getDynamicObject();
        jassert (object != nullptr);
        auto& style = node.style;

        auto readLength = [&] (const juce::Identifier& key, std::optional<Length>& destination)
        {
            if (! object->hasProperty (key))
                return;

            const auto raw = object->getProperty (key);
            if (auto length = parseOptionalLength (raw, pathJoin (path, key.toString().toStdString())))
                destination = *length;
        };

        readLength ("width", style.width);
        readLength ("height", style.height);
        readLength ("minWidth", style.minWidth);
        readLength ("minHeight", style.minHeight);
        readLength ("maxWidth", style.maxWidth);
        readLength ("maxHeight", style.maxHeight);
        readLength ("flexBasis", style.flexBasis);

        if (object->hasProperty ("margin"))
            style.margin = parseEdges (object->getProperty ("margin"), pathJoin (path, "margin"));

        if (object->hasProperty ("padding"))
            style.padding = parseEdges (object->getProperty ("padding"), pathJoin (path, "padding"));

        if (object->hasProperty ("gap"))
        {
            bool valid = true;
            style.gap = parseLength (object->getProperty ("gap"), pathJoin (path, "gap"), &valid);
            if (! valid)
                style.gap = Length::automatic();
        }

        auto readString = [&] (const juce::Identifier& key) -> std::string
        {
            if (! object->hasProperty (key))
                return {};

            return object->getProperty (key).toString().toStdString();
        };

        if (object->hasProperty ("direction") || object->hasProperty ("flexDirection"))
        {
            const auto text = object->hasProperty ("direction")
                                ? readString ("direction")
                                : readString ("flexDirection");

            if (text == "row")            style.direction = Direction::row;
            else if (text == "column")    style.direction = Direction::column;
            else warning (pathJoin (path, "direction"), "unsupported direction: " + text);
        }

        if (auto text = readString ("justifyContent"); ! text.empty() || object->hasProperty ("justifyContent"))
        {
            if (text == "start" || text == "flex-start")       style.justify = JustifyContent::start;
            else if (text == "center" || text == "centre")     style.justify = JustifyContent::centre;
            else if (text == "end" || text == "flex-end")      style.justify = JustifyContent::end;
            else if (text == "space-between")                  style.justify = JustifyContent::spaceBetween;
            else if (text == "space-around")                   style.justify = JustifyContent::spaceAround;
            else if (text == "space-evenly")                   style.justify = JustifyContent::spaceEvenly;
            else warning (pathJoin (path, "justifyContent"), "unsupported justifyContent: " + text);
        }

        auto parseAlign = [&] (const std::string& text) -> std::optional<AlignItems>
        {
            if (text == "start" || text == "flex-start") return AlignItems::start;
            if (text == "center" || text == "centre")    return AlignItems::centre;
            if (text == "end" || text == "flex-end")     return AlignItems::end;
            if (text == "stretch")                       return AlignItems::stretch;
            return std::nullopt;
        };

        if (object->hasProperty ("alignItems"))
        {
            if (const auto align = parseAlign (readString ("alignItems")))
                style.align = *align;
            else
                warning (pathJoin (path, "alignItems"), "unsupported alignItems: " + readString ("alignItems"));
        }

        if (object->hasProperty ("alignSelf"))
        {
            if (const auto align = parseAlign (readString ("alignSelf")))
                style.alignSelf = *align;
            else
                warning (pathJoin (path, "alignSelf"), "unsupported alignSelf: " + readString ("alignSelf"));
        }

        if (auto text = readString ("position"); ! text.empty())
        {
            if (text == "relative")      style.position = Position::relative;
            else if (text == "absolute") style.position = Position::absolute;
            else warning (pathJoin (path, "position"), "unsupported position: " + text);
        }

        if (auto text = readString ("display"); ! text.empty())
        {
            if (text == "flex" || text == "block") style.display = Display::flex;
            else if (text == "none")               style.display = Display::none;
            else warning (pathJoin (path, "display"), "unsupported display: " + text);
        }

        if (object->hasProperty ("flex"))
        {
            const auto flexValue = object->getProperty ("flex");
            if (flexValue.isInt() || flexValue.isDouble())
            {
                style.flexGrow = std::max (0.0f, static_cast<float> (static_cast<double> (flexValue)));
            }
            else if (flexValue.isObject())
            {
                const auto* flexObject = flexValue.getDynamicObject();
                jassert (flexObject != nullptr);

                if (flexObject->hasProperty ("grow"))
                    style.flexGrow = std::max (0.0f, static_cast<float> (static_cast<double> (flexObject->getProperty ("grow"))));
                if (flexObject->hasProperty ("shrink"))
                    style.flexShrink = std::max (0.0f, static_cast<float> (static_cast<double> (flexObject->getProperty ("shrink"))));
                if (flexObject->hasProperty ("basis"))
                {
                    bool valid = true;
                    const auto basis = parseLength (flexObject->getProperty ("basis"),
                                                    pathJoin (path, "flex.basis"), &valid);
                    if (valid && basis.isDefinite())
                        style.flexBasis = basis;
                }
            }
            else
            {
                warning (pathJoin (path, "flex"), "flex must be a number or an object");
            }
        }

        if (object->hasProperty ("flexGrow"))
            style.flexGrow = std::max (0.0f, static_cast<float> (static_cast<double> (object->getProperty ("flexGrow"))));
        if (object->hasProperty ("flexShrink"))
            style.flexShrink = std::max (0.0f, static_cast<float> (static_cast<double> (object->getProperty ("flexShrink"))));

        if (object->hasProperty ("aspectRatio"))
        {
            const auto ratio = static_cast<float> (static_cast<double> (object->getProperty ("aspectRatio")));
            if (std::isfinite (ratio) && ratio > 0.0f)
                style.aspectRatio = ratio;
            else
                warning (pathJoin (path, "aspectRatio"), "aspectRatio must be positive");
        }

        if (object->hasProperty ("inset"))
            style.inset = parseEdges (object->getProperty ("inset"), pathJoin (path, "inset"));

        for (const auto& key : { "top", "right", "bottom", "left" })
        {
            if (! object->hasProperty (key))
                continue;

            bool valid = true;
            const auto length = parseLength (object->getProperty (key), pathJoin (path, key), &valid);
            if (! valid)
                continue;

            auto& edge = std::string_view (key) == "top" ? style.inset.top
                       : std::string_view (key) == "right" ? style.inset.right
                       : std::string_view (key) == "bottom" ? style.inset.bottom
                                                           : style.inset.left;
            edge = length;
        }

        // Unknown style properties are warnings, not errors: forward compatibility
        // matters more than strictness for skins authored by newer engines.
        for (const auto& property : object->getProperties())
        {
            const auto key = property.name.toString().toStdString();

            if (key == "id" || key == "type" || key == "children"
                || key == "width" || key == "height" || key == "minWidth" || key == "minHeight"
                || key == "maxWidth" || key == "maxHeight" || key == "flexBasis"
                || key == "margin" || key == "padding" || key == "gap"
                || key == "direction" || key == "flexDirection"
                || key == "justifyContent" || key == "alignItems" || key == "alignSelf"
                || key == "position" || key == "display" || key == "flex"
                || key == "flexGrow" || key == "flexShrink" || key == "aspectRatio"
                || key == "inset" || key == "top" || key == "right" || key == "bottom" || key == "left"
                || key == "$schema" || key == "root" || key == "style" || key == "bindings"
                // Visual-only fields are owned by the renderer/theme, not the
                // layout solver, but are legal on a node or style object.
                || key == "background" || key == "text")
                continue;

            warning (pathJoin (path, key), "unknown style property ignored");
        }
    }

    ParseResult& result;
    const DimensionTokenResolver& resolver;
    std::unordered_set<std::string> ids;
    int autoIdCounter = 0;
};

} // namespace

const LayoutNode* LayoutNode::findById (std::string_view wantedId) const noexcept
{
    if (id == wantedId)
        return this;

    for (const auto& child : children)
        if (const auto* found = child.findById (wantedId))
            return found;

    return nullptr;
}

ParseResult parseLayout (const juce::var& document, const DimensionTokenResolver& resolveToken)
{
    ParseResult result;
    Parser parser (result, resolveToken);
    result.root = parser.parseRoot (document);
    return result;
}

} // namespace mvt::layout
