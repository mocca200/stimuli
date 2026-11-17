#include "Designer/CanvasComponent.h"

#include "UIEngine/DesignerDocument.h"
#include "UIEngine/RenderEngine.h"
#include "UIEngine/Skin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace mvt::ui
{
namespace
{
constexpr float kHandleSize = 8.0f;
constexpr float kMinimumFrame = 8.0f;
constexpr float kSnapThreshold = 6.0f;

bool intersects (const layout::Rect& a, const layout::Rect& b) noexcept
{
    return a.x < b.x + b.width && b.x < a.x + a.width
        && a.y < b.y + b.height && b.y < a.y + a.height;
}

bool isRootId (const DesignerDocument* document, const std::string& id)
{
    return document != nullptr && document->root().id == id;
}

float area (const layout::Rect& rect) noexcept
{
    return std::max (0.0f, rect.width) * std::max (0.0f, rect.height);
}
} // namespace

CanvasComponent::CanvasComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    demoSignal_.reset();
    demoSignal_.advance (0.0, snapshot_);
    startTimerHz (60);
}

void CanvasComponent::setDocument (DesignerDocument* document)
{
    document_ = document;
    repaint();
}

void CanvasComponent::setRenderEngine (RenderEngine* engine)
{
    engine_ = engine;
    repaint();
}

void CanvasComponent::setSkin (Skin* skin)
{
    skin_ = skin;
    safetyGate_.reset();
    repaint();
}

void CanvasComponent::setPaused (bool shouldPause)
{
    paused_ = shouldPause;
}

void CanvasComponent::invalidateLayout()
{
    if (engine_ != nullptr)
        engine_->invalidate();

    repaint();
}

void CanvasComponent::timerCallback()
{
    if (! paused_)
    {
        demoSignal_.advance (1.0 / 60.0, snapshot_);

        if (engine_ != nullptr)
            engine_->tick (1.0 / 60.0);

        repaint();
    }
}

void CanvasComponent::paint (juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds();

    if (engine_ == nullptr)
    {
        graphics.fillAll (juce::Colour (0xff0b0d10));
        return;
    }

    auto& frame = safetyGate_.beginFrame (bounds.getWidth(), bounds.getHeight());

    {
        juce::Graphics frameGraphics (frame);
        frameGraphics.fillAll (juce::Colour (0xff0b0d10));
        engine_->render (frameGraphics, designBounds(), snapshot_);
    }

    const auto& presented = safetyGate_.endFrame (juce::Time::getMillisecondCounterHiRes() * 0.001);
    graphics.drawImageAt (presented, 0, 0);

    drawOverlay (graphics);
}

void CanvasComponent::resized()
{
    invalidateLayout();
}

void CanvasComponent::setDesignSize (layout::Size size)
{
    designSize_ = size;
    invalidateLayout();
}

void CanvasComponent::prewarm (int frames)
{
    if (engine_ == nullptr || skin_ == nullptr)
        return;

    for (int i = 0; i < frames; ++i)
    {
        demoSignal_.advance (1.0 / 60.0, snapshot_);
        engine_->update (designBounds(), snapshot_);
    }

    repaint();
}

juce::Rectangle<int> CanvasComponent::designBounds() const
{
    if (designSize_.width <= 0.0f || designSize_.height <= 0.0f)
        return getLocalBounds();

    return { 0, 0, static_cast<int> (designSize_.width), static_cast<int> (designSize_.height) };
}

const layout::LayoutBox* CanvasComponent::selectedBox() const
{
    if (engine_ == nullptr || document_ == nullptr)
        return nullptr;

    return boxForId (document_->selectedId());
}

const layout::LayoutBox* CanvasComponent::boxForId (std::string_view id) const
{
    if (engine_ == nullptr)
        return nullptr;

    return layout::FlexSolver::findById (engine_->solvedRoot(), id);
}

const layout::LayoutBox* CanvasComponent::parentBoxOf (const layout::LayoutBox& root,
                                                       std::string_view id)
{
    for (const auto& child : root.children)
    {
        if (child.id == id)
            return &root;

        if (const auto* found = parentBoxOf (child, id))
            return found;
    }

    return nullptr;
}

