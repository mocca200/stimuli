#include "UIEngine/DesignerDocument.h"

#include "Engine/Layout/LayoutParser.h"
#include "Engine/Layout/LayoutWriter.h"
#include "UIEngine/Skin.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <unordered_set>

namespace mvt::ui
{
namespace
{
constexpr std::size_t kMaxUndoEntries = 64;

juce::var deepCopy (const juce::var& value)
{
    return juce::JSON::parse (juce::JSON::toString (value));
}

layout::LayoutNode* findMutable (layout::LayoutNode& node, std::string_view id)
{
    if (node.id == id)
        return &node;

    for (auto& child : node.children)
        if (auto* found = findMutable (child, id))
            return found;

    return nullptr;
}

const layout::LayoutNode* findConst (const layout::LayoutNode& node, std::string_view id)
{
    if (node.id == id)
        return &node;

    for (const auto& child : node.children)
        if (const auto* found = findConst (child, id))
            return found;

    return nullptr;
}

layout::LayoutNode* findParent (layout::LayoutNode& node, std::string_view id, std::size_t* index)
{
    for (std::size_t i = 0; i < node.children.size(); ++i)
    {
        if (node.children[i].id == id)
        {
            if (index != nullptr)
                *index = i;

            return &node;
        }

        if (auto* found = findParent (node.children[i], id, index))
            return found;
    }

    return nullptr;
}

void collectIds (const layout::LayoutNode& node, std::unordered_set<std::string>& ids)
{
    ids.insert (node.id);

    for (const auto& child : node.children)
        collectIds (child, ids);
}

std::string uniqueId (const std::unordered_set<std::string>& ids, const std::string& base)
{
    if (ids.count (base) == 0)
        return base;

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        auto candidate = base + "-" + std::to_string (suffix);

        if (ids.count (candidate) == 0)
            return candidate;
    }

    return base + "-copy";
}

layout::LayoutNode duplicateTree (const layout::LayoutNode& source, std::unordered_set<std::string>& ids)
{
    layout::LayoutNode copy;
    copy.id = uniqueId (ids, source.id + "-copy");
    ids.insert (copy.id);
    copy.type = source.type;
    copy.text = source.text;
    copy.style = source.style;
    copy.children.reserve (source.children.size());

    for (const auto& child : source.children)
        copy.children.push_back (duplicateTree (child, ids));

    return copy;
}

juce::var tokenValueVar (const theme::TokenValue& value)
{
    if (const auto* color = std::get_if<theme::Color> (&value))
        return juce::var (juce::String (color->toHex (color->a != 255).c_str()));

    if (const auto* dimension = std::get_if<theme::Dimension> (&value))
        return juce::var (static_cast<double> (dimension->pixels));

    if (const auto* number = std::get_if<float> (&value))
        return juce::var (static_cast<double> (*number));

    if (const auto* boolean = std::get_if<bool> (&value))
        return juce::var (*boolean);

    if (const auto* text = std::get_if<std::string> (&value))
        return juce::var (juce::String (text->c_str()));

    if (const auto* shadow = std::get_if<theme::Shadow> (&value))
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("x", static_cast<double> (shadow->offsetX));
        object->setProperty ("y", static_cast<double> (shadow->offsetY));
        object->setProperty ("blur", static_cast<double> (shadow->blur));
        object->setProperty ("color", juce::String (shadow->color.toHex (true).c_str()));
        return juce::var (object);
    }

    if (const auto* type = std::get_if<theme::Typography> (&value))
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("family", juce::String (type->family.c_str()));
        object->setProperty ("size", static_cast<double> (type->sizePx));
        object->setProperty ("weight", type->weight);
        object->setProperty ("lineHeight", static_cast<double> (type->lineHeightPx));
        return juce::var (object);
    }

    return {};
}

float clampDimension (float value,
                      const std::optional<layout::Length>& minLength,
                      const std::optional<layout::Length>& maxLength)
{
    auto result = value;

    const auto minPixels = minLength.has_value()
                             && minLength->kind == layout::LengthKind::pixels
                             ? minLength->value
                             : 0.0f;

    if (minLength.has_value() && minLength->kind == layout::LengthKind::pixels)
        result = std::max (result, minPixels);

    if (maxLength.has_value() && maxLength->kind == layout::LengthKind::pixels)
        result = std::min (result, std::max (minPixels, maxLength->value));

    return std::max (1.0f, result);
}
} // namespace

bool DesignerDocument::loadFromSkin (const Skin& skin)
{
    auto themeDocument = deepCopy (skin.themeDocument());
    auto candidate = theme::ThemeRegistry();

    if (! candidate.loadFromJson (themeDocument))
        return false;

    theme_ = std::move (candidate);
    themeDocument_ = std::move (themeDocument);
    root_ = skin.layoutRoot();

    overrides_.clear();
    appliedOverrides_.clear();
    undoStack_.clear();
    redoStack_.clear();
    selectedId_ = root_.id;
    ++revision_;
    dirty_ = false;
    return true;
}

