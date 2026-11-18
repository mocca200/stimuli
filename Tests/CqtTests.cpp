#include "AllocationProbe.h"
#include "Engine/DSP/AnalysisTypes.h"
#include "Engine/DSP/Cqt.h"
#include "Engine/DSP/DspPipeline.h"
#include "Engine/DSP/Fft.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>
#include <vector>

using namespace mvt::dsp;

namespace
{
constexpr int kFftSize = 2048;
constexpr double kSampleRate = 48000.0;

/** Windowed-free complex transform of a pure tone, ready for the CQT. */
struct Tone
{
    std::vector<float> real;
    std::vector<float> imaginary;

    explicit Tone (float frequencyHz)
        : real (kFftSize), imaginary (kFftSize)
    {
        Fft fft (11);
        std::vector<float> input (kFftSize);

        constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

        for (int i = 0; i < kFftSize; ++i)
            input[static_cast<std::size_t> (i)] =
                std::sin (twoPi * frequencyHz * static_cast<float> (i)
                          / static_cast<float> (kSampleRate));

        fft.processRealComplex (input.data(), real.data(), imaginary.data());
    }
};

std::size_t peakBand (const std::array<float, kCqtBandCount>& magnitudes)
{
    std::size_t peak = 0;

    for (std::size_t i = 1; i < magnitudes.size(); ++i)
        if (magnitudes[i] > magnitudes[peak])
            peak = i;

    return peak;
}
} // namespace

TEST_CASE ("Cqt geometry is one semitone per band", "[dsp][cqt]")
{
    Cqt cqt (kFftSize, kSampleRate);

    REQUIRE (cqt.bandCount() == static_cast<int> (kCqtBandCount));
    REQUIRE (cqt.bandCenterFrequency (0) == Catch::Approx (kCqtMinFrequencyHz).margin (0.01f));

    // Twelve bands make exactly one octave.
    REQUIRE (cqt.bandCenterFrequency (12) == Catch::Approx (cqt.bandCenterFrequency (0) * 2.0f)
                                                .epsilon (0.001f));
    REQUIRE (cqt.bandCenterFrequency (24) == Catch::Approx (cqt.bandCenterFrequency (0) * 4.0f)
                                                .epsilon (0.001f));
}

TEST_CASE ("Cqt interpolates bands narrower than the FFT bin spacing", "[dsp][cqt]")
{
    Cqt cqt (kFftSize, kSampleRate);

    // 2048 bins at 48 kHz is 23.4 Hz per bin. A 3.9 Hz-wide C2 band cannot own a
    // bin, so it must interpolate instead of reporting nothing.
    REQUIRE_FALSE (cqt.bandIsResolved (0));
    REQUIRE (cqt.bandBinCount (0) == 0);
    REQUIRE (cqt.bandFirstBin (0) == -1);

    // High bands are much wider than the bin spacing and own several bins.
    const auto highest = cqt.bandCount() - 1;
    REQUIRE (cqt.bandIsResolved (highest));
    REQUIRE (cqt.bandBinCount (highest) > 4);

    // Owning bands must be ordered and disjoint, so no bin is double counted
    // and every owned range stays inside the spectrum.
    auto previousEnd = 1;

    for (int band = 0; band < cqt.bandCount(); ++band)
    {
        if (! cqt.bandIsResolved (band))
            continue;

        REQUIRE (cqt.bandFirstBin (band) >= previousEnd);
        REQUIRE (cqt.bandFirstBin (band) + cqt.bandBinCount (band) <= kFftSize / 2 + 1);

        previousEnd = cqt.bandFirstBin (band) + cqt.bandBinCount (band);
    }

    // The crossover sits where a band is one bin wide, around 1-2 kHz here.
    REQUIRE (cqt.bandIsResolved (48));
}

TEST_CASE ("Cqt places a pure tone in the band nearest its frequency", "[dsp][cqt]")
{
    Cqt cqt (kFftSize, kSampleRate);
    Tone tone (1000.0f);

    std::array<float, kCqtBandCount> magnitudes {};
    cqt.process (tone.real.data(), tone.imaginary.data(), magnitudes.data());

    const auto peak = peakBand (magnitudes);
    const auto centre = cqt.bandCenterFrequency (static_cast<int> (peak));

    // One semitone is 5.9% at 1 kHz, so allow a band either side.
    REQUIRE (centre == Catch::Approx (1000.0f).margin (60.0f));
    REQUIRE (magnitudes[peak] > 0.0f);
}

TEST_CASE ("Cqt chroma folds a tone onto its pitch class", "[dsp][cqt]")
{
    Cqt cqt (kFftSize, kSampleRate);
    Tone a4 (440.0f);

    std::array<float, kCqtBandCount> magnitudes {};
    std::array<float, kChromaCount> chroma {};

    cqt.process (a4.real.data(), a4.imaginary.data(), magnitudes.data());
    cqt.foldToChroma (magnitudes.data(), chroma.data());

    const auto peak = std::size_t (std::distance (chroma.begin(),
                                                  std::max_element (chroma.begin(), chroma.end())));

    REQUIRE (peak == 9); // A, counting C as 0
    REQUIRE (chroma[peak] == Catch::Approx (1.0f).margin (0.001f));
}

