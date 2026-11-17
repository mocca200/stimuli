#include "Engine/Layout/FlexSolver.h"
#include "Engine/Layout/LayoutParser.h"

#include <catch2/catch_all.hpp>
#include <juce_core/juce_core.h>

#include <optional>
#include <string>
#include <unordered_map>

using namespace mvt::layout;

namespace
{
std::string errorsToString (const ParseResult& result)
{
    std::string text;
    for (const auto& error : result.errors)
    {
        text += error.path;
        text += ": ";
        text += error.message;
        text += error.warning ? " (warning)\n" : "\n";
    }
    return text;
}

std::optional<LayoutBox> solve (const char* json,
                                float width = 800.0f,
                                float height = 600.0f,
                                DimensionTokenResolver resolver = {},
                                float scale = 0.0f)
{
    const auto document = juce::JSON::parse (juce::String (json));
    auto parsed = parseLayout (document, resolver);

    INFO (errorsToString (parsed));

    if (! parsed.ok() || ! parsed.root.has_value())
        return std::nullopt;

    return FlexSolver::solve (*parsed.root, { width, height }, scale);
}

void checkRect (const LayoutBox& box, float x, float y, float width, float height)
{
    INFO ("node " << box.id);
    REQUIRE (box.frame.x == Catch::Approx (x).margin (0.001f));
    REQUIRE (box.frame.y == Catch::Approx (y).margin (0.001f));
    REQUIRE (box.frame.width == Catch::Approx (width).margin (0.001f));
    REQUIRE (box.frame.height == Catch::Approx (height).margin (0.001f));
}
} // namespace

TEST_CASE ("Row layout honours padding and gap", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "type": "container",
            "style": { "direction": "row", "padding": 10, "gap": 10 },
            "children": [
                { "id": "a", "type": "panel", "style": { "width": 100, "height": 50 } },
                { "id": "b", "type": "panel", "style": { "width": 200, "height": 50 } }
            ]
        }
    })", 500.0f, 300.0f);

    REQUIRE (box.has_value());
    checkRect (*box, 0, 0, 500, 300);
    checkRect (box->children[0], 10, 10, 100, 50);
    checkRect (box->children[1], 120, 10, 200, 50);
}

TEST_CASE ("Flex grow distributes free space", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root",
            "style": { "width": 300, "height": 100, "direction": "row" },
            "children": [
                { "id": "fixed", "style": { "width": 100, "height": 40 } },
                { "id": "grow1", "style": { "flex": 1, "height": 40 } },
                { "id": "grow2", "style": { "flex": 2, "height": 40 } }
            ]
        }
    })", 800.0f, 600.0f);

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 100, 40);
    checkRect (box->children[1], 100, 0, 66.6667f, 40);
    checkRect (box->children[2], 166.6667f, 0, 133.3333f, 40);
}

TEST_CASE ("Percent sizes resolve against the parent content box", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root",
            "style": { "width": 400, "height": 200, "padding": 10, "gap": 20 },
            "children": [
                { "id": "a", "style": { "width": "25%", "height": 20 } },
                { "id": "b", "style": { "width": "50%", "height": 20 } }
            ]
        }
    })", 800.0f, 600.0f);

    REQUIRE (box.has_value());
    checkRect (box->children[0], 10, 10, 95, 20);
    checkRect (box->children[1], 125, 10, 190, 20);
}

