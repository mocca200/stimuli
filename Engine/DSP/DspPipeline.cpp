#include "Engine/DSP/DspPipeline.h"

#include "Engine/Core/SpscRingBuffer.h"
#include "Engine/Core/TripleBuffer.h"
#include "Engine/DSP/Cqt.h"
#include "Engine/DSP/Fft.h"
#include "Engine/DSP/Hpss.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <thread>
#include <vector>

namespace mvt::dsp
{
namespace
{
constexpr int kFftOrder = 11;
constexpr std::size_t kFftSize = std::size_t { 1 } << kFftOrder;
constexpr std::size_t kBinCount = kFftSize / 2 + 1;
constexpr std::size_t kHopSize = 512;
constexpr std::size_t kChunkFrames = 8192;

// The CQT runs on its own longer frame: a 2048-point frame resolves only ~23 Hz,
// which is more than a semitone at 130 Hz and folds low notes onto the wrong
// pitch class. 8192 points is 5.9 Hz at 48 kHz and gets every tone from C2 up
// onto the right class. It updates every fourth hop because a 170 ms window
// barely changes in 10 ms, which keeps the worker inside its 3% budget.
constexpr int kCqtFftOrder = 13;
constexpr std::size_t kCqtFftSize = std::size_t { 1 } << kCqtFftOrder;
constexpr std::size_t kCqtUpdateHops = 4;
constexpr std::size_t kScopeHistory = kScopePointCount;

float clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

float amplitudeToNormalisedDecibels (float amplitude) noexcept
{
    constexpr auto floorAmplitude = 1.0e-5f;
    const auto decibels = 20.0f * std::log10 (std::max (amplitude, floorAmplitude));
    return clamp01 ((decibels + 100.0f) / 100.0f);
}
} // namespace

struct DspPipeline::Impl
{
    Impl (double sampleRateIn, int maxBlockSizeIn)
        : sampleRate (sampleRateIn)
        , maxBlockFrames (std::max (1, maxBlockSizeIn))
        , ring (std::make_unique<mvt::core::SpscRingBuffer<float>> (
              static_cast<std::size_t> (std::ceil (sampleRateIn * 2.0)) * 2))
        , fft (kFftOrder)
        , hpss (kBinCount, 17)
        , cqt (static_cast<int> (kCqtFftSize), sampleRateIn)
        , cqtFft (kCqtFftOrder)
        , window (kFftSize, 0.0f)
        , fftInput (kFftSize, 0.0f)
        , magnitudes (kFftSize, 0.0f)
        , harmonic (kFftSize, 0.0f)
        , percussive (kFftSize, 0.0f)
        , previousMagnitudes (kFftSize, 0.0f)
        , historyMono (kFftSize, 0.0f)
        , historyLeft (kFftSize, 0.0f)
        , historyRight (kFftSize, 0.0f)
    {
        pushBuffer.resize (static_cast<std::size_t> (maxBlockFrames) * 2);
        chunkBuffer.resize (kChunkFrames * 2);
        fftReal.resize (kFftSize, 0.0f);
        fftImaginary.resize (kFftSize, 0.0f);
        cqtMagnitudes.resize (kCqtBandCount, 0.0f);
        cqtHistory.resize (kCqtFftSize, 0.0f);
        cqtWindow.resize (kCqtFftSize, 0.0f);
        cqtInput.resize (kCqtFftSize, 0.0f);
        cqtReal.resize (kCqtFftSize, 0.0f);
        cqtImaginary.resize (kCqtFftSize, 0.0f);

        for (std::size_t i = 0; i < kCqtFftSize; ++i)
            cqtWindow[i] = 0.5f - 0.5f * std::cos (6.28318530717958647692f
                                                   * static_cast<float> (i)
                                                   / static_cast<float> (kCqtFftSize));

        constexpr auto twoPi = 6.28318530717958647692f;
        auto windowSum = 0.0f;

        for (std::size_t i = 0; i < kFftSize; ++i)
        {
            window[i] = 0.5f - 0.5f * std::cos (twoPi * static_cast<float> (i)
                                                / static_cast<float> (kFftSize));
            windowSum += window[i];
        }

        windowScale = windowSum > 0.0f ? 2.0f / windowSum : 0.0f;

        configureBands();
        configureChroma();
    }

