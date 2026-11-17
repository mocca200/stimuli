#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <cmath>

namespace mvt::ui
{
class ChromaWheelView final : public VisualizerView
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

        const auto radius = std::min (inner.getWidth(), inner.getHeight()) * 0.46f;
        const auto centre = inner.getCentre();

        constexpr auto twoPi = juce::MathConstants<float>::twoPi;
        const auto slot = twoPi / static_cast<float> (dsp::kChromaCount);
        constexpr auto gap = 0.025f;

        for (std::size_t i = 0; i < dsp::kChromaCount; ++i)
        {
            const auto start = -juce::MathConstants<float>::halfPi
                             + static_cast<float> (i) * slot + gap;
            const auto end = -juce::MathConstants<float>::halfPi
                           + static_cast<float> (i + 1) * slot - gap;
            const auto value = snapshot.valid
                                 ? std::clamp (snapshot.chroma[i], 0.0f, 1.0f)
                                 : 0.0f;

            const auto base = palette.accent.interpolatedWith (
                palette.harmonic, static_cast<float> (i) / 11.0f);

            juce::Path background;
            background.addPieSegment (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                                          .withCentre (centre),
                                      start, end, 0.62f);
            graphics.setColour (base.withAlpha (0.18f));
            graphics.fillPath (background);

            const auto innerRadius = radius * 0.66f;
            const auto outer = radius * (0.66f + 0.34f * value);
            const auto midAngle = (start + end) * 0.5f;
            const juce::Point<float> from (centre.x + std::cos (midAngle) * innerRadius,
                                           centre.y + std::sin (midAngle) * innerRadius);
            const juce::Point<float> to (centre.x + std::cos (midAngle) * outer,
                                         centre.y + std::sin (midAngle) * outer);

            graphics.setColour (base.withAlpha (0.4f + 0.6f * value));
            graphics.drawLine (juce::Line<float> (from, to), 7.0f);
            graphics.setColour (base.withAlpha (0.9f));
            graphics.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (to));
        }

        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.setFont (palette.monoFont);
        graphics.drawText ("CHROMA", inner.toNearestInt().removeFromTop (18),
                           juce::Justification::centredLeft);
    }
};

} // namespace mvt::ui
