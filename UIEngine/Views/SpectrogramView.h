#pragma once

#include "UIEngine/VisualizerView.h"

#include <cstdint>
#include <cstring>

namespace mvt::ui
{
/**
 * Scrolling spectrogram. The view owns a small RGB image and shifts it one
 * column per published analysis frame, so paint cost is a single image blit.
 *
 * The history image is deliberately software-backed: JUCE's native (Direct2D)
 * image type may defer or redirect pixel writes, and the spectrogram needs
 * deterministic read-after-write behaviour every frame. The image is small
 * enough that a texture upload during painting is negligible.
 */
class SpectrogramView final : public VisualizerView
{
public:
    static constexpr int kHistoryColumns = 180;

    void update (const dsp::AnalysisSnapshot& snapshot) override
    {
        if (! snapshot.valid || snapshot.sequence == lastSequence)
            return;

        lastSequence = snapshot.sequence;

        constexpr auto rows = static_cast<int> (dsp::kSpectrumBandCount);

        if (! image.isValid())
            image = juce::Image (juce::Image::RGB,
                                 kHistoryColumns,
                                 rows,
                                 true,
                                 juce::SoftwareImageType());

        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);

        const auto rowBytes = static_cast<std::size_t> (pixels.pixelStride)
                            * static_cast<std::size_t> (kHistoryColumns - 1);

        for (int row = 0; row < rows; ++row)
            std::memmove (pixels.getPixelPointer (0, row),
                          pixels.getPixelPointer (1, row),
                          rowBytes);

        for (std::size_t band = 0; band < dsp::kSpectrumBandCount; ++band)
        {
            const auto row = static_cast<int> (dsp::kSpectrumBandCount - 1 - band);
            pixels.setPixelColour (kHistoryColumns - 1, row, colourFor (snapshot.spectrum[band]));
        }
    }

    void paint (juce::Graphics& graphics,
                juce::Rectangle<float> bounds,
                const dsp::AnalysisSnapshot& snapshot,
                const Palette& palette) const override
    {
        juce::ignoreUnused (snapshot);

        graphics.setColour (palette.surfaceRaised.withAlpha (0.92f));
        graphics.fillRoundedRectangle (bounds, 10.0f);

        if (image.isValid())
        {
            graphics.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            graphics.drawImage (image, bounds.reduced (1.0f));
        }
        else
        {
            graphics.setColour (palette.textMuted.withAlpha (0.65f));
            graphics.setFont (palette.monoFont);
            graphics.drawText ("WAITING FOR AUDIO", bounds.toNearestInt(),
                               juce::Justification::centred);
        }

        graphics.setColour (palette.border.withAlpha (0.7f));
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    }

private:
    static juce::Colour colourFor (float value)
    {
        const auto clamped = juce::jlimit (0.0f, 1.0f, value);
        const auto hue = 0.58f - clamped * 0.30f;
        return juce::Colour::fromHSV (hue, 0.85f, 0.12f + 0.88f * clamped, 1.0f);
    }

    juce::Image image;
    std::uint64_t lastSequence = 0;
};

} // namespace mvt::ui
