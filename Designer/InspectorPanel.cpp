#include "Designer/InspectorPanel.h"

#include "UIEngine/DesignerDocument.h"
#include "UIEngine/Fonts.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace mvt::ui
{
namespace
{
struct ParsedLength
{
    bool valid = true;
    std::optional<layout::Length> length;
};

ParsedLength parseLengthText (const juce::String& raw)
{
    const auto text = raw.trim();

    if (text.isEmpty() || text.equalsIgnoreCase ("auto"))
        return { true, std::nullopt };

    if (text.endsWithChar ('%'))
    {
        const auto numberText = text.dropLastCharacters (1).trim();

        if (! numberText.containsAnyOf ("0123456789"))
            return { false, std::nullopt };

        const auto number = numberText.getFloatValue();

        if (! std::isfinite (number) || number < 0.0)
            return { false, std::nullopt };

        return { true, layout::Length::percent (number) };
    }

    auto numberText = text;

    if (numberText.endsWithIgnoreCase ("px"))
        numberText = numberText.dropLastCharacters (2).trim();

    if (! numberText.containsAnyOf ("0123456789"))
        return { false, std::nullopt };

    const auto number = numberText.getFloatValue();

    if (! std::isfinite (number) || number < 0.0)
        return { false, std::nullopt };

    return { true, layout::Length::px (number) };
}

juce::String lengthText (const layout::Length& length)
{
    switch (length.kind)
    {
        case layout::LengthKind::pixels:
            return juce::String (length.value,
                                 std::abs (length.value - std::round (length.value)) < 0.005f ? 0 : 2);

        case layout::LengthKind::percent:
            return juce::String (length.value, 2) + "%";

        case layout::LengthKind::autoValue:
            break;
    }

    return "auto";
}

juce::String optionalLengthText (const std::optional<layout::Length>& length)
{
    return length.has_value() ? lengthText (*length) : juce::String ("auto");
}

juce::String optionalNumberText (const std::optional<float>& value)
{
    if (! value.has_value())
        return "auto";

    return juce::String (*value, std::abs (*value - std::round (*value)) < 0.0005f ? 0 : 2);
}
} // namespace

class InspectorPanel::Row
{
public:
    juce::Label caption;
    std::unique_ptr<juce::Component> editor;
    juce::TextEditor* text = nullptr;
    juce::ComboBox* combo = nullptr;
    juce::String key;
    bool section = false;
};

InspectorPanel::InspectorPanel()
{
    setOpaque (true);

    header_.setText ("Inspector", juce::dontSendNotification);
    header_.setFont (fonts::bold (13.0f));
    header_.setColour (juce::Label::textColourId, juce::Colour (0xffd9e2ec));
    addAndMakeVisible (header_);

    viewport_.setViewedComponent (&content_, false);
    viewport_.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport_);

    tokenCaption_.setText ("token", juce::dontSendNotification);
    tokenCaption_.setFont (fonts::normal (12.0f));
    tokenCaption_.setColour (juce::Label::textColourId, juce::Colour (0xff8b98a8));
    tokenCaption_.setJustificationType (juce::Justification::centredRight);
    content_.addAndMakeVisible (tokenCaption_);

    tokenBox_.setEditableText (false);
    tokenBox_.onChange = [this]
    {
        selectedToken_ = tokenBox_.getText().toStdString();
        updateTokenPreview();
    };
    content_.addAndMakeVisible (tokenBox_);

    tokenValueLabel_.setFont (fonts::normal (11.0f));
    tokenValueLabel_.setColour (juce::Label::textColourId, juce::Colour (0xff7f8b99));
    content_.addAndMakeVisible (tokenValueLabel_);

    tokenHexEditor_.setFont (fonts::normal (12.0f));
    tokenHexEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141a24));
    tokenHexEditor_.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6edf6));
    tokenHexEditor_.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff283244));
    tokenHexEditor_.onReturnKey = [this] { applyTokenHex(); };
    content_.addAndMakeVisible (tokenHexEditor_);

    clearTokenButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b222c));
    clearTokenButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd9e2ec));
    clearTokenButton_.onClick = [this]
    {
        if (document_ != nullptr && ! selectedToken_.empty()
            && document_->clearThemeOverride (selectedToken_))
        {
            updateTokenPreview();
            notifyChanged();
        }
    };
    content_.addAndMakeVisible (clearTokenButton_);

    readout_.setFont (fonts::normal (11.0f));
    readout_.setColour (juce::Label::textColourId, juce::Colour (0xff93c5e8));
    readout_.setJustificationType (juce::Justification::topLeft);
    content_.addAndMakeVisible (readout_);

    buildRows();
    startTimerHz (10);
}

