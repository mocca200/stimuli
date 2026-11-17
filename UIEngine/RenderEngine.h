#pragma once

#include "Engine/DSP/AnalysisTypes.h"
#include "Engine/Layout/FlexSolver.h"
#include "UIEngine/Palette.h"
#include "UIEngine/VisualizerView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace mvt::ui
{
class Skin;

/** Solves a skin layout and paints it with the current analysis snapshot.
    Owned by the editor/designer and used only from the message thread. */
class RenderEngine
{
public:
    RenderEngine();
    ~RenderEngine();

    RenderEngine (const RenderEngine&) = delete;
    RenderEngine& operator= (const RenderEngine&) = delete;

    void setSkin (const Skin* skin);
    void invalidate();

    /** Advances time-based views; call from the editor timer. */
    void tick (double deltaSeconds);

    void render (juce::Graphics& graphics,
                 juce::Rectangle<int> bounds,
                 const dsp::AnalysisSnapshot& snapshot,
                 float pixelScale = 0.0f);

    /** Feeds a snapshot into the view state without painting. Useful for
        warming up history-based views such as the spectrogram before the
        first frame, for example in headless preview and export. */
    void update (juce::Rectangle<int> bounds,
                 const dsp::AnalysisSnapshot& snapshot,
                 float pixelScale = 0.0f);

    [[nodiscard]] const Palette& palette() const noexcept { return palette_; }

    /** The most recently solved layout tree, in component-local coordinates.
        Valid after render()/update(); used by the designer for hit testing. */
    [[nodiscard]] const layout::LayoutBox& solvedRoot() const noexcept { return rootBox_; }

    [[nodiscard]] const layout::LayoutBox* hitTest (layout::Point point) const noexcept;

private:
    void rebuild (juce::Rectangle<int> bounds, float pixelScale);
    void renderBox (juce::Graphics& graphics,
                    const layout::LayoutBox& box,
                    const dsp::AnalysisSnapshot& snapshot,
                    int depth);
    void collectViews (const layout::LayoutBox& box);
    void pruneViews();

    const Skin* skin_ = nullptr;
    Palette palette_;
    layout::LayoutBox rootBox_;
    juce::Rectangle<int> cachedBounds_;
    std::uint64_t cachedGeneration_ = 0;
    float cachedScale_ = -1.0f;
    bool layoutValid_ = false;

    std::unordered_map<std::string, std::unique_ptr<VisualizerView>> views_;
};
} // namespace mvt::ui
