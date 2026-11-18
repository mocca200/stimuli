#include "Engine/DSP/Cqt.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mvt::dsp
{
namespace
{
constexpr int kMinBandBin = 1;
constexpr int kCqtBandCountInt = static_cast<int> (kCqtBandCount);

/** Fractional semitone position of a frequency above the lowest band. */
double semitonesAboveLowest (double frequency, double lowest)
{
    return 12.0 * std::log2 (frequency / lowest);
}
} // namespace

Cqt::Cqt (int fftSize, double sampleRate, float minFrequency)
    : fftSize_ (std::max (16, fftSize))
    , sampleRate_ (sampleRate > 0.0 ? sampleRate : 48000.0)
    , minFrequency_ (minFrequency > 0.0f ? minFrequency : kCqtMinFrequencyHz)
{
    const auto binHz = sampleRate_ / static_cast<double> (fftSize_);
    const auto maxBin = fftSize_ / 2; // inclusive Nyquist bin
    const auto lowest = static_cast<double> (minFrequency_);

    bands_.assign (static_cast<std::size_t> (kCqtBandCountInt), {});
    weights_.clear();
    weights_.reserve (static_cast<std::size_t> (maxBin));

    // Pass 1: give every FFT bin to the band whose centre is nearest, tapered so
    // the weights sum to one per band. Bins further than half a semitone from
    // the band grid (below the lowest band or above the highest) are dropped.
    constexpr auto twoPi = std::numbers::pi_v<double> * 2.0;

    for (auto bin = kMinBandBin; bin <= maxBin; ++bin)
    {
        const auto frequency = static_cast<double> (bin) * binHz;
        const auto position = semitonesAboveLowest (frequency, lowest);

        if (position < -0.5 || position > static_cast<double> (kCqtBandCountInt) - 0.5)
            continue;

        const auto centre = std::clamp (static_cast<int> (std::lround (position)),
                                        0, kCqtBandCountInt - 1);
        const auto offset = position - static_cast<double> (centre);
        const auto weight = static_cast<float> (0.5 - 0.5 * std::cos (twoPi * offset));

        auto& band = bands_[static_cast<std::size_t> (centre)];

        if (band.binCount == 0)
        {
            band.firstBin = bin;
            band.weightOffset = weights_.size();
        }

        ++band.binCount;
        weights_.push_back (weight);
    }

    // Normalise each band's taper so a flat spectrum produces flat bands.
    for (auto& band : bands_)
    {
        if (band.binCount == 0)
            continue;

        auto weightSum = 0.0f;

        for (auto i = 0; i < band.binCount; ++i)
            weightSum += weights_[band.weightOffset + static_cast<std::size_t> (i)];

        if (weightSum > 0.0f)
            for (auto i = 0; i < band.binCount; ++i)
                weights_[band.weightOffset + static_cast<std::size_t> (i)] /= weightSum;
    }

    // Pass 2: bands narrower than the bin spacing interpolate between the two
    // bins bracketing their centre, so the low end stays populated.
    for (auto band = 0; band < kCqtBandCountInt; ++band)
    {
        auto& entry = bands_[static_cast<std::size_t> (band)];

        if (entry.binCount > 0)
            continue;

        const auto centre = static_cast<double> (bandCenterFrequency (band));
        const auto exactBin = centre / binHz;

        auto lower = static_cast<int> (std::floor (exactBin));
        lower = std::clamp (lower, kMinBandBin, maxBin);

        const auto upper = std::clamp (lower + 1, kMinBandBin, maxBin);
        const auto fraction = static_cast<float> (std::clamp (exactBin - static_cast<double> (lower),
                                                              0.0, 1.0));

        entry.firstBin = -1;
        entry.lowerBin = lower;
        entry.upperBin = upper;
        entry.fraction = fraction;
    }
}

float Cqt::bandCenterFrequency (int band) const noexcept
{
    if (band < 0 || band >= kCqtBandCountInt)
        return 0.0f;

    return static_cast<float> (static_cast<double> (minFrequency_)
                               * std::pow (2.0, static_cast<double> (band)
                                                / static_cast<double> (kCqtBinsPerOctave)));
}

int Cqt::bandBinCount (int band) const noexcept
{
    if (band < 0 || band >= static_cast<int> (bands_.size()))
        return 0;

    return bands_[static_cast<std::size_t> (band)].binCount;
}

int Cqt::bandFirstBin (int band) const noexcept
{
    if (band < 0 || band >= static_cast<int> (bands_.size()))
        return -1;

    return bands_[static_cast<std::size_t> (band)].firstBin;
}

bool Cqt::bandIsResolved (int band) const noexcept
{
    return bandBinCount (band) > 0;
}

void Cqt::process (const float* realBins, const float* imaginaryBins,
                   float* magnitudes) noexcept
{
    if (realBins == nullptr || imaginaryBins == nullptr || magnitudes == nullptr)
        return;

    const auto binEnergy = [realBins, imaginaryBins] (int bin)
    {
        const auto index = static_cast<std::size_t> (bin);
        return realBins[index] * realBins[index]
             + imaginaryBins[index] * imaginaryBins[index];
    };

    for (std::size_t band = 0; band < bands_.size(); ++band)
    {
        const auto& entry = bands_[band];

        if (entry.binCount > 0)
        {
            auto energy = 0.0f;

            for (auto i = 0; i < entry.binCount; ++i)
            {
                const auto bin = entry.firstBin + i;
                const auto weight = weights_[entry.weightOffset + static_cast<std::size_t> (i)];
                energy += weight * binEnergy (bin);
            }

            magnitudes[band] = std::sqrt (energy);
            continue;
        }

        const auto lower = binEnergy (entry.lowerBin);
        const auto upper = binEnergy (entry.upperBin);
        magnitudes[band] = std::sqrt (lower + (upper - lower) * entry.fraction);
    }
}

void Cqt::foldToChroma (const float* magnitudes, float* chroma) noexcept
{
    if (magnitudes == nullptr || chroma == nullptr)
        return;

    std::fill (chroma, chroma + kChromaCount, 0.0f);

    for (std::size_t band = 0; band < bands_.size(); ++band)
    {
        const auto pitchClass = band % static_cast<std::size_t> (kCqtBinsPerOctave);
        chroma[pitchClass] = std::max (chroma[pitchClass], magnitudes[band]);
    }

    const auto peak = *std::max_element (chroma, chroma + kChromaCount);

    if (peak > 1.0e-9f)
        for (std::size_t i = 0; i < kChromaCount; ++i)
            chroma[i] = std::clamp (chroma[i] / peak, 0.0f, 1.0f);
}
} // namespace mvt::dsp