    ~Impl()
    {
        worker.request_stop();
        if (worker.joinable())
            worker.join();
    }

    void start()
    {
        worker = std::jthread ([this] (std::stop_token token) { run (token); });
    }

    void setGainDecibels (float decibels) noexcept
    {
        const auto clamped = std::clamp (decibels, -48.0f, 24.0f);
        gainLinear.store (std::pow (10.0f, clamped / 20.0f), std::memory_order_relaxed);
    }

    void setSmoothingMilliseconds (float milliseconds) noexcept
    {
        const auto clamped = std::clamp (milliseconds, 0.0f, 1000.0f);
        const auto hopSeconds = static_cast<double> (kHopSize) / sampleRate;
        const auto timeConstantSeconds = static_cast<double> (clamped) * 0.001;

        // One-pole coefficient for a per-hop step: alpha = 1 - exp(-dt / tau).
        // A zero time constant yields alpha == 1, which publishes frames as-is.
        const auto alpha = timeConstantSeconds > 0.0
                             ? 1.0 - std::exp (-hopSeconds / timeConstantSeconds)
                             : 1.0;

        smoothingAlpha.store (static_cast<float> (alpha), std::memory_order_relaxed);
    }

    void push (const float* left, const float* right, int numSamples) noexcept
    {
        auto offset = 0;

        while (offset < numSamples)
        {
            const auto chunk = std::min (numSamples - offset, maxBlockFrames);
            const auto sampleCount = static_cast<std::size_t> (chunk);

            for (std::size_t i = 0; i < sampleCount; ++i)
            {
                const auto source = static_cast<std::size_t> (offset) + i;
                pushBuffer[i * 2] = left[source];
                pushBuffer[i * 2 + 1] = right != nullptr ? right[source] : left[source];
            }

            const auto written = ring->pushBlock (pushBuffer.data(), sampleCount * 2);
            const auto writtenFrames = written / 2;

            if (writtenFrames < sampleCount)
            {
                dropped.fetch_add (static_cast<std::uint32_t> (sampleCount - writtenFrames),
                                   std::memory_order_relaxed);
                break;
            }

            offset += chunk;
        }
    }

    void run (const std::stop_token& token)
    {
        while (! token.stop_requested())
        {
            const auto read = ring->popBlock (chunkBuffer.data(), chunkBuffer.size());

            if (read < 2)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (2));
                continue;
            }

            const auto frames = read / 2;

