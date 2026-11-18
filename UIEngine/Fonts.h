#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mvt::ui
{
/** Project typography rule: Nunito is the only UI family.

    Normal copy uses the Nunito Bold face (weight 700), emphasised copy uses
    Nunito Black (weight 900). Both faces are embedded through BinaryData, so
    the plugin, the standalone app, the Designer, and the offline render tools
    resolve identical glyphs with no font installed on the machine.
*/
namespace fonts
{
/** Nunito Bold at the requested height. This is normal text. */
[[nodiscard]] juce::Font normal (float height);

/** Nunito Black at the requested height. This is bold text. */
[[nodiscard]] juce::Font bold (float height);

/** Maps a CSS-style weight onto the two bundled faces: 800 and above renders
    Black, anything lower renders Bold, so a theme token cannot fall back to a
    regular-weight face that ships with the OS.
*/
[[nodiscard]] juce::Font forWeight (int cssWeight, float height);

/** True when both embedded faces loaded. When false the helpers above return
    JUCE's default sans-serif so the UI still runs.
*/
[[nodiscard]] bool isAvailable();
} // namespace fonts

/** Applies the same rule to JUCE chrome: buttons, combo boxes, editors and
    their popup menus.

    Component-scoped on purpose. A VST3 must not replace the host's default
    LookAndFeel, and a process-wide static would outlive JUCE's font caches, so
    each top-level UI owns one.

    Usage: hold one as the first member of the top-level component, call
    `setLookAndFeel (&lookAndFeel)` once it exists, and clear the pointer in the
    destructor before the member goes away.
*/
class NunitoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NunitoLookAndFeel() = default;

    /** JUCE only routes typeface resolution through the process default
        LookAndFeel, so the widget fonts below are what give component-scoped
        instances Nunito. This override covers the case where an instance is
        installed process-wide (the standalone app), never the plugin. */
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
};
} // namespace mvt::ui
