#include "Designer/HotReload.h"

namespace mvt::ui
{
void HotReload::watch (const juce::File& folder)
{
    folder_ = folder;
    primed_ = false;
    layoutStamp_ = stampOf (folder_.getChildFile ("layout.json"));
    themeStamp_ = stampOf (folder_.getChildFile ("theme.json"));
    manifestStamp_ = stampOf (folder_.getChildFile ("manifest.json"));
    primed_ = true;

    if (folder_.isDirectory())
        startTimer (400);
    else
        stopTimer();
}

void HotReload::stop()
{
    stopTimer();
    folder_ = juce::File {};
    primed_ = false;
}

juce::int64 HotReload::stampOf (const juce::File& file)
{
    return file.existsAsFile() ? file.getLastModificationTime().toMilliseconds() : 0;
}

void HotReload::timerCallback()
{
    if (! folder_.isDirectory())
    {
        stopTimer();
        return;
    }

    const auto layout = stampOf (folder_.getChildFile ("layout.json"));
    const auto theme = stampOf (folder_.getChildFile ("theme.json"));
    const auto manifest = stampOf (folder_.getChildFile ("manifest.json"));

    if (layout == layoutStamp_ && theme == themeStamp_ && manifest == manifestStamp_)
        return;

    layoutStamp_ = layout;
    themeStamp_ = theme;
    manifestStamp_ = manifest;

    if (primed_ && onChanged != nullptr)
        onChanged();
}

} // namespace mvt::ui