            for (std::size_t i = 0; i < frames; ++i)
                processFrame (chunkBuffer[i * 2], chunkBuffer[i * 2 + 1]);
        }
    }

    void processFrame (float left, float right) noexcept
    {
        historyLeft[historyPosition] = left;
        historyRight[historyPosition] = right;
        historyMono[historyPosition] = (left + right) * 0.5f;
        historyPosition = (historyPosition + 1) % kFftSize;

        cqtHistory[cqtHistoryPosition] = (left + right) * 0.5f;
        cqtHistoryPosition = (cqtHistoryPosition + 1) % kCqtFftSize;

        ++samplesSeen;
        ++hopCounter;

        hopPeakLeft = std::max (hopPeakLeft, std::abs (left));
        hopPeakRight = std::max (hopPeakRight, std::abs (right));
        hopSumSquaresLeft += static_cast<double> (left) * static_cast<double> (left);
        hopSumSquaresRight += static_cast<double> (right) * static_cast<double> (right);

        if (hopCounter >= kHopSize)
            analyse();
    }

    void analyse() noexcept
    {
        if (samplesSeen < kFftSize)
        {
            resetHopStatistics();
            return;
        }

        for (std::size_t i = 0; i < kFftSize; ++i)
        {
            const auto index = (historyPosition + i) % kFftSize;
            fftInput[i] = historyMono[index] * window[i];
        }

        // One transform feeds both the STFT magnitudes and the CQT filterbank.
        fft.processRealComplex (fftInput.data(), fftReal.data(), fftImaginary.data());

        const auto gain = gainLinear.load (std::memory_order_relaxed);

        for (std::size_t bin = 0; bin < kBinCount; ++bin)
        {
            magnitudes[bin] = std::sqrt (fftReal[bin] * fftReal[bin]
                                         + fftImaginary[bin] * fftImaginary[bin]);
            magnitudes[bin] *= windowScale * gain;
        }

        hpss.process (magnitudes.data(), harmonic.data(), percussive.data());

        AnalysisSnapshot frame;
        frame.sequence = ++sequence;
        frame.sampleRate = sampleRate;
        frame.peakLeft = hopPeakLeft;
        frame.peakRight = hopPeakRight;

        const auto hopCount = static_cast<double> (std::max<std::size_t> (1, hopCounter));
        frame.rmsLeft = static_cast<float> (std::sqrt (hopSumSquaresLeft / hopCount));
        frame.rmsRight = static_cast<float> (std::sqrt (hopSumSquaresRight / hopCount));

        fillSpectrum (frame);
        fillChroma (frame);
        updateCqt (frame);
        fillEnergy (frame);
        fillTimeDomain (frame);
        smoothPublishedViews (frame);

        frame.valid = true;
        snapshot.write (frame);

        resetHopStatistics();
    }

    /** Applies the one-pole smoother to every spectral view. The smoothers keep
        their own state so switching the parameter mid-stream ramps from what is
        currently on screen instead of jumping. */
    void smoothPublishedViews (AnalysisSnapshot& frame) noexcept
    {
        const auto alpha = smoothingAlpha.load (std::memory_order_relaxed);

        if (alpha >= 0.999f)
        {
            // Smoothing off: publish raw frames, and keep the smoother state in
            // step so enabling it later starts from the visible values.
            smoothedSpectrum = frame.spectrum;
            smoothedHarmonic = frame.harmonicSpectrum;
            smoothedPercussive = frame.percussiveSpectrum;
            smoothedCqt = frame.cqt;
            smoothedChroma = frame.chroma;
            smoothedChromaCqt = frame.chromaCqt;
            return;
        }

        smoothArray (frame.spectrum, smoothedSpectrum, alpha);
        smoothArray (frame.harmonicSpectrum, smoothedHarmonic, alpha);
        smoothArray (frame.percussiveSpectrum, smoothedPercussive, alpha);
        smoothArray (frame.cqt, smoothedCqt, alpha);
        smoothArray (frame.chroma, smoothedChroma, alpha);
        smoothArray (frame.chromaCqt, smoothedChromaCqt, alpha);
    }

    template <typename ArrayType>
    static void smoothArray (ArrayType& published, ArrayType& state, float alpha) noexcept
    {
        for (std::size_t i = 0; i < published.size(); ++i)
        {
            state[i] += alpha * (published[i] - state[i]);
            published[i] = state[i];
        }
    }

    void resetHopStatistics() noexcept
    {
        hopCounter = 0;
        hopPeakLeft = 0.0f;
        hopPeakRight = 0.0f;
        hopSumSquaresLeft = 0.0;
        hopSumSquaresRight = 0.0;
    }

    void fillSpectrum (AnalysisSnapshot& frame) noexcept
    {
        double weightedFrequency = 0.0;
        double magnitudeSum = 0.0;
        float flux = 0.0f;

        for (std::size_t band = 0; band < kSpectrumBandCount; ++band)
        {
            float peak = 0.0f;
            float harmonicPeak = 0.0f;
            float percussivePeak = 0.0f;

            const auto begin = static_cast<std::size_t> (bandStart[band]);
            const auto end = static_cast<std::size_t> (bandEnd[band]);

            for (auto bin = begin; bin < end && bin < kBinCount; ++bin)
            {
                peak = std::max (peak, magnitudes[bin]);
                harmonicPeak = std::max (harmonicPeak, harmonic[bin]);
                percussivePeak = std::max (percussivePeak, percussive[bin]);
            }

            frame.spectrum[band] = amplitudeToNormalisedDecibels (peak);
            frame.harmonicSpectrum[band] = amplitudeToNormalisedDecibels (harmonicPeak);
            frame.percussiveSpectrum[band] = amplitudeToNormalisedDecibels (percussivePeak);
        }

        for (std::size_t bin = 1; bin < kBinCount; ++bin)
        {
            const auto frequency = static_cast<double> (bin) * sampleRate
                                 / static_cast<double> (kFftSize);
            weightedFrequency += frequency * static_cast<double> (magnitudes[bin]);
            magnitudeSum += static_cast<double> (magnitudes[bin]);

            flux += std::max (0.0f, magnitudes[bin] - previousMagnitudes[bin]);
            previousMagnitudes[bin] = magnitudes[bin];
        }

        frame.spectralCentroidHz = magnitudeSum > 1.0e-9
                                     ? static_cast<float> (weightedFrequency / magnitudeSum)
                                     : 0.0f;

        const auto meanFlux = flux / static_cast<float> (kBinCount - 1);
        frame.spectralFlux = clamp01 (meanFlux * 16.0f);
    }

    void fillChroma (AnalysisSnapshot& frame) noexcept
    {
        std::array<float, kChromaCount> accumulation {};

        for (std::size_t bin = 1; bin < kBinCount; ++bin)
        {
            const auto pitchClass = chromaBin[bin];

            if (pitchClass >= 0 && pitchClass < static_cast<int> (kChromaCount))
                accumulation[static_cast<std::size_t> (pitchClass)] += harmonic[bin];
        }

        const auto peak = *std::max_element (accumulation.begin(), accumulation.end());

        if (peak > 1.0e-9f)
        {
            for (std::size_t i = 0; i < kChromaCount; ++i)
                frame.chroma[i] = clamp01 (accumulation[i] / peak);
        }
    }

    void updateCqt (AnalysisSnapshot& frame) noexcept
    {
        if (++cqtHopCounter >= kCqtUpdateHops)
        {
            cqtHopCounter = 0;

            if (samplesSeen >= kCqtFftSize)
            {
                for (std::size_t i = 0; i < kCqtFftSize; ++i)
                {
                    const auto index = (cqtHistoryPosition + i) % kCqtFftSize;
                    cqtInput[i] = cqtHistory[index] * cqtWindow[i];
                }

                cqtFft.processRealComplex (cqtInput.data(), cqtReal.data(),
                                           cqtImaginary.data());
                cqt.process (cqtReal.data(), cqtImaginary.data(), cqtMagnitudes.data());

                auto peak = 0.0f;

                for (const auto magnitude : cqtMagnitudes)
                    peak = std::max (peak, magnitude);

                if (peak > 1.0e-9f)
                    for (std::size_t band = 0; band < kCqtBandCount; ++band)
                        lastCqt[band] = clamp01 (cqtMagnitudes[band] / peak);

                cqt.foldToChroma (cqtMagnitudes.data(), lastChromaCqt.data());
            }
        }

        // Bands hold their last value between updates so a 23-frame-per-second
        // CQT still yields a complete snapshot on every hop.
        frame.cqt = lastCqt;
        frame.chromaCqt = lastChromaCqt;
    }

    void fillEnergy (AnalysisSnapshot& frame) noexcept
    {
        double low = 0.0;
        double mid = 0.0;
        double high = 0.0;
        double total = 0.0;
        double harmonicPower = 0.0;
        double percussivePower = 0.0;

        for (std::size_t bin = 1; bin < kBinCount; ++bin)
        {
            const auto frequency = static_cast<double> (bin) * sampleRate
                                 / static_cast<double> (kFftSize);
            const auto power = static_cast<double> (magnitudes[bin])
                             * static_cast<double> (magnitudes[bin]);

            total += power;
            harmonicPower += static_cast<double> (harmonic[bin])
                           * static_cast<double> (harmonic[bin]);
            percussivePower += static_cast<double> (percussive[bin])
                             * static_cast<double> (percussive[bin]);

            if (frequency < 250.0)
                low += power;
            else if (frequency < 2500.0)
                mid += power;
            else if (frequency <= 20000.0)
                high += power;
        }

        if (total > 1.0e-12)
        {
            frame.lowEnergy = static_cast<float> (low / total);
            frame.midEnergy = static_cast<float> (mid / total);
            frame.highEnergy = static_cast<float> (high / total);
        }

        const auto separation = harmonicPower + percussivePower;

        if (separation > 1.0e-12)
        {
            frame.harmonicRatio = static_cast<float> (harmonicPower / separation);
            frame.percussiveRatio = static_cast<float> (percussivePower / separation);
        }
    }

    void fillTimeDomain (AnalysisSnapshot& frame) noexcept
    {
        constexpr auto stride = kFftSize / kWaveformPointCount;

        for (std::size_t i = 0; i < kWaveformPointCount; ++i)
        {
            const auto index = (historyPosition + i * stride) % kFftSize;
            frame.waveform[i] = historyMono[index];
        }

        for (std::size_t i = 0; i < kScopeHistory; ++i)
        {
            const auto index = (historyPosition + kFftSize - kScopeHistory + i) % kFftSize;
            const auto left = historyLeft[index];
            const auto right = historyRight[index];

            frame.scope[i * 2] = (left - right) * 0.5f;
            frame.scope[i * 2 + 1] = (left + right) * 0.5f;
        }
    }

    void configureBands() noexcept
    {
        const auto nyquist = sampleRate * 0.5;
        const auto lowest = 20.0;
        const auto highest = std::min (20000.0, nyquist * 0.95);

        if (highest <= lowest)
        {
            for (std::size_t band = 0; band < kSpectrumBandCount; ++band)
            {
                bandStart[band] = 1;
                bandEnd[band] = 2;
            }
            return;
        }

        const auto ratio = highest / lowest;
        const auto bandCount = static_cast<double> (kSpectrumBandCount);

        for (std::size_t band = 0; band < kSpectrumBandCount; ++band)
        {
            const auto t0 = static_cast<double> (band) / bandCount;
            const auto t1 = static_cast<double> (band + 1) / bandCount;
            const auto f0 = lowest * std::pow (ratio, t0);
            const auto f1 = lowest * std::pow (ratio, t1);

            auto start = static_cast<int> (std::floor (f0 * static_cast<double> (kFftSize) / sampleRate));
            auto end = static_cast<int> (std::ceil (f1 * static_cast<double> (kFftSize) / sampleRate));

            start = std::clamp (start, 1, static_cast<int> (kBinCount) - 1);
            end = std::clamp (end, start + 1, static_cast<int> (kBinCount));

            bandStart[band] = start;
            bandEnd[band] = end;
        }
    }

    void configureChroma() noexcept
    {
        for (std::size_t bin = 0; bin < kBinCount; ++bin)
        {
            const auto frequency = static_cast<double> (bin) * sampleRate
                                 / static_cast<double> (kFftSize);
            chromaBin[bin] = -1;

            if (frequency < 35.0 || frequency > 5000.0)
                continue;

            auto pitchClass = static_cast<int> (
                std::lround (12.0 * std::log2 (frequency / 440.0)));

            pitchClass %= 12;
            if (pitchClass < 0)
                pitchClass += 12;

            chromaBin[bin] = pitchClass;
        }
    }

    double sampleRate = 48000.0;
    int maxBlockFrames = 512;

    std::unique_ptr<mvt::core::SpscRingBuffer<float>> ring;
    std::vector<float> pushBuffer;
    std::vector<float> chunkBuffer;
    std::atomic<std::uint32_t> dropped { 0 };
    std::atomic<float> gainLinear { 1.0f };
    std::atomic<float> smoothingAlpha { 1.0f };

    Fft fft;
    Hpss hpss;
    Cqt cqt;
    Fft cqtFft;

    std::vector<float> window;
    std::vector<float> fftInput;
    std::vector<float> fftReal;
    std::vector<float> fftImaginary;
    std::vector<float> cqtMagnitudes;
    std::vector<float> cqtHistory;
    std::vector<float> cqtWindow;
    std::vector<float> cqtInput;
    std::vector<float> cqtReal;
    std::vector<float> cqtImaginary;
    std::vector<float> magnitudes;

    std::array<float, kCqtBandCount> lastCqt {};
    std::array<float, kChromaCount> lastChromaCqt {};

    std::array<float, kSpectrumBandCount> smoothedSpectrum {};
    std::array<float, kSpectrumBandCount> smoothedHarmonic {};
    std::array<float, kSpectrumBandCount> smoothedPercussive {};
    std::array<float, kCqtBandCount> smoothedCqt {};
    std::array<float, kChromaCount> smoothedChroma {};
    std::array<float, kChromaCount> smoothedChromaCqt {};
    std::vector<float> harmonic;
    std::vector<float> percussive;
    std::vector<float> previousMagnitudes;
    std::vector<float> historyMono;
    std::vector<float> historyLeft;
    std::vector<float> historyRight;

    float windowScale = 1.0f;
    std::size_t historyPosition = 0;
    std::size_t cqtHistoryPosition = 0;
    std::size_t cqtHopCounter = 0;
    std::size_t hopCounter = 0;
    std::uint64_t samplesSeen = 0;
    std::uint64_t sequence = 0;

    float hopPeakLeft = 0.0f;
    float hopPeakRight = 0.0f;
    double hopSumSquaresLeft = 0.0;
    double hopSumSquaresRight = 0.0;

    std::array<int, kSpectrumBandCount> bandStart {};
    std::array<int, kSpectrumBandCount> bandEnd {};
    std::array<int, kBinCount> chromaBin {};

    mvt::core::TripleBuffer<AnalysisSnapshot> snapshot;
    std::jthread worker;
};

