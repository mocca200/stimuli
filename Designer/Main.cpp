#include "Designer/CanvasComponent.h"
#include "Designer/HierarchyPanel.h"
#include "Designer/HotReload.h"
#include "Designer/InspectorPanel.h"
#include "UIEngine/DesignerDocument.h"
#include "UIEngine/Fonts.h"
#include "UIEngine/RenderEngine.h"
#include "UIEngine/Skin.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <iostream>
#include <memory>

namespace mvt
{
class DesignerContent final : public juce::Component,
                             public juce::DragAndDropContainer,
                             private juce::Timer
{
public:
    DesignerContent();
    ~DesignerContent() override;

    void paint (juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureButton (juce::TextButton& button, const juce::String& text);

    void loadInitial();
    void chooseSkin();
    void saveSkin();
    void saveToFolder (const juce::File& folder);
    void reloadSkin (bool announce);
    void hotReloadTriggered();
    void documentChanged();
    void refreshAll();
    void refreshStatus();

    ui::NunitoLookAndFeel nunitoLookAndFeel;
    ui::Skin skin;
    ui::RenderEngine engine;
    ui::DesignerDocument document;
    ui::CanvasComponent canvas;
    ui::HierarchyPanel hierarchy;
    ui::InspectorPanel inspector;
    ui::HotReload hotReload;

    juce::TextButton openButton;
    juce::TextButton saveButton;
    juce::TextButton reloadButton;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton pauseButton;
    juce::Label titleLabel;
    juce::ComboBox viewportBox;
    juce::Label statusLabel;
    juce::Label safetyLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String message;
    std::uint64_t themeGeneration = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DesignerContent)
};

DesignerContent::DesignerContent()
{
    setLookAndFeel (&nunitoLookAndFeel);
    setOpaque (true);

    engine.setSkin (&skin);
    canvas.setSkin (&skin);
    canvas.setRenderEngine (&engine);
    canvas.setDocument (&document);
    hierarchy.setDocument (&document);
    inspector.setDocument (&document);
    inspector.setSnapshotSource ([this]() -> const dsp::AnalysisSnapshot&
    {
        return canvas.snapshot();
    });

    configureButton (openButton, "Open Skin...");
    configureButton (saveButton, "Save");
    configureButton (reloadButton, "Reload");
    configureButton (undoButton, "Undo");
    configureButton (redoButton, "Redo");
    configureButton (pauseButton, "Pause");

    openButton.onClick = [this] { chooseSkin(); };
    saveButton.onClick = [this] { saveSkin(); };
    reloadButton.onClick = [this] { reloadSkin (true); };
    undoButton.onClick = [this] { if (document.undo()) refreshAll(); };
    redoButton.onClick = [this] { if (document.redo()) refreshAll(); };
    pauseButton.onClick = [this]
    {
        canvas.setPaused (! canvas.isPaused());
        pauseButton.setButtonText (canvas.isPaused() ? "Resume" : "Pause");
        refreshStatus();
    };

    canvas.onSelectionChanged = [this]
    {
        hierarchy.refreshSelection();
        inspector.refresh();
        refreshStatus();
    };
    canvas.onDocumentChanged = [this] { documentChanged(); };

    hierarchy.onSelectionChanged = [this]
    {
        canvas.repaint();
        inspector.refresh();
        refreshStatus();
    };
    hierarchy.onDocumentChanged = [this] { documentChanged(); };
    hierarchy.onGroup = [this] { canvas.groupSelection(); };
    hierarchy.onDuplicate = [this] { canvas.duplicateSelection(); };
    hierarchy.onDelete = [this] { canvas.deleteSelection(); };

    inspector.onDocumentChanged = [this] { documentChanged(); };
    hotReload.onChanged = [this] { hotReloadTriggered(); };

    viewportBox.addItemList ({ "Viewport: fill", "1280 x 720", "1600 x 900", "1920 x 1080" }, 1);
    viewportBox.setSelectedItemIndex (0, juce::dontSendNotification);
    viewportBox.onChange = [this]
    {
        static constexpr layout::Size sizes[] { { 0.0f, 0.0f }, { 1280.0f, 720.0f },
                                                { 1600.0f, 900.0f }, { 1920.0f, 1080.0f } };
        const auto index = juce::jlimit (0, 3, viewportBox.getSelectedItemIndex());
        canvas.setDesignSize (sizes[index]);
        refreshStatus();
    };
    addAndMakeVisible (viewportBox);

    titleLabel.setText ("MVT Designer", juce::dontSendNotification);
    titleLabel.setFont (ui::fonts::bold (14.0f));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd9e2ec));
    addAndMakeVisible (titleLabel);

    statusLabel.setFont (ui::fonts::normal (12.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaeb7c2));
    addAndMakeVisible (statusLabel);

    safetyLabel.setFont (ui::fonts::normal (11.5f));
    safetyLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7f8b99));
    safetyLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (safetyLabel);

    addAndMakeVisible (hierarchy);
    addAndMakeVisible (canvas);
    addAndMakeVisible (inspector);

    loadInitial();
    startTimerHz (4);
    setSize (1280, 800);
}

