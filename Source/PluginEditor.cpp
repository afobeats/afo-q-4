#include "PluginEditor.h"
using namespace afoq; using namespace afoq::ui;

namespace
{
struct PB { FilterType t; double f, g, q; int slope; bool dyn, expand; double thr, range, atk, rel; };
struct Preset { const char* name; int sel; std::vector<PB> b; };
const std::vector<Preset>& presets()
{
    using T = FilterType;
    static const std::vector<Preset> p = {
        { "Leadsynth", 3, { { T::LowCut, 80, 0, .7, 24, 0, 0, 0, 0, 0, 0 }, { T::Bell, 190, -16, 8, 12, 0, 0, 0, 0, 0, 0 },
                            { T::Bell, 700, 8, .55, 12, 1, 0, -32, -5, 20, 220 }, { T::Bell, 1890, 0, 1, 12, 1, 0, -32, -9, 12, 160 },
                            { T::Bell, 4800, -4, .8, 12, 0, 0, 0, 0, 0, 0 }, { T::HighShelf, 9000, 3, .7, 12, 0, 0, 0, 0, 0, 0 } } },
        { "Vocal air", 2, { { T::LowCut, 100, 0, .7, 12, 0, 0, 0, 0, 0, 0 }, { T::Bell, 300, -3, 1.5, 12, 0, 0, 0, 0, 0, 0 },
                            { T::Bell, 3200, 2.5, .9, 12, 1, 1, -28, 3, 10, 140 }, { T::HighShelf, 11000, 4, .6, 12, 0, 0, 0, 0, 0, 0 } } },
        { "Mud cleanup", 1, { { T::LowCut, 40, 0, .7, 48, 0, 0, 0, 0, 0, 0 }, { T::Bell, 250, 0, 1.2, 12, 1, 0, -32, -6, 25, 260 },
                              { T::Notch, 120, 0, 8, 12, 0, 0, 0, 0, 0, 0 }, { T::Tilt, 1000, -1.5, .5, 12, 0, 0, 0, 0, 0, 0 } } },
        { "Init", -1, {} } };
    return p;
}
}

AfoQ4Editor::AfoQ4Editor (AfoQ4Processor& p)
    : AudioProcessorEditor (&p), proc (p), bank (p.apvts), display (p, bank), panel (bank),
      outLabel (*p.apvts.getParameter ("out_gain"), [] (float v) { return juce::String (v, 1) + " dB"; })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display); addChildComponent (panel);
    for (auto* b : { &undoB, &redoB, &abB, &copyB, &prevB, &presetB, &nextB, &rangeB, &phaseB, &resB, &analyzerB })
    { addAndMakeVisible (b); b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack); }
    addAndMakeVisible (outLabel);

    display.onSelect  = [this] (int i) { selectBand (i); };
    display.onEditEnd = [this] { commit(); };
    panel.onEditEnd = [this] { commit(); };
    panel.onLayout  = [this] { resized(); };
    panel.onDelete  = [this] { display.deleteBand (sel); };
    panel.onStep    = [this] (int d)
    {
        for (int k = 1; k < kMaxBands; ++k) { const int j = ((sel + d * k) % kMaxBands + kMaxBands) % kMaxBands; if (bank.isOn (j)) { selectBand (j); return; } }
    };

    undoB.onClick = [this] { if (hi > 0) restore (hi - 1); };
    redoB.onClick = [this] { if (hi < (int) hist.size() - 1) restore (hi + 1); };
    abB.onClick = [this]
    {
        (onB ? slotB : slotA) = proc.apvts.copyState(); onB = ! onB;
        auto& dst = onB ? slotB : slotA; if (dst.isValid()) proc.apvts.replaceState (dst.createCopy());
        abB.setButtonText (onB ? "A/[B]" : "[A]/B"); fixSelection(); commit();
    };
    copyB.onClick = [this] { (onB ? slotA : slotB) = proc.apvts.copyState(); };
    prevB.onClick = [this] { loadPreset (presetIdx - 1); };
    nextB.onClick = [this] { loadPreset (presetIdx + 1); };
    presetB.onClick = [this]
    {
        juce::PopupMenu m; for (size_t i = 0; i < presets().size(); ++i) m.addItem ((int) i + 1, presets()[i].name, true, (int) i == presetIdx);
        juce::Component::SafePointer<AfoQ4Editor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetB), [sp] (int r) { if (sp && r > 0) sp->loadPreset (r - 1); });
    };
    rangeB.onClick = [this]
    {
        juce::PopupMenu m; for (int v : { 3, 6, 12, 30 }) m.addItem (v, juce::String (v) + " dB", true, range == v);
        juce::Component::SafePointer<AfoQ4Editor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (rangeB), [sp] (int r) { if (sp && r > 0) { sp->range = r; sp->display.setRange (r); sp->rangeB.setButtonText (juce::String (r) + " dB"); } });
    };
    phaseB.onClick    = [this] { menuFor (phaseB, { "Zero Latency", "Natural Phase", "Linear Phase" }, *bank.phase); };
    resB.onClick      = [this] { menuFor (resB, { "Low", "Medium", "High", "Max" }, *bank.res); };
    analyzerB.onClick = [this]
    {
        juce::PopupMenu m; const int an = Bank::geti (bank.analyzer); const char* n[] = { "Off", "Pre", "Post", "Pre+Post" };
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, n[i], true, an == i);
        m.addSeparator(); m.addItem (10, "Peak labels", true, display.showPeaks); m.addItem (11, "Collision highlight", true, display.showColl);
        juce::Component::SafePointer<AfoQ4Editor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (analyzerB), [sp] (int r)
        {
            if (! sp || r == 0) return;
            if (r <= 4) Bank::setG (sp->bank.analyzer, (float) (r - 1)); else if (r == 10) sp->display.showPeaks = ! sp->display.showPeaks; else sp->display.showColl = ! sp->display.showColl;
        });
    };

    setSize (940, 560); setResizable (true, true); setResizeLimits (760, 460, 2000, 1200);
    loadPreset (0);
    startTimerHz (12);
}