layout::Rect CanvasComponent::parentContentOf (const layout::LayoutBox& box) const
{
    const auto id = box.id;

    if (engine_ != nullptr)
    {
        if (const auto* parent = parentBoxOf (engine_->solvedRoot(), id))
            return parent->content;
    }

    return { 0.0f, 0.0f, static_cast<float> (getWidth()), static_cast<float> (getHeight()) };
}

CanvasComponent::Handle CanvasComponent::handleAt (juce::Point<int> position) const
{
    const auto* box = selectedBox();

    if (box == nullptr || isRootId (document_, box->id))
        return Handle::none;

    const auto frame = juce::Rectangle<float> (box->frame.x, box->frame.y,
                                               box->frame.width, box->frame.height);
    const auto point = position.toFloat();

    const auto near = [&point] (float x, float y)
    {
        return std::abs (point.x - x) <= kHandleSize && std::abs (point.y - y) <= kHandleSize;
    };

    const auto midX = frame.getCentreX();
    const auto midY = frame.getCentreY();

    if (near (frame.getX(), frame.getY()))                 return Handle::topLeft;
    if (near (frame.getRight(), frame.getY()))             return Handle::topRight;
    if (near (frame.getRight(), frame.getBottom()))        return Handle::bottomRight;
    if (near (frame.getX(), frame.getBottom()))            return Handle::bottomLeft;
    if (near (midX, frame.getY()))                         return Handle::top;
    if (near (frame.getRight(), midY))                     return Handle::right;
    if (near (midX, frame.getBottom()))                    return Handle::bottom;
    if (near (frame.getX(), midY))                         return Handle::left;

    return Handle::none;
}

void CanvasComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto handle = handleAt (event.getPosition());

    switch (handle)
    {
        case Handle::topLeft:
        case Handle::bottomRight:
            setMouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor);
            break;
        case Handle::topRight:
        case Handle::bottomLeft:
            setMouseCursor (juce::MouseCursor::TopRightCornerResizeCursor);
            break;
        case Handle::left:
        case Handle::right:
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            break;
        case Handle::top:
        case Handle::bottom:
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            break;
        case Handle::body:
        case Handle::none:
            setMouseCursor (juce::MouseCursor::NormalCursor);
            break;
    }
}

void CanvasComponent::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (engine_ == nullptr || document_ == nullptr)
        return;

    engine_->update (designBounds(), snapshot_);

    const auto handle = handleAt (event.getPosition());

    if (handle != Handle::none)
    {
        beginDrag (handle, event.getPosition());
        return;
    }

    const auto point = layout::Point { static_cast<float> (event.x), static_cast<float> (event.y) };
    const auto* hit = engine_->hitTest (point);

    if (hit != nullptr && hit->id != document_->root().id)
    {
        document_->select (hit->id);

        if (onSelectionChanged != nullptr)
            onSelectionChanged();

        beginDrag (Handle::body, event.getPosition());
        repaint();
        return;
    }

    gesture_ = Gesture::marquee;
    dragStart_ = event.getPosition();
    marquee_ = { event.x, event.y, 0, 0 };
}

void CanvasComponent::beginDrag (Handle handle, juce::Point<int> position)
{
    const auto* box = selectedBox();

    if (box == nullptr || isRootId (document_, box->id))
        return;

    gesture_ = Gesture::drag;
    activeHandle_ = handle;
    dragNodeId_ = box->id;
    dragStart_ = position;
    startFrame_ = box->frame;
    dragParentContent_ = parentContentOf (*box);
}

std::vector<float> CanvasComponent::horizontalSnapTargets (const layout::LayoutBox& box,
                                                           float width) const
{
    std::vector<float> targets;
    const auto content = dragParentContent_;

    targets.push_back (content.x);
    targets.push_back (content.x + content.width - width);

    if (const auto* parent = parentBoxOf (engine_->solvedRoot(), box.id))
    {
        for (const auto& sibling : parent->children)
        {
            if (sibling.id == box.id)
                continue;

            targets.push_back (sibling.frame.x);
            targets.push_back (sibling.frame.x + sibling.frame.width - width);
        }
    }

    return targets;
}

