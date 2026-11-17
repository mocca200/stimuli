#include "Engine/Layout/FlexSolver.h"
#include "Engine/Layout/LayoutParser.h"
#include "Engine/Layout/LayoutWriter.h"

#include <catch2/catch_all.hpp>
#include <juce_core/juce_core.h>

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

LayoutNode sampleTree()
{
    LayoutNode root;
    root.id = "root";
    root.type = "container";
    root.style.width = Length::percent (100.0f);
    root.style.minHeight = Length::px (60.0f);
    root.style.maxWidth = Length::px (900.0f);
    root.style.aspectRatio = 1.5f;
    root.style.flexGrow = 1.0f;
    root.style.flexShrink = 0.5f;
    root.style.direction = Direction::column;
    root.style.justify = JustifyContent::spaceBetween;
    root.style.align = AlignItems::centre;
    root.style.position = Position::relative;
    root.style.display = Display::flex;
    root.style.gap = Length::px (8.0f);
    root.style.padding = { Length::px (12.0f), Length::percent (4.5f),
                           Length::px (3.0f), Length::automatic() };

    LayoutNode child;
    child.id = "badge";
    child.type = "label";
    child.text = "Hello world";
    child.style.position = Position::absolute;
    child.style.inset = { Length::px (4.0f), Length::automatic(),
                          Length::automatic(), Length::px (7.0f) };
    child.style.width = Length::px (120.5f);
    child.style.height = Length::px (24.0f);
    child.style.margin.top = Length::px (2.0f);
    child.style.alignSelf = AlignItems::end;

    root.children.push_back (std::move (child));
    return root;
}
} // namespace

TEST_CASE ("Layout writer round-trips through the parser", "[layout][writer]")
{
    const auto original = sampleTree();
    const auto document = writeLayout (original);
    const auto parsed = parseLayout (document);

    INFO (errorsToString (parsed));
    REQUIRE (parsed.ok());
    REQUIRE (parsed.root.has_value());

    const auto rewritten = writeLayout (*parsed.root);
    REQUIRE (juce::JSON::toString (document) == juce::JSON::toString (rewritten));
}

TEST_CASE ("Layout writer preserves every style field", "[layout][writer]")
{
    const auto document = writeLayout (sampleTree());
    const auto parsed = parseLayout (document);

    REQUIRE (parsed.ok());
    REQUIRE (parsed.root.has_value());

    const auto& root = *parsed.root;
    REQUIRE (root.id == "root");
    REQUIRE (root.style.width.has_value());
    REQUIRE (root.style.width->kind == LengthKind::percent);
    REQUIRE (root.style.width->value == Catch::Approx (100.0f));
    REQUIRE (root.style.minHeight.has_value());
    REQUIRE (root.style.minHeight->value == Catch::Approx (60.0f));
    REQUIRE (root.style.maxWidth.has_value());
    REQUIRE (root.style.aspectRatio.has_value());
    REQUIRE (*root.style.aspectRatio == Catch::Approx (1.5f));
    REQUIRE (root.style.flexGrow == Catch::Approx (1.0f));
    REQUIRE (root.style.flexShrink == Catch::Approx (0.5f));
    REQUIRE (root.style.direction == Direction::column);
    REQUIRE (root.style.justify == JustifyContent::spaceBetween);
    REQUIRE (root.style.align == AlignItems::centre);
    REQUIRE (root.style.gap.value == Catch::Approx (8.0f));
    REQUIRE (root.style.padding.top.value == Catch::Approx (12.0f));
    REQUIRE (root.style.padding.right.kind == LengthKind::percent);
    REQUIRE (root.style.padding.right.value == Catch::Approx (4.5f));
    REQUIRE (root.style.padding.left.kind == LengthKind::autoValue);

    REQUIRE (root.children.size() == 1);
    const auto& child = root.children.front();
    REQUIRE (child.text == "Hello world");
    REQUIRE (child.style.position == Position::absolute);
    REQUIRE (child.style.inset.left.value == Catch::Approx (7.0f));
    REQUIRE (child.style.inset.top.value == Catch::Approx (4.0f));
    REQUIRE (child.style.inset.right.kind == LengthKind::autoValue);
    REQUIRE (child.style.width->value == Catch::Approx (120.5f));
    REQUIRE (child.style.alignSelf.has_value());
    REQUIRE (*child.style.alignSelf == AlignItems::end);
}

TEST_CASE ("Written documents solve to the same boxes", "[layout][writer]")
{
    const auto original = sampleTree();
    const auto document = writeLayout (original);
    const auto parsed = parseLayout (document);

    REQUIRE (parsed.ok());

    const auto direct = FlexSolver::solve (original, { 800.0f, 600.0f });
    const auto roundTripped = FlexSolver::solve (*parsed.root, { 800.0f, 600.0f });

    REQUIRE (direct.frame.width == Catch::Approx (roundTripped.frame.width));
    REQUIRE (direct.frame.height == Catch::Approx (roundTripped.frame.height));
    REQUIRE (direct.children.size() == roundTripped.children.size());
    REQUIRE (direct.children[0].frame.x == Catch::Approx (roundTripped.children[0].frame.x));
    REQUIRE (direct.children[0].frame.y == Catch::Approx (roundTripped.children[0].frame.y));
}