bool DesignerDocument::setLayoutDocument (const juce::var& layoutDocument)
{
    const auto resolver = [this] (std::string_view id) -> std::optional<float>
    {
        if (const auto dimension = theme_.resolveDimension (id))
            return dimension->pixels;

        return std::nullopt;
    };

    auto parsed = layout::parseLayout (layoutDocument, resolver);

    if (! parsed.ok() || ! parsed.root.has_value())
        return false;

    root_ = std::move (*parsed.root);
    undoStack_.clear();
    redoStack_.clear();
    selectedId_ = root_.id;
    ++revision_;
    dirty_ = false;
    return true;
}

bool DesignerDocument::setThemeDocument (const juce::var& themeDocument)
{
    auto copy = deepCopy (themeDocument);
    auto candidate = theme::ThemeRegistry();

    if (! candidate.loadFromJson (copy))
        return false;

    theme_ = std::move (candidate);
    themeDocument_ = std::move (copy);
    syncThemeOverrides();
    ++revision_;
    dirty_ = true;
    return true;
}

const layout::LayoutNode* DesignerDocument::find (std::string_view id) const noexcept
{
    return findConst (root_, id);
}

void DesignerDocument::select (std::string id)
{
    if (findConst (root_, id) != nullptr)
        selectedId_ = std::move (id);
}

bool DesignerDocument::setNodeFrame (std::string_view id, layout::Rect frameInParentContent)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr)
        return false;

    const auto x = std::max (0.0f, frameInParentContent.x);
    const auto y = std::max (0.0f, frameInParentContent.y);
    const auto width = clampDimension (frameInParentContent.width, node->style.minWidth, node->style.maxWidth);
    const auto height = clampDimension (frameInParentContent.height, node->style.minHeight, node->style.maxHeight);

    beginChange();

    auto& style = node->style;
    style.position = layout::Position::absolute;
    style.inset.left = layout::Length::px (x);
    style.inset.top = layout::Length::px (y);
    style.inset.right = layout::Length::automatic();
    style.inset.bottom = layout::Length::automatic();
    style.width = layout::Length::px (width);
    style.height = layout::Length::px (height);

    finishChange();
    return true;
}

bool DesignerDocument::moveNode (std::string_view id, float deltaX, float deltaY)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr || node->style.position != layout::Position::absolute)
        return false;

    beginChange();

    const auto currentX = node->style.inset.left.kind == layout::LengthKind::pixels
                            ? node->style.inset.left.value
                            : 0.0f;
    const auto currentY = node->style.inset.top.kind == layout::LengthKind::pixels
                            ? node->style.inset.top.value
                            : 0.0f;

    node->style.inset.left = layout::Length::px (std::max (0.0f, currentX + deltaX));
    node->style.inset.top = layout::Length::px (std::max (0.0f, currentY + deltaY));

    finishChange();
    return true;
}

bool DesignerDocument::resizeNode (std::string_view id, float width, float height)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr)
        return false;

    beginChange();

    node->style.width = layout::Length::px (clampDimension (width, node->style.minWidth, node->style.maxWidth));
    node->style.height = layout::Length::px (clampDimension (height, node->style.minHeight, node->style.maxHeight));

    finishChange();
    return true;
}

bool DesignerDocument::setStyle (std::string_view id, const layout::Style& style)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr)
        return false;

    beginChange();
    node->style = style;
    finishChange();
    return true;
}

bool DesignerDocument::setNodeText (std::string_view id, std::string text)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr)
        return false;

    beginChange();
    node->text = std::move (text);
    finishChange();
    return true;
}

bool DesignerDocument::setNodeType (std::string_view id, std::string type)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr || type.empty())
        return false;

    beginChange();
    node->type = std::move (type);
    finishChange();
    return true;
}

bool DesignerDocument::renameId (std::string_view id, std::string newId)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr || newId.empty() || newId == id)
        return false;

    if (findConst (root_, newId) != nullptr)
        return false;

    beginChange();

    const auto previousId = node->id;
    node->id = std::move (newId);

    if (selectedId_ == previousId)
        selectedId_ = node->id;

    finishChange();
    return true;
}

bool DesignerDocument::reorderNode (std::string_view id, std::size_t newIndex)
{
    std::size_t currentIndex = 0;
    auto* parent = findParent (root_, id, &currentIndex);

    if (parent == nullptr || parent->children.size() < 2)
        return false;

    const auto targetIndex = std::min (newIndex, parent->children.size() - 1);

    if (targetIndex == currentIndex)
        return false;

    beginChange();

    auto moved = std::move (parent->children[currentIndex]);
    parent->children.erase (parent->children.begin() + static_cast<std::ptrdiff_t> (currentIndex));
    parent->children.insert (parent->children.begin() + static_cast<std::ptrdiff_t> (targetIndex),
                             std::move (moved));

    finishChange();
    return true;
}

