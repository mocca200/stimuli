#include "UIEngine/Skin.h"

#include "Engine/Layout/LayoutParser.h"

#include <juce_core/juce_core.h>

#include <optional>
#include <string>
#include <string_view>

namespace mvt::ui
{
namespace
{
constexpr auto kFallbackTheme = R"({
  "$schema": "mvt.theme/1",
  "tokens": {
    "color.bg": "#0B0D10",
    "color.surface": "#141A24",
    "color.surfaceRaised": "#1B2230",
    "color.border": "#283244",
    "color.text": "#E8EEF7",
    "color.textMuted": "#7A879B",
    "color.accent": "#5EE7FF",
    "color.accentWarm": "#FFB86B",
    "color.harmonic": "#7EA6FF",
    "color.percussive": "#FF7A90",
    "type.title": { "family": "Inter", "size": "18px", "weight": 600, "lineHeight": 24 },
    "type.body": { "family": "Inter", "size": "14px", "weight": 400, "lineHeight": 20 },
    "type.mono": { "family": "JetBrains Mono", "size": "12px", "weight": 400, "lineHeight": 16 }
  }
})";

constexpr auto kFallbackLayout = R"({
  "$schema": "mvt.layout/1",
  "root": {
    "id": "root",
    "type": "container",
    "style": { "direction": "column", "padding": 20, "gap": 12 },
    "children": [
      { "id": "title", "type": "label", "text": "MVT Visualizer",
        "style": { "height": 36 } },
      { "id": "spectrum", "type": "spectrum",
        "style": { "flex": 1, "minHeight": 180 } },
      {
        "id": "lower", "type": "container",
        "style": { "direction": "row", "height": 200, "gap": 12 },
        "children": [
          { "id": "scope", "type": "vectorscope", "style": { "flex": 1 } },
          { "id": "wave", "type": "waveform", "style": { "flex": 1 } }
        ]
      }
    ]
  }
})";

bool loadJsonFile (const juce::File& file, juce::var& destination, juce::String* error)
{
    if (! file.existsAsFile())
    {
        if (error != nullptr)
            *error = "missing file: " + file.getFullPathName();
        return false;
    }

    const auto text = file.loadFileAsString();

    if (text.isEmpty())
    {
        if (error != nullptr)
            *error = "empty file: " + file.getFullPathName();
        return false;
    }

    const auto parsed = juce::JSON::parse (text);

    if (parsed.isVoid())
    {
        if (error != nullptr)
            *error = "invalid JSON: " + file.getFullPathName();
        return false;
    }

    destination = parsed;
    return true;
}

bool resolveChild (const juce::File& folder, const juce::String& relative, juce::File& result,
                   juce::String* error)
{
    if (relative.isEmpty() || juce::File::isAbsolutePath (relative)
        || relative.contains (".."))
    {
        if (error != nullptr)
            *error = "skin path must be relative and stay inside the skin folder: " + relative;
        return false;
    }

    result = folder.getChildFile (relative);
    return true;
}
} // namespace

Skin::Skin()
    : theme_ (std::make_unique<theme::ThemeRegistry>())
{
}

Skin::~Skin() = default;

bool Skin::loadDefault (juce::String* error)
{
    const auto defaultDir = defaultFolder();

    if (defaultDir.isDirectory())
    {
        juce::String folderError;
        if (loadFromFolder (defaultDir, &folderError))
        {
            folder_ = defaultDir;
            return true;
        }

        if (error != nullptr)
            *error = folderError;
    }

    return loadFallback (error);
}

bool Skin::loadFallback (juce::String* error)
{
    const auto layout = juce::JSON::parse (juce::String (kFallbackLayout));
    const auto theme = juce::JSON::parse (juce::String (kFallbackTheme));

    if (! loadFromDocuments (layout, theme, error))
        return false;

    name_ = "Fallback";
    folder_ = {};
    return true;
}

