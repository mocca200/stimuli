#include "Engine/Layout/FlexSolver.h"
#include "Engine/Layout/LayoutParser.h"
#include "Engine/Theme/ThemeRegistry.h"

#include <juce_core/juce_core.h>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
std::optional<std::string> readFile (const std::string& path)
{
    std::ifstream stream (path, std::ios::binary);
    if (! stream)
        return std::nullopt;

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void printBox (const mvt::layout::LayoutBox& box, int depth)
{
    std::cout << std::string (static_cast<std::size_t> (depth) * 2, ' ')
              << box.id << " [" << box.type << "]"
              << " frame=(" << std::fixed << std::setprecision (2)
              << box.frame.x << ", " << box.frame.y << ", "
              << box.frame.width << ", " << box.frame.height << ")"
              << " content=(" << box.content.x << ", " << box.content.y << ", "
              << box.content.width << ", " << box.content.height << ")\n";

    for (const auto& child : box.children)
        printBox (child, depth + 1);
}

void printErrors (const std::vector<mvt::layout::LayoutError>& errors)
{
    for (const auto& error : errors)
        std::cerr << (error.warning ? "warning: " : "error: ")
                  << error.path << ": " << error.message << '\n';
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: mvt_layout_dump <layout.json> "
                     "[--width N] [--height N] [--scale S] [--theme theme.json]\n";
        return 2;
    }

    const std::string layoutPath = argv[1];
    float width = 1280.0f;
    float height = 720.0f;
    float scale = 0.0f;
    std::string themePath;

    for (int i = 2; i < argc; ++i)
    {
        const std::string_view argument = argv[i];

        auto readValue = [&] (const char* name) -> std::optional<float>
        {
            if (argument == name && i + 1 < argc)
            {
                try
                {
                    return std::stof (argv[++i]);
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        };

        if (const auto widthValue = readValue ("--width"))
            width = *widthValue;
        else if (const auto heightValue = readValue ("--height"))
            height = *heightValue;
        else if (const auto scaleValue = readValue ("--scale"))
            scale = *scaleValue;
        else if (argument == "--theme" && i + 1 < argc)
            themePath = argv[++i];
        else
        {
            std::cerr << "unknown argument: " << argument << '\n';
            return 2;
        }
    }

    const auto layoutText = readFile (layoutPath);
    if (! layoutText.has_value())
    {
        std::cerr << "cannot read layout file: " << layoutPath << '\n';
        return 1;
    }

    mvt::theme::ThemeRegistry theme;
    const bool haveTheme = ! themePath.empty();

    if (haveTheme)
    {
        const auto themeText = readFile (themePath);
        if (! themeText.has_value())
        {
            std::cerr << "cannot read theme file: " << themePath << '\n';
            return 1;
        }

        const auto themeJson = juce::JSON::parse (juce::String::fromUTF8 (themeText->c_str()));
        if (! theme.loadFromJson (themeJson))
        {
            std::cerr << "theme validation failed:\n";
            for (const auto& error : theme.errors())
                std::cerr << "  " << error.path << ": " << error.message << '\n';
            return 1;
        }
    }

    const auto document = juce::JSON::parse (juce::String::fromUTF8 (layoutText->c_str()));

    const mvt::layout::DimensionTokenResolver resolver = [&] (std::string_view id) -> std::optional<float>
    {
        if (! haveTheme)
            return std::nullopt;

        std::string error;
        const auto dimension = theme.resolveDimension (id, {}, &error);
        if (! dimension.has_value())
            return std::nullopt;

        return dimension->pixels;
    };

    const auto parsed = mvt::layout::parseLayout (document, resolver);
    printErrors (parsed.errors);

    if (! parsed.ok() || ! parsed.root.has_value())
        return 1;

    const auto box = mvt::layout::FlexSolver::solve (*parsed.root, { width, height }, scale);
    printBox (box, 0);
    return 0;
}
