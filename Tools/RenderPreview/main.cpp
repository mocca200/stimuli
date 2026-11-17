#include "UIEngine/DemoSignal.h"
#include "UIEngine/FrameSafetyGate.h"
#include "UIEngine/RenderEngine.h"
#include "UIEngine/Skin.h"
#include "UIEngine/Views/SpectrogramView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace
{
void printUsage()
{
    std::cerr << "usage: mvt_render_preview [--skin folder] [--out preview.png] "
                 "[--width 1280] [--height 720] [--time 1.5]\n";
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;

    std::string skinPath;
    std::string outputPath = "preview.png";
    int width = 1280;
    int height = 720;
    double time = 1.5;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument = argv[i];

        auto next = [&] (const char* name) -> std::string
        {
            if (argument == name && i + 1 < argc)
                return argv[++i];

            return {};
        };

        if (const auto skinValue = next ("--skin"); ! skinValue.empty())
            skinPath = skinValue;
        else if (const auto outValue = next ("--out"); ! outValue.empty())
            outputPath = outValue;
        else if (const auto widthValue = next ("--width"); ! widthValue.empty())
            width = std::max (64, std::stoi (widthValue));
        else if (const auto heightValue = next ("--height"); ! heightValue.empty())
            height = std::max (64, std::stoi (heightValue));
        else if (const auto timeValue = next ("--time"); ! timeValue.empty())
            time = std::max (0.0, std::stod (timeValue));
        else
        {
            printUsage();
            return 2;
        }
    }

    mvt::ui::Skin skin;
    juce::String error;

    const auto loaded = skinPath.empty()
                          ? skin.loadDefault (&error)
                          : skin.loadFromFolder (juce::File (juce::String (skinPath)), &error);

    if (! loaded)
    {
        std::cerr << "skin load failed: " << error.toStdString() << '\n';
        return 1;
    }

    mvt::ui::RenderEngine renderEngine;
    renderEngine.setSkin (&skin);

    mvt::ui::DemoSignal demo;
    demo.reset();

    const juce::Rectangle<int> canvas { 0, 0, width, height };

    // History-based views (the spectrogram) need several frames before they
    // have anything to show, so fill the whole history window before painting.
    constexpr auto warmupFrames = mvt::ui::SpectrogramView::kHistoryColumns;
    const auto step = time > 0.0 ? time / static_cast<double> (warmupFrames) : 0.0;

    mvt::dsp::AnalysisSnapshot snapshot;

    for (int frame = 1; frame < warmupFrames; ++frame)
    {
        demo.advance (step, snapshot);
        renderEngine.update (canvas, snapshot);
        renderEngine.tick (step);
    }

    demo.advance (step, snapshot);
    renderEngine.tick (step);

    mvt::ui::FrameSafetyGate safetyGate;
    auto& frame = safetyGate.beginFrame (width, height);

    {
        juce::Graphics graphics (frame);
        renderEngine.render (graphics, canvas, snapshot);
    }

    const auto& image = safetyGate.endFrame (time);

    const auto output = juce::File::getCurrentWorkingDirectory()
                            .getChildFile (juce::String (outputPath));

    output.deleteFile();

    auto stream = std::unique_ptr<juce::FileOutputStream> (output.createOutputStream());

    if (stream == nullptr || ! stream->openedOk())
    {
        std::cerr << "cannot write: " << output.getFullPathName().toStdString() << '\n';
        return 1;
    }

    juce::PNGImageFormat png;

    if (! png.writeImageToStream (image, *stream))
    {
        std::cerr << "PNG encoding failed\n";
        return 1;
    }

    stream.reset();

    std::cout << output.getFullPathName().toStdString() << '\n';
    return 0;
}
