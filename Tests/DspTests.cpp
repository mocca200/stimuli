#include "Engine/DSP/AnalysisTypes.h"
#include "Engine/DSP/DspPipeline.h"
#include "Engine/DSP/Fft.h"
#include "Engine/DSP/Hpss.h"
#include "AllocationProbe.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <numbers>
#include <thread>
#include <vector>

using namespace mvt::dsp;

namespace
{
float binToFrequency (int bin)
{
    return static_cast<float> (bin) * 48000.0f / 2048.0f;
}

std::size_t argMax (const std::array<float, kSpectrumBandCount>& values)
{
    return static_cast<std::size_t> (std::distance (values.begin(),
                                                    std::max_element (values.begin(), values.end())));
}
} // namespace

TEST_CASE ("Fft resolves an exact-bin sine at the right amplitude", "[dsp][fft]")
{
    constexpr int size = 2048;
    Fft fft (11);
    REQUIRE (fft.size() == size);

    std::vector<float> input (size);
    std::vector<float> magnitudes (size);

    const auto frequency = binToFrequency (20);
    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

    for (int i = 0; i < size; ++i)
        input[static_cast<std::size_t> (i)] = 0.5f * std::sin (twoPi * frequency
                                                               * static_cast<float> (i) / 48000.0f);

    fft.processReal (input.data(), magnitudes.data());

    const auto peak = static_cast<int> (std::distance (
        magnitudes.begin(), std::max_element (magnitudes.begin(), magnitudes.begin() + size / 2 + 1)));

    REQUIRE (peak == 20);
    REQUIRE (magnitudes[20] == Catch::Approx (0.5f * static_cast<float> (size) / 2.0f).epsilon (0.01));
}

TEST_CASE ("Hpss keeps a steady tone harmonic and an impulse percussive", "[dsp][hpss]")
{
    constexpr std::size_t bins = 64;
    constexpr std::size_t kernel = 9;

    std::vector<float> magnitudes (bins, 0.0f);
    std::vector<float> harmonic (bins, 0.0f);
    std::vector<float> percussive (bins, 0.0f);

    Hpss steady (bins, kernel);

    for (int frame = 0; frame < 24; ++frame)
    {
        std::fill (magnitudes.begin(), magnitudes.end(), 0.0f);
        magnitudes[10] = 1.0f;
        steady.process (magnitudes.data(), harmonic.data(), percussive.data());
    }

    REQUIRE (harmonic[10] == Catch::Approx (1.0f).margin (0.001f));
    REQUIRE (percussive[10] == Catch::Approx (0.0f).margin (0.001f));

    Hpss transient (bins, kernel);

    for (int frame = 0; frame < 8; ++frame)
    {
        std::fill (magnitudes.begin(), magnitudes.end(), 0.0f);
        transient.process (magnitudes.data(), harmonic.data(), percussive.data());
    }

    std::fill (magnitudes.begin(), magnitudes.end(), 1.0f);
    transient.process (magnitudes.data(), harmonic.data(), percussive.data());

    const auto harmonicSum = std::accumulate (harmonic.begin(), harmonic.end(), 0.0f);
    const auto percussiveSum = std::accumulate (percussive.begin(), percussive.end(), 0.0f);

    REQUIRE (percussiveSum > harmonicSum * 10.0f);
    REQUIRE (percussive[0] == Catch::Approx (1.0f).margin (0.001f));
}

TEST_CASE ("DspPipeline publishes a tonal snapshot for a 440 Hz sine", "[dsp][pipeline]")
{
    DspPipeline pipeline;
    pipeline.prepare (48000.0, 512);
    REQUIRE (pipeline.isRunning());

    constexpr int blockSize = 512;
    constexpr int blocks = 120;

    std::vector<float> left (blockSize);
    std::vector<float> right (blockSize);

    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;
    auto phase = 0.0f;

    for (int block = 0; block < blocks; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto sample = 0.5f * std::sin (phase);
            left[static_cast<std::size_t> (i)] = sample;
            right[static_cast<std::size_t> (i)] = sample;

            phase += twoPi * 440.0f / 48000.0f;

            if (phase > twoPi)
                phase -= twoPi;
        }

        pipeline.pushAudio (left.data(), right.data(), blockSize);
    }

    AnalysisSnapshot snapshot;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (3);
    auto received = false;

    while (! received && std::chrono::steady_clock::now() < deadline)
    {
        if (pipeline.readSnapshot (snapshot) && snapshot.valid && snapshot.sequence >= 20)
            received = true;
        else
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }

    REQUIRE (received);
    REQUIRE (pipeline.droppedFrames() == 0);
    REQUIRE (snapshot.peakLeft == Catch::Approx (0.5f).margin (0.02f));
    REQUIRE (snapshot.spectralCentroidHz > 380.0f);
    REQUIRE (snapshot.spectralCentroidHz < 520.0f);
    REQUIRE (snapshot.harmonicRatio > 0.2f);
    REQUIRE (snapshot.chroma[0] == Catch::Approx (1.0f).margin (0.001f));

    const auto peakBand = argMax (snapshot.spectrum);
    REQUIRE (peakBand >= 53);
    REQUIRE (peakBand <= 62);

    const auto waveformPeak = *std::max_element (snapshot.waveform.begin(), snapshot.waveform.end());
    REQUIRE (waveformPeak > 0.2f);

    pipeline.stop();
    REQUIRE_FALSE (pipeline.isRunning());
}