std::vector<float> CanvasComponent::verticalSnapTargets (const layout::LayoutBox& box,
                                                         float height) const
{
    std::vector<float> targets;
    const auto content = dragParentContent_;

    targets.push_back (content.y);
    targets.push_back (content.y + content.height - height);

    if (const auto* parent = parentBoxOf (engine_->solvedRoot(), box.id))
    {
        for (const auto& sibling : parent->children)
        {
            if (sibling.id == box.id)
                continue;

            targets.push_back (sibling.frame.y);
            targets.push_back (sibling.frame.y + sibling.frame.height - height);
        }
    }

    return targets;
}

float CanvasComponent::snapValue (float candidate, const std::vector<float>& targets)
{
    auto best = candidate;
    auto bestDistance = kSnapThreshold;

    for (const auto target : targets)
    {
        const auto distance = std::abs (target - candidate);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = target;
        }
    }

    return best;
}

void CanvasComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (gesture_ == Gesture::marquee)
    {
        marquee_ = juce::Rectangle<int>::leftTopRightBottom (
            std::min (dragStart_.x, event.x),
            std::min (dragStart_.y, event.y),
            std::max (dragStart_.x, event.x),
            std::max (dragStart_.y, event.y));

        repaint();
        return;
    }

    if (gesture_ == Gesture::drag)
        updateDrag (event.getPosition());
}

void CanvasComponent::updateDrag (juce::Point<int> position)
{
    if (document_ == nullptr || gesture_ != Gesture::drag)
        return;

    const auto* box = boxForId (dragNodeId_);

    if (box == nullptr)
        return;

    const auto dx = static_cast<float> (position.x - dragStart_.x);
    const auto dy = static_cast<float> (position.y - dragStart_.y);

    auto frame = startFrame_;
    const auto content = dragParentContent_;

    switch (activeHandle_)
    {
        case Handle::body:
            frame.x += dx;
            frame.y += dy;
            frame.x = std::clamp (frame.x, content.x,
                                  std::max (content.x, content.x + content.width - frame.width));
            frame.y = std::clamp (frame.y, content.y,
                                  std::max (content.y, content.y + content.height - frame.height));
            frame.x = snapValue (frame.x, horizontalSnapTargets (*box, frame.width));
            frame.y = snapValue (frame.y, verticalSnapTargets (*box, frame.height));
            break;

        case Handle::topLeft:
        {
            const auto right = startFrame_.x + startFrame_.width;
            const auto bottom = startFrame_.y + startFrame_.height;
            frame.x = std::min (startFrame_.x + dx, right - kMinimumFrame);
            frame.y = std::min (startFrame_.y + dy, bottom - kMinimumFrame);
            frame.width = right - frame.x;
            frame.height = bottom - frame.y;
            break;
        }

        case Handle::top:
        {
            const auto bottom = startFrame_.y + startFrame_.height;
            frame.y = std::min (startFrame_.y + dy, bottom - kMinimumFrame);
            frame.height = bottom - frame.y;
            break;
        }

        case Handle::topRight:
        {
            const auto bottom = startFrame_.y + startFrame_.height;
            frame.y = std::min (startFrame_.y + dy, bottom - kMinimumFrame);
            frame.height = bottom - frame.y;
            frame.width = std::max (kMinimumFrame, startFrame_.width + dx);
            break;
        }

        case Handle::right:
            frame.width = std::max (kMinimumFrame, startFrame_.width + dx);
            break;

        case Handle::bottomRight:
            frame.width = std::max (kMinimumFrame, startFrame_.width + dx);
            frame.height = std::max (kMinimumFrame, startFrame_.height + dy);
            break;

        case Handle::bottom:
            frame.height = std::max (kMinimumFrame, startFrame_.height + dy);
            break;

        case Handle::bottomLeft:
        {
            const auto right = startFrame_.x + startFrame_.width;
            frame.x = std::min (startFrame_.x + dx, right - kMinimumFrame);
            frame.width = right - frame.x;
            frame.height = std::max (kMinimumFrame, startFrame_.height + dy);
            break;
        }

        case Handle::left:
        {
            const auto right = startFrame_.x + startFrame_.width;
            frame.x = std::min (startFrame_.x + dx, right - kMinimumFrame);
            frame.width = right - frame.x;
            break;
        }

        case Handle::none:
            return;
    }

    frame.width = std::min (frame.width, std::max (kMinimumFrame, content.x + content.width - frame.x));
    frame.height = std::min (frame.height, std::max (kMinimumFrame, content.y + content.height - frame.y));

    document_->setNodeFrame (dragNodeId_,
                             { frame.x - content.x, frame.y - content.y,
                               frame.width, frame.height });

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    repaint();
}

