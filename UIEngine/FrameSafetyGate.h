#pragma once

#include "Engine/Safety/FlashLimiter.h"

#include <juce_graphics/juce_graphics.h>

namespace mvt::ui
{
/** Result of pushing a frame through the safety gate. */
struct FrameSafetyReport
{
    /** Metrics measured from the candidate frame before attenuation. */
    safety::FrameMetrics candidate {};

    /** Metrics measured from the frame that was actually presented. */
    safety::FrameMetrics presented {};

    /** Flash-limiter verdict for the candidate frame. */
    safety::FrameDecision decision {};

    /** True when the frame was blended toward the last accepted frame. */
    bool attenuated = false;
};

/**
 * Final framebuffer authority for photosensitivity safety.
 *
 * The UI renders each frame into a software-backed buffer supplied by
 * beginFrame(); endFrame() measures the frame, runs the flash limiter and,
 * when the flash budget would be exceeded, blends the candidate toward the
 * last accepted frame until it is below the significant-change threshold.
 *
 * The gate owns both frame buffers and alternates them so the accepted
 * reference stays intact while the next frame is rendered. All work happens
 * on the message thread; nothing here allocates after the first frame at a
 * given size.
 */
class FrameSafetyGate
{
public:
    FrameSafetyGate() = default;
    explicit FrameSafetyGate (safety::FlashLimiterConfig config) noexcept;

    void setConfig (safety::FlashLimiterConfig config) noexcept;
    [[nodiscard]] const safety::FlashLimiterConfig& config() const noexcept;

    /** Clears the sliding window and accepts initial metrics as the starting
        reference. Call this when a new stream, skin or window size begins. */
    void reset (safety::FrameMetrics initial = {});

    /** Returns the software-backed buffer to render the next frame into. */
    juce::Image& beginFrame (int width, int height);

    /** Measures and, if necessary, attenuates the frame prepared by
        beginFrame(). Returns the frame that must be presented. */
    const juce::Image& endFrame (double timeSeconds);

    [[nodiscard]] const FrameSafetyReport& lastReport() const noexcept { return lastReport_; }
    [[nodiscard]] const safety::FlashLimiter& limiter() const noexcept { return limiter_; }

    /** Measures the mean linear relative luminance and a simplified
        saturated-red share of the image. Treats the frame as one visual
        field; callers that need area weighting should crop or downscale
        before passing the image. */
    static safety::FrameMetrics measure (const juce::Image& frame);

private:
    static void ensureBuffer (juce::Image& image, int width, int height);
    static void copyInto (juce::Image& destination, const juce::Image& source);
    static void blendInto (juce::Image& destination,
                           const juce::Image& base,
                           const juce::Image& overlay,
                           float alpha);

    safety::FlashLimiter limiter_;
    juce::Image buffers_[2];
    juce::Image candidateSnapshot_;
    juce::Image referenceFrame_;
    int referenceIndex_ = -1;
    int writeIndex_ = 0;
    FrameSafetyReport lastReport_;
};
} // namespace mvt::ui
