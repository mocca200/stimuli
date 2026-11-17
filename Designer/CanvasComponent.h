#pragma once

#include "Engine/DSP/AnalysisTypes.h"
#include "Engine/Layout/LayoutTypes.h"
#include "UIEngine/DemoSignal.h"
#include "UIEngine/FrameSafetyGate.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mvt::ui
{
class DesignerDocument;
class RenderEngine;
class Skin;

/** Direct-manipulation canvas: renders the production engine, selects nodes,
    moves and resizes them through the document model, and draws the editor
    chrome (selection outline, handles, guides, marquee) on top of the
    safety-gated frame. */
class CanvasComponent final : public juce::Component,
                             private juce::Timer
{
public:
    CanvasComponent();
    ~CanvasComponent() override = default;

    void setDocument (DesignerDocument* document);
    void setRenderEngine (RenderEngine* engine);
    void setSkin (Skin* skin);

    void setPaused (bool shouldPause);
    [[nodiscard]] bool isPaused() const noexcept { return paused_; }

    /** True while a drag or marquee gesture is in progress. */
    [[nodiscard]] bool isInteracting() const noexcept { return gesture_ != Gesture::none; }

    [[nodiscard]] const dsp::AnalysisSnapshot& snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] const FrameSafetyGate& safetyGate() const noexcept { return safetyGate_; }

    /** Re-reads the solved layout, for example after a resize or skin load. */
    void invalidateLayout();

    /** Design viewport for the layout. A zero size means "fill the canvas". */
    void setDesignSize (layout::Size size);
    [[nodiscard]] layout::Size designSize() const noexcept { return designSize_; }

    /** Feeds deterministic history frames into the render engine so
        history-based views (spectrogram, trails) are populated on open. */
    void prewarm (int frames);

    void groupSelection();
    void duplicateSelection();
    void deleteSelection();
    void nudgeSelection (float deltaX, float deltaY);

    std::function<void()> onSelectionChanged;
    std::function<void()> onDocumentChanged;

    void paint (juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    enum class Handle
    {
        none,
        body,
        topLeft,
        top,
        topRight,
        right,
        bottomRight,
        bottom,
        bottomLeft,
        left
    };

    enum class Gesture
    {
        none,
        drag,
        marquee
    };

    void timerCallback() override;

    [[nodiscard]] juce::Rectangle<int> designBounds() const;
    [[nodiscard]] const layout::LayoutBox* selectedBox() const;
    [[nodiscard]] const layout::LayoutBox* boxForId (std::string_view id) const;
    [[nodiscard]] static const layout::LayoutBox* parentBoxOf (const layout::LayoutBox& root,
                                                               std::string_view id);
    [[nodiscard]] Handle handleAt (juce::Point<int> position) const;
    [[nodiscard]] layout::Rect parentContentOf (const layout::LayoutBox& box) const;
    [[nodiscard]] std::vector<float> horizontalSnapTargets (const layout::LayoutBox& box,
                                                            float width) const;
    [[nodiscard]] std::vector<float> verticalSnapTargets (const layout::LayoutBox& box,
                                                          float height) const;
    [[nodiscard]] static float snapValue (float candidate, const std::vector<float>& targets);

    void beginDrag (Handle handle, juce::Point<int> position);
    void updateDrag (juce::Point<int> position);
    void finishMarquee();
    void drawOverlay (juce::Graphics& graphics);

    DesignerDocument* document_ = nullptr;
    RenderEngine* engine_ = nullptr;
    Skin* skin_ = nullptr;

    DemoSignal demoSignal_;
    dsp::AnalysisSnapshot snapshot_;
    FrameSafetyGate safetyGate_;
    bool paused_ = false;

    Gesture gesture_ = Gesture::none;
    Handle activeHandle_ = Handle::none;
    std::string dragNodeId_;
    juce::Point<int> dragStart_;
    layout::Rect startFrame_ {};
    layout::Rect dragParentContent_ {};
    juce::Rectangle<int> marquee_ {};
    layout::Size designSize_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasComponent)
};
} // namespace mvt::ui
