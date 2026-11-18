#pragma once

#include <cstdint>
#include <vector>

namespace mvt::dsp
{
/**
 * In-place iterative radix-2 FFT for power-of-two sizes.

 * All tables and working buffers are allocated by the constructor. processReal
 * performs no allocation, locking, or I/O, so it is safe to call from a worker
 * thread with a preallocated instance.
 */
class Fft
{
public:
    explicit Fft (int order = 11);

    Fft (const Fft&) = delete;
    Fft& operator= (const Fft&) = delete;

    [[nodiscard]] int order() const noexcept { return order_; }
    [[nodiscard]] int size() const noexcept { return size_; }

    /** Applies an unnormalised forward FFT to `size()` real samples and writes
        `size()` magnitudes. Only bins [0, size()/2] contain unique data. */
    void processReal (const float* input, float* magnitudeOutput) noexcept;

    /** Same transform as processReal(), but keeps the complex bins so a
        constant-Q filterbank can combine them with phase coherence. Both
        outputs receive size() entries; bins above Nyquist hold mirrored data. */
    void processRealComplex (const float* input, float* realOutput,
                             float* imaginaryOutput) noexcept;

private:
    void transform (const float* input) noexcept;

    int order_ = 11;
    int size_ = 2048;
    std::vector<float> cosTable_;
    std::vector<float> sinTable_;
    std::vector<std::uint32_t> bitReversed_;
    std::vector<float> real_;
    std::vector<float> imaginary_;
};

} // namespace mvt::dsp