InspectorPanel::~InspectorPanel()
{
    content_.removeAllChildren();
}

void InspectorPanel::setDocument (DesignerDocument* document)
{
    document_ = document;
    refresh();
}

void InspectorPanel::setSnapshotSource (std::function<const dsp::AnalysisSnapshot&()> source)
{
    snapshotSource_ = std::move (source);
}

void InspectorPanel::addSection (const juce::String& title)
{
    auto row = std::make_unique<Row>();
    row->section = true;
    row->key = juce::String {};
    row->caption.setText (title, juce::dontSendNotification);
    row->caption.setFont (fonts::bold (11.5f));
    row->caption.setColour (juce::Label::textColourId, juce::Colour (0xff5ee7ff));
    row->caption.setJustificationType (juce::Justification::centredLeft);
    content_.addAndMakeVisible (row->caption);
    rows_.push_back (std::move (row));
}

juce::TextEditor& InspectorPanel::addTextRow (const juce::String& caption, const juce::String& key)
{
    auto row = std::make_unique<Row>();
    row->key = key;

    row->caption.setText (caption, juce::dontSendNotification);
    row->caption.setFont (fonts::normal (12.0f));
    row->caption.setColour (juce::Label::textColourId, juce::Colour (0xff8b98a8));
    row->caption.setJustificationType (juce::Justification::centredRight);
    content_.addAndMakeVisible (row->caption);

    auto editor = std::make_unique<juce::TextEditor>();
    editor->setFont (fonts::normal (12.0f));
    editor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141a24));
    editor->setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6edf6));
    editor->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff283244));
    editor->setSelectAllWhenFocused (true);
    editor->onReturnKey = [this, key] { commitText (key); };
    editor->onFocusLost = [this, key]
    {
        if (! building_)
            commitText (key);
    };

    auto& reference = *editor;
    content_.addAndMakeVisible (reference);
    row->text = &reference;
    row->editor = std::move (editor);
    rows_.push_back (std::move (row));
    return reference;
}

juce::ComboBox& InspectorPanel::addComboRow (const juce::String& caption,
                                             const juce::String& key,
                                             const juce::StringArray& items)
{
    auto row = std::make_unique<Row>();
    row->key = key;

    row->caption.setText (caption, juce::dontSendNotification);
    row->caption.setFont (fonts::normal (12.0f));
    row->caption.setColour (juce::Label::textColourId, juce::Colour (0xff8b98a8));
    row->caption.setJustificationType (juce::Justification::centredRight);
    content_.addAndMakeVisible (row->caption);

    auto combo = std::make_unique<juce::ComboBox>();
    combo->addItemList (items, 1);
    combo->onChange = [this, key]
    {
        if (! building_)
        {
            if (auto* box = comboFor (key))
                applyCombo (key, box->getSelectedItemIndex());
        }
    };

    auto& reference = *combo;
    content_.addAndMakeVisible (reference);
    row->combo = &reference;
    row->editor = std::move (combo);
    rows_.push_back (std::move (row));
    return reference;
}

