#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Params.h"

namespace afoq::ui
{
/** Message-thread access to the parameter tree, one band at a time. */
class Bank
{
public:
    struct Ref { juce::RangedAudioParameter *on, *type, *freq, *gain, *q, *slope, *ch, *dyn, *dir, *thr, *range, *atk, *rel, *wide; };
    std::array<Ref, kMaxBands> r {};
    juce::RangedAudioParameter *solo, *phase, *res, *analyzer, *out;

    explicit Bank (juce::AudioProcessorValueTreeState& s)
    {
        for (int i = 0; i < kMaxBands; ++i)
        {
            auto P = [&] (const char* n) { return s.getParameter (pid (i, n)); };
            r[(size_t) i] = { P ("on"), P ("type"), P ("freq"), P ("gain"), P ("q"), P ("slope"), P ("ch"), P ("dyn"), P ("dir"), P ("thr"), P ("range"), P ("atk"), P ("rel"), P ("wide") };
        }
        solo = s.getParameter ("solo"); phase = s.getParameter ("phase"); res = s.getParameter ("lin_res");
        analyzer = s.getParameter ("analyzer"); out = s.getParameter ("out_gain");
    }

    static float get (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
    static int   geti (juce::RangedAudioParameter* p) { return juce::roundToInt (get (p)); }
    static void  set (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    static void  setG (juce::RangedAudioParameter* p, float v) { p->beginChangeGesture(); set (p, v); p->endChangeGesture(); }

    BandParams read (int i) const
    {
        const auto& q = r[(size_t) i]; BandParams p;
        p.on = get (q.on) > 0.5f; p.type = (FilterType) geti (q.type);
        p.freq = get (q.freq); p.gain = get (q.gain); p.q = get (q.q);
        p.slope = kSlopes[juce::jlimit (0, 8, geti (q.slope))]; p.ch = (ChannelMode) geti (q.ch);
        p.dynOn = get (q.dyn) > 0.5f; p.expand = get (q.dir) > 0.5f; p.wide = get (q.wide) > 0.5f;
        p.thr = get (q.thr); p.range = get (q.range); p.atk = get (q.atk); p.rel = get (q.rel);
        return p;
    }
    void write (int i, const BandParams& p)
    {
        const auto& q = r[(size_t) i]; int si = 1;
        for (int k = 0; k < 9; ++k) if (kSlopes[k] == p.slope) si = k;
        setG (q.type, (float) (int) p.type); setG (q.freq, (float) p.freq); setG (q.gain, (float) p.gain); setG (q.q, (float) p.q);
        setG (q.slope, (float) si); setG (q.ch, (float) (int) p.ch); setG (q.dyn, p.dynOn ? 1.f : 0.f); setG (q.dir, p.expand ? 1.f : 0.f);
        setG (q.wide, p.wide ? 1.f : 0.f); setG (q.thr, (float) p.thr); setG (q.range, (float) p.range); setG (q.atk, (float) p.atk); setG (q.rel, (float) p.rel);
        setG (q.on, p.on ? 1.f : 0.f);
    }
    void reset (int i)
    {
        const auto& q = r[(size_t) i];
        for (auto* p : { q.on, q.type, q.freq, q.gain, q.q, q.slope, q.ch, q.dyn, q.dir, q.thr, q.range, q.atk, q.rel, q.wide })
            setG (p, p->convertFrom0to1 (p->getDefaultValue()));
    }
    int firstFree() const { for (int i = 0; i < kMaxBands; ++i) if (get (r[(size_t) i].on) < 0.5f) return i; return -1; }
    bool isOn (int i) const { return i >= 0 && get (r[(size_t) i].on) > 0.5f; }

    void setType (int i, FilterType t)
    {
        const auto& q = r[(size_t) i];
        setG (q.type, (float) (int) t);
        if (! hasGain (t)) setG (q.gain, 0.f);
        if (t == FilterType::Notch && get (q.q) < 2.f) setG (q.q, 6.f);
        if ((t == FilterType::LowShelf || t == FilterType::HighShelf || t == FilterType::Tilt) && get (q.q) > 2.f) setG (q.q, 0.7f);
        if (! canDyn (t)) setG (q.dyn, 0.f);
    }
};

inline const char* typeName (FilterType t)
{
    static const char* n[] = { "Bell", "Low shelf", "High shelf", "Low cut", "High cut", "Notch", "Tilt shelf" };
    return n[(int) t];
}
inline juce::String slopeName (int s) { return s == 0 ? juce::String ("Brickwall") : juce::String (s) + " dB/oct"; }
inline const char* channelName (ChannelMode m)
{
    static const char* n[] = { "Stereo", "Left", "Right", "Mid", "Side" };
    return n[(int) m];
}
}
