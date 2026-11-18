#pragma once

#include "Engine/DSP/AnalysisTypes.h"

#include <vector>

namespace mvt::dsp
{
/**
 * Constant-Q transform on top of a fixed-size STFT frame.

 * Band k is centred on `minFrequency * 2^(k / binsPerOctave)`, so bands are one
 * twelfth of an octave apart and every band has the same Q. Two things follow
 * from the frame size, and both are handled explicitly rather than hidden:

 *  - Where a band is wider than the FFT bin spacing, the complex bins inside it
 *    are integrated with a raised-cosine taper and the band reports
 *    sqrt(weighted energy). Energy, not a coherent complex sum: at 440 Hz a
 *    coherent sum lets bin phases cancel and moves the peak a whole semitone up
 *    (measured, see the tests), which is unacceptable for a chroma view.

 *  - Where a band is narrower than the bin spacing (low frequencies with a
 *    2048-point frame), it contains no bin of its own. Rather than report zero
 *    and leave a comb, the band interpolates the energy of the two bins that
 *    bracket its centre. Every band therefore has a value everywhere, and the
 *    low end is spectrally smoothed by the frame resolution instead of aliasing.

 * Bands that own bins never share one, so nothing is double counted. Each bin
 * is owned by the band whose centre is nearest, and the taper sums to one.

 * Constructing allocates; process() and foldToChroma() do not, so they are safe
 * on the analysis worker hot path.
 */
class Cqt
{
public:
    Cqt (int fftSize, double sampleRate, float minFrequency = kCqtMinFrequencyHz);

    Cqt (const Cqt&) = delete;
    Cqt& operator= (const Cqt&) = delete;

    [[nodiscard]] int bandCount() const noexcept { return static_cast<int> (bands_.size()); }
    [[nodiscard]] int fftSize() const noexcept { return fftSize_; }

    /** Unclamped centre frequency of a band in Hz. */
    [[nodiscard]] float bandCenterFrequency (int band) const noexcept;

    /** How many FFT bins the band integrates. Zero means the band is narrower
        than the bin spacing and interpolates from neighbouring bins instead. */
    [[nodiscard]] int bandBinCount (int band) const noexcept;

    /** First integrated FFT bin, or -1 when the band interpolates. */
    [[nodiscard]] int bandFirstBin (int band) const noexcept;

    /** True when the band integrates bins (rather than interpolating). */
    [[nodiscard]] bool bandIsResolved (int band) const noexcept;

    /** Turns complex FFT bins into linear band magnitudes. Allocation-free.
        Both inputs must hold fftSize() entries. */
    void process (const float* realBins, const float* imaginaryBins,
                  float* magnitudes) noexcept;

    /** Folds bands onto pitch classes (0 = C) and normalises to 0..1 by the
        loudest class. Allocation-free. */
    void foldToChroma (const float* magnitudes, float* chroma) noexcept;

private:
    struct Band
    {
        int firstBin = -1;
        int binCount = 0;
        std::size_t weightOffset = 0;
        int lowerBin = 1;      // interpolation source, used when binCount == 0
        int upperBin = 2;
        float fraction = 0.0f;
    };

    int fftSize_ = 2048;
    double sampleRate_ = 48000.0;
    float minFrequency_ = kCqtMinFrequencyHz;
    std::vector<Band> bands_;
    std::vector<float> weights_;
};
} // namespace mvt::dsp
