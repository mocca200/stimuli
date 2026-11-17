#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace mvt::core
{
/**
 * Wait-free single-writer / single-reader triple buffer.

 * Three slots guarantee that the writer and reader never touch the same slot:
 * the writer owns one slot, the reader owns one slot, and one slot is published
 * through the shared state word.

 * The reader's result pointer remains valid until the next call to read() or
 * tryRead() on the same thread. Prefer the copying overload when a stable value
 * is needed beyond that point (our analysis snapshots are a few kilobytes, so
 * the copy is cheap and easier to reason about).
 */
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer()
        : slots_ {}
    {
        static_assert (std::atomic<std::uint32_t>::is_always_lock_free,
                       "TripleBuffer requires lock-free 32-bit atomics");
        // Writer owns 0, reader owns 2, slot 1 is in the shared position.
        shared_.store (1u, std::memory_order_relaxed);
        writeIndex_ = 0;
        readIndex_ = 2;
    }

    TripleBuffer (const TripleBuffer&) = delete;
    TripleBuffer& operator= (const TripleBuffer&) = delete;

    void write (const T& value) noexcept
    {
        slots_[writeIndex_] = value;

        const auto previous = shared_.exchange (writeIndex_ | kNewDataBit,
                                                std::memory_order_acq_rel);
        writeIndex_ = previous & kIndexMask;
    }

    [[nodiscard]] bool tryRead (T& destination) noexcept
    {
        const auto state = shared_.load (std::memory_order_acquire);

        if ((state & kNewDataBit) == 0)
            return false;

        const auto previous = shared_.exchange (readIndex_,
                                                std::memory_order_acq_rel);
        readIndex_ = previous & kIndexMask;
        destination = slots_[readIndex_];
        return true;
    }

    /** Returns a pointer valid until the next read on this thread, or nullptr. */
    [[nodiscard]] const T* read() noexcept
    {
        const auto state = shared_.load (std::memory_order_acquire);

        if ((state & kNewDataBit) == 0)
            return nullptr;

        const auto previous = shared_.exchange (readIndex_,
                                                std::memory_order_acq_rel);
        readIndex_ = previous & kIndexMask;
        return &slots_[readIndex_];
    }

    /** Discards the pending publication flag. Writer-side only. */
    void clearPending() noexcept
    {
        shared_.fetch_and (~kNewDataBit, std::memory_order_acq_rel);
    }

private:
    static constexpr std::uint32_t kIndexMask = 0x3u;
    static constexpr std::uint32_t kNewDataBit = 0x4u;

    std::array<T, 3> slots_ {};
    alignas (64) std::atomic<std::uint32_t> shared_ { 1u };
    std::uint32_t writeIndex_ = 0; // writer-only
    std::uint32_t readIndex_ = 2;  // reader-only
};

} // namespace mvt::core