void InspectorPanel::buildRows()
{
    building_ = true;

    addSection ("Node");
    addTextRow ("id", "id");
    addTextRow ("type", "type");
    addTextRow ("text", "text");

    addSection ("Geometry");
    addTextRow ("width", "width");
    addTextRow ("height", "height");
    addTextRow ("min width", "minWidth");
    addTextRow ("min height", "minHeight");
    addTextRow ("max width", "maxWidth");
    addTextRow ("max height", "maxHeight");
    addTextRow ("flex basis", "flexBasis");
    addTextRow ("gap", "gap");

    addSection ("Margin");
    addTextRow ("top", "margin.top");
    addTextRow ("right", "margin.right");
    addTextRow ("bottom", "margin.bottom");
    addTextRow ("left", "margin.left");

    addSection ("Padding");
    addTextRow ("top", "padding.top");
    addTextRow ("right", "padding.right");
    addTextRow ("bottom", "padding.bottom");
    addTextRow ("left", "padding.left");

    addSection ("Inset (absolute)");
    addTextRow ("top", "inset.top");
    addTextRow ("right", "inset.right");
    addTextRow ("bottom", "inset.bottom");
    addTextRow ("left", "inset.left");

    addSection ("Flex");
    addTextRow ("grow", "flexGrow");
    addTextRow ("shrink", "flexShrink");
    addTextRow ("aspect", "aspectRatio");

    addSection ("Flow");
    addComboRow ("direction", "direction", { "row", "column" });
    addComboRow ("justify", "justifyContent",
                 { "start", "center", "end", "space-between", "space-around", "space-evenly" });
    addComboRow ("align", "alignItems", { "start", "center", "end", "stretch" });
    addComboRow ("align self", "alignSelf", { "(inherit)", "start", "center", "end", "stretch" });
    addComboRow ("position", "position", { "relative", "absolute" });
    addComboRow ("display", "display", { "flex", "none" });

    addSection ("Theme token");

    building_ = false;
}

void InspectorPanel::refresh()
{
    refreshTokens();
    populate();
    refreshReadout();
    lastRevision_ = document_ != nullptr ? document_->revision() : 0;
    resized();
}

void InspectorPanel::timerCallback()
{
    refreshReadout();

    if (document_ == nullptr || building_ || isEditing())
        return;

    if (document_->revision() != lastRevision_)
    {
        populate();
        refreshTokens();
        lastRevision_ = document_->revision();
    }
}

bool InspectorPanel::isEditing() const
{
    for (const auto& row : rows_)
        if (row->text != nullptr && row->text->hasKeyboardFocus (true))
            return true;

    return tokenHexEditor_.hasKeyboardFocus (true);
}

juce::TextEditor* InspectorPanel::textFor (const juce::String& key) const
{
    for (const auto& row : rows_)
        if (row->key == key)
            return row->text;

    return nullptr;
}

juce::ComboBox* InspectorPanel::comboFor (const juce::String& key) const
{
    for (const auto& row : rows_)
        if (row->key == key)
            return row->combo;

    return nullptr;
}