DspPipeline::DspPipeline() = default;
DspPipeline::~DspPipeline() = default;

void DspPipeline::prepare (double sampleRate, int maxBlockSize)
{
    stop();

    if (sampleRate <= 0.0 || ! std::isfinite (sampleRate))
        return;

    impl_ = std::make_unique<Impl> (sampleRate, maxBlockSize);
    impl_->setGainDecibels (analysisGainDecibels_.load (std::memory_order_relaxed));
    impl_->setSmoothingMilliseconds (smoothingMilliseconds_.load (std::memory_order_relaxed));
    impl_->start();
}

void DspPipeline::stop()
{
    impl_.reset();
}

void DspPipeline::pushAudio (const float* left, const float* right, int numSamples) noexcept
{
    if (impl_ == nullptr || left == nullptr || numSamples <= 0)
        return;

    impl_->push (left, right, numSamples);
}

bool DspPipeline::readSnapshot (AnalysisSnapshot& destination) noexcept
{
    if (impl_ == nullptr)
        return false;

    return impl_->snapshot.tryRead (destination);
}

void DspPipeline::setAnalysisGainDecibels (float decibels) noexcept
{
    analysisGainDecibels_.store (decibels, std::memory_order_relaxed);

    if (impl_ != nullptr)
        impl_->setGainDecibels (decibels);
}

void DspPipeline::setSmoothingMilliseconds (float milliseconds) noexcept
{
    smoothingMilliseconds_.store (milliseconds, std::memory_order_relaxed);

    if (impl_ != nullptr)
        impl_->setSmoothingMilliseconds (milliseconds);
}

double DspPipeline::sampleRate() const noexcept
{
    return impl_ != nullptr ? impl_->sampleRate : 0.0;
}

std::uint32_t DspPipeline::droppedFrames() const noexcept
{
    return impl_ != nullptr ? impl_->dropped.load (std::memory_order_relaxed) : 0;
}

bool DspPipeline::isRunning() const noexcept
{
    return impl_ != nullptr && impl_->worker.joinable();
}

} // namespace mvt::dsp
