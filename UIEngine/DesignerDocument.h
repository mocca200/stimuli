#pragma once

#include "Engine/Layout/LayoutTypes.h"
#include "Engine/Theme/ThemeRegistry.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mvt::ui
{
class Skin;

/**
 * Editable layout + theme document behind the designer.

 * Owns a mutable copy of a skin's layout tree and theme document, a selection,
 * and a bounded undo/redo history. Every command validates before mutating and
 * bumps a revision counter so views can refresh cheaply.

 * Message-thread only. It never touches the audio thread and never starts a
 * worker; the plugin does not link this class.
 */
class DesignerDocument
{
public:
    DesignerDocument() = default;
    ~DesignerDocument() = default;

    DesignerDocument (const DesignerDocument&) = delete;
    DesignerDocument& operator= (const DesignerDocument&) = delete;
    DesignerDocument (DesignerDocument&&) = default;
    DesignerDocument& operator= (DesignerDocument&&) = default;

    /** Copies the layout tree, theme document, and resolved theme from a skin. */
    bool loadFromSkin (const Skin& skin);

    /** Parses a layout document using the current theme's dimension tokens. */
    bool setLayoutDocument (const juce::var& layoutDocument);

    /** Replaces the theme document; runtime overrides are re-applied on top. */
    bool setThemeDocument (const juce::var& themeDocument);

    [[nodiscard]] const layout::LayoutNode& root() const noexcept { return root_; }
    [[nodiscard]] const layout::LayoutNode* find (std::string_view id) const noexcept;
    [[nodiscard]] const juce::var& themeDocument() const noexcept { return themeDocument_; }

    [[nodiscard]] theme::ThemeRegistry& theme() noexcept { return theme_; }
    [[nodiscard]] const theme::ThemeRegistry& theme() const noexcept { return theme_; }

    [[nodiscard]] const std::string& selectedId() const noexcept { return selectedId_; }
    void select (std::string id);

    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }
    void markSaved() noexcept { dirty_ = false; }

    /** Absolute-positioning edit: sets inset and size from a frame expressed
        relative to the parent's content box. */
    bool setNodeFrame (std::string_view id, layout::Rect frameInParentContent);

    /** Adds a delta to the inset of an already absolute node. */
    bool moveNode (std::string_view id, float deltaX, float deltaY);

    /** Sets explicit pixel width/height, clamped to pixel min/max constraints. */
    bool resizeNode (std::string_view id, float width, float height);

    bool setStyle (std::string_view id, const layout::Style& style);
    bool setNodeText (std::string_view id, std::string text);
    bool setNodeType (std::string_view id, std::string type);
    bool renameId (std::string_view id, std::string newId);

    bool reorderNode (std::string_view id, std::size_t newIndex);
    bool reparentNode (std::string_view id, std::string_view newParentId, std::size_t index);

    /** Wraps a node in a new container that occupies the supplied frame. */
    bool groupSelection (std::string_view id, layout::Rect frameInParentContent);

    bool duplicateNode (std::string_view id);
    bool deleteNode (std::string_view id);

    bool setThemeOverride (std::string tokenId, theme::TokenValue value);
    bool clearThemeOverride (std::string_view tokenId);

    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return ! undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return ! redoStack_.empty(); }

    [[nodiscard]] juce::var toJson() const;

    /** The theme document with runtime overrides written into base tokens. */
    [[nodiscard]] juce::var savedThemeDocument() const;

    /** Writes layout.json and theme.json into a skin folder. */
    bool save (const juce::File& folder, juce::String* error = nullptr);

private:
    struct Snapshot
    {
        layout::LayoutNode root;
        std::string selection;
        std::vector<std::pair<std::string, theme::TokenValue>> overrides;
    };

    void beginChange();
    void finishChange();
    void pushUndo();
    void syncThemeOverrides();

    layout::LayoutNode root_;
    juce::var themeDocument_ { new juce::DynamicObject() };
    theme::ThemeRegistry theme_;
    std::string selectedId_;
    std::vector<std::pair<std::string, theme::TokenValue>> overrides_;
    std::vector<std::string> appliedOverrides_;

    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    std::uint64_t revision_ = 0;
    bool dirty_ = false;
};
} // namespace mvt::ui
