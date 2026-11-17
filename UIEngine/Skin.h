#pragma once

#include "Engine/Layout/LayoutTypes.h"
#include "Engine/Theme/ThemeRegistry.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <string>

namespace mvt::ui
{
/** A loaded skin: manifest, layout document, and resolved theme. */
class Skin
{
public:
    Skin();
    ~Skin();

    Skin (const Skin&) = delete;
    Skin& operator= (const Skin&) = delete;

    /** Loads manifest.json, theme.json, and layout.json from a skin folder.
        On failure the previous skin (if any) is left untouched. */
    bool loadFromFolder (const juce::File& folder, juce::String* error = nullptr);

    bool loadFromDocuments (const juce::var& layoutDocument,
                            const juce::var& themeDocument,
                            juce::String* error = nullptr);

    /** Loads the default skin, falling back to a built-in document when no
        assets can be found. Returns false only if even the fallback fails. */
    bool loadDefault (juce::String* error = nullptr);

    bool loadFallback (juce::String* error = nullptr);

    [[nodiscard]] const layout::LayoutNode& layoutRoot() const noexcept { return layoutRoot_; }
    [[nodiscard]] const theme::ThemeRegistry& theme() const noexcept { return *theme_; }
    [[nodiscard]] const juce::String& name() const noexcept { return name_; }
    [[nodiscard]] const juce::File& folder() const noexcept { return folder_; }

    /** The raw theme document this skin was loaded from. */
    [[nodiscard]] const juce::var& themeDocument() const noexcept { return themeDocument_; }

    /** Replaces the solved layout tree. Used by the designer after an edit;
        the skin does not re-parse tokens, so edited values are already pixels. */
    void setLayoutRoot (layout::LayoutNode root);

    /** Rebuilds the theme registry from a document, leaving the layout alone.
        On failure the existing theme is kept. */
    bool setThemeDocument (const juce::var& document, juce::String* error = nullptr);

    [[nodiscard]] static juce::File defaultFolder();

private:
    layout::LayoutNode layoutRoot_;
    juce::var themeDocument_ { new juce::DynamicObject() };
    std::unique_ptr<theme::ThemeRegistry> theme_;
    juce::String name_ { "Fallback" };
    juce::File folder_;
};
} // namespace mvt::ui
