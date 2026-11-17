#include "Designer/HierarchyPanel.h"

#include "UIEngine/DesignerDocument.h"

#include <utility>

namespace mvt::ui
{
class HierarchyPanel::NodeItem final : public juce::TreeViewItem
{
public:
    NodeItem (HierarchyPanel& owner, std::string id)
        : owner_ (owner), nodeId_ (std::move (id))
    {
    }

    bool mightContainSubItems() override
    {
        const auto* node = owner_.document_ != nullptr ? owner_.document_->find (nodeId_) : nullptr;
        return node != nullptr && ! node->children.empty();
    }

    juce::String getUniqueName() const override
    {
        return juce::String (nodeId_);
    }

    int getItemHeight() const override { return 20; }

    void paintItem (juce::Graphics& graphics, int width, int height) override
    {
        const auto* node = owner_.document_ != nullptr ? owner_.document_->find (nodeId_) : nullptr;

        if (node == nullptr)
            return;

        if (isSelected())
        {
            graphics.setColour (juce::Colour (0xff25313f));
            graphics.fillRect (0, 0, width, height);
        }

        graphics.setFont (juce::Font (juce::FontOptions (12.5f)));
        graphics.setColour (node->type == "container" ? juce::Colour (0xffd9e2ec)
                                                      : juce::Colour (0xff93c5e8));
        graphics.drawText (juce::String (node->id) + "  (" + juce::String (node->type) + ")",
                           4, 0, width - 8, height,
                           juce::Justification::centredLeft, true);
    }

    void itemOpennessChanged (bool isNowOpen) override
    {
        if (isNowOpen)
        {
            if (getNumSubItems() == 0)
                buildChildren();
        }
        else
        {
            clearSubItems();
        }
    }

    void itemSelectionChanged (bool isNowSelected) override
    {
        if (! isNowSelected || owner_.syncing_ || owner_.document_ == nullptr)
            return;

        owner_.document_->select (nodeId_);

        if (owner_.onSelectionChanged != nullptr)
            owner_.onSelectionChanged();
    }

    juce::var getDragSourceDescription() override
    {
        if (owner_.document_ == nullptr || nodeId_ == owner_.document_->root().id)
            return {};

        return juce::String (nodeId_);
    }

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override
    {
        if (owner_.document_ == nullptr)
            return false;

        const auto source = details.description.toString().toStdString();
        const auto* node = owner_.document_->find (nodeId_);

        if (node == nullptr || node->type != "container" || source == nodeId_)
            return false;

        if (const auto* dragged = owner_.document_->find (source))
            if (dragged->findById (nodeId_) != nullptr)
                return false;

        return true;
    }

    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details, int insertIndex) override
    {
        owner_.handleDrop (details.description.toString(), juce::String (nodeId_), insertIndex);
    }

    [[nodiscard]] const std::string& nodeId() const noexcept { return nodeId_; }

private:
    void buildChildren()
    {
        const auto* node = owner_.document_ != nullptr ? owner_.document_->find (nodeId_) : nullptr;

        if (node == nullptr)
            return;

        for (const auto& child : node->children)
            addSubItem (new NodeItem (owner_, child.id));
    }

    HierarchyPanel& owner_;
    std::string nodeId_;
};

HierarchyPanel::HierarchyPanel()
{
    setOpaque (true);
    tree_.setRootItemVisible (true);
    tree_.setDefaultOpenness (true);
    tree_.setIndentSize (14);
    tree_.setColour (juce::TreeView::backgroundColourId, juce::Colour (0xff0f141b));
    tree_.setColour (juce::TreeView::selectedItemBackgroundColourId, juce::Colour (0xff25313f));
    addAndMakeVisible (tree_);

    header_.setText ("Hierarchy", juce::dontSendNotification);
    header_.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    header_.setColour (juce::Label::textColourId, juce::Colour (0xffd9e2ec));
    addAndMakeVisible (header_);

    for (auto* button : { &groupButton_, &duplicateButton_, &deleteButton_ })
    {
        addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b222c));
        button->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd9e2ec));
    }

    groupButton_.onClick = [this] { if (onGroup != nullptr) onGroup(); };
    duplicateButton_.onClick = [this] { if (onDuplicate != nullptr) onDuplicate(); };
    deleteButton_.onClick = [this] { if (onDelete != nullptr) onDelete(); };

    updateButtonStates();
}

HierarchyPanel::~HierarchyPanel()
{
    tree_.setRootItem (nullptr);
}

void HierarchyPanel::setDocument (DesignerDocument* document)
{
    document_ = document;
    refresh();
}

void HierarchyPanel::refresh()
{
    rebuild();
    refreshSelection();
    updateButtonStates();
}

void HierarchyPanel::rebuild()
{
    tree_.setRootItem (nullptr);
    rootItem_.reset();

    if (document_ == nullptr)
        return;

    rootItem_ = std::make_unique<NodeItem> (*this, document_->root().id);
    tree_.setRootItem (rootItem_.get());
    rootItem_->setOpen (true);
}

juce::TreeViewItem* HierarchyPanel::findItem (juce::TreeViewItem& root, const std::string& id) const
{
    if (dynamic_cast<NodeItem*> (&root) != nullptr
        && static_cast<NodeItem*> (&root)->nodeId() == id)
        return &root;

    for (int i = 0; i < root.getNumSubItems(); ++i)
        if (auto* found = findItem (*root.getSubItem (i), id))
            return found;

    return nullptr;
}

void HierarchyPanel::refreshSelection()
{
    if (document_ == nullptr || tree_.getRootItem() == nullptr)
        return;

    if (auto* item = findItem (*tree_.getRootItem(), document_->selectedId()))
    {
        const juce::ScopedValueSetter<bool> guard (syncing_, true);
        item->setOpen (true);
        item->setSelected (true, true);
        tree_.scrollToKeepItemVisible (item);
    }

    updateButtonStates();
}

void HierarchyPanel::updateButtonStates()
{
    const auto hasSelection = document_ != nullptr
                           && document_->selectedId() != document_->root().id;

    groupButton_.setEnabled (hasSelection);
    duplicateButton_.setEnabled (hasSelection);
    deleteButton_.setEnabled (hasSelection);
}

bool HierarchyPanel::handleDrop (const juce::String& draggedId,
                                 const juce::String& targetId,
                                 int insertIndex)
{
    if (document_ == nullptr)
        return false;

    const auto target = targetId.toStdString();
    const auto* targetNode = document_->find (target);

    if (targetNode == nullptr || targetNode->type != "container")
        return false;

    const auto index = insertIndex < 0 ? targetNode->children.size()
                                       : static_cast<std::size_t> (insertIndex);

    if (! document_->reparentNode (draggedId.toStdString(), target, index))
        return false;

    refresh();

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    return true;
}

void HierarchyPanel::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (0xff0f141b));
    graphics.setColour (juce::Colour (0xff232a34));
    graphics.drawVerticalLine (getWidth() - 1, 0.0f, static_cast<float> (getHeight()));
}

void HierarchyPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    header_.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    auto buttons = area.removeFromBottom (26);
    const auto third = (buttons.getWidth() - 8) / 3;
    groupButton_.setBounds (buttons.removeFromLeft (third));
    buttons.removeFromLeft (4);
    duplicateButton_.setBounds (buttons.removeFromLeft (third));
    buttons.removeFromLeft (4);
    deleteButton_.setBounds (buttons);

    area.removeFromBottom (6);
    tree_.setBounds (area);
}

} // namespace mvt::ui