void AfoQ4Editor::menuFor (juce::Component& c, const juce::StringArray& items, juce::RangedAudioParameter& p)
{
    juce::PopupMenu m; const int cur = Bank::geti (&p);
    for (int i = 0; i < items.size(); ++i) m.addItem (i + 1, items[i], true, cur == i);
    juce::Component::SafePointer<AfoQ4Editor> sp (this); auto* pp = &p;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (c), [sp, pp] (int r) { if (sp && r > 0) Bank::setG (pp, (float) (r - 1)); });
}

void AfoQ4Editor::selectBand (int i)
{
    sel = i; display.setSelected (i); panel.setBand (i); resized(); repaint();
}
void AfoQ4Editor::fixSelection()
{
    if (bank.isOn (sel)) { panel.setBand (sel); return; }
    int j = -1; for (int k = 0; k < kMaxBands; ++k) if (bank.isOn (k)) { j = k; break; }
    selectBand (j);
}

void AfoQ4Editor::commit()
{
    auto s = proc.apvts.copyState();
    if (hi >= 0 && hist[(size_t) hi].isEquivalentTo (s)) return;
    hist.resize ((size_t) hi + 1); hist.push_back (s.createCopy());
    if (hist.size() > 100) hist.erase (hist.begin());
    hi = (int) hist.size() - 1;
}
void AfoQ4Editor::restore (int idx)
{
    hi = idx; proc.apvts.replaceState (hist[(size_t) idx].createCopy()); fixSelection();
}

void AfoQ4Editor::loadPreset (int i)
{
    const int n = (int) presets().size(); presetIdx = (i % n + n) % n; const auto& p = presets()[(size_t) presetIdx];
    for (int b = 0; b < kMaxBands; ++b) bank.reset (b);
    Bank::setG (bank.solo, 0.f);
    for (size_t b = 0; b < p.b.size(); ++b)
    {
        const auto& s = p.b[b]; BandParams bp; bp.on = true; bp.type = s.t; bp.freq = s.f; bp.gain = s.g; bp.q = s.q; bp.slope = s.slope;
        bp.dynOn = s.dyn; bp.expand = s.expand; if (s.dyn) { bp.thr = s.thr; bp.range = s.range; bp.atk = s.atk; bp.rel = s.rel; }
        bank.write ((int) b, bp);
    }
    presetB.setButtonText (p.name); hist.clear(); hi = -1;
    selectBand (p.sel); commit(); repaint();
}