bool DesignerDocument::reparentNode (std::string_view id, std::string_view newParentId, std::size_t index)
{
    if (id == newParentId || id == root_.id)
        return false;

    auto* node = findMutable (root_, id);

    if (node == nullptr)
        return false;

    const auto* candidateParent = findConst (root_, newParentId);

    if (candidateParent == nullptr || candidateParent->type != "container")
        return false;

    // Reject moving a node into its own subtree, which would orphan it.
    if (findConst (*node, newParentId) != nullptr)
        return false;

    std::size_t oldIndex = 0;
    auto* oldParent = findParent (root_, id, &oldIndex);

    if (oldParent == nullptr)
        return false;

    if (oldParent == candidateParent && (index == oldIndex || index == oldIndex + 1))
        return false;

    beginChange();

    auto moved = std::move (oldParent->children[oldIndex]);
    oldParent->children.erase (oldParent->children.begin() + static_cast<std::ptrdiff_t> (oldIndex));

    // The parent pointer may have been invalidated by the erase above, so the
    // target is looked up again by id before inserting.
    auto* parentAfterRemoval = findMutable (root_, newParentId);

    if (parentAfterRemoval == nullptr)
    {
        // Restore the node and drop the undo entry; the document is unchanged.
        oldParent->children.insert (oldParent->children.begin() + static_cast<std::ptrdiff_t> (oldIndex),
                                    std::move (moved));
        undoStack_.pop_back();
        return false;
    }

    const auto targetIndex = std::min (index, parentAfterRemoval->children.size());
    parentAfterRemoval->children.insert (parentAfterRemoval->children.begin()
                                            + static_cast<std::ptrdiff_t> (targetIndex),
                                         std::move (moved));

    finishChange();
    return true;
}

bool DesignerDocument::groupSelection (std::string_view id, layout::Rect frameInParentContent)
{
    auto* node = findMutable (root_, id);

    if (node == nullptr || id == root_.id)
        return false;

    std::size_t index = 0;
    auto* parent = findParent (root_, id, &index);

    if (parent == nullptr)
        return false;

    std::unordered_set<std::string> ids;
    collectIds (root_, ids);

    beginChange();

    layout::LayoutNode group;
    group.id = uniqueId (ids, "group");
    group.type = "container";
    group.style.direction = node->style.direction;
    group.style.position = layout::Position::absolute;
    group.style.inset.left = layout::Length::px (std::max (0.0f, frameInParentContent.x));
    group.style.inset.top = layout::Length::px (std::max (0.0f, frameInParentContent.y));
    group.style.width = layout::Length::px (std::max (1.0f, frameInParentContent.width));
    group.style.height = layout::Length::px (std::max (1.0f, frameInParentContent.height));
    group.style.flexGrow = 0.0f;

    auto child = std::move (parent->children[index]);
    child.style.position = layout::Position::relative;
    child.style.inset = {};
    child.style.width = layout::Length::percent (100.0f);
    child.style.height = layout::Length::percent (100.0f);
    child.style.flexGrow = 0.0f;
    child.style.flexShrink = 0.0f;

    group.children.push_back (std::move (child));
    parent->children[index] = std::move (group);

    selectedId_ = parent->children[index].id;
    finishChange();
    return true;
}

bool DesignerDocument::duplicateNode (std::string_view id)
{
    std::size_t index = 0;
    auto* parent = findParent (root_, id, &index);

    if (parent == nullptr)
        return false;

    const auto* node = findConst (root_, id);

    if (node == nullptr)
        return false;

    std::unordered_set<std::string> ids;
    collectIds (root_, ids);

    auto copy = duplicateTree (*node, ids);

    beginChange();

    parent->children.insert (parent->children.begin() + static_cast<std::ptrdiff_t> (index + 1),
                             std::move (copy));

    selectedId_ = parent->children[index + 1].id;
    finishChange();
    return true;
}

bool DesignerDocument::deleteNode (std::string_view id)
{
    if (id == root_.id)
        return false;

    std::size_t index = 0;
    auto* parent = findParent (root_, id, &index);

    if (parent == nullptr)
        return false;

    const auto parentId = parent->id;

    beginChange();

    parent->children.erase (parent->children.begin() + static_cast<std::ptrdiff_t> (index));

    if (selectedId_ == id)
        selectedId_ = parentId;

    finishChange();
    return true;
}

