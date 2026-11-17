#pragma once

#include "UIEngine/VisualizerView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mvt::ui
{
/** Deterministic 3D particle cloud. This is the CPU fallback renderer for the
    particle field; the OpenGL path will share the same simulation state. */
class ParticleFieldView final : public VisualizerView
{
public:
    static constexpr std::size_t kParticleCount = 640;

    void update (const dsp::AnalysisSnapshot& snapshot) override
    {
        if (! snapshot.valid)
            return;

        bass_ = std::clamp (snapshot.lowEnergy, 0.0f, 1.0f);
        energy_ = std::clamp (0.35f * std::sqrt (std::max (0.0f, snapshot.lowEnergy))
                                + 0.45f * std::sqrt (std::max (0.0f, snapshot.midEnergy))
                                + 0.20f * std::sqrt (std::max (0.0f, snapshot.highEnergy)),
                              0.0f, 1.0f);
    }

    void tick (double deltaSeconds) override
    {
        const auto dt = static_cast<float> (std::clamp (deltaSeconds, 0.0, 0.1));
        angle_ += dt * (0.18f + 0.55f * energy_);

        if (angle_ >= juce::MathConstants<float>::twoPi)
            angle_ -= juce::MathConstants<float>::twoPi;
    }

    void paint (juce::Graphics& graphics,
                juce::Rectangle<float> bounds,
                const dsp::AnalysisSnapshot& snapshot,
                const Palette& palette) const override
    {
        juce::ignoreUnused (snapshot);

        graphics.setColour (palette.surfaceRaised.withAlpha (0.9f));
        graphics.fillRoundedRectangle (bounds, 10.0f);
        graphics.setColour (palette.border.withAlpha (0.7f));
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        const auto inner = bounds.reduced (12.0f, 10.0f);

        if (inner.getWidth() <= 1.0f || inner.getHeight() <= 1.0f)
            return;

        const auto centre = inner.getCentre();
        const auto scale = std::min (inner.getWidth(), inner.getHeight()) * 0.40f;
        const auto spread = 1.0f + 0.30f * bass_;
        const auto cosYaw = std::cos (angle_);
        const auto sinYaw = std::sin (angle_);
        const auto cosTilt = std::cos (0.42f);
        const auto sinTilt = std::sin (0.42f);

        for (const auto& particle : particles_)
        {
            const auto yawX = particle.x * cosYaw - particle.z * sinYaw;
            const auto yawZ = particle.x * sinYaw + particle.z * cosYaw;
            const auto tiltY = particle.y * cosTilt - yawZ * sinTilt;
            const auto depth = 1.85f + yawZ * 0.62f;
            const auto perspective = 1.0f / depth;

            const auto x = centre.x + yawX * scale * perspective * spread * 1.7f;
            const auto y = centre.y + tiltY * scale * perspective * spread * 1.7f;

            if (x < inner.getX() || x > inner.getRight()
                || y < inner.getY() || y > inner.getBottom())
            {
                continue;
            }

            const auto radius = 0.6f + 2.4f * perspective;
            const auto alpha = std::clamp (0.12f + 0.62f * perspective + 0.22f * energy_,
                                          0.0f, 0.95f);
            const auto colour = palette.accent.interpolatedWith (
                palette.harmonic, particle.hue * 0.85f + 0.15f * energy_);

            graphics.setColour (colour.withAlpha (alpha));
            graphics.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                                      .withCentre ({ x, y }));
        }

        graphics.setColour (palette.textMuted.withAlpha (0.65f));
        graphics.setFont (palette.monoFont);
        graphics.drawText ("PARTICLE FIELD", inner.toNearestInt().removeFromTop (18),
                           juce::Justification::centredLeft);
    }

private:
    struct Particle
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float hue = 0.0f;
    };

    static std::array<Particle, kParticleCount> makeParticles()
    {
        std::array<Particle, kParticleCount> result {};
        std::uint32_t state = 0x9e3779b9u;

        const auto next = [&state]
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<float> ((state >> 8) & 0x00ffffffu) / 16777215.0f;
        };

        for (auto& particle : result)
        {
            const auto theta = next() * juce::MathConstants<float>::twoPi;
            const auto radius = std::sqrt (next()) * 0.9f;

            particle.x = std::cos (theta) * radius;
            particle.z = std::sin (theta) * radius;
            particle.y = (next() * 2.0f - 1.0f) * 0.55f
                       + 0.18f * std::sin (theta * 3.0f);
            particle.hue = next();
        }

        return result;
    }

    std::array<Particle, kParticleCount> particles_ = makeParticles();
    float energy_ = 0.0f;
    float bass_ = 0.0f;
    float angle_ = 0.0f;
};
} // namespace mvt::ui