void InspectorPanel::populate()
{
    if (document_ == nullptr)
        return;

    const auto* node = document_->find (document_->selectedId());

    if (node == nullptr)
        return;

    const juce::ScopedValueSetter<bool> guard (building_, true);

    if (auto* editor = textFor ("id"))       editor->setText (node->id, false);
    if (auto* editor = textFor ("type"))     editor->setText (node->type, false);
    if (auto* editor = textFor ("text"))     editor->setText (node->text, false);

    if (auto* editor = textFor ("width"))    editor->setText (optionalLengthText (node->style.width), false);
    if (auto* editor = textFor ("height"))   editor->setText (optionalLengthText (node->style.height), false);
    if (auto* editor = textFor ("minWidth")) editor->setText (optionalLengthText (node->style.minWidth), false);
    if (auto* editor = textFor ("minHeight")) editor->setText (optionalLengthText (node->style.minHeight), false);
    if (auto* editor = textFor ("maxWidth")) editor->setText (optionalLengthText (node->style.maxWidth), false);
    if (auto* editor = textFor ("maxHeight")) editor->setText (optionalLengthText (node->style.maxHeight), false);
    if (auto* editor = textFor ("flexBasis")) editor->setText (optionalLengthText (node->style.flexBasis), false);
    if (auto* editor = textFor ("gap"))      editor->setText (lengthText (node->style.gap), false);

    const auto setEdge = [this] (const juce::String& key, const layout::Length& length)
    {
        if (auto* editor = textFor (key))
            editor->setText (lengthText (length), false);
    };

    setEdge ("margin.top", node->style.margin.top);
    setEdge ("margin.right", node->style.margin.right);
    setEdge ("margin.bottom", node->style.margin.bottom);
    setEdge ("margin.left", node->style.margin.left);

    setEdge ("padding.top", node->style.padding.top);
    setEdge ("padding.right", node->style.padding.right);
    setEdge ("padding.bottom", node->style.padding.bottom);
    setEdge ("padding.left", node->style.padding.left);

    setEdge ("inset.top", node->style.inset.top);
    setEdge ("inset.right", node->style.inset.right);
    setEdge ("inset.bottom", node->style.inset.bottom);
    setEdge ("inset.left", node->style.inset.left);

    if (auto* editor = textFor ("flexGrow"))
        editor->setText (juce::String (node->style.flexGrow, 2), false);
    if (auto* editor = textFor ("flexShrink"))
        editor->setText (juce::String (node->style.flexShrink, 2), false);
    if (auto* editor = textFor ("aspectRatio"))
        editor->setText (optionalNumberText (node->style.aspectRatio), false);

    if (auto* box = comboFor ("direction"))
        box->setSelectedItemIndex (node->style.direction == layout::Direction::row ? 0 : 1,
                                   juce::dontSendNotification);

    if (auto* box = comboFor ("justifyContent"))
        box->setSelectedItemIndex (static_cast<int> (node->style.justify), juce::dontSendNotification);

    if (auto* box = comboFor ("alignItems"))
        box->setSelectedItemIndex (static_cast<int> (node->style.align), juce::dontSendNotification);

    if (auto* box = comboFor ("alignSelf"))
        box->setSelectedItemIndex (node->style.alignSelf.has_value()
                                     ? static_cast<int> (*node->style.alignSelf) + 1
                                     : 0,
                                   juce::dontSendNotification);

    if (auto* box = comboFor ("position"))
        box->setSelectedItemIndex (node->style.position == layout::Position::absolute ? 1 : 0,
                                   juce::dontSendNotification);

    if (auto* box = comboFor ("display"))
        box->setSelectedItemIndex (node->style.display == layout::Display::none ? 1 : 0,
                                   juce::dontSendNotification);
}

void InspectorPanel::commitText (const juce::String& key)
{
    if (building_)
        return;

    if (auto* editor = textFor (key))
        applyText (key, editor->getText());
}