bool DesignerDocument::setThemeOverride (std::string tokenId, theme::TokenValue value)
{
    if (tokenId.empty() || ! theme_.hasToken (tokenId))
        return false;

    beginChange();

    const auto existing = std::find_if (overrides_.begin(), overrides_.end(),
                                        [&tokenId] (const auto& entry) { return entry.first == tokenId; });

    if (existing != overrides_.end())
        existing->second = value;
    else
        overrides_.emplace_back (tokenId, std::move (value));

    syncThemeOverrides();
    finishChange();
    return true;
}

bool DesignerDocument::clearThemeOverride (std::string_view tokenId)
{
    const auto existing = std::find_if (overrides_.begin(), overrides_.end(),
                                        [tokenId] (const auto& entry) { return entry.first == tokenId; });

    if (existing == overrides_.end())
        return false;

    beginChange();
    overrides_.erase (existing);
    syncThemeOverrides();
    finishChange();
    return true;
}

bool DesignerDocument::undo()
{
    if (undoStack_.empty())
        return false;

    redoStack_.push_back ({ root_, selectedId_, overrides_ });
    auto snapshot = std::move (undoStack_.back());
    undoStack_.pop_back();

    root_ = std::move (snapshot.root);
    selectedId_ = std::move (snapshot.selection);
    overrides_ = std::move (snapshot.overrides);
    syncThemeOverrides();

    ++revision_;
    dirty_ = true;
    return true;
}

bool DesignerDocument::redo()
{
    if (redoStack_.empty())
        return false;

    undoStack_.push_back ({ root_, selectedId_, overrides_ });
    auto snapshot = std::move (redoStack_.back());
    redoStack_.pop_back();

    root_ = std::move (snapshot.root);
    selectedId_ = std::move (snapshot.selection);
    overrides_ = std::move (snapshot.overrides);
    syncThemeOverrides();

    ++revision_;
    dirty_ = true;
    return true;
}

juce::var DesignerDocument::toJson() const
{
    return layout::writeLayout (root_);
}

juce::var DesignerDocument::savedThemeDocument() const
{
    auto document = deepCopy (themeDocument_);
    auto* root = document.getDynamicObject();

    if (root == nullptr)
        return document;

    if (! root->hasProperty ("tokens"))
        root->setProperty ("tokens", juce::var (new juce::DynamicObject()));

    auto tokens = root->getProperty ("tokens");

    if (auto* tokensObject = tokens.getDynamicObject())
    {
        for (const auto& [id, value] : overrides_)
            tokensObject->setProperty (juce::Identifier (juce::String (id)), tokenValueVar (value));
    }

    return document;
}

bool DesignerDocument::save (const juce::File& folder, juce::String* error)
{
    if (! folder.isDirectory() && ! folder.createDirectory())
    {
        if (error != nullptr)
            *error = "cannot create skin folder: " + folder.getFullPathName();

        return false;
    }

    const auto layoutTarget = folder.getChildFile ("layout.json");

    if (! layoutTarget.replaceWithText (juce::JSON::toString (toJson())))
    {
        if (error != nullptr)
            *error = "cannot write " + layoutTarget.getFullPathName();

        return false;
    }

    const auto themeTarget = folder.getChildFile ("theme.json");

    if (! themeTarget.replaceWithText (juce::JSON::toString (savedThemeDocument())))
    {
        if (error != nullptr)
            *error = "cannot write " + themeTarget.getFullPathName();

        return false;
    }

    // A skin folder is not loadable without a manifest, so saving into a new
    // folder creates a minimal one instead of leaving a half-valid skin.
    const auto manifestTarget = folder.getChildFile ("manifest.json");

    if (! manifestTarget.existsAsFile())
    {
        auto* manifest = new juce::DynamicObject();
        manifest->setProperty ("name", folder.getFileName().isNotEmpty()
                                         ? folder.getFileName()
                                         : juce::String ("Untitled"));
        manifest->setProperty ("layout", "layout.json");
        manifest->setProperty ("theme", "theme.json");
        manifestTarget.replaceWithText (juce::JSON::toString (juce::var (manifest)));
    }

    markSaved();
    return true;
}

void DesignerDocument::beginChange()
{
    pushUndo();
}

void DesignerDocument::finishChange()
{
    ++revision_;
    dirty_ = true;
}

void DesignerDocument::pushUndo()
{
    undoStack_.push_back ({ root_, selectedId_, overrides_ });

    if (undoStack_.size() > kMaxUndoEntries)
        undoStack_.erase (undoStack_.begin());

    redoStack_.clear();
}

void DesignerDocument::syncThemeOverrides()
{
    for (const auto& id : appliedOverrides_)
        theme_.clearOverride (id);

    appliedOverrides_.clear();

    for (const auto& [id, value] : overrides_)
    {
        theme_.setOverride (id, value);
        appliedOverrides_.push_back (id);
    }
}

} // namespace mvt::ui
