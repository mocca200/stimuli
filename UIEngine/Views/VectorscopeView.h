#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>

namespace mvt::ui
{
class VectorscopeView final : public VisualizerView
{
public:
    void paint (juce::Graphics& graphics,
                juce::Rectangle<float> bounds,
                const dsp::AnalysisSnapshot& snapshot,
                const Palette& palette) const override
    {
        graphics.setColour (palette.surfaceRaised.withAlpha (0.9f));
        graphics.fillRoundedRectangle (bounds, 10.0f);
        graphics.setColour (palette.border.withAlpha (0.7f));
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        const auto inner = bounds.reduced (12.0f, 10.0f);

        if (inner.getWidth() <= 1.0f || inner.getHeight() <= 1.0f)
            return;

        const auto radius = std::min (inner.getWidth(), inner.getHeight()) * 0.47f;
        const auto centre = inner.getCentre();

        graphics.setColour (palette.border.withAlpha (0.45f));
        graphics.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                              1.0f);
        graphics.drawEllipse (juce::Rectangle<float> (radius * 1.0f, radius * 1.0f).withCentre (centre),
                              1.0f);
        graphics.drawHorizontalLine (juce::roundToInt (centre.y),
                                     centre.x - radius, centre.x + radius);
        graphics.drawVerticalLine (juce::roundToInt (centre.x),
                                   centre.y - radius, centre.y + radius);

        if (! snapshot.valid)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.65f));
            graphics.setFont (palette.monoFont);
            graphics.drawText ("NO SIGNAL", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        juce::Path trace;

        for (std::size_t i = 0; i < dsp::kScopePointCount; ++i)
        {
            const auto x = centre.x + std::clamp (snapshot.scope[i * 2], -1.0f, 1.0f) * radius;
            const auto y = centre.y - std::clamp (snapshot.scope[i * 2 + 1], -1.0f, 1.0f) * radius;

            if (i == 0)
                trace.startNewSubPath (x, y);
            else
                trace.lineTo (x, y);
        }

        graphics.setColour (palette.harmonic.withAlpha (0.22f));
        graphics.strokePath (trace, juce::PathStrokeType (4.0f));
        graphics.setColour (palette.accent);
        graphics.strokePath (trace, juce::PathStrokeType (1.2f,
                                                          juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
    }
};

} // namespace mvt::ui