void InspectorPanel::applyText (const juce::String& key, const juce::String& text)
{
    if (document_ == nullptr)
        return;

    const auto id = document_->selectedId();
    const auto* node = document_->find (id);

    if (node == nullptr)
        return;

    bool changed = false;

    if (key == "id")
    {
        changed = document_->renameId (id, text.trim().toStdString());
    }
    else if (key == "type")
    {
        changed = document_->setNodeType (id, text.trim().toStdString());
    }
    else if (key == "text")
    {
        changed = document_->setNodeText (id, text.toStdString());
    }
    else
    {
        const auto parsed = parseLengthText (text);

        if (! parsed.valid)
        {
            populate();
            return;
        }

        auto style = node->style;
        const auto value = parsed.length;
        const auto edgeValue = value.value_or (layout::Length::automatic());

        if (key == "width")            style.width = value;
        else if (key == "height")      style.height = value;
        else if (key == "minWidth")    style.minWidth = value;
        else if (key == "minHeight")   style.minHeight = value;
        else if (key == "maxWidth")    style.maxWidth = value;
        else if (key == "maxHeight")   style.maxHeight = value;
        else if (key == "flexBasis")   style.flexBasis = value;
        else if (key == "gap")         style.gap = edgeValue;
        else if (key == "margin.top")    style.margin.top = edgeValue;
        else if (key == "margin.right")  style.margin.right = edgeValue;
        else if (key == "margin.bottom") style.margin.bottom = edgeValue;
        else if (key == "margin.left")   style.margin.left = edgeValue;
        else if (key == "padding.top")    style.padding.top = edgeValue;
        else if (key == "padding.right")  style.padding.right = edgeValue;
        else if (key == "padding.bottom") style.padding.bottom = edgeValue;
        else if (key == "padding.left")   style.padding.left = edgeValue;
        else if (key == "inset.top")    style.inset.top = edgeValue;
        else if (key == "inset.right")  style.inset.right = edgeValue;
        else if (key == "inset.bottom") style.inset.bottom = edgeValue;
        else if (key == "inset.left")   style.inset.left = edgeValue;
        else if (key == "flexGrow")     style.flexGrow = std::max (0.0f, text.getFloatValue());
        else if (key == "flexShrink")   style.flexShrink = std::max (0.0f, text.getFloatValue());
        else if (key == "aspectRatio")
        {
            const auto ratio = text.getFloatValue();

            if (text.containsAnyOf ("0123456789") && ratio > 0.0f)
                style.aspectRatio = ratio;

            changed = document_->setStyle (id, style);

            if (changed)
                notifyChanged();

            return;
        }
        else
        {
            return;
        }

        changed = document_->setStyle (id, style);
    }

    if (changed)
        notifyChanged();
}

void InspectorPanel::applyCombo (const juce::String& key, int index)
{
    if (document_ == nullptr || index < 0)
        return;

    const auto id = document_->selectedId();
    const auto* node = document_->find (id);

    if (node == nullptr)
        return;

    auto style = node->style;

    if (key == "direction")
        style.direction = index == 0 ? layout::Direction::row : layout::Direction::column;
    else if (key == "justifyContent")
        style.justify = static_cast<layout::JustifyContent> (
            std::clamp (index, 0, static_cast<int> (layout::JustifyContent::spaceEvenly)));
    else if (key == "alignItems")
        style.align = static_cast<layout::AlignItems> (
            std::clamp (index, 0, static_cast<int> (layout::AlignItems::stretch)));
    else if (key == "alignSelf")
        style.alignSelf = index == 0
                            ? std::nullopt
                            : std::optional<layout::AlignItems> (
                                  static_cast<layout::AlignItems> (index - 1));
    else if (key == "position")
        style.position = index == 1 ? layout::Position::absolute : layout::Position::relative;
    else if (key == "display")
        style.display = index == 1 ? layout::Display::none : layout::Display::flex;
    else
        return;

    if (document_->setStyle (id, style))
        notifyChanged();
}

void InspectorPanel::refreshTokens()
{
    if (document_ == nullptr)
        return;

    const juce::ScopedValueSetter<bool> guard (building_, true);

    const auto previous = selectedToken_;
    const auto ids = document_->theme().tokenIds();

    tokenBox_.clear (juce::dontSendNotification);

    int itemId = 1;

    for (const auto& id : ids)
        tokenBox_.addItem (juce::String (id), itemId++);

    const auto found = std::find (ids.begin(), ids.end(), previous);

    if (! ids.empty())
    {
        if (found != ids.end())
        {
            selectedToken_ = previous;
            tokenBox_.setSelectedItemIndex (static_cast<int> (std::distance (ids.begin(), found)),
                                            juce::dontSendNotification);
        }
        else
        {
            selectedToken_ = ids.front();
            tokenBox_.setSelectedItemIndex (0, juce::dontSendNotification);
        }
    }
    else
    {
        selectedToken_.clear();
    }

    updateTokenPreview();
}

