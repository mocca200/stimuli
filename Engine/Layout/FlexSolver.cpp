#include "Engine/Layout/FlexSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace mvt::layout
{
namespace
{
constexpr int kMaxDepth = 64;
constexpr float kInfinity = std::numeric_limits<float>::max();

float mainOf (Size size, Direction direction) noexcept
{
    return direction == Direction::row ? size.width : size.height;
}

float crossOf (Size size, Direction direction) noexcept
{
    return direction == Direction::row ? size.height : size.width;
}

struct IntrinsicSize
{
    float width = 0.0f;
    float height = 0.0f;
};

float mainOf (IntrinsicSize size, Direction direction) noexcept
{
    return direction == Direction::row ? size.width : size.height;
}

float crossOf (IntrinsicSize size, Direction direction) noexcept
{
    return direction == Direction::row ? size.height : size.width;
}

float resolvePixels (const Length& length, float reference, float fallback = 0.0f) noexcept
{
    return resolveLength (length, reference, fallback);
}

float resolveEdgesMainStart (const ResolvedEdges& edges, Direction direction,
                             float referenceWidth, float referenceHeight) noexcept
{
    return direction == Direction::row ? resolvePixels (edges.left, referenceWidth)
                                       : resolvePixels (edges.top, referenceHeight);
}

float resolveEdgesMainEnd (const ResolvedEdges& edges, Direction direction,
                           float referenceWidth, float referenceHeight) noexcept
{
    return direction == Direction::row ? resolvePixels (edges.right, referenceWidth)
                                       : resolvePixels (edges.bottom, referenceHeight);
}

float resolveEdgesCrossStart (const ResolvedEdges& edges, Direction direction,
                              float referenceWidth, float referenceHeight) noexcept
{
    return direction == Direction::row ? resolvePixels (edges.top, referenceHeight)
                                       : resolvePixels (edges.left, referenceWidth);
}

float resolveEdgesCrossEnd (const ResolvedEdges& edges, Direction direction,
                            float referenceWidth, float referenceHeight) noexcept
{
    return direction == Direction::row ? resolvePixels (edges.bottom, referenceHeight)
                                       : resolvePixels (edges.right, referenceWidth);
}

float clampSize (float value,
                 const std::optional<Length>& minLength,
                 const std::optional<Length>& maxLength,
                 float reference) noexcept
{
    const auto minValue = minLength ? resolvePixels (*minLength, reference) : 0.0f;
    const auto maxValue = maxLength ? resolvePixels (*maxLength, reference, kInfinity) : kInfinity;
    return std::clamp (value, minValue, std::max (minValue, maxValue));
}

void applyAspectRatio (const Style& style, float& width, float& height) noexcept
{
    if (! style.aspectRatio.has_value() || *style.aspectRatio <= 0.0f)
        return;

    const auto ratio = *style.aspectRatio;
    const auto hasWidth = style.width.has_value();
    const auto hasHeight = style.height.has_value();

    if (hasWidth && ! hasHeight)
        height = width / ratio;
    else if (hasHeight && ! hasWidth)
        width = height * ratio;
}

class Solver
{
public:
    explicit Solver (float pixelScaleIn)
        : pixelScale (pixelScaleIn)
    {
    }

    LayoutBox solve (const LayoutNode& root, Size available)
    {
        const auto rootWidth = root.style.width
                                 ? resolvePixels (*root.style.width, available.width, available.width)
                                 : available.width;
        const auto rootHeight = root.style.height
                                  ? resolvePixels (*root.style.height, available.height, available.height)
                                  : available.height;

        Rect frame { 0.0f, 0.0f, std::max (0.0f, rootWidth), std::max (0.0f, rootHeight) };
        applyAspectRatio (root.style, frame.width, frame.height);
        frame.width = clampSize (frame.width, root.style.minWidth, root.style.maxWidth, available.width);
        frame.height = clampSize (frame.height, root.style.minHeight, root.style.maxHeight, available.height);

        LayoutBox result;
        layoutNode (root, frame, result, 0);
        snapTree (result);
        return result;
    }

private:
    IntrinsicSize measure (const LayoutNode& node, Size available, int depth) const
    {
        if (node.style.display == Display::none || depth > kMaxDepth)
            return {};

        const auto paddingX = resolvePixels (node.style.padding.left, available.width)
                            + resolvePixels (node.style.padding.right, available.width);
        const auto paddingY = resolvePixels (node.style.padding.top, available.height)
                            + resolvePixels (node.style.padding.bottom, available.height);

        auto measuredWidth = paddingX;
        auto measuredHeight = paddingY;

        std::vector<const LayoutNode*> flowChildren;
        flowChildren.reserve (node.children.size());

        for (const auto& child : node.children)
            if (child.style.display != Display::none && child.style.position != Position::absolute)
                flowChildren.push_back (&child);

        if (! flowChildren.empty())
        {
            const auto gap = resolvePixels (node.style.gap,
                                            node.style.direction == Direction::row ? available.width
                                                                                   : available.height);
            float contentMain = 0.0f;
            float contentCross = 0.0f;

            for (std::size_t i = 0; i < flowChildren.size(); ++i)
            {
                const auto& child = *flowChildren[i];
                const auto childSize = measure (child, available, depth + 1);

                const auto marginMainStart = resolveEdgesMainStart (child.style.margin,
                                                                    node.style.direction,
                                                                    available.width, available.height);
                const auto marginMainEnd = resolveEdgesMainEnd (child.style.margin,
                                                                node.style.direction,
                                                                available.width, available.height);
                const auto marginCrossStart = resolveEdgesCrossStart (child.style.margin,
                                                                      node.style.direction,
                                                                      available.width, available.height);
                const auto marginCrossEnd = resolveEdgesCrossEnd (child.style.margin,
                                                                  node.style.direction,
                                                                  available.width, available.height);

                if (node.style.direction == Direction::row)
                {
                    contentMain += childSize.width + marginMainStart + marginMainEnd;
                    contentCross = std::max (contentCross, childSize.height + marginCrossStart + marginCrossEnd);
                }
                else
                {
                    contentMain += childSize.height + marginMainStart + marginMainEnd;
                    contentCross = std::max (contentCross, childSize.width + marginCrossStart + marginCrossEnd);
                }

                if (i + 1 < flowChildren.size())
                    contentMain += gap;
            }

            if (node.style.direction == Direction::row)
            {
                measuredWidth += contentMain;
                measuredHeight += contentCross;
            }
            else
            {
                measuredHeight += contentMain;
                measuredWidth += contentCross;
            }
        }

        auto width = node.style.width ? resolvePixels (*node.style.width, available.width, measuredWidth)
                                      : measuredWidth;
        auto height = node.style.height ? resolvePixels (*node.style.height, available.height, measuredHeight)
                                        : measuredHeight;

        applyAspectRatio (node.style, width, height);

        width = clampSize (width, node.style.minWidth, node.style.maxWidth, available.width);
        height = clampSize (height, node.style.minHeight, node.style.maxHeight, available.height);

        return { std::max (0.0f, width), std::max (0.0f, height) };
    }

    struct Item
    {
        const LayoutNode* node = nullptr;
        float main = 0.0f;
        float cross = 0.0f;
        float minMain = 0.0f;
        float maxMain = kInfinity;
        float minCross = 0.0f;
        float maxCross = kInfinity;
        float grow = 0.0f;
        float shrink = 1.0f;
        float marginMainStart = 0.0f;
        float marginMainEnd = 0.0f;
        float marginCrossStart = 0.0f;
        float marginCrossEnd = 0.0f;
        bool stretchCross = false;
    };

    void layoutNode (const LayoutNode& node, const Rect& borderBox, LayoutBox& result, int depth)
    {
        result.id = node.id;
        result.type = node.type;
        result.text = node.text;
        result.frame = borderBox;
        result.children.clear();
        result.children.resize (node.children.size());

        if (depth > kMaxDepth || node.style.display == Display::none)
        {
            result.frame = {};
            result.content = {};
            return;
        }

        const auto padding = EdgeInsets {
            resolvePixels (node.style.padding.top, borderBox.height),
            resolvePixels (node.style.padding.right, borderBox.width),
            resolvePixels (node.style.padding.bottom, borderBox.height),
            resolvePixels (node.style.padding.left, borderBox.width)
        };

        result.content = borderBox.inset (padding.top, padding.right, padding.bottom, padding.left);

        const auto contentWidth = result.content.width;
        const auto contentHeight = result.content.height;
        const auto direction = node.style.direction;
        const auto mainReference = direction == Direction::row ? contentWidth : contentHeight;
        const auto crossReference = direction == Direction::row ? contentHeight : contentWidth;
        const auto gap = resolvePixels (node.style.gap, mainReference);

        std::vector<Item> flowItems;
        std::vector<std::size_t> flowIndices;
        flowItems.reserve (node.children.size());
        flowIndices.reserve (node.children.size());

        for (std::size_t i = 0; i < node.children.size(); ++i)
        {
            const auto& child = node.children[i];

            if (child.style.display == Display::none)
                continue;

            if (child.style.position == Position::absolute)
                continue;

            Item item;
            item.node = &child;
            item.grow = child.style.flexGrow;
            item.shrink = child.style.flexShrink;

            item.marginMainStart = resolveEdgesMainStart (child.style.margin, direction,
                                                           contentWidth, contentHeight);
            item.marginMainEnd = resolveEdgesMainEnd (child.style.margin, direction,
                                                       contentWidth, contentHeight);
            item.marginCrossStart = resolveEdgesCrossStart (child.style.margin, direction,
                                                             contentWidth, contentHeight);
            item.marginCrossEnd = resolveEdgesCrossEnd (child.style.margin, direction,
                                                         contentWidth, contentHeight);

            const auto intrinsic = measure (child, { contentWidth, contentHeight }, depth + 1);

            const auto explicitMain = direction == Direction::row ? child.style.width : child.style.height;
            const auto explicitCross = direction == Direction::row ? child.style.height : child.style.width;

            // In CSS, a definite flex-basis wins over width/height on the main axis.
            auto baseMain = child.style.flexBasis
                              ? resolvePixels (*child.style.flexBasis, mainReference)
                              : (explicitMain ? resolvePixels (*explicitMain, mainReference)
                                              : mainOf (intrinsic, direction));

            const auto& minMainLength = direction == Direction::row ? child.style.minWidth : child.style.minHeight;
            const auto& maxMainLength = direction == Direction::row ? child.style.maxWidth : child.style.maxHeight;

            item.minMain = minMainLength ? resolvePixels (*minMainLength, mainReference) : 0.0f;
            item.maxMain = maxMainLength ? resolvePixels (*maxMainLength, mainReference, kInfinity) : kInfinity;

            baseMain = std::clamp (baseMain, item.minMain, std::max (item.minMain, item.maxMain));

            const auto align = child.style.alignSelf.value_or (node.style.align);
            item.stretchCross = align == AlignItems::stretch
                                && ! explicitCross.has_value()
                                && ! child.style.aspectRatio.has_value();

            auto baseCross = explicitCross ? resolvePixels (*explicitCross, crossReference)
                                           : crossOf (intrinsic, direction);

            const auto& minCrossLength = direction == Direction::row ? child.style.minHeight : child.style.minWidth;
            const auto& maxCrossLength = direction == Direction::row ? child.style.maxHeight : child.style.maxWidth;

            item.minCross = minCrossLength ? resolvePixels (*minCrossLength, crossReference) : 0.0f;
            item.maxCross = maxCrossLength ? resolvePixels (*maxCrossLength, crossReference, kInfinity) : kInfinity;

            item.main = baseMain;
            item.cross = baseCross;

            flowItems.push_back (item);
            flowIndices.push_back (i);
        }

        const auto itemCount = flowItems.size();
        float fixedMain = 0.0f;
        float usedMain = 0.0f;

        for (const auto& item : flowItems)
        {
            fixedMain += item.marginMainStart + item.marginMainEnd;
            usedMain += item.main;
        }

        if (itemCount > 1)
            fixedMain += gap * static_cast<float> (itemCount - 1);

        auto freeMain = mainReference - fixedMain - usedMain;

        if (freeMain > 0.0f)
        {
            for (int iteration = 0; iteration < 8 && freeMain > 0.0001f; ++iteration)
            {
                float totalGrow = 0.0f;
                for (const auto& item : flowItems)
                    if (item.grow > 0.0f && item.main < item.maxMain - 0.0001f)
                        totalGrow += item.grow;

                if (totalGrow <= 0.0001f)
                    break;

                bool clampedAny = false;

                for (auto& item : flowItems)
                {
                    if (item.grow <= 0.0f || item.main >= item.maxMain - 0.0001f)
                        continue;

                    const auto share = freeMain * (item.grow / totalGrow);
                    const auto next = std::min (item.maxMain, item.main + share);
                    if (next < item.main + share - 0.0001f)
                        clampedAny = true;
                    item.main = next;
                }

                float newUsed = 0.0f;
                for (const auto& item : flowItems)
                    newUsed += item.main;

                freeMain = mainReference - fixedMain - newUsed;

                if (! clampedAny)
                    break;
            }
        }
        else if (freeMain < 0.0f)
        {
            for (int iteration = 0; iteration < 8 && freeMain < -0.0001f; ++iteration)
            {
                float totalWeight = 0.0f;
                for (const auto& item : flowItems)
                    if (item.shrink > 0.0f && item.main > item.minMain + 0.0001f)
                        totalWeight += item.shrink * std::max (item.main, 1.0f);

                if (totalWeight <= 0.0001f)
                    break;

                bool clampedAny = false;

                for (auto& item : flowItems)
                {
                    if (item.shrink <= 0.0f || item.main <= item.minMain + 0.0001f)
                        continue;

                    const auto weight = item.shrink * std::max (item.main, 1.0f);
                    const auto share = freeMain * (weight / totalWeight);
                    const auto next = std::max (item.minMain, item.main + share);
                    if (next > item.main + share + 0.0001f)
                        clampedAny = true;
                    item.main = next;
                }

                float newUsed = 0.0f;
                for (const auto& item : flowItems)
                    newUsed += item.main;

                freeMain = mainReference - fixedMain - newUsed;

                if (! clampedAny)
                    break;
            }
        }

        for (auto& item : flowItems)
        {
            item.main = std::max (0.0f, item.main);

            if (item.stretchCross)
            {
                const auto availableCross = std::max (0.0f, crossReference - item.marginCrossStart - item.marginCrossEnd);
                item.cross = std::clamp (availableCross, item.minCross, std::max (item.minCross, item.maxCross));
            }
            else
            {
                item.cross = std::clamp (item.cross, item.minCross, std::max (item.minCross, item.maxCross));
            }
        }

        float usedAfterFlex = 0.0f;
        for (const auto& item : flowItems)
            usedAfterFlex += item.main + item.marginMainStart + item.marginMainEnd;

        if (itemCount > 1)
            usedAfterFlex += gap * static_cast<float> (itemCount - 1);

        const auto leftover = mainReference - usedAfterFlex;
        float leading = 0.0f;
        float extraGap = 0.0f;

        switch (node.style.justify)
        {
            case JustifyContent::start:
                break;
            case JustifyContent::centre:
                leading = leftover * 0.5f;
                break;
            case JustifyContent::end:
                leading = leftover;
                break;
            case JustifyContent::spaceBetween:
                if (itemCount > 1 && leftover > 0.0f)
                    extraGap = leftover / static_cast<float> (itemCount - 1);
                break;
            case JustifyContent::spaceAround:
                if (itemCount > 0 && leftover > 0.0f)
                {
                    const auto perItem = leftover / static_cast<float> (itemCount);
                    leading = perItem * 0.5f;
                    extraGap = perItem;
                }
                break;
            case JustifyContent::spaceEvenly:
                if (itemCount > 0 && leftover > 0.0f)
                {
                    const auto perItem = leftover / static_cast<float> (itemCount + 1);
                    leading = perItem;
                    extraGap = perItem;
                }
                break;
        }

        auto mainCursor = (direction == Direction::row ? result.content.x : result.content.y) + leading;
        const auto crossOrigin = direction == Direction::row ? result.content.y : result.content.x;

        for (std::size_t i = 0; i < flowItems.size(); ++i)
        {
            const auto& item = flowItems[i];
            const auto align = item.node->style.alignSelf.value_or (node.style.align);

            float crossOffset = 0.0f;
            const auto availableCross = std::max (0.0f, crossReference - item.marginCrossStart - item.marginCrossEnd);

            switch (align)
            {
                case AlignItems::start:
                case AlignItems::stretch:
                    crossOffset = 0.0f;
                    break;
                case AlignItems::centre:
                    crossOffset = (availableCross - item.cross) * 0.5f;
                    break;
                case AlignItems::end:
                    crossOffset = availableCross - item.cross;
                    break;
            }

            const auto mainStart = mainCursor + item.marginMainStart;
            const auto crossStart = crossOrigin + item.marginCrossStart + crossOffset;

            Rect childFrame;
            if (direction == Direction::row)
                childFrame = { mainStart, crossStart, item.main, item.cross };
            else
                childFrame = { crossStart, mainStart, item.cross, item.main };

            layoutNode (*item.node, childFrame, result.children[flowIndices[i]], depth + 1);

            mainCursor = mainStart + item.main + item.marginMainEnd + gap + extraGap;
        }

        // Absolute children are laid out after flow, but written into their
        // original DOM index so painter order remains intuitive.
        for (std::size_t i = 0; i < node.children.size(); ++i)
        {
            const auto& child = node.children[i];

            if (child.style.display == Display::none || child.style.position != Position::absolute)
                continue;

            std::optional<float> left;
            std::optional<float> top;
            std::optional<float> right;
            std::optional<float> bottom;

            if (child.style.inset.left.isDefinite())
                left = resolvePixels (child.style.inset.left, contentWidth);
            if (child.style.inset.top.isDefinite())
                top = resolvePixels (child.style.inset.top, contentHeight);
            if (child.style.inset.right.isDefinite())
                right = resolvePixels (child.style.inset.right, contentWidth);
            if (child.style.inset.bottom.isDefinite())
                bottom = resolvePixels (child.style.inset.bottom, contentHeight);

            const auto intrinsic = measure (child, { contentWidth, contentHeight }, depth + 1);

            auto width = child.style.width
                           ? resolvePixels (*child.style.width, contentWidth, intrinsic.width)
                           : (left.has_value() && right.has_value()
                                  ? std::max (0.0f, contentWidth - *left - *right)
                                  : intrinsic.width);
            auto height = child.style.height
                            ? resolvePixels (*child.style.height, contentHeight, intrinsic.height)
                            : (top.has_value() && bottom.has_value()
                                   ? std::max (0.0f, contentHeight - *top - *bottom)
                                   : intrinsic.height);

            applyAspectRatio (child.style, width, height);

            width = clampSize (width, child.style.minWidth, child.style.maxWidth, contentWidth);
            height = clampSize (height, child.style.minHeight, child.style.maxHeight, contentHeight);

            const auto x = result.content.x + (left.has_value()
                                                   ? *left
                                                   : (right.has_value() ? contentWidth - *right - width : 0.0f));
            const auto y = result.content.y + (top.has_value()
                                                   ? *top
                                                   : (bottom.has_value() ? contentHeight - *bottom - height : 0.0f));

            layoutNode (child, { x, y, width, height }, result.children[i], depth + 1);
        }
    }

    void snapTree (LayoutBox& box) const
    {
        box.frame = snapRect (box.frame, pixelScale);
        box.content = snapRect (box.content, pixelScale);

        for (auto& child : box.children)
            snapTree (child);
    }

    float pixelScale = 1.0f;
};

} // namespace

LayoutBox FlexSolver::solve (const LayoutNode& root, Size available, float pixelScale)
{
    Solver solver (pixelScale);
    return solver.solve (root, available);
}

const LayoutBox* FlexSolver::hitTest (const LayoutBox& root, Point point) noexcept
{
    if (! root.frame.contains (point))
        return nullptr;

    for (auto it = root.children.rbegin(); it != root.children.rend(); ++it)
        if (const auto* hit = hitTest (*it, point))
            return hit;

    return &root;
}

const LayoutBox* FlexSolver::findById (const LayoutBox& root, std::string_view id) noexcept
{
    if (root.id == id)
        return &root;

    for (const auto& child : root.children)
        if (const auto* found = findById (child, id))
            return found;

    return nullptr;
}

} // namespace mvt::layout
