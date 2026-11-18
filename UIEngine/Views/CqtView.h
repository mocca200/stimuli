#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <cmath>

namespace mvt::ui
{
/** One bar per constant-Q band, C2 on the left and B8 on the right.

    The bands are already geometrically spaced by semitone, so an even bar
    width is an accurate log-frequency axis. That is the point of the view: an
    FFT view has to warp a linear axis to look like pitch, and below the first
    octave the bins stop separating notes at all.
*/
class CqtView final : public VisualizerView
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

        auto header = inner.toNearestInt().removeFromTop (16);
        auto plot = inner.withTrimmedTop (16.0f);

        graphics.setFont (palette.monoFont);
        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.drawText ("CQT", header, juce::Justification::centredLeft);

        if (plot.getWidth() <= 1.0f || plot.getHeight() <= 1.0f)
            return;

        if (! snapshot.valid)
        {
            graphics.drawText ("NO SIGNAL", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        const auto bandCount = static_cast<float> (dsp::kCqtBandCount);
        const auto slot = plot.getWidth() / bandCount;
        const auto barWidth = std::max (1.0f, slot * 0.74f);

        // Octave gridlines first so the bars sit on top of them.
        const auto labelEveryOctave = slot * static_cast<float> (dsp::kCqtBinsPerOctave) >= 34.0f;
        const auto baseline = plot.getBottom();
        auto quiet = juce::Path();

        for (std::size_t band = 0; band < dsp::kCqtBandCount; ++band)
        {
            if (static_cast<int> (band) % dsp::kCqtBinsPerOctave != 0)
                continue;

            const auto x = plot.getX() + slot * static_cast<float> (band);
            quiet.startNewSubPath (x, plot.getY());
            quiet.lineTo (x, baseline);
        }

        graphics.setColour (palette.border.withAlpha (0.45f));
        graphics.strokePath (quiet, juce::PathStrokeType (1.0f));

        if (labelEveryOctave)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.55f));

            for (std::size_t band = 0; band < dsp::kCqtBandCount;
                 band += static_cast<std::size_t> (dsp::kCqtBinsPerOctave))
            {
                const auto x = plot.getX() + slot * static_cast<float> (band);
                auto cell = juce::Rectangle<float> (x, baseline - 13.0f, slot * 6.0f, 13.0f);
                graphics.drawText (juce::String ("C") + juce::String (2 + static_cast<int> (band) / 12),
                                   cell.toNearestInt(),
                                   juce::Justification::centredLeft);
            }
        }

        std::size_t loudest = 0;
        auto loudestValue = 0.0f;

        for (std::size_t band = 0; band < dsp::kCqtBandCount; ++band)
        {
            const auto value = std::clamp (snapshot.cqt[band], 0.0f, 1.0f);

            if (value > loudestValue)
            {
                loudestValue = value;
                loudest = band;
            }

            if (value <= 0.001f)
                continue;

            const auto x = plot.getX() + slot * static_cast<float> (band);
            const auto height = std::max (1.0f, std::pow (value, 1.15f) * plot.getHeight());
            const auto bar = juce::Rectangle<float> (x, baseline - height, barWidth, height);

            // Bass to treble reads as accent to warm accent, so the octave
            // position is legible without a legend.
            const auto position = static_cast<float> (band) / (bandCount - 1.0f);
            const auto base = palette.accent.interpolatedWith (palette.accentWarm, position);

            graphics.setColour (base.withAlpha (0.35f + 0.65f * value));
            graphics.fillRect (bar);
        }

        if (loudestValue > 0.05f)
        {
            graphics.setColour (palette.accentWarm.withAlpha (0.9f));
            graphics.drawText (noteName (loudest), header, juce::Justification::centredRight);
        }
    }

private:
    /** Band 0 is C2 (MIDI 36), so the band index is the semitone offset from it. */
    [[nodiscard]] static juce::String noteName (std::size_t band)
    {
        static constexpr const char* names[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        const auto semitone = static_cast<int> (band);
        const auto pitchClass = semitone % dsp::kCqtBinsPerOctave;
        const auto octave = 2 + semitone / dsp::kCqtBinsPerOctave;

        return juce::String (names[pitchClass]) + juce::String (octave);
    }
};

} // namespace mvt::ui