void InspectorPanel::updateTokenPreview()
{
    if (document_ == nullptr || selectedToken_.empty())
    {
        tokenValueLabel_.setText ({}, juce::dontSendNotification);
        tokenHexEditor_.setText ({}, false);
        return;
    }

    const auto value = document_->theme().resolve (selectedToken_);

    if (! value.has_value())
    {
        tokenValueLabel_.setText ("unresolved", juce::dontSendNotification);
        return;
    }

    if (const auto* color = std::get_if<theme::Color> (&*value))
    {
        const auto hex = juce::String (color->toHex (color->a != 255));
        tokenValueLabel_.setText (hex + "   colour", juce::dontSendNotification);
        tokenHexEditor_.setText (hex, false);
        tokenHexEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (color->packedARGB()));
        tokenHexEditor_.setColour (juce::TextEditor::textColourId,
                                   color->relativeLuminance() > 0.45f ? juce::Colours::black
                                                                     : juce::Colours::white);
    }
    else if (const auto* dimension = std::get_if<theme::Dimension> (&*value))
    {
        tokenValueLabel_.setText (juce::String (dimension->pixels, 1) + " px", juce::dontSendNotification);
        tokenHexEditor_.setText ({}, false);
        tokenHexEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141a24));
        tokenHexEditor_.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6edf6));
    }
    else
    {
        tokenValueLabel_.setText (juce::String (theme::tokenValueToString (*value).c_str()),
                                  juce::dontSendNotification);
        tokenHexEditor_.setText ({}, false);
        tokenHexEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141a24));
        tokenHexEditor_.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6edf6));
    }
}

void InspectorPanel::applyTokenHex()
{
    if (document_ == nullptr || selectedToken_.empty())
        return;

    const auto color = theme::Color::parse (tokenHexEditor_.getText().trim().toStdString());

    if (! color.has_value())
    {
        updateTokenPreview();
        return;
    }

    if (document_->setThemeOverride (selectedToken_, *color))
    {
        updateTokenPreview();
        notifyChanged();
    }
}

void InspectorPanel::notifyChanged()
{
    if (document_ != nullptr)
        lastRevision_ = document_->revision();

    if (onDocumentChanged != nullptr)
        onDocumentChanged();
}

void InspectorPanel::refreshReadout()
{
    if (! snapshotSource_)
        return;

    const auto& snapshot = snapshotSource_();

    readout_.setText (juce::String::formatted (
                          "peak  L %.2f   R %.2f\nrms   L %.2f   R %.2f\ncentroid  %.0f Hz\nflux      %.2f\nharmonic  %.2f\npercussive %.2f",
                          snapshot.peakLeft, snapshot.peakRight,
                          snapshot.rmsLeft, snapshot.rmsRight,
                          snapshot.spectralCentroidHz,
                          snapshot.spectralFlux,
                          snapshot.harmonicRatio,
                          snapshot.percussiveRatio),
                      juce::dontSendNotification);
}

void InspectorPanel::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (0xff0f141b));
    graphics.setColour (juce::Colour (0xff232a34));
    graphics.drawVerticalLine (0, 0.0f, static_cast<float> (getHeight()));
}

void InspectorPanel::resized()
{
    auto area = getLocalBounds();
    header_.setBounds (area.removeFromTop (24).reduced (10, 4));
    viewport_.setBounds (area);

    const auto width = std::max (296, viewport_.getWidth() - 12);
    auto y = 4;

    for (auto& row : rows_)
    {
        if (row->section)
        {
            row->caption.setBounds (8, y + 6, width - 16, 16);
            y += 26;
        }
        else
        {
            row->caption.setBounds (6, y, 84, 22);

            if (row->editor != nullptr)
                row->editor->setBounds (94, y, width - 100, 22);

            y += 25;
        }
    }

    tokenCaption_.setBounds (6, y, 84, 22);
    tokenBox_.setBounds (94, y, width - 100, 22);
    y += 25;

    tokenValueLabel_.setBounds (94, y, width - 100, 16);
    y += 18;

    tokenHexEditor_.setBounds (94, y, 110, 22);
    clearTokenButton_.setBounds (210, y, 64, 22);
    y += 28;

    readout_.setBounds (8, y, width - 16, 92);
    y += 100;

    content_.setSize (width, y);
}

} // namespace mvt::ui
