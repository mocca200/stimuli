#pragma once

#include "Engine/Layout/LayoutTypes.h"

#include <juce_core/juce_core.h>

namespace mvt::layout
{
/** Serialises a layout tree back into an "mvt.layout/1" document.

    The writer is the inverse of parseLayout(): every field it emits is read
    back by the parser, and a parse of the written document round-trips to the
    same canonical output. The designer uses it to persist edits.
*/
[[nodiscard]] juce::var writeLayout (const LayoutNode& root);
} // namespace mvt::layout
