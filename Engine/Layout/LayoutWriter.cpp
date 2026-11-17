#include "Engine/Layout/LayoutWriter.h"

#include <utility>

namespace mvt::layout
{
namespace
{
juce::var lengthVar (const Length& length)
{
    switch (length.kind)
    {
        case LengthKind::pixels:  return juce::var (static_cast<double> (length.value));
        case LengthKind::percent: return juce::var (juce::String (length.value, 6) + "%");
        case LengthKind::autoValue: break;
    }

    return juce::var ("auto");
}

juce::var optionalLengthVar (const std::optional<Length>& length)
{
    return length.has_value() ? lengthVar (*length) : lengthVar (Length::automatic());
}

juce::var edgesVar (const ResolvedEdges& edges)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("top", lengthVar (edges.top));
    object->setProperty ("right", lengthVar (edges.right));
    object->setProperty ("bottom", lengthVar (edges.bottom));
    object->setProperty ("left", lengthVar (edges.left));
    return juce::var (object);
}

juce::String directionText (Direction direction)
{
    return direction == Direction::row ? "row" : "column";
}

juce::String justifyText (JustifyContent justify)
{
    switch (justify)
    {
        case JustifyContent::start:        return "start";
        case JustifyContent::centre:       return "center";
        case JustifyContent::end:          return "end";
        case JustifyContent::spaceBetween: return "space-between";
        case JustifyContent::spaceAround:  return "space-around";
        case JustifyContent::spaceEvenly:  return "space-evenly";
    }

    return "start";
}

juce::String alignText (AlignItems align)
{
    switch (align)
    {
        case AlignItems::start:   return "start";
        case AlignItems::centre:  return "center";
        case AlignItems::end:     return "end";
        case AlignItems::stretch: return "stretch";
    }

    return "stretch";
}

juce::var styleVar (const Style& style)
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("width", optionalLengthVar (style.width));
    object->setProperty ("height", optionalLengthVar (style.height));
    object->setProperty ("minWidth", optionalLengthVar (style.minWidth));
    object->setProperty ("minHeight", optionalLengthVar (style.minHeight));
    object->setProperty ("maxWidth", optionalLengthVar (style.maxWidth));
    object->setProperty ("maxHeight", optionalLengthVar (style.maxHeight));
    object->setProperty ("flexBasis", optionalLengthVar (style.flexBasis));

    object->setProperty ("margin", edgesVar (style.margin));
    object->setProperty ("padding", edgesVar (style.padding));

    object->setProperty ("gap", lengthVar (style.gap));
    object->setProperty ("direction", directionText (style.direction));
    object->setProperty ("justifyContent", justifyText (style.justify));
    object->setProperty ("alignItems", alignText (style.align));

    if (style.alignSelf.has_value())
        object->setProperty ("alignSelf", alignText (*style.alignSelf));

    object->setProperty ("position", style.position == Position::absolute ? "absolute" : "relative");
    object->setProperty ("display", style.display == Display::none ? "none" : "flex");
    object->setProperty ("flexGrow", static_cast<double> (style.flexGrow));
    object->setProperty ("flexShrink", static_cast<double> (style.flexShrink));

    if (style.aspectRatio.has_value())
        object->setProperty ("aspectRatio", static_cast<double> (*style.aspectRatio));

    object->setProperty ("inset", edgesVar (style.inset));

    return juce::var (object);
}

juce::var nodeVar (const LayoutNode& node)
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("id", juce::String (node.id));
    object->setProperty ("type", juce::String (node.type));

    if (! node.text.empty())
        object->setProperty ("text", juce::String (node.text));

    object->setProperty ("style", styleVar (node.style));

    if (! node.children.empty())
    {
        juce::Array<juce::var> children;
        children.ensureStorageAllocated (static_cast<int> (node.children.size()));

        for (const auto& child : node.children)
            children.add (nodeVar (child));

        object->setProperty ("children", children);
    }

    return juce::var (object);
}
} // namespace

juce::var writeLayout (const LayoutNode& root)
{
    auto* document = new juce::DynamicObject();
    document->setProperty ("$schema", "mvt.layout/1");
    document->setProperty ("root", nodeVar (root));
    return juce::var (document);
}

} // namespace mvt::layout
