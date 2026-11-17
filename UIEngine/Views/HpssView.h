#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>

namespace mvt::ui
{
/** Harmonic energy above the centre line, percussive energy below. */
class HpssView final : public VisualizerView
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
        const auto halfHeight = inner.getHeight() * 0.46f;
        const auto count = static_cast<float> (dsp::kSpectrumBandCount);

        graphics.setColour (palette.border.withAlpha (0.55f));
        graphics.drawHorizontalLine (juce::roundToInt (centreY), inner.getX(), inner.getRight());

        if (! snapshot.valid)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.65f));
            graphics.setFont (palette.monoFont);
            graphics.drawText ("NO SIGNAL", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        juce::Path harmonic;
        juce::Path percussive;

        for (std::size_t i = 0; i < dsp::kSpectrumBandCount; ++i)
        {
            const auto x = inner.getX() + inner.getWidth() * static_cast<float> (i) / (count - 1.0f);
            const auto h = std::pow (snapshot.harmonicSpectrum[i], 1.35f) * halfHeight;
            const auto p = std::pow (snapshot.percussiveSpectrum[i], 1.35f) * halfHeight;

            if (i == 0)
            {
                harmonic.startNewSubPath (x, centreY - h);
                percussive.startNewSubPath (x, centreY + p);
            }
            else
            {
                harmonic.lineTo (x, centreY - h);
                percussive.lineTo (x, centreY + p);
            }
        }

        auto harmonicFill = harmonic;
        harmonicFill.lineTo (inner.getRight(), centreY);
        harmonicFill.lineTo (inner.getX(), centreY);
        harmonicFill.closeSubPath();

        auto percussiveFill = percussive;
        percussiveFill.lineTo (inner.getRight(), centreY);
        percussiveFill.lineTo (inner.getX(), centreY);
        percussiveFill.closeSubPath();

        graphics.setColour (palette.harmonic.withAlpha (0.35f));
        graphics.fillPath (harmonicFill);
        graphics.setColour (palette.harmonic);
        graphics.strokePath (harmonic, juce::PathStrokeType (1.3f));

        graphics.setColour (palette.percussive.withAlpha (0.35f));
        graphics.fillPath (percussiveFill);
        graphics.setColour (palette.percussive);
        graphics.strokePath (percussive, juce::PathStrokeType (1.3f));

        graphics.setFont (palette.monoFont);
        graphics.setColour (palette.harmonic.withAlpha (0.85f));
        graphics.drawText ("H", inner.toNearestInt().removeFromTop (14),
                           juce::Justification::topLeft);
        graphics.setColour (palette.percussive.withAlpha (0.85f));
        graphics.drawText ("P", inner.toNearestInt().removeFromBottom (14),
                           juce::Justification::bottomLeft);
    }
};

} // namespace mvt::ui