TEST_CASE ("Column alignment centres children on the cross axis", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 300, "direction": "column", "alignItems": "center" },
            "children": [
                { "id": "a", "style": { "width": 100, "height": 50 } },
                { "id": "b", "style": { "width": 200, "height": 50 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 100, 0, 100, 50);
    checkRect (box->children[1], 50, 50, 200, 50);
}

TEST_CASE ("Justify content distributes leftover space", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 100, "justifyContent": "space-between" },
            "children": [
                { "id": "a", "style": { "width": 100, "height": 20 } },
                { "id": "b", "style": { "width": 100, "height": 20 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 100, 20);
    checkRect (box->children[1], 200, 0, 100, 20);
}

TEST_CASE ("Weighted shrink respects minimum sizes", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 150, "height": 100 },
            "children": [
                { "id": "a", "style": { "width": 100, "height": 20, "minWidth": 80 } },
                { "id": "b", "style": { "width": 100, "height": 20 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 80, 20);
    checkRect (box->children[1], 80, 0, 70, 20);
}

TEST_CASE ("Margins and padding compose", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 200, "height": 200, "padding": 10 },
            "children": [
                { "id": "a", "style": { "width": "50%", "height": 30, "margin": 5 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 15, 15, 90, 30);
}

TEST_CASE ("Absolute positioning resolves inset and size", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 400, "height": 300, "padding": 10 },
            "children": [
                { "id": "a", "style": { "position": "absolute", "left": 20, "top": 30, "width": 100, "height": 50 } },
                { "id": "b", "style": { "position": "absolute", "right": 10, "bottom": 20, "width": 60, "height": 40 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 30, 40, 100, 50);
    checkRect (box->children[1], 320, 230, 60, 40);
}

TEST_CASE ("Max size survives grow distribution", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 100 },
            "children": [
                { "id": "a", "style": { "flex": 1, "height": 20, "maxWidth": 120 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 120, 20);
}

TEST_CASE ("Aspect ratio derives the missing dimension", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 200 },
            "children": [
                { "id": "a", "style": { "width": 160, "aspectRatio": 1.7777778 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 160, 90);
}

TEST_CASE ("Dimension tokens are resolved through the injected resolver", "[layout][tokens]")
{
    const std::unordered_map<std::string, float> tokens {
        { "space.md", 12.0f },
        { "size.card", 150.0f }
    };

    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 400, "height": 200, "padding": "$space.md", "gap": "$space.md" },
            "children": [
                { "id": "a", "style": { "width": "$size.card", "height": 30 } },
                { "id": "b", "style": { "flex": 1, "height": 30 } }
            ]
        }
    })", 400.0f, 200.0f, [&] (std::string_view id) -> std::optional<float>
    {
        const auto it = tokens.find (std::string (id));
        if (it == tokens.end())
            return std::nullopt;
        return it->second;
    });

    REQUIRE (box.has_value());
    checkRect (box->children[0], 12, 12, 150, 30);
}

TEST_CASE ("Display none removes a node from flow and hit testing", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 100 },
            "children": [
                { "id": "hidden", "style": { "display": "none", "width": 100 } },
                { "id": "visible", "style": { "width": 100, "height": 20 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 0, 0);
    checkRect (box->children[1], 0, 0, 100, 20);
    REQUIRE (FlexSolver::hitTest (*box, { 50, 10 })->id == "visible");
}

TEST_CASE ("Nested flex containers compose", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 400, "height": 300, "direction": "column" },
            "children": [
                { "id": "top", "style": { "flex": 1, "direction": "row", "padding": 10, "gap": 10 },
                  "children": [
                      { "id": "left", "style": { "flex": 1, "height": 50 } },
                      { "id": "right", "style": { "width": 100, "height": 50 } }
                  ] },
                { "id": "bottom", "style": { "height": 80 } }
            ]
        }
    })");

    REQUIRE (box.has_value());
    checkRect (box->children[0], 0, 0, 400, 220);
    checkRect (box->children[0].children[0], 10, 10, 270, 50);
    checkRect (box->children[0].children[1], 290, 10, 100, 50);
    checkRect (box->children[1], 0, 220, 400, 80);
}

TEST_CASE ("Pixel snapping produces stable physical edges", "[layout]")
{
    const auto box = solve (R"({
        "$schema": "mvt.layout/1",
        "root": {
            "id": "root", "style": { "width": 300, "height": 100 },
            "children": [
                { "id": "a", "style": { "width": 100.3, "height": 20 } },
                { "id": "b", "style": { "width": 100.3, "height": 20 } }
            ]
        }
    })", 300.0f, 100.0f, {}, 2.0f);

    REQUIRE (box.has_value());
    const auto edge = box->children[1].frame.x;
    REQUIRE (edge == Catch::Approx (std::round (edge * 2.0f) / 2.0f).margin (0.0001f));
}

TEST_CASE ("Parser rejects duplicate ids and unknown tokens", "[layout][parser]")
{
    const auto duplicate = juce::JSON::parse (juce::String (R"({
        "$schema": "mvt.layout/1",
        "root": { "id": "same", "children": [ { "id": "same" } ] }
    })"));

    const auto duplicateResult = parseLayout (duplicate);
    REQUIRE_FALSE (duplicateResult.ok());

    const auto tokenDocument = juce::JSON::parse (juce::String (R"({
        "$schema": "mvt.layout/1",
        "root": { "id": "root", "width": "$missing.token" }
    })"));

    const auto tokenResult = parseLayout (tokenDocument, [] (std::string_view) -> std::optional<float>
    {
        return std::nullopt;
    });
    REQUIRE_FALSE (tokenResult.ok());
}

TEST_CASE ("Layout solving is deterministic", "[layout]")
{
    const auto first = solve (R"({
        "$schema": "mvt.layout/1",
        "root": { "id": "root", "style": { "width": 500, "height": 300, "gap": 7 },
          "children": [ { "id": "a", "style": { "flex": 1 } }, { "id": "b", "style": { "flex": 2 } } ] }
    })");

    const auto second = solve (R"({
        "$schema": "mvt.layout/1",
        "root": { "id": "root", "style": { "width": 500, "height": 300, "gap": 7 },
          "children": [ { "id": "a", "style": { "flex": 1 } }, { "id": "b", "style": { "flex": 2 } } ] }
    })");

    REQUIRE (first.has_value());
    REQUIRE (second.has_value());
    REQUIRE (first->children[0].frame.width == second->children[0].frame.width);
    REQUIRE (first->children[1].frame.width == second->children[1].frame.width);
}
