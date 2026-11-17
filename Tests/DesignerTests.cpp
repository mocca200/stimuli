#include "UIEngine/DesignerDocument.h"

#include <catch2/catch_all.hpp>
#include <juce_core/juce_core.h>

#include <string>

using namespace mvt;
using namespace mvt::ui;

namespace
{
constexpr auto kTheme = R"({
  "$schema": "mvt.theme/1",
  "tokens": {
    "color.accent": "#5EE7FF",
    "space.md": "12px",
    "size.card": 200
  }
})";

constexpr auto kLayout = R"({
  "$schema": "mvt.layout/1",
  "root": {
    "id": "root", "type": "container",
    "style": { "width": 800, "height": 600, "direction": "column" },
    "children": [
      { "id": "header", "type": "label", "text": "Title",
        "style": { "height": 40 } },
      { "id": "left", "type": "container",
        "style": { "flex": 1, "direction": "row" },
        "children": [
          { "id": "inner", "type": "label",
            "style": { "width": 100, "height": 30, "minWidth": 80 } },
          { "id": "sub", "type": "container" }
        ] },
      { "id": "scope", "type": "vectorscope", "style": { "flex": 2 } }
    ]
  }
})";

void loadDocument (DesignerDocument& document)
{
    REQUIRE (document.setThemeDocument (juce::JSON::parse (juce::String (kTheme))));
    REQUIRE (document.setLayoutDocument (juce::JSON::parse (juce::String (kLayout))));
}

juce::File makeTempFolder()
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("mvt-designer-tests-"
                                           + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));

    folder.createDirectory();
    return folder;
}
} // namespace

TEST_CASE ("Designer document loads a layout and selects the root", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.root().id == "root");
    REQUIRE (document.find ("header") != nullptr);
    REQUIRE (document.find ("missing") == nullptr);
    REQUIRE (document.selectedId() == "root");
    REQUIRE_FALSE (document.isDirty());
    REQUIRE_FALSE (document.canUndo());
}

TEST_CASE ("Moving a node writes absolute geometry", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.setNodeFrame ("header", { 12.0f, 8.0f, 300.0f, 44.0f }));

    const auto* node = document.find ("header");
    REQUIRE (node != nullptr);
    REQUIRE (node->style.position == layout::Position::absolute);
    REQUIRE (node->style.inset.left.value == Catch::Approx (12.0f));
    REQUIRE (node->style.inset.top.value == Catch::Approx (8.0f));
    REQUIRE (node->style.width.has_value());
    REQUIRE (node->style.width->value == Catch::Approx (300.0f));
    REQUIRE (node->style.height->value == Catch::Approx (44.0f));
    REQUIRE (document.isDirty());
    REQUIRE (document.canUndo());
}

TEST_CASE ("Resizing respects pixel minimum sizes", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.resizeNode ("inner", 20.0f, 10.0f));

    const auto* node = document.find ("inner");
    REQUIRE (node != nullptr);
    REQUIRE (node->style.width->value == Catch::Approx (80.0f));
    REQUIRE (node->style.height->value == Catch::Approx (10.0f));
}

TEST_CASE ("Reparenting rejects cycles and non-containers", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE_FALSE (document.reparentNode ("left", "inner", 0));
    REQUIRE_FALSE (document.reparentNode ("left", "sub", 0));
    REQUIRE_FALSE (document.reparentNode ("root", "left", 0));

    REQUIRE (document.reparentNode ("scope", "left", 0));

    const auto* left = document.find ("left");
    REQUIRE (left != nullptr);
    REQUIRE (left->children.front().id == "scope");
}

TEST_CASE ("Reordering keeps siblings intact", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.reorderNode ("scope", 0));

    const auto& children = document.root().children;
    REQUIRE (children.size() == 3);
    REQUIRE (children[0].id == "scope");
    REQUIRE (children[1].id == "header");
    REQUIRE (children[2].id == "left");
}

