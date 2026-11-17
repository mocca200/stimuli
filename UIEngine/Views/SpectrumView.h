#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <cmath>

namespace mvt::ui
{
class SpectrumView final : public VisualizerView
{
public:
    void paint (juce::Graphics& graphics,
                juce::Rectangle<float> bounds,
                const dsp::AnalysisSnapshot& snapshot,
                const Palette& palette) const override
    {
        drawFrame (graphics, bounds, palette);

        const auto inner = bounds.reduced (10.0f, 8.0f);

        if (inner.getWidth() <= 1.0f || inner.getHeight() <= 1.0f)
            return;

        graphics.setColour (palette.border.withAlpha (0.45f));
        for (int line = 1; line < 4; ++line)
        {
            const auto y = inner.getY() + inner.getHeight() * static_cast<float> (line) / 4.0f;
            graphics.drawHorizontalLine (juce::roundToInt (y), inner.getX(), inner.getRight());
        }

        if (! snapshot.valid)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.65f));
            graphics.setFont (palette.monoFont);
            graphics.drawText ("NO SIGNAL", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        juce::Path line;
        const auto count = static_cast<float> (dsp::kSpectrumBandCount);

        for (std::size_t i = 0; i < dsp::kSpectrumBandCount; ++i)
        {
            const auto x = inner.getX() + inner.getWidth() * static_cast<float> (i) / (count - 1.0f);
            const auto value = std::pow (snapshot.spectrum[i], 1.35f);
            const auto y = inner.getBottom() - value * inner.getHeight();

            if (i == 0)
                line.startNewSubPath (x, y);
            else
                line.lineTo (x, y);
        }

        auto filled = line;
        filled.lineTo (inner.getBottomRight());
        filled.lineTo (inner.getBottomLeft());
        filled.closeSubPath();

        graphics.setGradientFill (juce::ColourGradient (palette.accent.withAlpha (0.42f),
                                                        inner.getCentreX(), inner.getY(),
                                                        palette.accent.withAlpha (0.02f),
                                                        inner.getCentreX(), inner.getBottom(),
                                                        false));
        graphics.fillPath (filled);

        graphics.setColour (palette.accent);
        graphics.strokePath (line, juce::PathStrokeType (1.6f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
    }

private:
    static void drawFrame (juce::Graphics& graphics, juce::Rectangle<float> bounds, const Palette& palette)
    {
        graphics.setColour (palette.surfaceRaised.withAlpha (0.9f));
        graphics.fillRoundedRectangle (bounds, 10.0f);
        graphics.setColour (palette.border.withAlpha (0.7f));
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    }
};

} // namespace mvt::ui
