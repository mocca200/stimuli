#include "Engine/DSP/Hpss.h"

#include <algorithm>
#include <cmath>

namespace mvt::dsp
{
Hpss::Hpss (std::size_t binCount, std::size_t kernelSize)
    : binCount_ (binCount)
    , kernelSize_ (std::max<std::size_t> (1, kernelSize))
{
    timeHistory_.assign (binCount_ * kernelSize_, 0.0f);
    timeScratch_.assign (kernelSize_, 0.0f);
    frequencyScratch_.assign (kernelSize_, 0.0f);
}

void Hpss::reset() noexcept
{
    std::fill (timeHistory_.begin(), timeHistory_.end(), 0.0f);
    frameIndex_ = 0;
}

void Hpss::process (const float* magnitudes, float* harmonic, float* percussive) noexcept
{
    if (binCount_ == 0)
        return;

    // Bootstrap the time history with the first frame. Without this, a steady
    // tone is briefly classified as fully percussive while the median window
    // fills, which looks like a flash in the HPSS view.
    if (frameIndex_ == 0)
    {
        for (std::size_t bin = 0; bin < binCount_; ++bin)
            for (std::size_t k = 0; k < kernelSize_; ++k)
                timeHistory_[bin * kernelSize_ + k] = magnitudes[bin];
    }

    const auto slot = frameIndex_ % kernelSize_;
    const auto middle = kernelSize_ / 2;

    for (std::size_t bin = 0; bin < binCount_; ++bin)
        timeHistory_[bin * kernelSize_ + slot] = magnitudes[bin];

    // Time median per bin, then frequency median per bin from the current frame.
    for (std::size_t bin = 0; bin < binCount_; ++bin)
    {
        const auto base = bin * kernelSize_;
        std::copy_n (timeHistory_.begin() + static_cast<std::ptrdiff_t> (base),
                     kernelSize_, timeScratch_.begin());

        std::nth_element (timeScratch_.begin(),
                          timeScratch_.begin() + static_cast<std::ptrdiff_t> (middle),
                          timeScratch_.end());

        const auto harmonicRaw = timeScratch_[middle];

        const auto halfKernel = kernelSize_ / 2;
        for (std::size_t offset = 0; offset < kernelSize_; ++offset)
        {
            const auto source = static_cast<std::ptrdiff_t> (bin)
                              + static_cast<std::ptrdiff_t> (offset)
                              - static_cast<std::ptrdiff_t> (halfKernel);
            const auto clamped = std::clamp<std::ptrdiff_t> (source, 0,
                                                             static_cast<std::ptrdiff_t> (binCount_ - 1));
            frequencyScratch_[offset] = magnitudes[static_cast<std::size_t> (clamped)];
        }

        std::nth_element (frequencyScratch_.begin(),
                          frequencyScratch_.begin() + static_cast<std::ptrdiff_t> (middle),
                          frequencyScratch_.end());

        const auto percussiveRaw = frequencyScratch_[middle];
        const auto harmonicPower = harmonicRaw * harmonicRaw;
        const auto percussivePower = percussiveRaw * percussiveRaw;
        const auto mask = harmonicPower / (harmonicPower + percussivePower + 1.0e-12f);

        harmonic[bin] = magnitudes[bin] * mask;
        percussive[bin] = magnitudes[bin] * (1.0f - mask);
    }

    ++frameIndex_;
}

} // namespace mvt::dsp
