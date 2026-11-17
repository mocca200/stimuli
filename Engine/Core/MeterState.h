#pragma once

#include <atomic>
#include <cstdint>

namespace mvt::core
{
/** Atomic scalar values shared between the audio worker and the UI. */
struct MeterState
{
    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };
    std::atomic<float> rmsLeft { 0.0f };
    std::atomic<float> rmsRight { 0.0f };
    std::atomic<std::uint64_t> sampleCount { 0 };
    std::atomic<std::uint32_t> droppedFrames { 0 };

    void reset() noexcept
    {
        peakLeft.store (0.0f, std::memory_order_relaxed);
        peakRight.store (0.0f, std::memory_order_relaxed);
        rmsLeft.store (0.0f, std::memory_order_relaxed);
        rmsRight.store (0.0f, std::memory_order_relaxed);
        sampleCount.store (0, std::memory_order_relaxed);
        droppedFrames.store (0, std::memory_order_relaxed);
    }
};
} // namespace mvt::core
