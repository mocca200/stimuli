#pragma once

#include "Engine/DSP/AnalysisTypes.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace mvt::ui
{
/** Deterministic synthetic analysis frames for the designer preview. */
class DemoSignal
{
public:
    void reset() noexcept
    {
        time_ = 0.0;
        sequence_ = 0;
    }

    void advance (double deltaSeconds, dsp::AnalysisSnapshot& snapshot) noexcept
    {
        time_ += deltaSeconds;
        ++sequence_;

        snapshot = {};
        snapshot.sequence = sequence_;
        snapshot.sampleRate = 48000.0;
        snapshot.valid = true;

        const auto t = static_cast<float> (time_);
        const auto pulse = 0.5f + 0.5f * std::sin (t * 3.1f);
        const auto sweep = 0.5f + 0.5f * std::sin (t * 0.42f);

        for (std::size_t band = 0; band < dsp::kSpectrumBandCount; ++band)
        {
            const auto x = static_cast<float> (band);

            const auto fundamental = std::exp (-std::pow ((x - (46.0f + sweep * 22.0f)) / 5.5f, 2.0f));
            const auto third = 0.55f * std::exp (-std::pow ((x - 66.0f) / 3.5f, 2.0f));
            const auto fifth = 0.35f * std::exp (-std::pow ((x - 78.0f) / 4.5f, 2.0f));
            const auto shimmer = 0.08f * (0.5f + 0.5f * std::sin (x * 0.7f + t * 2.0f));
            const auto noise = 0.03f * (0.5f + 0.5f * std::sin (x * 2.3f + t * 7.0f));

            const auto value = juce::jlimit (0.0f, 1.0f,
                                             0.16f + fundamental * (0.55f + 0.25f * pulse)
                                                   + third + fifth + shimmer + noise);

            snapshot.spectrum[band] = value;
            snapshot.harmonicSpectrum[band] = juce::jlimit (0.0f, 1.0f, value * (0.72f + 0.2f * pulse));
            snapshot.percussiveSpectrum[band] = juce::jlimit (0.0f, 1.0f,
                                                              value * (0.16f + 0.55f * pulse * pulse));
        }

        for (std::size_t i = 0; i < dsp::kWaveformPointCount; ++i)
        {
            const auto phase = static_cast<float> (i) / static_cast<float> (dsp::kWaveformPointCount);
            const auto envelope = 0.7f + 0.3f * std::sin (t * 1.7f);
            const auto sample = 0.44f * std::sin (juce::MathConstants<float>::twoPi * phase * 3.0f
                                                  + t * 2.0f)
                              + 0.18f * std::sin (juce::MathConstants<float>::twoPi * phase * 7.0f
                                                  - t * 3.0f)
                              + 0.06f * std::sin (juce::MathConstants<float>::twoPi * phase * 13.0f
                                                  + t * 5.0f);

            snapshot.waveform[i] = sample * envelope;
        }

        for (std::size_t i = 0; i < dsp::kScopePointCount; ++i)
        {
            const auto angle = static_cast<float> (i) / static_cast<float> (dsp::kScopePointCount)
                             * juce::MathConstants<float>::twoPi;
            const auto rotation = t * 0.35f;

            snapshot.scope[i * 2] = 0.62f * std::sin (angle + rotation);
            snapshot.scope[i * 2 + 1] = 0.55f * std::sin (angle * 2.0f + rotation * 1.7f);
        }

        const std::array<float, dsp::kChromaCount> chordA {
            1.0f, 0.12f, 0.28f, 0.55f, 0.18f, 0.22f,
            0.34f, 0.72f, 0.1f, 0.2f, 0.16f, 0.08f
        };
        const std::array<float, dsp::kChromaCount> chordB {
            0.10f, 0.55f, 0.14f, 0.18f, 0.62f, 0.12f,
            0.20f, 0.16f, 0.85f, 0.18f, 0.30f, 0.12f
        };

        const auto chordBlend = 0.5f + 0.5f * std::sin (t * 2.6f);

        for (std::size_t i = 0; i < dsp::kChromaCount; ++i)
        {
            snapshot.chroma[i] = juce::jlimit (0.0f, 1.0f,
                                               (chordA[i] * (1.0f - chordBlend)
                                                + chordB[i] * chordBlend)
                                                   * (0.82f + 0.18f * pulse));
        }

        snapshot.peakLeft = 0.68f + 0.08f * pulse;
        snapshot.peakRight = 0.64f + 0.07f * pulse;
        snapshot.rmsLeft = 0.34f + 0.04f * pulse;
        snapshot.rmsRight = 0.32f + 0.04f * pulse;
        snapshot.spectralCentroidHz = 520.0f + 180.0f * sweep;
        snapshot.spectralFlux = 0.18f + 0.4f * pulse;
        snapshot.lowEnergy = 0.22f;
        snapshot.midEnergy = 0.51f;
        snapshot.highEnergy = 0.27f;
        snapshot.harmonicRatio = 0.66f;
        snapshot.percussiveRatio = 0.34f;
    }

private:
    double time_ = 0.0;
    std::uint64_t sequence_ = 0;
};

} // namespace mvt::ui
