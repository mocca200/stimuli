#include "Engine/Safety/FlashLimiter.h"
#include "UIEngine/FrameSafetyGate.h"

#include <catch2/catch_all.hpp>

#include <juce_graphics/juce_graphics.h>

#include <cmath>

using namespace mvt::safety;
using namespace mvt::ui;

namespace
{
FrameMetrics dark()
{
    return { 0.10f, 0.0f };
}

FrameMetrics bright()
{
    return { 0.40f, 0.0f };
}

juce::Image solidImage (juce::Colour colour, int width = 64, int height = 64)
{
    juce::Image image (juce::Image::ARGB, width, height, false, juce::SoftwareImageType());
    juce::Graphics graphics (image);
    graphics.fillAll (colour);
    return image;
}
} // namespace

TEST_CASE ("FlashLimiter ignores changes below the threshold")
{
    FlashLimiter limiter;
    limiter.reset (dark());

    for (int i = 0; i < 120; ++i)
    {
        auto metrics = bright();
        metrics.luminance = 0.19f; // 0.09 below/above the 0.10 threshold

        const auto decision = limiter.evaluate (static_cast<double> (i) * 0.016, metrics);

        REQUIRE_FALSE (decision.transition);
        REQUIRE_FALSE (decision.blocked);

        limiter.commit (static_cast<double> (i) * 0.016, metrics);
    }
}

TEST_CASE ("FlashLimiter blocks once the opposing transition budget is spent")
{
    FlashLimiter limiter;
    limiter.reset (dark());

    int allowedTransitions = 0;
    int blockedTransitions = 0;

    for (int i = 0; i < 24; ++i)
    {
        const auto metrics = i % 2 == 0 ? bright() : dark();
        const auto now = static_cast<double> (i) * 0.01;
        const auto decision = limiter.evaluate (now, metrics);

        if (decision.blocked)
        {
            ++blockedTransitions;
            continue;
        }

        if (decision.transition)
            ++allowedTransitions;

        limiter.commit (now, metrics);
    }

    // The first brightening is an initial transition, not an opposing one.
    // Six opposing transitions are allowed; the seventh is blocked.
    REQUIRE (allowedTransitions == 7);
    REQUIRE (blockedTransitions > 0);
}

TEST_CASE ("FlashLimiter treats saturated red flashes independently")
{
    FlashLimiter limiter;
    limiter.reset ({ 0.20f, 0.0f });

    int blocked = 0;

    for (int i = 0; i < 24; ++i)
    {
        auto metrics = FrameMetrics { 0.20f, 0.0f };

        if (i % 2 == 0)
            metrics.red = 0.60f;

        const auto now = static_cast<double> (i) * 0.01;
        const auto decision = limiter.evaluate (now, metrics);

        if (decision.blocked)
        {
            ++blocked;
            continue;
        }

        limiter.commit (now, metrics);
    }

    REQUIRE (blocked > 0);
}

TEST_CASE ("FlashLimiter forgets transitions outside the sliding window")
{
    FlashLimiter limiter;
    limiter.reset (dark());

    for (int i = 1; i <= 7; ++i)
    {
        const auto metrics = i % 2 == 1 ? bright() : dark();
        limiter.commit (static_cast<double> (i) * 0.05, metrics);
    }

    REQUIRE (limiter.transitionsInWindow (0.5) == 6);
    REQUIRE (limiter.transitionsInWindow (2.0) == 0);
}

TEST_CASE ("FrameSafetyGate measures linear luminance and red dominance")
{
    const auto black = FrameSafetyGate::measure (solidImage (juce::Colours::black));
    const auto white = FrameSafetyGate::measure (solidImage (juce::Colours::white));
    const auto red = FrameSafetyGate::measure (solidImage (juce::Colours::red));
    const auto green = FrameSafetyGate::measure (solidImage (juce::Colours::lime));

    REQUIRE (black.luminance == Catch::Approx (0.0f).margin (0.0001f));
    REQUIRE (black.red == Catch::Approx (0.0f).margin (0.0001f));

    REQUIRE (white.luminance == Catch::Approx (1.0f).margin (0.001f));
    REQUIRE (white.red == Catch::Approx (0.0f).margin (0.0001f));

    REQUIRE (red.luminance == Catch::Approx (0.2126f).margin (0.002f));
    REQUIRE (red.red == Catch::Approx (1.0f).margin (0.001f));

    REQUIRE (green.luminance == Catch::Approx (0.7152f).margin (0.002f));
    REQUIRE (green.red == Catch::Approx (0.0f).margin (0.0001f));
}

TEST_CASE ("FrameSafetyGate passes steady frames through unchanged")
{
    FrameSafetyGate gate;
    const auto input = solidImage (juce::Colour (0xff808080));
    const auto inputMetrics = FrameSafetyGate::measure (input);

    for (int i = 0; i < 30; ++i)
    {
        auto& frame = gate.beginFrame (input.getWidth(), input.getHeight());
        {
            juce::Graphics graphics (frame);
            graphics.drawImageAt (input, 0, 0);
        }

        const auto& presented = gate.endFrame (static_cast<double> (i) * 0.016);
        const auto report = gate.lastReport();

        REQUIRE_FALSE (report.attenuated);
        REQUIRE_FALSE (report.decision.blocked);
        REQUIRE (FrameSafetyGate::measure (presented).luminance
                 == Catch::Approx (inputMetrics.luminance).margin (0.001f));
    }
}

TEST_CASE ("FrameSafetyGate attenuates frames that would exceed the flash budget")
{
    FrameSafetyGate gate;
    int attenuated = 0;

    for (int i = 0; i < 40; ++i)
    {
        auto& frame = gate.beginFrame (64, 64);
        {
            juce::Graphics graphics (frame);
            graphics.fillAll (i % 2 == 0 ? juce::Colours::white : juce::Colours::black);
        }

        const auto& presented = gate.endFrame (static_cast<double> (i) * 0.01);
        const auto report = gate.lastReport();
        const auto reference = gate.limiter().reference();

        if (report.attenuated)
        {
            ++attenuated;

            REQUIRE (std::abs (report.presented.luminance - reference.luminance) < 0.10f);
            REQUIRE (std::abs (report.presented.red - reference.red) < 0.10f);
            REQUIRE (FrameSafetyGate::measure (presented).luminance
                     == Catch::Approx (report.presented.luminance).margin (0.001f));
        }
    }

    REQUIRE (attenuated > 0);
}

TEST_CASE ("FrameSafetyGate resets its reference when the frame size changes")
{
    FrameSafetyGate gate;

    auto& first = gate.beginFrame (64, 64);
    {
        juce::Graphics graphics (first);
        graphics.fillAll (juce::Colours::white);
    }
    gate.endFrame (0.0);

    auto& second = gate.beginFrame (128, 96);

    REQUIRE (second.getWidth() == 128);
    REQUIRE (second.getHeight() == 96);
    REQUIRE (gate.limiter().transitionsInWindow (0.0) == 0);
}
