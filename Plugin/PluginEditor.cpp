#include "PluginEditor.h"
#include "PluginParameters.h"
#include "PluginProcessor.h"

namespace mvt
{
namespace
{
/** Maps the Flash Limit choice onto gate thresholds. Both settings are at or
    below the WCAG 2.3.1 three-flashes-per-second ceiling: there is no mode that
    lets the visualiser flash faster than the guideline allows. */
safety::FlashLimiterConfig limiterConfigFor (int flashLimitIndex) noexcept
{
    safety::FlashLimiterConfig config;

    if (flashLimitIndex == params::flashLimitIndex::strict)
    {
        // One flash per second: two opposing transitions in the window.
        config.maxTransitionsPerSecond = 2;
        config.luminanceThreshold = 0.08f;
        config.redThreshold = 0.08f;
    }

    return config;
}
} // namespace

PluginEditor::PluginEditor (AudioProcessor& processorIn)
    : juce::AudioProcessorEditor (processorIn)
    , processor (processorIn)
    , controlStrip (processorIn.state)
{
    setLookAndFeel (&nunitoLookAndFeel);

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (720, 420, 3840, 2160);

    renderEngine.setSkin (&skin);
    addAndMakeVisible (controlStrip);

    // Applies the restored Skin and Flash Limit values, including the skin load.
    syncParameters();

    setSize (1120, 680);
    startTimerHz (60);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& graphics)
{
    const auto bounds = visualisationBounds();
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
    controlStrip.setBounds (0, juce::jmax (0, getHeight() - ControlStrip::preferredHeight),
                           getWidth(), ControlStrip::preferredHeight);

    renderEngine.invalidate();
    safetyGate.reset();
}

void PluginEditor::timerCallback()
{
    syncParameters();

    if (processor.readSnapshot (snapshot))
    {
        renderEngine.tick (1.0 / 60.0);

        // Only the visualisation changes per frame; invalidating the bar too
        // would repaint four controls sixty times a second for nothing.
        repaint (visualisationBounds());
    }
}

juce::Rectangle<int> PluginEditor::visualisationBounds() const noexcept
{
    return getLocalBounds().withHeight (juce::jmax (1, getHeight() - ControlStrip::preferredHeight));
}

void PluginEditor::syncParameters()
{
    if (const auto* flashLimit = processor.state.getRawParameterValue (params::id::flashLimit))
    {
        const auto index = juce::roundToInt (flashLimit->load());

        if (index != currentFlashIndex)
        {
            currentFlashIndex = index;
            safetyGate.setConfig (limiterConfigFor (index));
            safetyGate.reset();
        }
    }

    if (const auto* skinParameter = processor.state.getRawParameterValue (params::id::skin))
    {
        const auto index = juce::roundToInt (skinParameter->load());

        if (index != currentSkinIndex)
            loadSkin (index);
    }
}

void PluginEditor::loadSkin (int skinIndex)
{
    currentSkinIndex = skinIndex;

    const auto name = skinIndex == params::skinIndex::showcase
                          ? juce::String (params::skinName::showcase)
                          : juce::String (params::skinName::standard);
    const auto folder = ui::Skin::folderForName (name);

    juce::String error;

    if (folder.isDirectory() && skin.loadFromFolder (folder, &error))
    {
        loadError.clear();
    }
    else
    {
        const auto failure = error.isNotEmpty() ? error : "Skin folder not found: " + name;

        if (skin.loadDefault (&error))
            DBG ("Skin '" + name + "' unavailable (" + failure + "), using the built-in skin");
        else
            loadError = failure + "; " + error;
    }

    // Kept for diagnostics. DBG compiles away in Release, so this is not a
    // bare unbraced if whose body could warn as empty there.
    if (loadError.isNotEmpty())
    {
        DBG ("Skin load failed: " + loadError);
    }

    renderEngine.setSkin (&skin);
    renderEngine.invalidate();
    safetyGate.reset();
    repaint();
}

} // namespace mvt
