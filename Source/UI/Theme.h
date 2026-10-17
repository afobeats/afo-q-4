#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace afoq::ui
{
inline const juce::Colour amber { 0xfff4c542 };

inline juce::Font font (float h, bool bold = false)
{ return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

inline float textWidth (const juce::Font& f, const juce::String& s)
{ juce::GlyphArrangement ga; ga.addLineOfText (f, s, 0.f, 0.f); return ga.getBoundingBox (0, -1, true).getWidth(); }

inline juce::Colour bandColour (int i)
{
    static const juce::uint32 pal[8] = { 0xffff5a7a, 0xff3fa7ff, 0xffc95bff, 0xff2fd8a4, 0xffffb347, 0xff7f7bff, 0xffff7b45, 0xff58e0ea };
    return juce::Colour (pal[(size_t) i % 8]);
}
inline juce::String fmtHz (float f)
{
    if (f < 1000.f) return juce::String (f, f < 100.f ? 1 : 0) + " Hz";
    return juce::String (f / 1000.f, f < 10000.f ? 2 : 1) + " kHz";
}
inline juce::String fmtHz1 (float f) { return juce::String (f, 1) + " Hz"; }
inline juce::String fmtDb (float v) { return (v > 0 ? "+" : "") + juce::String (v, 2) + " dB"; }

struct AfoLookAndFeel : juce::LookAndFeel_V4
{
    AfoLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffdedeea));
        setColour (juce::TextButton::textColourOnId,  juce::Colours::white);
        setColour (juce::TextButton::buttonOnColourId, amber);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf21b1b21));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffdcdce6));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3b5bdb));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff7d7d8c));
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xff2c2d3b) : down ? juce::Colour (0xff30313f) : over ? juce::Colour (0xff2a2b38) : juce::Colour (0xff23242f));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (on ? b.findColour (juce::TextButton::buttonOnColourId) : juce::Colour (0xff3a3c4e));
        g.drawRoundedRectangle (r, 8.f, 1.f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.f); }
    juce::Font getPopupMenuFont() override { return font (13.f); }
};
}