TEST_CASE ("Duplicating assigns unique ids", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.duplicateNode ("header"));
    REQUIRE (document.find ("header-copy") != nullptr);
    REQUIRE (document.selectedId() == "header-copy");

    REQUIRE (document.duplicateNode ("header"));
    REQUIRE (document.find ("header-copy-2") != nullptr);
    REQUIRE (document.find ("header") != nullptr);
}

TEST_CASE ("Grouping wraps the node in a container", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.groupSelection ("inner", { 10.0f, 20.0f, 120.0f, 44.0f }));

    const auto groupId = document.selectedId();
    REQUIRE (groupId.rfind ("group", 0) == 0);

    const auto* group = document.find (groupId);
    REQUIRE (group != nullptr);
    REQUIRE (group->type == "container");
    REQUIRE (group->style.position == layout::Position::absolute);
    REQUIRE (group->style.inset.left.value == Catch::Approx (10.0f));
    REQUIRE (group->children.size() == 1);
    REQUIRE (group->children.front().id == "inner");
    REQUIRE (group->children.front().style.width.has_value());
    REQUIRE (group->children.front().style.width->kind == layout::LengthKind::percent);
}

TEST_CASE ("Delete, undo, and redo restore the tree", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.deleteNode ("header"));
    REQUIRE (document.find ("header") == nullptr);
    REQUIRE (document.selectedId() == "root");

    REQUIRE (document.undo());
    REQUIRE (document.find ("header") != nullptr);

    REQUIRE (document.redo());
    REQUIRE (document.find ("header") == nullptr);
}

TEST_CASE ("Renaming rejects empty and duplicate ids", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE_FALSE (document.renameId ("header", "scope"));
    REQUIRE_FALSE (document.renameId ("header", ""));

    REQUIRE (document.renameId ("header", "title"));
    REQUIRE (document.find ("title") != nullptr);
    REQUIRE (document.find ("header") == nullptr);
}

TEST_CASE ("Theme overrides apply live and persist on save", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.setThemeOverride ("color.accent", theme::Color { 255, 0, 0, 255 }));

    const auto live = document.theme().resolveColor ("color.accent");
    REQUIRE (live.has_value());
    REQUIRE (live->toHex() == "#FF0000");

    REQUIRE (document.setNodeFrame ("header", { 16.0f, 10.0f, 250.0f, 40.0f }));

    const auto folder = makeTempFolder();
    juce::String error;

    REQUIRE (document.save (folder, &error));
    INFO (error.toStdString());
    REQUIRE_FALSE (document.isDirty());

    const auto layoutDocument = juce::JSON::parse (folder.getChildFile ("layout.json"));
    REQUIRE (! layoutDocument.isVoid());
    const auto rootDocument = layoutDocument.getProperty ("root", {});
    REQUIRE (rootDocument.getProperty ("children", {})
                .getArray()->getFirst().getProperty ("id", {}).toString() == "header");

    const auto themeDocument = juce::JSON::parse (folder.getChildFile ("theme.json"));
    REQUIRE (! themeDocument.isVoid());
    const auto tokens = themeDocument.getProperty ("tokens", {});
    REQUIRE (tokens.getProperty ("color.accent", {}).toString() == "#FF0000");

    REQUIRE (folder.getChildFile ("manifest.json").existsAsFile());
    folder.deleteRecursively();
}

TEST_CASE ("Undoing a theme override restores the base token", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    REQUIRE (document.setThemeOverride ("color.accent", theme::Color { 0, 255, 0, 255 }));
    REQUIRE (document.undo());

    const auto restored = document.theme().resolveColor ("color.accent");
    REQUIRE (restored.has_value());
    REQUIRE (restored->toHex() == "#5EE7FF");
}

TEST_CASE ("Saving without a theme change leaves base tokens untouched", "[designer]")
{
    DesignerDocument document;
    loadDocument (document);

    const auto saved = document.savedThemeDocument();
    const auto tokens = saved.getProperty ("tokens", {});
    REQUIRE (tokens.getProperty ("color.accent", {}).toString() == "#5EE7FF");
    REQUIRE (tokens.getProperty ("space.md", {}).toString() == "12px");
}
