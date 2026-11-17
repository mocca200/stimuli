#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>

namespace mvt::ui
{
/** Polls a skin folder for layout/theme/manifest edits.

    JUCE 9 no longer ships juce::FileWatcher, so this uses a 400 ms timer and
    modification stamps. It is message-thread only and calls onChanged at most
    once per detected change. */
class HotReload final : private juce::Timer
{
public:
    HotReload() = default;
    ~HotReload() override = default;

    void watch (const juce::File& folder);
    void stop();

    [[nodiscard]] bool isWatching() const noexcept { return folder_.isDirectory(); }

    std::function<void()> onChanged;

private:
    void timerCallback() override;
    [[nodiscard]] static juce::int64 stampOf (const juce::File& file);

    juce::File folder_;
    juce::int64 layoutStamp_ = 0;
    juce::int64 themeStamp_ = 0;
    juce::int64 manifestStamp_ = 0;
    bool primed_ = false;
};
} // namespace mvt::ui
