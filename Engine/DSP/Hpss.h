#pragma once

#include <cstddef>
#include <vector>

namespace mvt::dsp
{
/**
 * Harmonic/percussive source separation using the classic median-filter
 * approach: harmonic content is a median over time at a fixed bin, percussive
 * content is a median over neighbouring bins in the same frame.

 * The instance owns its history and scratch buffers; process() is
 * allocation-free. Both outputs are soft-masked magnitudes suitable for
 * chroma, spectral, and transient visualisations.
 */
class Hpss
{
public:
    explicit Hpss (std::size_t binCount = 1025, std::size_t kernelSize = 17);

    Hpss (const Hpss&) = delete;
    Hpss& operator= (const Hpss&) = delete;

    [[nodiscard]] std::size_t binCount() const noexcept { return binCount_; }
    [[nodiscard]] std::size_t kernelSize() const noexcept { return kernelSize_; }

    void reset() noexcept;

    void process (const float* magnitudes, float* harmonic, float* percussive) noexcept;

private:
    std::size_t binCount_ = 0;
    std::size_t kernelSize_ = 0;
    std::size_t frameIndex_ = 0;

    std::vector<float> timeHistory_;
    std::vector<float> timeScratch_;
    std::vector<float> frequencyScratch_;
};

} // namespace mvt::dsp
