#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <vector>

namespace mvt::core
{
/**
 * Single-producer / single-consumer lock-free ring buffer.

 * Capacity is rounded up to a power of two. The producer owns writeIndex(),
 * the consumer owns readIndex(); no method may be called concurrently by more
 * than one producer or one consumer. All operations are wait-free.
 *
 * T must be trivially copyable so that block transfers are plain memcpy.
 */
template <typename T>
class SpscRingBuffer
{
    static_assert (std::is_trivially_copyable_v<T>,
                   "SpscRingBuffer requires trivially copyable element types");
    static_assert (std::atomic<std::size_t>::is_always_lock_free,
                   "SpscRingBuffer requires lock-free size_t atomics");

public:
    explicit SpscRingBuffer (std::size_t requestedCapacity)
    {
        auto capacity = std::size_t { 2 };
        while (capacity < requestedCapacity)
            capacity <<= 1;

        buffer_.resize (capacity);
        capacity_ = capacity;
        mask_ = capacity - 1;
    }

    SpscRingBuffer (const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator= (const SpscRingBuffer&) = delete;

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t availableToRead() const noexcept
    {
        const auto w = write_.load (std::memory_order_acquire);
        const auto r = read_.load (std::memory_order_acquire);
        return w - r;
    }

    [[nodiscard]] std::size_t availableToWrite() const noexcept
    {
        return capacity_ - availableToRead();
    }

    [[nodiscard]] bool empty() const noexcept { return availableToRead() == 0; }
    [[nodiscard]] bool full() const noexcept { return availableToRead() == capacity_; }

    bool push (const T& value) noexcept
    {
        const auto w = write_.load (std::memory_order_relaxed);
        const auto r = read_.load (std::memory_order_acquire);

        if (w - r >= capacity_)
            return false;

        buffer_[w & mask_] = value;
        write_.store (w + 1, std::memory_order_release);
        return true;
    }

    bool pop (T& destination) noexcept
    {
        const auto r = read_.load (std::memory_order_relaxed);
        const auto w = write_.load (std::memory_order_acquire);

        if (r == w)
            return false;

        destination = buffer_[r & mask_];
        read_.store (r + 1, std::memory_order_release);
        return true;
    }

    /** Pushes as many elements as fit. Returns the number written. */
    std::size_t pushBlock (const T* source, std::size_t count) noexcept
    {
        const auto w = write_.load (std::memory_order_relaxed);
        const auto r = read_.load (std::memory_order_acquire);
        const auto free = capacity_ - (w - r);
        const auto n = std::min (count, free);

        if (n == 0)
            return 0;

        const auto index = w & mask_;
        const auto first = std::min (n, capacity_ - index);

        std::memcpy (buffer_.data() + index, source, first * sizeof (T));

        if (n > first)
            std::memcpy (buffer_.data(), source + first, (n - first) * sizeof (T));

        write_.store (w + n, std::memory_order_release);
        return n;
    }

    /** Pops as many elements as are available. Returns the number read. */
    std::size_t popBlock (T* destination, std::size_t count) noexcept
    {
        const auto r = read_.load (std::memory_order_relaxed);
        const auto w = write_.load (std::memory_order_acquire);
        const auto n = std::min (count, w - r);

        if (n == 0)
            return 0;

        const auto index = r & mask_;
        const auto first = std::min (n, capacity_ - index);

        std::memcpy (destination, buffer_.data() + index, first * sizeof (T));

        if (n > first)
            std::memcpy (destination + first, buffer_.data(), (n - first) * sizeof (T));

        read_.store (r + n, std::memory_order_release);
        return n;
    }

    /** Only safe when neither side is active. */
    void reset() noexcept
    {
        read_.store (0, std::memory_order_relaxed);
        write_.store (0, std::memory_order_relaxed);
    }

private:
    std::vector<T> buffer_;
    std::size_t capacity_ = 0;
    std::size_t mask_ = 0;

    // Keep the two indices on separate cache lines.
    alignas (64) std::atomic<std::size_t> write_ { 0 };
    alignas (64) std::atomic<std::size_t> read_ { 0 };
};

} // namespace mvt::core
