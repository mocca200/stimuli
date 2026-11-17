#pragma once

#include <cstddef>

namespace mvt::test
{
/** Enables counting of global allocations on all threads. Intended for narrow
    real-time-path tests; disable before asserting so diagnostics can allocate. */
void setAllocationTracking (bool enabled) noexcept;

void resetTrackedAllocations() noexcept;
[[nodiscard]] std::size_t trackedAllocationCount() noexcept;

} // namespace mvt::test
