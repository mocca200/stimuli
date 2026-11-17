#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mvt::dsp
{
inline constexpr std::size_t kSpectrumBandCount = 128;
inline constexpr std::size_t kWaveformPointCount = 512;
inline constexpr std::size_t kScopePointCount = 256;
inline constexpr std::size_t kChromaCount = 12;

/**
 * Immutable analysis frame published from the DSP worker to the UI.

 * Every member is trivially copyable so the triple buffer can move frames with
 * plain copies. Spectrum values are normalised 0..1 (0 = -100 dBFS floor).
 */
struct AnalysisSnapshot
{
    std::uint64_t sequence = 0;
    double sampleRate = 0.0;

    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    float rmsLeft = 0.0f;
    float rmsRight = 0.0f;

    float spectralCentroidHz = 0.0f;
    float spectralFlux = 0.0f;
    float lowEnergy = 0.0f;
    float midEnergy = 0.0f;
    float highEnergy = 0.0f;
    float harmonicRatio = 0.0f;
    float percussiveRatio = 0.0f;

    std::array<float, kSpectrumBandCount> spectrum {};
    std::array<float, kSpectrumBandCount> harmonicSpectrum {};
    std::array<float, kSpectrumBandCount> percussiveSpectrum {};
    std::array<float, kWaveformPointCount> waveform {};
    std::array<float, kScopePointCount * 2> scope {};
    std::array<float, kChromaCount> chroma {};

    bool valid = false;
};

static_assert (std::is_trivially_copyable_v<AnalysisSnapshot>,
               "AnalysisSnapshot must remain trivially copyable for the audio/UI handoff");
static_assert (std::is_trivially_destructible_v<AnalysisSnapshot>,
               "AnalysisSnapshot must not require destruction during publishing");

/** Maps a 0..1 spectrum value back to dBFS for labels and debugging. */
[[nodiscard]] constexpr float spectrumValueToDecibels (float value) noexcept
{
    return value * 100.0f - 100.0f;
}

} // namespace mvt::dsp
