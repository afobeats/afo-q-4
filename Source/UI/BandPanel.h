#pragma once
#include "Knob.h"
#include "ParamBank.h"

namespace afoq::ui
{
class IconButton : public juce::Component
{
public:
    enum Kind { Power, Phones, Dyn, Close, Prev, Next };
    explicit IconButton (Kind k) : kind (k) {}
    std::function<void()> onClick;
    void setLook (bool a, juce::Colour c) { active = a; col = c; repaint(); }
    void setDimmed (bool d) { dimmed = d; setInterceptsMouseClicks (! d, false); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (hov && ! dimmed) { g.setColour (juce::Colours::white.withAlpha (0.1f)); g.fillRoundedRectangle (r, 6.f); }
        g.setColour ((active ? col : juce::Colour (0xffa2a2b4)).withAlpha (dimmed ? 0.25f : 1.f));
        const float cx = r.getCentreX(), cy = r.getCentreY();
        juce::Path p; const juce::PathStrokeType st (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        switch (kind)
        {
            case Power:  p.addCentredArc (cx, cy + 0.5f, 5.f, 5.f, 0.f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true); p.startNewSubPath (cx, cy - 6.f); p.lineTo (cx, cy - 0.5f); g.strokePath (p, st); break;
            case Phones: p.addCentredArc (cx, cy, 5.5f, 5.5f, 0.f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true); g.strokePath (p, st);
                         g.fillRoundedRectangle (cx - 7.f, cy, 3.5f, 5.5f, 1.2f); g.fillRoundedRectangle (cx + 3.5f, cy, 3.5f, 5.5f, 1.2f); break;
            case Dyn:    for (int i = 0; i < 4; ++i) { const float x = cx - 5.f + i * 3.3f, h = (i == 0 ? 2.f : i == 1 ? 7.f : i == 2 ? 11.f : 5.f); p.startNewSubPath (x, cy + h / 2); p.lineTo (x, cy - h / 2); } g.strokePath (p, st); break;
            case Close:  p.startNewSubPath (cx - 4, cy - 4); p.lineTo (cx + 4, cy + 4); p.startNewSubPath (cx + 4, cy - 4); p.lineTo (cx - 4, cy + 4); g.strokePath (p, st); break;
            case Prev:   p.startNewSubPath (cx + 2, cy - 5); p.lineTo (cx - 2, cy); p.lineTo (cx + 2, cy + 5); g.strokePath (p, st); break;
            case Next:   p.startNewSubPath (cx - 2, cy - 5); p.lineTo (cx + 2, cy); p.lineTo (cx - 2, cy + 5); g.strokePath (p, st); break;
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { hov = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hov = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
private:
    Kind kind; bool active = false, hov = false, dimmed = false; juce::Colour col { 0xfff4c542 };
};

/** Floating panel for the selected band: shape/slope/channel, FREQ / GAIN / Q, and the dynamics row. */
class BandPanel : public juce::Component
{
public:
    std::function<void()> onEditEnd, onLayout, onDelete;
    std::function<void (int)> onStep;

    explicit BandPanel (Bank& b)
        : bank (b), pow (IconButton::Power), solo (IconButton::Phones), dyn (IconButton::Dyn), prev (IconButton::Prev), next (IconButton::Next), del (IconButton::Close),
          freqK ("FREQ", 46, false, [] (float v) { return fmtHz (v); }),
          gainK ("GAIN", 60, true,  [] (float v) { return fmtDb (v); }),
          qK    ("Q",    46, false, [] (float v) { return juce::String (v, 3); }),
          thrK  ("THRESHOLD", 34, false, [] (float v) { return juce::String (v, 1) + " dB"; }),
          rangeK("RANGE", 34, true,  [] (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }),
          atkK  ("ATTACK", 34, false, [] (float v) { return v < 10.f ? juce::String (v, 1) + " ms" : juce::String ((int) v) + " ms"; }),
          relK  ("RELEASE", 34, false, [] (float v) { return v < 1000.f ? juce::String ((int) v) + " ms" : juce::String (v / 1000.f, 2) + " s"; })
    {
        for (auto* c : std::initializer_list<juce::Component*> { &pow, &solo, &dyn, &prev, &next, &del, &typeB, &slopeB, &chB, &freqK, &gainK, &qK, &thrK, &rangeK, &atkK, &relK, &compB, &expB, &narB, &wideB })
            addAndMakeVisible (c);
        for (auto* k : { &freqK, &gainK, &qK, &thrK, &rangeK, &atkK, &relK }) k->onEditEnd = [this] { if (onEditEnd) onEditEnd(); };
        gainK.ringFn = [] (float v) { return v >= 0 ? juce::Colour (0xffffb347) : juce::Colour (0xffff6a5a); };
        thrK.ring = amber;
        for (auto* b2 : { &compB, &expB, &narB, &wideB }) b2->setClickingTogglesState (false);

        pow.onClick  = [this] { Bank::setG (bank.r[(size_t) idx].on, Bank::get (bank.r[(size_t) idx].on) > 0.5f ? 0.f : 1.f); done(); };
        solo.onClick = [this] { Bank::setG (bank.solo, Bank::geti (bank.solo) == idx + 1 ? 0.f : (float) (idx + 1)); done(); };
        dyn.onClick  = [this] { const auto p = bank.read (idx); if (canDyn (p.type)) { Bank::setG (bank.r[(size_t) idx].dyn, p.dynOn ? 0.f : 1.f); done(); } };
        prev.onClick = [this] { if (onStep) onStep (-1); };
        next.onClick = [this] { if (onStep) onStep (1); };
        del.onClick  = [this] { if (onDelete) onDelete(); };
        compB.onClick = [this] { auto& q = bank.r[(size_t) idx]; Bank::setG (q.dir, 0.f); if (Bank::get (q.range) > 0) Bank::setG (q.range, -Bank::get (q.range)); done(); };
        expB.onClick  = [this] { Bank::setG (bank.r[(size_t) idx].dir, 1.f); done(); };
        narB.onClick  = [this] { Bank::setG (bank.r[(size_t) idx].wide, 0.f); done(); };
        wideB.onClick = [this] { Bank::setG (bank.r[(size_t) idx].wide, 1.f); done(); };

        typeB.onClick = [this] {
            juce::PopupMenu m; const auto p = bank.read (idx);
            for (int t = 0; t < 7; ++t) m.addItem (t + 1, typeName ((FilterType) t), true, (int) p.type == t);
            juce::Component::SafePointer<BandPanel> sp (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (typeB), [sp] (int r) { if (sp && r > 0) { sp->bank.setType (sp->idx, (FilterType) (r - 1)); sp->done(); } });
        };
        slopeB.onClick = [this] {
            juce::PopupMenu m; const auto p = bank.read (idx);
            for (int s = 0; s < 9; ++s) m.addItem (s + 1, slopeName (kSlopes[s]), true, p.slope == kSlopes[s]);
            juce::Component::SafePointer<BandPanel> sp (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slopeB), [sp] (int r) { if (sp && r > 0) { Bank::setG (sp->bank.r[(size_t) sp->idx].slope, (float) (r - 1)); sp->done(); } });
        };
        chB.onClick = [this] {
            juce::PopupMenu m; const auto p = bank.read (idx);
            for (int c = 0; c < 5; ++c) m.addItem (c + 1, channelName ((ChannelMode) c), true, (int) p.ch == c);
            juce::Component::SafePointer<BandPanel> sp (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (chB), [sp] (int r) { if (sp && r > 0) { Bank::setG (sp->bank.r[(size_t) sp->idx].ch, (float) (r - 1)); sp->done(); } });
        };
        compB.setButtonText ("Compress"); expB.setButtonText ("Expand"); narB.setButtonText ("Narrow"); wideB.setButtonText ("Wide");
    }

    int preferredHeight() const { return dynOn ? 226 : 142; }

    void setBand (int i)
    {
        idx = i; watch.clear();
        if (i < 0) { setVisible (false); return; }
        setVisible (true);
        const auto col = bandColour (i); const auto& q = bank.r[(size_t) i];
        freqK.bind (q.freq); gainK.bind (q.gain); qK.bind (q.q); thrK.bind (q.thr); rangeK.bind (q.range); atkK.bind (q.atk); relK.bind (q.rel);
        freqK.ring = col; rangeK.ring = col;
        for (auto* p : { q.on, q.type, q.slope, q.ch, q.dyn, q.dir, q.wide, bank.solo })
            watch.push_back (std::make_unique<juce::ParameterAttachment> (*p, [this] (float) { refresh(); }));
        refresh();
    }
    void setGR (float db) { if (std::abs (db - gr) > 0.04f) { gr = db; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xf2303247), 0, 0, juce::Colour (0xf51b1c28), 0, r.getHeight(), false));
        g.fillRoundedRectangle (r, 20.f);
        g.setColour (juce::Colour (0xff4a4d63)); g.drawRoundedRectangle (r, 20.f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawLine (20, 2, r.getWidth() - 20, 2);
        g.setFont (font (12.f, true)); g.setColour (juce::Colour (0xffdcdce8));
        g.drawText (juce::String (idx + 1), numArea, juce::Justification::centred);
        if (dynOn)
        {
            g.setFont (font (11.f)); g.setColour (juce::Colour (0xffb8b8c8));
            g.drawText ("GR " + juce::String (gr > 0 ? "+" : "") + juce::String (gr, 1) + " dB", getWidth() - 110, 130, 96, 14, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        pow.setBounds (12, 6, 24, 22); solo.setBounds (38, 6, 24, 22); dyn.setBounds (64, 6, 24, 22);
        del.setBounds (getWidth() - 36, 6, 24, 22); next.setBounds (getWidth() - 62, 6, 24, 22);
        numArea = juce::Rectangle<float> ((float) getWidth() - 84.f, 6.f, 22.f, 22.f); prev.setBounds (getWidth() - 112, 6, 24, 22);
        typeB.setBounds (14, 44, 118, 27); slopeB.setBounds (14, 78, 118, 27); chB.setBounds (getWidth() - 132, 44, 118, 27);
        freqK.setBounds (146, 34, freqK.getWidth(), freqK.getHeight()); gainK.setBounds (238, 28, gainK.getWidth(), gainK.getHeight()); qK.setBounds (344, 34, qK.getWidth(), qK.getHeight());
        const int y = 150, xs[4] = { 14, 90, 166, 242 };
        thrK.setBounds (xs[0], y, thrK.getWidth(), thrK.getHeight()); rangeK.setBounds (xs[1], y, rangeK.getWidth(), rangeK.getHeight());
        atkK.setBounds (xs[2], y, atkK.getWidth(), atkK.getHeight()); relK.setBounds (xs[3], y, relK.getWidth(), relK.getHeight());
        compB.setBounds (338, y - 2, 88, 22); expB.setBounds (338, y + 24, 88, 22); narB.setBounds (440, y - 2, 88, 22); wideB.setBounds (440, y + 24, 88, 22);
    }

private:
    void done() { refresh(); if (onEditEnd) onEditEnd(); }
    void refresh()
    {
        if (idx < 0) return;
        const auto p = bank.read (idx); const auto col = bandColour (idx);
        typeB.setButtonText (typeName (p.type)); chB.setButtonText (channelName (p.ch));
        slopeB.setButtonText (isCut (p.type) ? slopeName (p.slope) : "12 dB/oct"); slopeB.setEnabled (isCut (p.type));
        compB.setToggleState (! p.expand, juce::dontSendNotification); expB.setToggleState (p.expand, juce::dontSendNotification);
        narB.setToggleState (! p.wide, juce::dontSendNotification);    wideB.setToggleState (p.wide, juce::dontSendNotification);
        for (auto* b : { &compB, &expB, &narB, &wideB }) b->setColour (juce::TextButton::buttonOnColourId, col);
        pow.setLook (p.on, col); solo.setLook (Bank::geti (bank.solo) == idx + 1, col); dyn.setLook (p.dynOn, col); dyn.setDimmed (! canDyn (p.type));
        gainK.setEnabled (hasGain (p.type)); qK.setEnabled (! isCut (p.type));
        const bool d = p.dynOn && canDyn (p.type);
        for (auto* c : std::initializer_list<juce::Component*> { &thrK, &rangeK, &atkK, &relK, &compB, &expB, &narB, &wideB }) c->setVisible (d);
        if (d != dynOn) { dynOn = d; if (onLayout) onLayout(); }
        repaint();
    }

    Bank& bank; int idx = -1; bool dynOn = false; float gr = 0.f; juce::Rectangle<float> numArea;
    IconButton pow, solo, dyn, prev, next, del;
    juce::TextButton typeB, slopeB, chB, compB, expB, narB, wideB;
    Knob freqK, gainK, qK, thrK, rangeK, atkK, relK;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watch;
};
}
