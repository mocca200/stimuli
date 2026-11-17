#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <cmath>

namespace mvt::ui
{
/** Spectrum wrapped around a circle: low frequencies at the top, high
    frequencies completing the ring clockwise. */
class RadialSpectrumView final : public VisualizerView
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

        const auto centre = inner.getCentre();
        const auto maxRadius = std::min (inner.getWidth(), inner.getHeight()) * 0.46f;

        graphics.setColour (palette.border.withAlpha (0.32f));
        for (int ring = 1; ring <= 3; ++ring)
        {
            const auto radius = maxRadius * static_cast<float> (ring) / 3.0f;
            graphics.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                                      .withCentre (centre),
                                  1.0f);
        }

        constexpr auto wedgeCount = std::size_t { 64 };
        constexpr auto bandStep = dsp::kSpectrumBandCount / wedgeCount;
        constexpr auto twoPi = juce::MathConstants<float>::twoPi;
        const auto step = twoPi / static_cast<float> (wedgeCount);
        const auto innerRadius = maxRadius * 0.30f;

        for (std::size_t i = 0; i < wedgeCount; ++i)
        {
            const auto band = std::min (dsp::kSpectrumBandCount - 1, i * bandStep);
            const auto value = snapshot.valid
                                 ? std::clamp (snapshot.spectrum[band], 0.0f, 1.0f)
                                 : 0.0f;
            const auto boosted = std::pow (value, 1.3f);
            const auto outerRadius = innerRadius
                                   + (0.20f + 0.80f * boosted) * (maxRadius - innerRadius);
            const auto start = -juce::MathConstants<float>::halfPi
                             + static_cast<float> (i) * step + step * 0.10f;
            const auto end = -juce::MathConstants<float>::halfPi
                           + static_cast<float> (i + 1) * step - step * 0.10f;

            juce::Path wedge;
            wedge.addPieSegment (juce::Rectangle<float> (outerRadius * 2.0f, outerRadius * 2.0f)
                                     .withCentre (centre),
                                 start,
                                 end,
                                 outerRadius > 0.0f ? innerRadius / outerRadius : 0.0f);

            const auto colour = palette.accent.interpolatedWith (
                palette.harmonic, static_cast<float> (i % 12) / 11.0f);
            graphics.setColour (colour.withAlpha (0.12f + 0.78f * boosted));
            graphics.fillPath (wedge);
        }

        graphics.setColour (palette.surface.withAlpha (0.92f));
        graphics.fillEllipse (juce::Rectangle<float> (innerRadius * 1.5f, innerRadius * 1.5f)
                                  .withCentre (centre));

        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.setFont (palette.monoFont);
        graphics.drawText ("RADIAL", inner.toNearestInt().removeFromTop (18),
                           juce::Justification::centredLeft);
    }
};
} // namespace mvt::ui