void AfoQ4Editor::timerCallback()
{
    undoB.setEnabled (hi > 0); redoB.setEnabled (hi < (int) hist.size() - 1);
    static const char* ph[] = { "Zero Latency", "Natural Phase", "Linear Phase" }; static const char* rs[] = { "Low", "Medium", "High", "Max" }; static const char* an[] = { "Off", "Pre", "Post", "Pre+Post" };
    phaseB.setButtonText (juce::String (ph[Bank::geti (bank.phase)]) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
    resB.setButtonText (juce::String ("Res: ") + rs[Bank::geti (bank.res)]);
    analyzerB.setButtonText (juce::String ("Analyzer: ") + an[Bank::geti (bank.analyzer)]);
    resB.setVisible (Bank::geti (bank.phase) == (int) PhaseMode::Linear);
    if (sel >= 0 && ! bank.isOn (sel)) { /* disabled by the user: keep the panel so it can be re-enabled */ }
    else if (sel < 0) fixSelection();
    if (sel >= 0) { const auto p = bank.read (sel); panel.setGR ((float) (p.dynOn ? p.range * proc.dynamicAmount (sel) : 0.0)); }
}

void AfoQ4Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0d));
    auto b = getLocalBounds(); auto head = b.removeFromTop (42).toFloat(), foot = b.removeFromBottom (30).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35353d), 0, 0, juce::Colour (0xff212127), 0, 42, false)); g.fillRect (head);
    g.setColour (juce::Colours::white.withAlpha (0.1f)); g.fillRect (0.f, 0.f, head.getWidth(), 1.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c21), 0, foot.getY(), juce::Colour (0xff141417), 0, foot.getBottom(), false)); g.fillRect (foot);

    g.setFont (font (7.5f)); g.setColour (juce::Colour (0xff8a8a96)); g.drawText ("afobeats audio", 14, 4, 120, 10, juce::Justification::left);
    g.setFont (font (22.f)); g.setColour (juce::Colour (0xfff1f1f5)); g.drawText ("Afo", 14, 13, 40, 26, juce::Justification::left);
    g.setColour (amber); g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "Q", 46, 13, 40, 26, juce::Justification::left);
    g.setFont (font (11.f, true)); g.setColour (juce::Colour (0xfff0a732)); g.drawText ("4", 80, 12, 14, 14, juce::Justification::left);

    const int pw = 190, px = (int) (foot.getCentreX() - pw / 2);
    g.setColour (juce::Colour (0xff26262e)); g.fillRoundedRectangle ((float) px, foot.getY() + 4, (float) pw, 22.f, 9.f);
    g.setColour (juce::Colour (0xff34343e)); g.drawRoundedRectangle ((float) px, foot.getY() + 4, (float) pw, 22.f, 9.f, 1.f);
    g.setColour (juce::Colour (0xff6b86ff)); g.fillEllipse ((float) px + 18, foot.getY() + 11, 7, 7);
    g.setFont (font (12.f)); g.setColour (juce::Colour (0xffdcdce6)); g.drawText (presets()[(size_t) presetIdx].name, px + 30, (int) foot.getY() + 4, pw - 40, 22, juce::Justification::centredLeft);
}

void AfoQ4Editor::resized()
{
    auto b = getLocalBounds(); auto head = b.removeFromTop (42), foot = b.removeFromBottom (30);
    display.setBounds (b);
    const int cx = head.getCentreX(); int x = cx - 250;
    for (auto& [btn, w] : std::initializer_list<std::pair<juce::TextButton*, int>> { { &undoB, 30 }, { &redoB, 30 }, { &abB, 64 }, { &copyB, 52 }, { &prevB, 28 }, { &presetB, 130 }, { &nextB, 28 } })
    { btn->setBounds (x, 8, w, 26); x += w + 6; }
    rangeB.setBounds (display.getRight() - 48 - 52, display.getY() + 6, 52, 20);
    phaseB.setBounds (8, foot.getY() + 3, 140, 24); resB.setBounds (152, foot.getY() + 3, 90, 24);
    outLabel.setBounds (foot.getRight() - 70, foot.getY() + 3, 62, 24); analyzerB.setBounds (foot.getRight() - 70 - 160, foot.getY() + 3, 154, 24);
    const int pw = 560, ph = panel.preferredHeight();
    panel.setBounds (display.getX() + (display.getWidth() - pw) / 2, display.getBottom() - 34 - ph, pw, ph);
}
