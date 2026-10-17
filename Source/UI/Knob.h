#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace afoq::ui
{
/** Rotary knob bound to a parameter. Drag vertically (Shift = fine), wheel to nudge, double-click resets. */
class Knob : public juce::Component
{
public:
    Knob (juce::String lbl, int diameter, bool bipolarArc, std::function<juce::String (float)> f)
        : label (std::move (lbl)), size (diameter), bipolar (bipolarArc), fmt (std::move (f))
    { setSize (std::max (diameter + 20, 70), diameter + 18); }

    std::function<void()> onEditEnd;
    std::function<juce::Colour (float)> ringFn;       // optional: ring colour from value
    juce::Colour ring { 0xffd8d8ea };

    void bind (juce::RangedAudioParameter* p)
    {
        att.reset(); param = p;
        if (p) { att = std::make_unique<juce::ParameterAttachment> (*p, [this] (float) { repaint(); }); att->sendInitialUpdate(); }
        repaint();
    }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.f : 0.3f); }

    void paint (juce::Graphics& g) override
    {
        const float S = (float) size, cx = getWidth() * 0.5f, cy = S * 0.5f + 2.f, R = S * 0.5f - 3.f;
        const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f);
        const float norm = param ? param->getValue() : 0.f, value = param ? param->convertFrom0to1 (norm) : 0.f;
        const float ang = a0 + (a1 - a0) * norm, from = bipolar ? 0.f : a0;

        juce::Path trk; trk.addCentredArc (cx, cy, R, R, 0.f, a0, a1, true);
        g.setColour (juce::Colour (0xff0e0f15));
        g.strokePath (trk, juce::PathStrokeType (S * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (std::abs (ang - from) > 0.01f)
        {
            juce::Path v; v.addCentredArc (cx, cy, R, R, 0.f, std::min (from, ang), std::max (from, ang), true);
            g.setColour (ringFn ? ringFn (value) : ring);
            g.strokePath (v, juce::PathStrokeType (S * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const float cr = R * 0.76f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff585a6a), cx - cr * 0.3f, cy - cr * 0.5f, juce::Colour (0xff14151b), cx + cr * 0.6f, cy + cr * 0.9f, true));
        g.fillEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f);
        g.setColour (juce::Colours::black.withAlpha (0.85f)); g.drawEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f, 1.3f);
        g.setColour (juce::Colour (0xfff0f0f7));
        g.drawLine (cx + std::sin (ang) * cr * 0.3f, cy - std::cos (ang) * cr * 0.3f, cx + std::sin (ang) * cr * 0.88f, cy - std::cos (ang) * cr * 0.88f, S * 0.045f + 1.f);

        g.setFont (font (10.5f)); g.setColour (juce::Colour (0xff9a9aae));
        g.drawText ((active || hov) && param ? fmt (value) : label, 0, size + 3, getWidth(), 14, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    { if (! param) return; att->beginGesture(); active = true; startNorm = param->getValue(); startY = e.y; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! param) return;
        const float n = juce::jlimit (0.f, 1.f, startNorm + (float) (startY - e.y) / (e.mods.isShiftDown() ? 700.f : 170.f));
        att->setValueAsPartOfGesture (param->convertFrom0to1 (n));
    }
    void mouseUp (const juce::MouseEvent&) override
    { if (! param) return; att->endGesture(); active = false; repaint(); if (onEditEnd) onEditEnd(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    { if (! param) return; att->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); if (onEditEnd) onEditEnd(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! param) return;
        const float n = juce::jlimit (0.f, 1.f, param->getValue() + w.deltaY * (w.isReversed ? -0.1f : 0.1f));
        att->setValueAsCompleteGesture (param->convertFrom0to1 (n)); if (onEditEnd) onEditEnd();
    }
    void mouseEnter (const juce::MouseEvent&) override { hov = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hov = false; repaint(); }

private:
    juce::String label; int size; bool bipolar; std::function<juce::String (float)> fmt;
    juce::RangedAudioParameter* param = nullptr; std::unique_ptr<juce::ParameterAttachment> att;
    bool active = false, hov = false; float startNorm = 0; int startY = 0;
};
}
