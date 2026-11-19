#pragma once

#include "Engine/DSP/AnalysisTypes.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace mvt::dsp
{
/**
 * Audio-thread capture, worker-thread analysis, and lock-free handoff.

 * prepare() allocates every buffer and starts the worker. pushAudio() is the
 * only method intended for the audio callback: it copies interleaved stereo
 * frames into a preallocated SPSC ring and never allocates or blocks.
 */
class DspPipeline
{
public:
    DspPipeline();
    ~DspPipeline();

    DspPipeline (const DspPipeline&) = delete;
    DspPipeline& operator= (const DspPipeline&) = delete;

    /** Stops any existing worker, allocates analysis buffers, and starts a new
        worker. Must not be called concurrently with pushAudio(). */
    void prepare (double sampleRate, int maxBlockSize);

    /** Stops the worker and releases analysis buffers. Safe to call repeatedly. */
    void stop();

    /** Audio-thread entry point. `right` may be null for mono input. */
    void pushAudio (const float* left, const float* right, int numSamples) noexcept;

    /** UI-thread entry point. Returns false when no newer frame is available. */
    [[nodiscard]] bool readSnapshot (AnalysisSnapshot& destination) noexcept;

    /** Visual analysis gain in decibels, applied to the STFT magnitudes before
        every feature is derived. Changes the sensitivity of the spectrogram,
        the spectrum and the note strip without touching the audio path.
        Safe to call from any thread, including while prepared. */
    void setAnalysisGainDecibels (float decibels) noexcept;

    /** Exponential smoothing time constant for the published spectral views in
        milliseconds. 0 disables smoothing and publishes raw frames. Time-domain
        views (waveform, vectorscope) are never smoothed. Safe to call from any
        thread, including while prepared. */
    void setSmoothingMilliseconds (float milliseconds) noexcept;

    [[nodiscard]] double sampleRate() const noexcept;
    [[nodiscard]] std::uint32_t droppedFrames() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Kept outside Impl so a value set before prepare() or across a prepare()
    // survives the worker being torn down and rebuilt. Atomic because setters
    // are documented as callable from any thread, including while prepared.
    std::atomic<float> analysisGainDecibels_ { 0.0f };
    std::atomic<float> smoothingMilliseconds_ { 0.0f };
};

} // namespace mvt::dsp
