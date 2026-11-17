#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mvt::ui
{
/** Chroma self-similarity over the most recent frames. The matrix compares
    every stored frame with every other frame using cosine similarity. */
class SelfSimilarityView final : public VisualizerView
{
public:
    static constexpr int kHistory = 64;

    void update (const dsp::AnalysisSnapshot& snapshot) override
    {
        if (! snapshot.valid || snapshot.sequence == lastSequence_)
            return;

        lastSequence_ = snapshot.sequence;

        auto& frame = frames_[static_cast<std::size_t> (writeIndex_)];

        float energy = 0.0f;

        for (std::size_t i = 0; i < dsp::kChromaCount; ++i)
        {
            const auto value = std::max (0.0f, snapshot.chroma[i]);
            frame[i] = value;
            energy += value * value;
        }

        const auto norm = std::sqrt (energy);

        if (norm > 1.0e-6f)
        {
            for (auto& value : frame)
                value /= norm;
        }
        else
        {
            frame.fill (0.0f);
        }

        writeIndex_ = (writeIndex_ + 1) % kHistory;

        if (count_ < kHistory)
            ++count_;

        recompute();
    }

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

        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.setFont (palette.monoFont);
        graphics.drawText ("SELF-SIMILARITY", inner.toNearestInt().removeFromTop (18),
                           juce::Justification::centredLeft);

        if (! snapshot.valid || count_ < 2)
        {
            graphics.setColour (palette.textMuted.withAlpha (0.55f));
            graphics.drawText ("COLLECTING HISTORY", inner.toNearestInt(), juce::Justification::centred);
            return;
        }

        const auto side = std::min (inner.getWidth(), inner.getHeight() - 18.0f);

        if (side <= 1.0f)
            return;

        const auto area = juce::Rectangle<float> (side, side)
                              .withCentre (inner.getCentre().translated (0.0f, 9.0f));

        if (! matrixImage_.isValid())
        {
            matrixImage_ = juce::Image (juce::Image::ARGB,
                                        kHistory,
                                        kHistory,
                                        false,
                                        juce::SoftwareImageType());
        }

        float minimum = 1.0f;
        float maximum = 0.0f;

        for (int y = 0; y < count_; ++y)
        {
            for (int x = 0; x < count_; ++x)
            {
                const auto value = similarity_[static_cast<std::size_t> (y)][static_cast<std::size_t> (x)];
                minimum = std::min (minimum, value);
                maximum = std::max (maximum, value);
            }
        }

        const auto range = maximum - minimum;

        {
            juce::Image::BitmapData pixels (matrixImage_, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < kHistory; ++y)
            {
                for (int x = 0; x < kHistory; ++x)
                {
                    if (x < count_ && y < count_)
                    {
                        const auto value = similarity_[static_cast<std::size_t> (y)][static_cast<std::size_t> (x)];
                        const auto normalized = range > 1.0e-4f
                                                  ? std::clamp ((value - minimum) / range, 0.0f, 1.0f)
                                                  : 0.5f;
                        const auto colour = palette.background.interpolatedWith (
                            palette.accent, 0.08f + 0.92f * normalized);
                        pixels.setPixelColour (x, y, colour);
                    }
                    else
                    {
                        pixels.setPixelColour (x, y, juce::Colours::transparentBlack);
                    }
                }
            }
        }

        graphics.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        graphics.drawImage (matrixImage_, area, juce::RectanglePlacement::stretchToFit);
        graphics.setColour (palette.border.withAlpha (0.5f));
        graphics.drawRect (area, 1.0f);
    }

private:
    [[nodiscard]] int historyIndex (int offset) const noexcept
    {
        const auto start = (writeIndex_ + kHistory - count_) % kHistory;
        return (start + offset) % kHistory;
    }

    void recompute() noexcept
    {
        for (int y = 0; y < count_; ++y)
        {
            const auto& a = frames_[static_cast<std::size_t> (historyIndex (y))];

            for (int x = 0; x < count_; ++x)
            {
                const auto& b = frames_[static_cast<std::size_t> (historyIndex (x))];

                float distance = 0.0f;

                for (std::size_t i = 0; i < dsp::kChromaCount; ++i)
                    distance += std::abs (a[i] - b[i]);

                // Both vectors are L2-normalised, so the L1 distance is in
                // [0, 2]: 0 for identical frames, 2 for disjoint chroma.
                similarity_[static_cast<std::size_t> (y)][static_cast<std::size_t> (x)]
                    = std::clamp (1.0f - 0.5f * distance, 0.0f, 1.0f);
            }
        }
    }

    std::array<std::array<float, dsp::kChromaCount>, static_cast<std::size_t> (kHistory)> frames_ {};
    std::array<std::array<float, static_cast<std::size_t> (kHistory)>,
               static_cast<std::size_t> (kHistory)> similarity_ {};
    int writeIndex_ = 0;
    int count_ = 0;
    std::uint64_t lastSequence_ = 0;
    mutable juce::Image matrixImage_;
};
} // namespace mvt::ui
