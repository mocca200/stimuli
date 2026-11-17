#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>

namespace mvt::ui
{
class DesignerDocument;

/** Tree view of the layout hierarchy with drag-and-drop reparenting. */
class HierarchyPanel final : public juce::Component
{
public:
    HierarchyPanel();
    ~HierarchyPanel() override;

    void setDocument (DesignerDocument* document);
    void refresh();
    void refreshSelection();

    std::function<void()> onSelectionChanged;
    std::function<void()> onDocumentChanged;
    std::function<void()> onGroup;
    std::function<void()> onDuplicate;
    std::function<void()> onDelete;

    /** Applies a drop reported by a tree item. Returns true when the document
        changed. */
    bool handleDrop (const juce::String& draggedId, const juce::String& targetId, int insertIndex);

    void paint (juce::Graphics& graphics) override;
    void resized() override;

private:
    class NodeItem;

    [[nodiscard]] juce::TreeViewItem* findItem (juce::TreeViewItem& root, const std::string& id) const;
    void rebuild();
    void updateButtonStates();

    DesignerDocument* document_ = nullptr;
    juce::TreeView tree_;
    std::unique_ptr<NodeItem> rootItem_;
    juce::Label header_;
    juce::TextButton groupButton_ { "Group" };
    juce::TextButton duplicateButton_ { "Duplicate" };
    juce::TextButton deleteButton_ { "Delete" };
    bool syncing_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HierarchyPanel)
};
} // namespace mvt::ui