DesignerContent::~DesignerContent()
{
    setLookAndFeel (nullptr);
}

void DesignerContent::configureButton (juce::TextButton& button, const juce::String& text)
{
    button.setButtonText (text);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b222c));
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd9e2ec));
    button.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff5ee7d5));
    addAndMakeVisible (button);
}

void DesignerContent::loadInitial()
{
    juce::String error;

    if (! skin.loadDefault (&error))
        message = "Default skin failed: " + error;
    else if (! document.loadFromSkin (skin))
        message = "Cannot open default skin: " + error;

    themeGeneration = document.theme().generation();
    refreshAll();
    canvas.prewarm (180);
}

void DesignerContent::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (0xff0d1117));

    graphics.setColour (juce::Colour (0xff232a34));
    graphics.drawHorizontalLine (43, 0.0f, static_cast<float> (getWidth()));
    graphics.drawHorizontalLine (getHeight() - 25, 0.0f, static_cast<float> (getWidth()));
}

void DesignerContent::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (44);
    auto status = area.removeFromBottom (24);

    int x = 10;

    const auto place = [&x] (juce::Component& component, int width)
    {
        component.setBounds (x, 10, width, 24);
        x += width + 6;
    };

    place (openButton, 96);
    place (saveButton, 58);
    place (reloadButton, 70);
    place (undoButton, 58);
    place (redoButton, 58);
    place (pauseButton, 66);

    viewportBox.setBounds (x + 6, 10, 132, 24);
    x += 140;

    titleLabel.setBounds (x + 10, 0, std::max (120, toolbar.getWidth() - x - 24),
                          toolbar.getHeight());

    safetyLabel.setBounds (status.removeFromRight (220).reduced (8, 3));
    statusLabel.setBounds (status.reduced (10, 3));

    hierarchy.setBounds (area.removeFromLeft (220));
    inspector.setBounds (area.removeFromRight (320));
    canvas.setBounds (area);
}

void DesignerContent::timerCallback()
{
    refreshStatus();
}

void DesignerContent::documentChanged()
{
    skin.setLayoutRoot (document.root());

    if (document.theme().generation() != themeGeneration)
    {
        juce::String error;

        if (skin.setThemeDocument (document.savedThemeDocument(), &error))
            themeGeneration = document.theme().generation();
        else
            message = "Theme update failed: " + error;
    }

    engine.invalidate();
    canvas.repaint();

    if (! canvas.isInteracting())
        hierarchy.refresh();

    undoButton.setEnabled (document.canUndo());
    redoButton.setEnabled (document.canRedo());
    refreshStatus();
}

void DesignerContent::refreshAll()
{
    skin.setLayoutRoot (document.root());

    if (document.theme().generation() != themeGeneration)
    {
        juce::String error;

        if (skin.setThemeDocument (document.savedThemeDocument(), &error))
            themeGeneration = document.theme().generation();
        else
            message = "Theme update failed: " + error;
    }

    engine.invalidate();
    canvas.invalidateLayout();
    hierarchy.refresh();
    inspector.refresh();

    undoButton.setEnabled (document.canUndo());
    redoButton.setEnabled (document.canRedo());

    if (skin.folder().isDirectory())
        hotReload.watch (skin.folder());
    else
        hotReload.stop();

    refreshStatus();
}

void DesignerContent::chooseSkin()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Open skin folder",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fileChooser)
                          {
                              const auto folder = fileChooser.getResult();

                              if (! folder.isDirectory())
                                  return;

                              juce::String error;

                              if (! skin.loadFromFolder (folder, &error))
                              {
                                  message = "Open failed: " + error;
                                  refreshStatus();
                                  return;
                              }

                              if (! document.loadFromSkin (skin))
                              {
                                  message = "Layout rejected";
                                  refreshStatus();
                                  return;
                              }

                              themeGeneration = document.theme().generation();
                              message = "Opened " + folder.getFileName();
                              refreshAll();
                          });
}

