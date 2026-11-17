#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>

namespace mvt::ui
{
class WaveformView final : public VisualizerView
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

        const auto inner = bounds.reduced (10.0f, 8.0f);

        if (inner.getWidth() <= 1.0f || inner.getHeight() <= 1.0f)
            return;

        const auto centreY = inner.getCentreY();

        graphics.setColour (palette.border.withAlpha (0.55f));
        graphics.drawHorizontalLine (juce::roundToInt (centreY), inner.getX(), inner.getRight());

        if (! snapshot.valid)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.65f));
            graphics.setFont (palette.monoFont);
            graphics.drawText ("NO SIGNAL", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        juce::Path upper;
        const auto count = static_cast<float> (dsp::kWaveformPointCount);
        const auto amplitude = inner.getHeight() * 0.46f;

        for (std::size_t i = 0; i < dsp::kWaveformPointCount; ++i)
        {
            const auto x = inner.getX() + inner.getWidth() * static_cast<float> (i) / (count - 1.0f);
            const auto y = centreY - std::clamp (snapshot.waveform[i], -1.0f, 1.0f) * amplitude;

            if (i == 0)
                upper.startNewSubPath (x, y);
            else
                upper.lineTo (x, y);
        }

        auto filled = upper;
        for (std::size_t i = dsp::kWaveformPointCount; i-- > 0;)
        {
            const auto x = inner.getX() + inner.getWidth() * static_cast<float> (i) / (count - 1.0f);
            const auto sample = std::clamp (snapshot.waveform[i], -1.0f, 1.0f);
            filled.lineTo (x, centreY + sample * amplitude);
        }
        filled.closeSubPath();

        graphics.setGradientFill (juce::ColourGradient (palette.accentWarm.withAlpha (0.30f),
                                                        inner.getCentreX(), inner.getY(),
                                                        palette.accent.withAlpha (0.12f),
                                                        inner.getCentreX(), inner.getBottom(),
                                                        false));
        graphics.fillPath (filled);

        graphics.setColour (palette.accentWarm);
        graphics.strokePath (upper, juce::PathStrokeType (1.4f,
                                                          juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
    }
};

} // namespace mvt::ui
