#include "Engine/DSP/Fft.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace mvt::dsp
{
Fft::Fft (int order)
    : order_ (order)
{
    if (order_ < 4 || order_ > 16)
        throw std::invalid_argument ("Fft order must be between 4 and 16");

    size_ = 1 << order_;
    const auto size = static_cast<std::size_t> (size_);
    const auto half = size / 2;

    cosTable_.resize (half);
    sinTable_.resize (half);
    bitReversed_.resize (size);
    real_.resize (size);
    imaginary_.resize (size);

    constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;

    for (std::size_t k = 0; k < half; ++k)
    {
        const auto angle = twoPi * static_cast<float> (k) / static_cast<float> (size);
        cosTable_[k] = std::cos (angle);
        sinTable_[k] = std::sin (angle);
    }

    auto bits = 0;
    for (auto value = static_cast<std::uint32_t> (size); value > 1; value >>= 1)
        ++bits;

    for (std::size_t i = 0; i < size; ++i)
    {
        std::uint32_t reversed = 0;
        const auto index = static_cast<std::uint32_t> (i);

        for (auto bit = 0; bit < bits; ++bit)
            if ((index & (1u << bit)) != 0)
                reversed |= 1u << (bits - 1 - bit);

        bitReversed_[i] = reversed;
    }
}

void Fft::processReal (const float* input, float* magnitudeOutput) noexcept
{
    const auto size = static_cast<std::size_t> (size_);

    for (std::size_t i = 0; i < size; ++i)
    {
        real_[bitReversed_[i]] = input[i];
        imaginary_[bitReversed_[i]] = 0.0f;
    }

    for (auto length = 2; length <= size_; length <<= 1)
    {
        const auto halfLength = length >> 1;
        const auto step = size_ / length;

        for (auto block = 0; block < size_; block += length)
        {
            for (auto j = 0; j < halfLength; ++j)
            {
                const auto twiddle = static_cast<std::size_t> (j * step);
                const auto cosine = cosTable_[twiddle];
                const auto sine = sinTable_[twiddle];

                const auto a = static_cast<std::size_t> (block + j);
                const auto b = a + static_cast<std::size_t> (halfLength);

                // e^{-i 2 pi j / length}
                const auto tReal = real_[b] * cosine + imaginary_[b] * sine;
                const auto tImaginary = imaginary_[b] * cosine - real_[b] * sine;

                real_[b] = real_[a] - tReal;
                imaginary_[b] = imaginary_[a] - tImaginary;
                real_[a] += tReal;
                imaginary_[a] += tImaginary;
            }
        }
    }

    for (std::size_t i = 0; i < size; ++i)
        magnitudeOutput[i] = std::sqrt (real_[i] * real_[i] + imaginary_[i] * imaginary_[i]);
}

} // namespace mvt::dsp
