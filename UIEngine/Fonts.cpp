#include "UIEngine/Fonts.h"

#include <MvtFontData.h>

namespace mvt::ui
{
namespace
{
juce::Typeface::Ptr createFace (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return {};

    return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size));
}

// Built once on first use; every Font below shares these refcounted faces. The
// statics are Typeface::Ptr values, not JPEG-style caches, so they are safe for
// the whole process lifetime and cheap to hand out per paint.
const juce::Typeface::Ptr& boldFace()
{
    static const juce::Typeface::Ptr face {
        createFace (MvtFontData::NunitoBold_ttf, MvtFontData::NunitoBold_ttfSize)
    };
    return face;
}

const juce::Typeface::Ptr& blackFace()
{
    static const juce::Typeface::Ptr face {
        createFace (MvtFontData::NunitoBlack_ttf, MvtFontData::NunitoBlack_ttfSize)
    };
    return face;
}

juce::Font makeFont (const juce::Typeface::Ptr& face, float height)
{
    auto options = juce::FontOptions (juce::jmax (1.0f, height));

    if (face != nullptr)
        options = options.withTypeface (face);

    return juce::Font (options);
}
} // namespace

namespace fonts
{
juce::Font normal (float height)
{
    return makeFont (boldFace(), height);
}

juce::Font bold (float height)
{
    return makeFont (blackFace(), height);
}

juce::Font forWeight (int cssWeight, float height)
{
    return makeFont (cssWeight >= 800 ? blackFace() : boldFace(), height);
}

bool isAvailable()
{
    return boldFace() != nullptr && blackFace() != nullptr;
}
} // namespace fonts

juce::Typeface::Ptr NunitoLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    return font.isBold() ? blackFace() : boldFace();
}

juce::Font NunitoLookAndFeel::getLabelFont (juce::Label& label)
{
    const auto font = label.getFont();
    const auto typeface = font.getTypefacePtr();

    // A label that already carries a bundled face keeps it. Emphasised labels
    // are set in Nunito Black, which has no bold style flag, so rebuilding them
    // from isBold() would silently downgrade them to Bold.
    if (typeface.get() == boldFace().get() || typeface.get() == blackFace().get())
        return font;

    return font.isBold() ? fonts::bold (font.getHeight())
                         : fonts::normal (font.getHeight());
}

juce::Font NunitoLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return fonts::normal (juce::jmin (16.0f, static_cast<float> (buttonHeight) * 0.6f));
}

juce::Font NunitoLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return fonts::normal (juce::jmin (16.0f, static_cast<float> (box.getHeight()) * 0.6f));
}

juce::Font NunitoLookAndFeel::getPopupMenuFont()
{
    return fonts::normal (17.0f);
}
} // namespace mvt::ui
