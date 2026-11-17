#pragma once

#include "Engine/DSP/AnalysisTypes.h"
#include "UIEngine/Palette.h"

#include <juce_graphics/juce_graphics.h>

namespace mvt::ui
{
/** Stateless paint interface, except where a view keeps bounded history.
    Views are owned by the RenderEngine and only touched on the message thread. */
class VisualizerView
{
public:
    virtual ~VisualizerView() = default;

    virtual void update (const dsp::AnalysisSnapshot& snapshot) { juce::ignoreUnused (snapshot); }

    virtual void paint (juce::Graphics& graphics,
                        juce::Rectangle<float> bounds,
                        const dsp::AnalysisSnapshot& snapshot,
                        const Palette& palette) const = 0;

    /** Views that need a running clock (particles, trails) override this. */
    virtual void tick (double deltaSeconds) { juce::ignoreUnused (deltaSeconds); }
};

} // namespace mvt::ui
