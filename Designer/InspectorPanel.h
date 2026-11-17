#pragma once

#include "Engine/DSP/AnalysisTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mvt::ui
{
class DesignerDocument;

/** Contextual property inspector: node identity, every layout style field,
    theme token overrides, and a live DSP readout. */
class InspectorPanel final : public juce::Component,
                             private juce::Timer
{
public:
    InspectorPanel();
    ~InspectorPanel() override;

    void setDocument (DesignerDocument* document);
    void setSnapshotSource (std::function<const dsp::AnalysisSnapshot&()> source);

    /** Re-reads the selected node. Safe to call when nothing is selected. */
    void refresh();

    std::function<void()> onDocumentChanged;

    void paint (juce::Graphics& graphics) override;
    void resized() override;

private:
    class Row;

    void timerCallback() override;
    void buildRows();
    void populate();
    void refreshReadout();
    void commitText (const juce::String& key);
    void applyText (const juce::String& key, const juce::String& text);
    void applyCombo (const juce::String& key, int index);
    void refreshTokens();
    void updateTokenPreview();
    void applyTokenHex();
    void notifyChanged();
    [[nodiscard]] bool isEditing() const;

    juce::TextEditor& addTextRow (const juce::String& caption, const juce::String& key);
    juce::ComboBox& addComboRow (const juce::String& caption, const juce::String& key,
                                 const juce::StringArray& items);
    void addSection (const juce::String& title);
    [[nodiscard]] juce::TextEditor* textFor (const juce::String& key) const;
    [[nodiscard]] juce::ComboBox* comboFor (const juce::String& key) const;

    DesignerDocument* document_ = nullptr;
    std::function<const dsp::AnalysisSnapshot&()> snapshotSource_;

    juce::Viewport viewport_;
    juce::Component content_;
    juce::Label header_;
    std::vector<std::unique_ptr<Row>> rows_;

    juce::Label tokenCaption_;
    juce::ComboBox tokenBox_;
    juce::TextEditor tokenHexEditor_;
    juce::TextButton clearTokenButton_ { "Clear" };
    juce::Label tokenValueLabel_;
    juce::Label readout_;

    std::string selectedToken_;
    std::uint64_t lastRevision_ = 0;
    bool building_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};
} // namespace mvt::ui