TEST_CASE ("Cqt process is allocation-free", "[dsp][cqt][realtime]")
{
    Cqt cqt (kFftSize, kSampleRate);
    Tone tone (523.25f);

    std::array<float, kCqtBandCount> magnitudes {};
    std::array<float, kChromaCount> chroma {};

    // Warm the code path first so one-time work cannot be counted.
    cqt.process (tone.real.data(), tone.imaginary.data(), magnitudes.data());

    mvt::test::setAllocationTracking (true);
    mvt::test::resetTrackedAllocations();

    for (int frame = 0; frame < 64; ++frame)
    {
        cqt.process (tone.real.data(), tone.imaginary.data(), magnitudes.data());
        cqt.foldToChroma (magnitudes.data(), chroma.data());
    }

    const auto allocations = mvt::test::trackedAllocationCount();
    mvt::test::setAllocationTracking (false);

    REQUIRE (allocations == 0);
}

TEST_CASE ("Cqt on the production 8192 frame folds every tone onto its class", "[dsp][cqt]")
{
    // The pipeline runs the CQT on a 8192-point frame (5.9 Hz per bin at 48 kHz).
    // With a 2048-point frame a semitone at 130 Hz is narrower than a bin, and
    // notes below ~250 Hz start folding onto the neighbouring class.
    struct Tone { float frequency; std::size_t pitchClass; };

    const Tone tones[] {
        { 65.41f, 0 }, { 98.00f, 7 }, { 130.81f, 0 }, { 164.81f, 4 }, { 196.00f, 7 },
        { 220.00f, 9 }, { 261.63f, 0 }, { 329.63f, 4 }, { 440.00f, 9 }, { 523.25f, 0 },
        { 659.26f, 4 }, { 880.00f, 9 }, { 1046.50f, 0 }, { 1760.00f, 9 }, { 3520.00f, 9 }
    };

    constexpr int order = 13;
    const auto size = 1 << order;

    Cqt cqt (size, kSampleRate);
    Fft fft (order);

    std::vector<float> input (static_cast<std::size_t> (size));
    std::vector<float> real (input.size());
    std::vector<float> imaginary (input.size());
    std::array<float, kCqtBandCount> magnitudes {};
    std::array<float, kChromaCount> chroma {};

    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

    for (const auto& tone : tones)
    {
        for (int i = 0; i < size; ++i)
            input[static_cast<std::size_t> (i)] = std::sin (
                twoPi * tone.frequency * static_cast<float> (i)
                / static_cast<float> (kSampleRate));

        fft.processRealComplex (input.data(), real.data(), imaginary.data());
        cqt.process (real.data(), imaginary.data(), magnitudes.data());
        cqt.foldToChroma (magnitudes.data(), chroma.data());

        const auto peak = std::size_t (std::distance (
            chroma.begin(), std::max_element (chroma.begin(), chroma.end())));

        INFO ("tone " << tone.frequency << " Hz expected class " << tone.pitchClass);
        REQUIRE (peak == tone.pitchClass);
    }
}

TEST_CASE ("DspPipeline publishes CQT chroma for a 440 Hz input", "[dsp][cqt][pipeline]")
{
    using namespace std::chrono_literals;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    DspPipeline pipeline;
    pipeline.prepare (sampleRate, blockSize);

    std::vector<float> block (blockSize);
    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

    // The phase must run continuously across blocks. Repeating one block would
    // put a click every 10.7 ms and that broadband splatter moves the chroma
    // peak, which is a property of the test signal, not of the transform.
    auto phase = 0.0f;
    const auto increment = twoPi * 440.0f / static_cast<float> (sampleRate);

    AnalysisSnapshot snapshot;
    auto sawCqt = false;
    std::size_t pitchClass = 0;

    for (int blockIndex = 0; blockIndex < 200 && ! sawCqt; ++blockIndex)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            block[static_cast<std::size_t> (i)] = 0.4f * std::sin (phase);
            phase += increment;

            if (phase > twoPi)
                phase -= twoPi;
        }

        pipeline.pushAudio (block.data(), block.data(), blockSize);

        while (pipeline.readSnapshot (snapshot))
        {
            if (snapshot.chromaCqt[9] > 0.9f)
            {
                sawCqt = true;
                pitchClass = std::size_t (std::distance (
                    snapshot.chromaCqt.begin(),
                    std::max_element (snapshot.chromaCqt.begin(), snapshot.chromaCqt.end())));
            }
        }

        std::this_thread::sleep_for (1ms);
    }

    pipeline.stop();

    REQUIRE (sawCqt);
    REQUIRE (pitchClass == 9);
}

TEST_CASE ("Cqt output is deterministic", "[dsp][cqt]")
{
    Cqt first (kFftSize, kSampleRate);
    Cqt second (kFftSize, kSampleRate);
    Tone tone (220.0f);

    std::array<float, kCqtBandCount> a {};
    std::array<float, kCqtBandCount> b {};

    first.process (tone.real.data(), tone.imaginary.data(), a.data());
    second.process (tone.real.data(), tone.imaginary.data(), b.data());

    REQUIRE (a == b);
}