void CanvasComponent::mouseUp (const juce::MouseEvent&)
{
    const auto wasDragging = gesture_ == Gesture::drag;

    if (gesture_ == Gesture::marquee)
        finishMarquee();

    gesture_ = Gesture::none;
    activeHandle_ = Handle::none;
    dragNodeId_.clear();

    // One extra notification after a drag lets the shell rebuild the tree that
    // was intentionally left alone while the pointer moved.
    if (wasDragging && onDocumentChanged != nullptr)
        onDocumentChanged();
}

void CanvasComponent::finishMarquee()
{
    const auto marquee = marquee_;
    marquee_ = {};

    if (document_ == nullptr || engine_ == nullptr || marquee.getWidth() < 4 || marquee.getHeight() < 4)
    {
        repaint();
        return;
    }

    const layout::Rect selection { static_cast<float> (marquee.getX()),
                                   static_cast<float> (marquee.getY()),
                                   static_cast<float> (marquee.getWidth()),
                                   static_cast<float> (marquee.getHeight()) };

    const layout::LayoutBox* best = nullptr;
    auto bestArea = std::numeric_limits<float>::max();

    std::function<void (const layout::LayoutBox&)> visit = [&] (const layout::LayoutBox& node)
    {
        if (node.id != document_->root().id
            && node.frame.width > 0.0f && node.frame.height > 0.0f
            && intersects (node.frame, selection)
            && area (node.frame) < bestArea)
        {
            best = &node;
            bestArea = area (node.frame);
        }

        for (const auto& child : node.children)
            visit (child);
    };

    visit (engine_->solvedRoot());

    if (best != nullptr)
    {
        document_->select (best->id);

        if (onSelectionChanged != nullptr)
            onSelectionChanged();
    }

    repaint();
}

void CanvasComponent::nudgeSelection (float deltaX, float deltaY)
{
    if (document_ == nullptr || engine_ == nullptr)
        return;

    engine_->update (designBounds(), snapshot_);

    const auto* box = selectedBox();

    if (box == nullptr || isRootId (document_, box->id))
        return;

    const auto content = parentContentOf (*box);

    document_->setNodeFrame (box->id,
                             { box->frame.x - content.x + deltaX,
                               box->frame.y - content.y + deltaY,
                               box->frame.width, box->frame.height });

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    repaint();
}

void CanvasComponent::groupSelection()
{
    if (document_ == nullptr || engine_ == nullptr)
        return;

    engine_->update (designBounds(), snapshot_);

    const auto* box = selectedBox();

    if (box == nullptr || isRootId (document_, box->id))
        return;

    const auto content = parentContentOf (*box);

    document_->groupSelection (box->id,
                               { box->frame.x - content.x, box->frame.y - content.y,
                                 box->frame.width, box->frame.height });

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    if (onSelectionChanged != nullptr)
        onSelectionChanged();

    repaint();
}

void CanvasComponent::duplicateSelection()
{
    if (document_ == nullptr)
        return;

    document_->duplicateNode (document_->selectedId());

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    if (onSelectionChanged != nullptr)
        onSelectionChanged();

    repaint();
}