TEST_CASE ("DspPipeline reports silence without stale data", "[dsp][pipeline]")
{
    DspPipeline pipeline;
    pipeline.prepare (44100.0, 256);

    std::vector<float> silence (256, 0.0f);

    for (int block = 0; block < 200; ++block)
        pipeline.pushAudio (silence.data(), nullptr, 256);

    AnalysisSnapshot snapshot;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (3);
    auto received = false;

    while (! received && std::chrono::steady_clock::now() < deadline)
    {
        if (pipeline.readSnapshot (snapshot) && snapshot.valid)
            received = true;
        else
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }

    REQUIRE (received);
    REQUIRE (snapshot.peakLeft == 0.0f);
    REQUIRE (snapshot.rmsLeft == 0.0f);
    REQUIRE (snapshot.harmonicRatio == 0.0f);

    for (const auto value : snapshot.spectrum)
        REQUIRE (value == 0.0f);
}

TEST_CASE ("Analysis gain scales the published spectrum", "[dsp][pipeline][gain]")
{
    constexpr int blockSize = 512;
    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

    // Feeds at roughly real time so the capture ring cannot overflow, then
    // waits until the worker has nothing left to publish. Without the pacing a
    // burst of blocks outruns the worker and the comparison below would read
    // frames captured before the parameter change.
    auto settle = [&] (DspPipeline& pipeline)
    {
        AnalysisSnapshot snapshot;
        AnalysisSnapshot last;
        auto haveFrame = false;
        auto idlePolls = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (5);

        while (idlePolls < 3 && std::chrono::steady_clock::now() < deadline)
        {
            if (pipeline.readSnapshot (snapshot) && snapshot.valid)
            {
                last = snapshot;
                haveFrame = true;
                idlePolls = 0;
            }
            else
            {
                ++idlePolls;
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (20));
        }

        return haveFrame ? last : AnalysisSnapshot {};
    };

    auto feedPaced = [&] (DspPipeline& pipeline, float& phase, int blocks)
    {
        std::vector<float> samples (blockSize);

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                samples[static_cast<std::size_t> (i)] = 0.5f * std::sin (phase);
                phase += twoPi * 440.0f / 48000.0f;

                if (phase > twoPi)
                    phase -= twoPi;
            }

            pipeline.pushAudio (samples.data(), samples.data(), blockSize);
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
    };

    DspPipeline pipeline;
    pipeline.prepare (48000.0, 512);
    REQUIRE (pipeline.isRunning());

    auto phase = 0.0f;
    feedPaced (pipeline, phase, 100);
    const auto unity = settle (pipeline);
    REQUIRE (unity.valid);
    REQUIRE (unity.spectrum[argMax (unity.spectrum)] > 0.2f);

    // 12 dB down must move the published band peak measurably: a 0.5 amplitude
    // sine sits around -6 dBFS, well above the -100 dBFS floor, so neither end
    // of the comparison clips.
    pipeline.setAnalysisGainDecibels (-12.0f);
    feedPaced (pipeline, phase, 40);
    const auto attenuated = settle (pipeline);
    REQUIRE (attenuated.valid);
    REQUIRE (attenuated.sequence > unity.sequence);
    REQUIRE (attenuated.spectrum[argMax (attenuated.spectrum)]
             < unity.spectrum[argMax (unity.spectrum)] - 0.05f);

    // Smoothing must remain a bounded, opt-in filter: turning it on never
    // produces values outside the normalised range.
    pipeline.setAnalysisGainDecibels (0.0f);
    pipeline.setSmoothingMilliseconds (120.0f);
    feedPaced (pipeline, phase, 40);
    const auto smoothed = settle (pipeline);

    REQUIRE (smoothed.valid);
    REQUIRE (smoothed.sequence > attenuated.sequence);

    for (const auto value : smoothed.spectrum)
    {
        REQUIRE (value >= 0.0f);
        REQUIRE (value <= 1.0f);
    }

    pipeline.stop();
}

TEST_CASE ("Spectrum values map back to decibels", "[dsp]")
{
    REQUIRE (spectrumValueToDecibels (0.0f) == Catch::Approx (-100.0f));
    REQUIRE (spectrumValueToDecibels (1.0f) == Catch::Approx (0.0f));
    REQUIRE (spectrumValueToDecibels (0.5f) == Catch::Approx (-50.0f));
}

TEST_CASE ("Audio capture and FFT analysis are allocation-free", "[dsp][realtime]")
{
    constexpr int blockSize = 512;

    DspPipeline pipeline;
    pipeline.prepare (48000.0, blockSize);

    std::vector<float> left (blockSize, 0.1f);
    std::vector<float> right (blockSize, -0.1f);

    mvt::test::setAllocationTracking (false);
    mvt::test::resetTrackedAllocations();
    mvt::test::setAllocationTracking (true);

    for (int block = 0; block < 200; ++block)
        pipeline.pushAudio (left.data(), right.data(), blockSize);

    mvt::test::setAllocationTracking (false);
    REQUIRE (mvt::test::trackedAllocationCount() == 0);

    Fft fft (11);
    std::vector<float> input (2048, 0.25f);
    std::vector<float> magnitudes (2048, 0.0f);

    mvt::test::resetTrackedAllocations();
    mvt::test::setAllocationTracking (true);

    for (int frame = 0; frame < 100; ++frame)
        fft.processReal (input.data(), magnitudes.data());

    mvt::test::setAllocationTracking (false);
    REQUIRE (mvt::test::trackedAllocationCount() == 0);

    pipeline.stop();
}
