#include "AllocationProbe.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
std::atomic<bool> tracking { false };
std::atomic<std::size_t> allocationCount { 0 };
} // namespace

void* operator new (std::size_t size)
{
    if (tracking.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);

    if (auto* memory = std::malloc (size))
        return memory;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* memory) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

namespace mvt::test
{
void setAllocationTracking (bool enabled) noexcept
{
    tracking.store (enabled, std::memory_order_relaxed);
}

void resetTrackedAllocations() noexcept
{
    allocationCount.store (0, std::memory_order_relaxed);
}

std::size_t trackedAllocationCount() noexcept
{
    return allocationCount.load (std::memory_order_relaxed);
}
} // namespace mvt::test
