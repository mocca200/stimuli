#include "UIEngine/RenderEngine.h"

#include "UIEngine/Skin.h"
#include "UIEngine/Views/ChromaWheelView.h"
#include "UIEngine/Views/HpssView.h"
#include "UIEngine/Views/ParticleFieldView.h"
#include "UIEngine/Views/RadialSpectrumView.h"
#include "UIEngine/Views/SelfSimilarityView.h"
#include "UIEngine/Views/SpectrogramView.h"
#include "UIEngine/Views/SpectrumView.h"
#include "UIEngine/Views/TonnetzView.h"
#include "UIEngine/Views/VectorscopeView.h"
#include "UIEngine/Views/WaveformView.h"

#include <algorithm>
#include <cstdio>

namespace mvt::ui
{
namespace
{
juce::Rectangle<float> toFloat (const layout::Rect& rect)
{
    return { rect.x, rect.y, rect.width, rect.height };
}

std::unique_ptr<VisualizerView> makeView (const std::string& type)
{
    if (type == "spectrum")     return std::make_unique<SpectrumView>();
    if (type == "spectrogram")  return std::make_unique<SpectrogramView>();
    if (type == "waveform")     return std::make_unique<WaveformView>();
    if (type == "vectorscope")  return std::make_unique<VectorscopeView>();
    if (type == "chromawheel")  return std::make_unique<ChromaWheelView>();
    if (type == "hpss")         return std::make_unique<HpssView>();
    if (type == "radialspectrum") return std::make_unique<RadialSpectrumView>();
    if (type == "tonnetz")      return std::make_unique<TonnetzView>();
    if (type == "selfsimilarity") return std::make_unique<SelfSimilarityView>();
    if (type == "particles")    return std::make_unique<ParticleFieldView>();
    return nullptr;
}
} // namespace

RenderEngine::RenderEngine() = default;
RenderEngine::~RenderEngine() = default;

const layout::LayoutBox* RenderEngine::hitTest (layout::Point point) const noexcept
{
    return layout::FlexSolver::hitTest (rootBox_, point);
}

void RenderEngine::setSkin (const Skin* skin)
{
    if (skin_ == skin)
        return;

    skin_ = skin;
    views_.clear();
    layoutValid_ = false;
}

void RenderEngine::invalidate()
{
    layoutValid_ = false;
}

void RenderEngine::tick (double deltaSeconds)
{
    for (auto& [id, view] : views_)
    {
        juce::ignoreUnused (id);
        view->tick (deltaSeconds);
    }
}

void RenderEngine::render (juce::Graphics& graphics,
                           juce::Rectangle<int> bounds,
                           const dsp::AnalysisSnapshot& snapshot,
                           float pixelScale)
{
    if (skin_ == nullptr)
    {
        graphics.fillAll (juce::Colour (0xff0b0d10));
        return;
    }

    update (bounds, snapshot, pixelScale);

    graphics.fillAll (palette_.background);
    renderBox (graphics, rootBox_, snapshot, 0);
}

void RenderEngine::update (juce::Rectangle<int> bounds,
                           const dsp::AnalysisSnapshot& snapshot,
                           float pixelScale)
{
    if (skin_ == nullptr)
        return;

    if (! layoutValid_
        || bounds != cachedBounds_
        || cachedGeneration_ != skin_->theme().generation()
        || cachedScale_ != pixelScale)
        rebuild (bounds, pixelScale);

    for (auto& [id, view] : views_)
    {
        juce::ignoreUnused (id);
        view->update (snapshot);
    }
}

void RenderEngine::rebuild (juce::Rectangle<int> bounds, float pixelScale)
{
    palette_ = makePalette (skin_->theme());

    const auto available = layout::Size { static_cast<float> (std::max (0, bounds.getWidth())),
                                          static_cast<float> (std::max (0, bounds.getHeight())) };

    rootBox_ = layout::FlexSolver::solve (skin_->layoutRoot(), available, pixelScale);
    collectViews (rootBox_);
    pruneViews();

    cachedBounds_ = bounds;
    cachedGeneration_ = skin_->theme().generation();
    cachedScale_ = pixelScale;
    layoutValid_ = true;
}

void RenderEngine::collectViews (const layout::LayoutBox& box)
{
    // Views are stateful (spectrogram history, particle positions), so a
    // re-solve for a new window size must keep the existing instances.
    if (views_.find (box.id) == views_.end())
        if (auto view = makeView (box.type))
            views_[box.id] = std::move (view);

    for (const auto& child : box.children)
        collectViews (child);
}

void RenderEngine::pruneViews()
{
    for (auto it = views_.begin(); it != views_.end();)
    {
        if (layout::FlexSolver::findById (rootBox_, it->first) == nullptr)
            it = views_.erase (it);
        else
            ++it;
    }
}

void RenderEngine::renderBox (juce::Graphics& graphics,
                              const layout::LayoutBox& box,
                              const dsp::AnalysisSnapshot& snapshot,
                              int depth)
{
    const auto frame = toFloat (box.frame);

    if (box.type == "container")
    {
        if (depth > 0 && frame.getWidth() > 0.0f && frame.getHeight() > 0.0f)
        {
            graphics.setColour (palette_.surface.withAlpha (0.55f));
            graphics.fillRoundedRectangle (frame, 12.0f);
            graphics.setColour (palette_.border.withAlpha (0.55f));
            graphics.drawRoundedRectangle (frame.reduced (0.5f), 12.0f, 1.0f);
        }
    }
    else if (box.type == "label")
    {
        const auto font = box.id == "title" ? palette_.titleFont : palette_.bodyFont;
        graphics.setFont (font);
        graphics.setColour (box.id == "title" ? palette_.text : palette_.textMuted);
        graphics.drawText (box.text.empty() ? box.id : box.text,
                           frame.toNearestInt().reduced (4),
                           juce::Justification::centredLeft, false);
    }
    else if (const auto it = views_.find (box.id); it != views_.end())
    {
        it->second->paint (graphics, frame, snapshot, palette_);
    }
    else if (frame.getWidth() > 0.0f && frame.getHeight() > 0.0f)
    {
        graphics.setColour (palette_.surfaceRaised.withAlpha (0.6f));
        graphics.fillRoundedRectangle (frame, 10.0f);
        graphics.setColour (palette_.textMuted);
        graphics.setFont (palette_.monoFont);
        graphics.drawText (juce::String (box.type) + " (not mounted)",
                           frame.toNearestInt(), juce::Justification::centred);
    }

    for (const auto& child : box.children)
        renderBox (graphics, child, snapshot, depth + 1);
}

} // namespace mvt::ui