void DesignerContent::saveSkin()
{
    if (skin.folder().isDirectory())
    {
        saveToFolder (skin.folder());
        return;
    }

    chooser = std::make_unique<juce::FileChooser> (
        "Save skin into folder",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fileChooser)
                          {
                              const auto folder = fileChooser.getResult();

                              if (folder.isDirectory())
                                  saveToFolder (folder);
                          });
}

void DesignerContent::saveToFolder (const juce::File& folder)
{
    juce::String error;

    if (! document.save (folder, &error))
    {
        message = "Save failed: " + error;
        refreshStatus();
        return;
    }

    skin.setLayoutRoot (document.root());

    if (document.theme().generation() != themeGeneration)
    {
        if (skin.setThemeDocument (document.savedThemeDocument()))
            themeGeneration = document.theme().generation();
    }

    message = "Saved to " + folder.getFullPathName();
    undoButton.setEnabled (document.canUndo());
    redoButton.setEnabled (document.canRedo());
    refreshStatus();
}

void DesignerContent::reloadSkin (bool announce)
{
    if (! skin.folder().isDirectory())
    {
        message = "Reload skipped: no skin folder";
        refreshStatus();
        return;
    }

    const auto folder = skin.folder();
    juce::String error;

    if (! skin.loadFromFolder (folder, &error))
    {
        message = "Reload failed: " + error;
        refreshStatus();
        return;
    }

    if (! document.loadFromSkin (skin))
    {
        message = "Reload failed: layout rejected";
        refreshStatus();
        return;
    }

    themeGeneration = document.theme().generation();
    message = announce ? "Reloaded " + folder.getFileName() : juce::String();
    refreshAll();
}

void DesignerContent::hotReloadTriggered()
{
    if (document.isDirty())
    {
        message = "Hot reload skipped: unsaved edits";
        refreshStatus();
        return;
    }

    reloadSkin (false);
    message = "Hot reloaded from disk";
    refreshStatus();
}

void DesignerContent::refreshStatus()
{
    const auto& report = canvas.safetyGate().lastReport();

    auto text = juce::String ("Skin: ") + (skin.name().isEmpty() ? juce::String ("Fallback")
                                                                : skin.name());
    text << "   Selection: " << juce::String (document.selectedId());

    if (document.isDirty())
        text << "   * unsaved";

    if (message.isNotEmpty())
        text << "   " << message;

    statusLabel.setText (text, juce::dontSendNotification);
    safetyLabel.setText (report.attenuated ? "SAFETY: attenuated" : "SAFETY: clear",
                         juce::dontSendNotification);
}

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, bool showWindow)
        : juce::DocumentWindow (name, juce::Colour (0xff0b0d10), juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new DesignerContent(), true);
        setResizable (true, true);
        setResizeLimits (1000, 620, 3840, 2160);
        centreWithSize (1280, 800);

        if (showWindow)
            setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

class DesignerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MVT Designer"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::StringArray arguments;
        arguments.addTokens (commandLine, true);

        const auto snapshotIndex = arguments.indexOf ("--snapshot");
        const auto snapshotPath = snapshotIndex >= 0 && snapshotIndex + 1 < arguments.size()
                                    ? arguments[snapshotIndex + 1].unquoted()
                                    : juce::String();

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), snapshotPath.isEmpty());

        if (snapshotPath.isNotEmpty())
        {
            juce::Timer::callAfterDelay (400, [this, snapshotPath]
            {
                if (auto* content = mainWindow != nullptr ? mainWindow->getContentComponent() : nullptr)
                {
                    const auto image = content->createComponentSnapshot (content->getLocalBounds());
                    const auto file = juce::File::getCurrentWorkingDirectory()
                                          .getChildFile (snapshotPath);

                    file.deleteFile();

                    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
                    {
                        juce::PNGImageFormat png;

                        if (png.writeImageToStream (image, *stream))
                            std::cout << file.getFullPathName().toStdString() << '\n';
                    }
                }

                quit();
            });
        }
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace mvt

START_JUCE_APPLICATION (mvt::DesignerApplication)
