#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace mvt::ui
{
/** Pitch-class lattice. Horizontal steps move by a perfect fifth, vertical
    steps by a major third; node brightness follows chroma energy. */
class TonnetzView final : public VisualizerView
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

        constexpr int columns = 7;
        constexpr int rows = 5;
        constexpr auto nodeCount = static_cast<std::size_t> (columns * rows);

        const auto cellWidth = inner.getWidth() / static_cast<float> (columns - 1);
        const auto cellHeight = (inner.getHeight() - 56.0f) / static_cast<float> (rows - 1);

        std::array<juce::Point<float>, nodeCount> points {};

        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const auto index = static_cast<std::size_t> (row * columns + column);
                points[index] = { inner.getX() + cellWidth * static_cast<float> (column),
                                  inner.getY() + 30.0f + cellHeight * static_cast<float> (row) };
            }
        }

        graphics.setColour (palette.border.withAlpha (0.28f));

        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const auto index = static_cast<std::size_t> (row * columns + column);

                if (column + 1 < columns)
                {
                    const auto right = static_cast<std::size_t> (row * columns + column + 1);
                    graphics.drawLine (juce::Line<float> (points[index], points[right]), 1.0f);
                }

                if (row + 1 < rows)
                {
                    const auto below = static_cast<std::size_t> ((row + 1) * columns + column);
                    graphics.drawLine (juce::Line<float> (points[index], points[below]), 1.0f);

                    if (column + 1 < columns)
                    {
                        const auto diagonal = static_cast<std::size_t> ((row + 1) * columns + column + 1);
                        graphics.drawLine (juce::Line<float> (points[index], points[diagonal]), 1.0f);
                    }
                }
            }
        }

        static constexpr std::array<const char*, 12> noteNames {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        const auto labelFont = palette.monoFont.withHeight (juce::jmin (10.0f, cellHeight * 0.26f));

        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const auto index = static_cast<std::size_t> (row * columns + column);
                const auto pitchClass = ((column * 7 + row * 4) % 12 + 12) % 12;
                const auto value = snapshot.valid
                                     ? std::clamp (snapshot.chroma[static_cast<std::size_t> (pitchClass)],
                                                   0.0f, 1.0f)
                                     : 0.0f;
                const auto radius = 3.0f + 6.5f * value;

                graphics.setColour (palette.accent.interpolatedWith (palette.accentWarm, value)
                                        .withAlpha (0.28f + 0.72f * value));
                graphics.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                                          .withCentre (points[index]));

                graphics.setColour (palette.text.withAlpha (0.35f + 0.65f * value));
                graphics.setFont (labelFont);
                graphics.drawText (noteNames[static_cast<std::size_t> (pitchClass)],
                                   juce::Rectangle<float> (points[index].x - 16.0f,
                                                           points[index].y + radius + 1.0f,
                                                           32.0f,
                                                           10.0f)
                                       .toNearestInt(),
                                   juce::Justification::centred);
            }
        }

        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.setFont (palette.monoFont);
        graphics.drawText ("TONNETZ", inner.toNearestInt().removeFromTop (18),
                           juce::Justification::centredLeft);
    }
};
} // namespace mvt::ui
