#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace mvt::safety
{
/**
 * Per-frame luminance description. Luminance is the mean linear relative
 * luminance of the frame in [0, 1]; red is a simplified saturated-red measure
 * used for WCAG 2.3.1 red-flash checks.
 */
struct FrameMetrics
{
    float luminance = 0.0f;
    float red = 0.0f;
};

struct FlashLimiterConfig
{
    /** A significant change is 10% of maximum relative luminance (WCAG 2.3.1). */
    float luminanceThreshold = 0.10f;

    /** Simplified saturated-red change threshold. */
    float redThreshold = 0.10f;

    /** Six opposing transitions per second == three flashes per second. */
    int maxTransitionsPerSecond = 6;

    /** Sliding window for the flash count. */
    double windowSeconds = 1.0;

    /** When a transition is blocked, target this fraction of the threshold. */
    float blockedTargetFraction = 0.95f;
};

struct FrameDecision
{
    /** The candidate differs from the reference by at least one threshold. */
    bool transition = false;

    /** The transition may be shown in full. */
    bool allowed = true;

    /** The transition exceeds the flash budget and must be attenuated. */
    bool blocked = false;

    /** +1 for a brighter/redder candidate, -1 for darker/less red, 0 for none. */
    int direction = 0;

    /** Number of opposing transitions already counted in the sliding window. */
    int transitionsInWindow = 0;

    /** Safe target when blocked; just inside the threshold on the candidate side. */
    FrameMetrics target;
};

/**
 * Frequency-domain flash limiter. It counts opposing luminance/red transitions
 * in a sliding one-second window and blocks any transition that would exceed
 * three flashes per second. The caller is responsible for attenuating blocked
 * frames; commit() records the accepted output.
 *
 * This class performs no allocation and is safe to run on the message thread.
 */
class FlashLimiter
{
public:
    static constexpr int kHistoryCapacity = 32;

    FlashLimiter() noexcept = default;
    explicit FlashLimiter (FlashLimiterConfig config) noexcept : config_ (config) {}

    void setConfig (FlashLimiterConfig config) noexcept { config_ = config; }
    [[nodiscard]] const FlashLimiterConfig& config() const noexcept { return config_; }

    void reset (FrameMetrics initial = {}) noexcept
    {
        reference_ = initial;
        lastDirection_ = 0;
        count_ = 0;
        head_ = 0;
    }

    [[nodiscard]] FrameMetrics reference() const noexcept { return reference_; }
    [[nodiscard]] int transitionsInWindow (double now) const noexcept
    {
        int total = 0;

        for (int i = 0; i < count_; ++i)
        {
            const auto index = (head_ + kHistoryCapacity - 1 - i) % kHistoryCapacity;
            const auto stamp = timestamps_[static_cast<std::size_t> (index)];

            if (now - stamp < config_.windowSeconds)
                ++total;
        }

        return total;
    }

    [[nodiscard]] FrameDecision evaluate (double now, FrameMetrics candidate) const noexcept
    {
        FrameDecision decision;

        const auto lumaDelta = candidate.luminance - reference_.luminance;
        const auto redDelta = candidate.red - reference_.red;
        const auto lumaTransition = std::abs (lumaDelta) >= config_.luminanceThreshold;
        const auto redTransition = std::abs (redDelta) >= config_.redThreshold;

        if (! lumaTransition && ! redTransition)
            return decision;

        decision.transition = true;

        const auto lumaScore = lumaTransition ? lumaDelta / config_.luminanceThreshold : 0.0f;
        const auto redScore = redTransition ? redDelta / config_.redThreshold : 0.0f;
        const auto score = std::abs (lumaScore) >= std::abs (redScore) ? lumaScore : redScore;

        decision.direction = score >= 0.0f ? 1 : -1;
        decision.transitionsInWindow = transitionsInWindow (now);

        const auto sameDirection = lastDirection_ == decision.direction;
        decision.allowed = sameDirection
                        || decision.transitionsInWindow < config_.maxTransitionsPerSecond;
        decision.blocked = ! decision.allowed;

        if (decision.blocked)
        {
            const auto sign = static_cast<float> (decision.direction);

            decision.target.luminance = reference_.luminance
                                      + sign * config_.luminanceThreshold
                                      * config_.blockedTargetFraction;
            decision.target.red = reference_.red
                                + sign * config_.redThreshold
                                * config_.blockedTargetFraction;
        }

        return decision;
    }

    /**
     * Records the frame that was actually shown. Pass adjusted=true when the
     * frame was attenuated toward the reference; the reference then stays put,
     * so repeated blocked frames cannot creep toward the unsafe target.
     */
    void commit (double now, FrameMetrics output, bool adjusted = false) noexcept
    {
        if (adjusted)
            return;

        const auto lumaDelta = output.luminance - reference_.luminance;
        const auto redDelta = output.red - reference_.red;
        const auto lumaTransition = std::abs (lumaDelta) >= config_.luminanceThreshold;
        const auto redTransition = std::abs (redDelta) >= config_.redThreshold;

        if (lumaTransition || redTransition)
        {
            const auto lumaScore = lumaTransition ? lumaDelta / config_.luminanceThreshold : 0.0f;
            const auto redScore = redTransition ? redDelta / config_.redThreshold : 0.0f;
            const auto score = std::abs (lumaScore) >= std::abs (redScore) ? lumaScore : redScore;
            const auto direction = score >= 0.0f ? 1 : -1;

            if (lastDirection_ != 0 && direction != lastDirection_)
                pushTransition (now);

            lastDirection_ = direction;
        }

        reference_ = output;
    }

private:
    void pushTransition (double now) noexcept
    {
        timestamps_[static_cast<std::size_t> (head_)] = now;
        head_ = (head_ + 1) % kHistoryCapacity;

        if (count_ < kHistoryCapacity)
            ++count_;
    }

    FlashLimiterConfig config_ {};
    FrameMetrics reference_ {};
    int lastDirection_ = 0;
    std::array<double, kHistoryCapacity> timestamps_ {};
    int head_ = 0;
    int count_ = 0;
};
} // namespace mvt::safety
