#include "UIEngine/FrameSafetyGate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mvt::ui
{
namespace
{
float linearFromSrgb (std::uint8_t value) noexcept
{
    static const auto table = []
    {
        std::array<float, 256> result {};

        for (std::size_t i = 0; i < result.size(); ++i)
        {
            const auto s = static_cast<float> (i) / 255.0f;
            result[i] = s <= 0.04045f ? s / 12.92f
                                      : std::pow ((s + 0.055f) / 1.055f, 2.4f);
        }

        return result;
    }();

    return table[value];
}
} // namespace

FrameSafetyGate::FrameSafetyGate (safety::FlashLimiterConfig config) noexcept
    : limiter_ (config)
{
}

void FrameSafetyGate::setConfig (safety::FlashLimiterConfig config) noexcept
{
    limiter_.setConfig (config);
}

const safety::FlashLimiterConfig& FrameSafetyGate::config() const noexcept
{
    return limiter_.config();
}

void FrameSafetyGate::reset (safety::FrameMetrics initial)
{
    limiter_.reset (initial);
    referenceFrame_ = {};
    referenceIndex_ = -1;
    writeIndex_ = 0;
    lastReport_ = {};
}

void FrameSafetyGate::ensureBuffer (juce::Image& image, int width, int height)
{
    if (image.isValid() && image.getWidth() == width && image.getHeight() == height)
        return;

    image = juce::Image (juce::Image::ARGB,
                         width,
                         height,
                         true,
                         juce::SoftwareImageType());
}

juce::Image& FrameSafetyGate::beginFrame (int width, int height)
{
    width = std::max (1, width);
    height = std::max (1, height);

    if (referenceFrame_.isValid()
        && (referenceFrame_.getWidth() != width || referenceFrame_.getHeight() != height))
    {
        limiter_.reset();
        referenceFrame_ = {};
        referenceIndex_ = -1;
    }

    ensureBuffer (buffers_[0], width, height);
    ensureBuffer (buffers_[1], width, height);
    ensureBuffer (candidateSnapshot_, width, height);

    const auto previousIndex = referenceIndex_;
    writeIndex_ = previousIndex == 0 ? 1 : 0;

    return buffers_[writeIndex_];
}

const juce::Image& FrameSafetyGate::endFrame (double timeSeconds)
{
    auto& candidate = buffers_[writeIndex_];

    lastReport_ = {};
    lastReport_.candidate = measure (candidate);

    const auto decision = limiter_.evaluate (timeSeconds, lastReport_.candidate);
    lastReport_.decision = decision;

    if (! decision.blocked)
    {
        limiter_.commit (timeSeconds, lastReport_.candidate);
        referenceFrame_ = candidate;
        referenceIndex_ = writeIndex_;
        lastReport_.presented = lastReport_.candidate;
        return referenceFrame_;
    }

    if (! referenceFrame_.isValid())
    {
        // A transition without a reference should be impossible, but fall
        // back to the accepted frame rather than leaving a blank screen.
        limiter_.commit (timeSeconds, lastReport_.candidate);
        referenceFrame_ = candidate;
        referenceIndex_ = writeIndex_;
        lastReport_.presented = lastReport_.candidate;
        return referenceFrame_;
    }

    lastReport_.attenuated = true;

    copyInto (candidateSnapshot_, candidate);

    const auto reference = limiter_.reference();
    const auto lumaLimit = config().luminanceThreshold * config().blockedTargetFraction;
    const auto redLimit = config().redThreshold * config().blockedTargetFraction;

    const auto isSafe = [&] (const safety::FrameMetrics& metrics)
    {
        return std::abs (metrics.luminance - reference.luminance) < lumaLimit
            && std::abs (metrics.red - reference.red) < redLimit;
    };

    // Alpha 0 presents the last accepted frame exactly; alpha 1 is the
    // rejected candidate. Find the largest safe crossfade. Five iterations
    // resolve the blend to within ~3%, which is well below the 5% margin.
    float lower = 0.0f;
    float upper = 1.0f;

    for (int iteration = 0; iteration < 5; ++iteration)
    {
        const auto alpha = 0.5f * (lower + upper);
        blendInto (candidate, referenceFrame_, candidateSnapshot_, alpha);

        if (isSafe (measure (candidate)))
            lower = alpha;
        else
            upper = alpha;
    }

    blendInto (candidate, referenceFrame_, candidateSnapshot_, lower);

    lastReport_.presented = measure (candidate);
    return candidate;
}

safety::FrameMetrics FrameSafetyGate::measure (const juce::Image& frame)
{
    if (! frame.isValid() || frame.getWidth() <= 0 || frame.getHeight() <= 0)
        return {};

    juce::Image::BitmapData pixels (frame, juce::Image::BitmapData::readOnly);

    const auto stepX = std::max (1, frame.getWidth() / 128);
    const auto stepY = std::max (1, frame.getHeight() / 72);

    double luminanceSum = 0.0;
    double redSum = 0.0;
    std::size_t count = 0;

    for (int y = stepY / 2; y < frame.getHeight(); y += stepY)
    {
        for (int x = stepX / 2; x < frame.getWidth(); x += stepX)
        {
            const auto colour = pixels.getPixelColour (x, y);

            const auto red = static_cast<std::uint8_t> (colour.getRed());
            const auto green = static_cast<std::uint8_t> (colour.getGreen());
            const auto blue = static_cast<std::uint8_t> (colour.getBlue());

            luminanceSum += 0.2126 * static_cast<double> (linearFromSrgb (red))
                          + 0.7152 * static_cast<double> (linearFromSrgb (green))
                          + 0.0722 * static_cast<double> (linearFromSrgb (blue));

            // Simplified saturated-red share: how far the red channel
            // dominates both other channels, averaged over the frame.
            const auto dominance = static_cast<int> (red)
                                 - std::max (static_cast<int> (green),
                                             static_cast<int> (blue));
            redSum += static_cast<double> (std::max (0, dominance)) / 255.0;
            ++count;
        }
    }

    if (count == 0)
        return {};

    return { static_cast<float> (luminanceSum / static_cast<double> (count)),
             static_cast<float> (redSum / static_cast<double> (count)) };
}

void FrameSafetyGate::copyInto (juce::Image& destination, const juce::Image& source)
{
    juce::Graphics graphics (destination);
    graphics.drawImageAt (source, 0, 0);
}

void FrameSafetyGate::blendInto (juce::Image& destination,
                                 const juce::Image& base,
                                 const juce::Image& overlay,
                                 float alpha)
{
    juce::Graphics graphics (destination);
    graphics.drawImageAt (base, 0, 0);
    graphics.setOpacity (alpha);
    graphics.drawImageAt (overlay, 0, 0);
}
} // namespace mvt::ui