void CanvasComponent::deleteSelection()
{
    if (document_ == nullptr)
        return;

    document_->deleteNode (document_->selectedId());

    if (onDocumentChanged != nullptr)
        onDocumentChanged();

    if (onSelectionChanged != nullptr)
        onSelectionChanged();

    repaint();
}

bool CanvasComponent::keyPressed (const juce::KeyPress& key)
{
    if (document_ == nullptr)
        return false;

    const auto step = key.getModifiers().isShiftDown() ? 10.0f : 1.0f;
    const auto code = key.getKeyCode();

    if (code == juce::KeyPress::leftKey)  { nudgeSelection (-step, 0.0f); return true; }
    if (code == juce::KeyPress::rightKey) { nudgeSelection (step, 0.0f); return true; }
    if (code == juce::KeyPress::upKey)    { nudgeSelection (0.0f, -step); return true; }
    if (code == juce::KeyPress::downKey)  { nudgeSelection (0.0f, step); return true; }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        deleteSelection();
        return true;
    }

    const auto command = key.getModifiers().isCommandDown();

    if (command && (code == 'd' || code == 'D'))
    {
        duplicateSelection();
        return true;
    }

    if (command && (code == 'g' || code == 'G'))
    {
        groupSelection();
        return true;
    }

    return false;
}

void CanvasComponent::drawOverlay (juce::Graphics& graphics)
{
    if (document_ == nullptr || engine_ == nullptr)
        return;

    const auto& palette = engine_->palette();
    const auto* box = selectedBox();

    if (box != nullptr)
    {
        const auto frame = juce::Rectangle<float> (box->frame.x, box->frame.y,
                                                  box->frame.width, box->frame.height);

        if (const auto* parent = parentBoxOf (engine_->solvedRoot(), box->id))
        {
            const auto content = juce::Rectangle<float> (parent->content.x, parent->content.y,
                                                         parent->content.width, parent->content.height);
            graphics.setColour (palette.accent.withAlpha (0.35f));
            graphics.drawRect (content.reduced (0.5f), 1.0f);
        }

        graphics.setColour (palette.accent);
        graphics.drawRect (frame, 1.5f);

        const auto handleSize = kHandleSize * 0.75f;
        const std::array<juce::Point<float>, 8> handles {
            juce::Point<float> { frame.getX(), frame.getY() },
            juce::Point<float> { frame.getCentreX(), frame.getY() },
            juce::Point<float> { frame.getRight(), frame.getY() },
            juce::Point<float> { frame.getRight(), frame.getCentreY() },
            juce::Point<float> { frame.getRight(), frame.getBottom() },
            juce::Point<float> { frame.getCentreX(), frame.getBottom() },
            juce::Point<float> { frame.getX(), frame.getBottom() },
            juce::Point<float> { frame.getX(), frame.getCentreY() }
        };

        for (const auto& handle : handles)
        {
            const auto rect = juce::Rectangle<float> (handleSize, handleSize).withCentre (handle);
            graphics.setColour (palette.background);
            graphics.fillRect (rect);
            graphics.setColour (palette.accent);
            graphics.drawRect (rect, 1.0f);
        }

        graphics.setFont (palette.monoFont);
        graphics.setColour (palette.accent.withAlpha (0.85f));
        graphics.drawText (juce::String (box->id) + "  "
                             + juce::String (juce::roundToInt (box->frame.width)) + " x "
                             + juce::String (juce::roundToInt (box->frame.height)),
                           juce::Rectangle<int> (juce::roundToInt (frame.getX()),
                                                 juce::roundToInt (frame.getY()) - 18,
                                                 260, 16),
                           juce::Justification::centredLeft, false);
    }

    if (! marquee_.isEmpty())
    {
        graphics.setColour (palette.accent.withAlpha (0.12f));
        graphics.fillRect (marquee_);
        graphics.setColour (palette.accent.withAlpha (0.8f));
        graphics.drawRect (marquee_.toFloat(), 1.0f);
    }

    juce::ignoreUnused (skin_);
}

} // namespace mvt::ui
