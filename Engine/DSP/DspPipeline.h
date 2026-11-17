#pragma once

#include "Engine/DSP/AnalysisTypes.h"

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

    [[nodiscard]] double sampleRate() const noexcept;
    [[nodiscard]] std::uint32_t droppedFrames() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mvt::dsp