bool Skin::loadFromFolder (const juce::File& folder, juce::String* error)
{
    if (! folder.isDirectory())
    {
        if (error != nullptr)
            *error = "skin folder not found: " + folder.getFullPathName();
        return false;
    }

    juce::var manifest;

    if (! loadJsonFile (folder.getChildFile ("manifest.json"), manifest, error))
        return false;

    const auto layoutName = manifest.getProperty ("layout", "layout.json").toString();
    const auto themeName = manifest.getProperty ("theme", "theme.json").toString();

    juce::File layoutFile;
    juce::File themeFile;

    if (! resolveChild (folder, layoutName, layoutFile, error)
        || ! resolveChild (folder, themeName, themeFile, error))
        return false;

    juce::var layoutDocument;
    juce::var themeDocument;

    if (! loadJsonFile (themeFile, themeDocument, error)
        || ! loadJsonFile (layoutFile, layoutDocument, error))
        return false;

    if (! loadFromDocuments (layoutDocument, themeDocument, error))
        return false;

    const auto manifestName = manifest.getProperty ("name", "Untitled").toString();
    name_ = manifestName.isNotEmpty() ? manifestName : juce::String ("Untitled");
    folder_ = folder;
    return true;
}

bool Skin::loadFromDocuments (const juce::var& layoutDocument,
                              const juce::var& themeDocument,
                              juce::String* error)
{
    auto candidateTheme = std::make_unique<theme::ThemeRegistry>();

    if (! candidateTheme->loadFromJson (themeDocument))
    {
        if (error != nullptr)
        {
            juce::String message = "theme validation failed:";

            for (const auto& themeError : candidateTheme->errors())
            {
                message += "\n";
                message += juce::String (themeError.path);
                message += ": ";
                message += juce::String (themeError.message);
            }

            *error = message;
        }

        return false;
    }

    const auto resolver = [&candidateTheme] (std::string_view id) -> std::optional<float>
    {
        const auto dimension = candidateTheme->resolveDimension (id);

        if (! dimension.has_value())
            return std::nullopt;

        return dimension->pixels;
    };

    auto parsed = layout::parseLayout (layoutDocument, resolver);

    if (! parsed.ok() || ! parsed.root.has_value())
    {
        if (error != nullptr)
        {
            juce::String message = "layout validation failed:";

            for (const auto& layoutError : parsed.errors)
            {
                message += "\n";
                message += juce::String (layoutError.path);
                message += ": ";
                message += juce::String (layoutError.message);
            }

            *error = message;
        }

        return false;
    }

    theme_ = std::move (candidateTheme);
    layoutRoot_ = std::move (*parsed.root);
    themeDocument_ = juce::JSON::parse (juce::JSON::toString (themeDocument));
    return true;
}

void Skin::setLayoutRoot (layout::LayoutNode root)
{
    layoutRoot_ = std::move (root);
}

bool Skin::setThemeDocument (const juce::var& document, juce::String* error)
{
    auto candidate = std::make_unique<theme::ThemeRegistry>();

    if (! candidate->loadFromJson (document))
    {
        if (error != nullptr)
        {
            juce::String message = "theme validation failed:";

            for (const auto& themeError : candidate->errors())
            {
                message += "\n";
                message += juce::String (themeError.path);
                message += ": ";
                message += juce::String (themeError.message);
            }

            *error = message;
        }

        return false;
    }

    theme_ = std::move (candidate);
    themeDocument_ = juce::JSON::parse (juce::JSON::toString (document));
    return true;
}

juce::File Skin::defaultFolder()
{
    const auto environment = juce::SystemStats::getEnvironmentVariable ("MVT_SKIN_DIR", {});
    if (environment.isNotEmpty())
    {
        const auto candidate = juce::File (environment);
        if (candidate.isDirectory())
            return candidate;
    }

   #ifdef MVT_DEV_ASSETS_DIR
    const auto development = juce::File (juce::String (MVT_DEV_ASSETS_DIR))
                                 .getChildFile ("skins")
                                 .getChildFile ("default");
    if (development.isDirectory())
        return development;
   #endif

    const auto working = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("Assets")
                             .getChildFile ("skins")
                             .getChildFile ("default");
    if (working.isDirectory())
        return working;

    const auto executable = juce::File::getSpecialLocation (
        juce::File::currentExecutableFile).getParentDirectory();

    const auto besideExecutable = executable.getChildFile ("skins").getChildFile ("default");
    if (besideExecutable.isDirectory())
        return besideExecutable;

    const auto executableAssets = executable.getChildFile ("Assets")
                                               .getChildFile ("skins")
                                               .getChildFile ("default");
    if (executableAssets.isDirectory())
        return executableAssets;

    return {};
}

} // namespace mvt::ui
