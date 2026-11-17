#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace mvt
{
PluginEditor::PluginEditor (AudioProcessor& processorIn)
    : juce::AudioProcessorEditor (processorIn)
    , processor (processorIn)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (720, 420, 3840, 2160);

    if (! skin.loadDefault (&loadError))
    {
        DBG ("Skin load failed: " + loadError);
    }

    renderEngine.setSkin (&skin);
    setSize (1120, 680);
    startTimerHz (60);
}

void PluginEditor::paint (juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds();
    auto& frame = safetyGate.beginFrame (bounds.getWidth(), bounds.getHeight());

    {
        juce::Graphics frameGraphics (frame);
        renderEngine.render (frameGraphics, bounds, snapshot);
    }

    const auto& presented = safetyGate.endFrame (juce::Time::getMillisecondCounterHiRes() * 0.001);
    graphics.drawImageAt (presented, 0, 0);
}

void PluginEditor::resized()
{
    renderEngine.invalidate();
    safetyGate.reset();
}

void PluginEditor::timerCallback()
{
    if (processor.readSnapshot (snapshot))
    {
        renderEngine.tick (1.0 / 60.0);
        repaint();
    }
}

} // namespace mvt
