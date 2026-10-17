#include "EqDisplay.h"
#include "../DSP/FilterDesign.h"

namespace afoq::ui
{
namespace { constexpr double FMIN = 10.0, FMAX = 30000.0; const double LR = std::log10 (FMAX / FMIN); }
using juce::Colour; using juce::Path; using juce::Point;

EqDisplay::EqDisplay (AfoQ4Processor& p, Bank& b) : proc (p), bank (b) { setOpaque (false); startTimerHz (30); }

float EqDisplay::xOf (double f) const { return (float) (std::log10 (f / FMIN) / LR * plotW); }
double EqDisplay::fOf (float x) const { return FMIN * std::pow (FMAX / FMIN, (double) x / plotW); }

void EqDisplay::resized()
{
    plotW = (float) getWidth() - 84.f; plotH = (float) getHeight() - 24.f;
    N = std::max (120, (int) (plotW / 2.2f));
    fr.assign ((size_t) N, 0.f); xs = tot = specPre = specPost = tmpPre = tmpPost = fr;
    std::fill (specPre.begin(), specPre.end(), -100.f); std::fill (specPost.begin(), specPost.end(), -100.f);
    for (int i = 0; i < N; ++i) { xs[(size_t) i] = (float) i / (float) (N - 1) * plotW; fr[(size_t) i] = (float) fOf (xs[(size_t) i]); }
    for (auto& v : views) v.valid = false;
    rebuildBins(); peaks.clear();
}

void EqDisplay::rebuildBins()
{
    lastFs = proc.getSampleRateHz(); binLo.assign ((size_t) N, 0.f); binHi = binLo;
    const double k = Analyzer::kSize / lastFs;
    for (int i = 0; i < N; ++i)
    {
        const double f = fr[(size_t) i], fp = fr[(size_t) std::max (i - 1, 0)], fn = fr[(size_t) std::min (i + 1, N - 1)];
        binLo[(size_t) i] = (float) juce::jlimit (1.0, Analyzer::kSize / 2.0 - 2, std::sqrt (fp * f) * k);
        binHi[(size_t) i] = (float) juce::jlimit (1.0, Analyzer::kSize / 2.0 - 2, std::sqrt (f * fn) * k);
    }
}

float EqDisplay::sampleSpectrum (const std::vector<float>& db, int i) const
{
    const float lo = binLo[(size_t) i], hi = binHi[(size_t) i];
    if (hi - lo < 1.f) { const float pos = (lo + hi) * 0.5f; const int b = (int) pos; const float f = pos - (float) b; return db[(size_t) b] * (1 - f) + db[(size_t) b + 1] * f; }
    double s = 0; int c = 0;
    for (int b = (int) lo; b <= (int) hi; ++b) { s += std::pow (10.0, db[(size_t) b] / 10.0); ++c; }
    return (float) (10.0 * std::log10 (s / c + 1e-12));
}

// ---------------------------------------------------------------- data -----------------------------------
void EqDisplay::timerCallback()
{
    if (N < 2) return;
    if (proc.getSampleRateHz() != lastFs) rebuildBins();
    updateSpectrum();
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastPeak > 450) { findPeaks(); lastPeak = now; }
    const float pl = proc.meterL.exchange (0.f), pr = proc.meterR.exchange (0.f);
    auto ball = [] (float& m, float& h, float v) { const float d = juce::Decibels::gainToDecibels (v, -100.f); m = d > m ? d : m - 2.2f; h = d > h ? d : h - 0.6f; };
    ball (meterL, holdL, pl); ball (meterR, holdR, pr);
    repaint();
}

void EqDisplay::updateSpectrum()
{
    an = Bank::geti (bank.analyzer);
    static std::vector<float> db;
    auto upd = [&] (Analyzer& a, std::vector<float>& disp, bool wanted)
    {
        if (! wanted) { std::fill (disp.begin(), disp.end(), -100.f); return; }
        a.getSpectrum (db);
        for (int i = 0; i < N; ++i)
        {
            const float t = sampleSpectrum (db, i); float& d = disp[(size_t) i];
            d += (t - d) * (t > d ? 0.5f : 0.2f);
        }
    };
    upd (proc.preAnalyzer, specPre, an == 1 || an == 3);
    upd (proc.postAnalyzer, specPost, an >= 2);
}

void EqDisplay::findPeaks()
{
    peaks.clear(); if (! an) return;
    const auto& v = an >= 2 ? specPost : specPre;
    std::vector<float> s ((size_t) N);
    for (int i = 0; i < N; ++i) { float a = 0; int c = 0; for (int j = std::max (0, i - 3); j <= std::min (N - 1, i + 3); ++j) { a += v[(size_t) j]; ++c; } s[(size_t) i] = a / c; }
    std::vector<int> cand;
    for (int i = 10; i < N - 10; ++i) { bool m = true; for (int j = -9; j <= 9; ++j) if (s[(size_t) (i + j)] > s[(size_t) i]) { m = false; break; } if (m && s[(size_t) i] > -88.f) cand.push_back (i); }
    std::sort (cand.begin(), cand.end(), [&] (int a, int b) { return s[(size_t) a] > s[(size_t) b]; });
    for (int i : cand) { bool ok = true; for (int j : peaks) if (std::abs (i - j) < std::max (18, N / 20)) ok = false; if (ok) peaks.push_back (i); if (peaks.size() >= 4) break; }
    std::sort (peaks.begin(), peaks.end());
}

void EqDisplay::computeViews()
{
    std::fill (tot.begin(), tot.end(), 0.f);
    const double fs = proc.getSampleRateHz(); const bool natural = Bank::geti (bank.phase) == (int) PhaseMode::Natural;
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& v = views[(size_t) i]; const auto p = bank.read (i);
        const bool same = v.valid && v.natural == natural && v.p.type == p.type && v.p.freq == p.freq && v.p.gain == p.gain && v.p.q == p.q
                          && v.p.slope == p.slope && v.p.dynOn == p.dynOn && v.p.range == p.range;
        v.dyn = p.on && p.dynOn && canDyn (p.type);
        v.p = p; v.natural = natural;
        if (! p.on) { v.valid = false; continue; }
        if (v.r.size() != (size_t) N) { v.r.assign ((size_t) N, 0.f); v.rg = v.l = v.r; }
        auto fill = [&] (std::vector<float>& out, double g)
        { const auto e = BandEval::make (p, g, fs, natural); for (int j = 0; j < N; ++j) out[(size_t) j] = (float) juce::jlimit (-90.0, 90.0, e.db (fr[(size_t) j], fs)); };
        if (! same) { fill (v.r, p.gain); if (v.dyn) fill (v.rg, p.gain + p.range); v.valid = true; }
        if (v.dyn) fill (v.l, p.gain + p.range * proc.dynamicAmount (i));
        const auto& live = v.dyn ? v.l : v.r;
        for (int j = 0; j < N; ++j) tot[(size_t) j] += live[(size_t) j];
    }
}

Point<float> EqDisplay::nodePos (const BandParams& p) const
{
    float y = yOf (0.f);
    if (p.type == FilterType::Bell) y = yOf ((float) p.gain);
    else if (p.type == FilterType::LowShelf || p.type == FilterType::HighShelf) y = yOf ((float) p.gain * 0.5f);
    return { xOf (p.freq), y };
}
Point<float> EqDisplay::rangePos (const BandParams& p) const
{
    const float v = p.type == FilterType::Bell ? (float) (p.gain + p.range) : (float) (p.gain + p.range) * 0.5f;
    return { xOf (p.freq), yOf (v) + (p.range < 0 ? 9.f : -9.f) };
}

// ---------------------------------------------------------------- painting -------------------------------
Path EqDisplay::curvePath (const std::vector<float>& a) const
{
    Path p;
    for (int i = 0; i < N; ++i) { const float y = yOf (a[(size_t) i]); if (i) p.lineTo (xs[(size_t) i], y); else p.startNewSubPath (xs[(size_t) i], y); }
    return p;
}

void EqDisplay::paint (juce::Graphics& g)
{
    if (N < 2) return;
    computeViews();
    drawGrid (g); drawSpectrum (g); drawBands (g);
    bool any = false; for (auto& v : views) any = any || v.p.on;
    if (any)
    {
        Path t; for (int i = 0; i < N; ++i) { const float y = yOf (juce::jlimit (-90.f, 90.f, tot[(size_t) i])); if (i) t.lineTo (xs[(size_t) i], y); else t.startNewSubPath (xs[(size_t) i], y); }
        g.setColour (Colour (0xfff6c744).withAlpha (0.13f)); g.strokePath (t, juce::PathStrokeType (7.f, juce::PathStrokeType::curved));
        g.setColour (Colour (0xfff6c744)); g.strokePath (t, juce::PathStrokeType (2.f, juce::PathStrokeType::curved));
    }
    else { g.setFont (font (13.f)); g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawText ("Double-click to add a band", 0, (int) yOf (0) - 36, (int) plotW, 20, juce::Justification::centred); }
    drawNodes (g); drawPeaks (g); drawScales (g); drawBottom (g);
}

void EqDisplay::drawGrid (juce::Graphics& g)
{
    for (double d = 10; d <= 10000; d *= 10)
        for (int m = 1; m < 10; ++m)
        {
            const double f = d * m; if (f < FMIN || f > FMAX) continue;
            g.setColour (juce::Colours::white.withAlpha (m == 1 ? 0.085f : 0.038f)); g.fillRect ((int) xOf (f), 0, 1, (int) plotH);
        }
    const int st[] = { 0, 0, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 3 };   // step per range (3->1, 6->2, 12->3, 30->10)
    const float step = R == 30 ? 10.f : (float) st[std::min (R, 12)];
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    for (float v = std::floor (topDb() / step) * step; v > -R * 1.9f; v -= step) g.fillRect (0, (int) yOf (v), (int) plotW, 1);
    g.setColour (Colour (150, 195, 255).withAlpha (0.5f)); g.fillRect (0, (int) yOf (0), (int) plotW, 1);
}

void EqDisplay::drawSpectrum (juce::Graphics& g)
{
    if (! an) return;
    auto series = [&] (const std::vector<float>& v, bool main)
    {
        Path p; for (int i = 0; i < N; ++i) { const float y = sy (v[(size_t) i]); if (i) p.lineTo (xs[(size_t) i], y); else p.startNewSubPath (xs[(size_t) i], y); }
        g.setColour (juce::Colours::white.withAlpha (main ? 0.55f : 0.2f)); g.strokePath (p, juce::PathStrokeType (main ? 1.3f : 1.f, juce::PathStrokeType::curved));
        p.lineTo (plotW, plotH); p.lineTo (0, plotH); p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Colour (200, 200, 225).withAlpha (main ? 0.24f : 0.07f), 0, 0, Colour (200, 200, 225).withAlpha (main ? 0.02f : 0.01f), 0, plotH, false));
        g.fillPath (p);
    };
    if (an == 3) { series (specPre, false); series (specPost, true); }
    else series (an == 1 ? specPre : specPost, true);
}

void EqDisplay::drawBands (juce::Graphics& g)
{
    const float y0 = yOf (0.f); bool anySolo = Bank::geti (bank.solo) > 0;
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& v = views[(size_t) i]; if (! v.p.on) continue;
        const auto col = bandColour (i); const float mul = anySolo && Bank::geti (bank.solo) != i + 1 ? 0.3f : 1.f;
        float ext = 0; for (float x : v.r) if (std::abs (x) > std::abs (ext)) ext = x;
        auto base = curvePath (v.r);
        Path fill (base); fill.lineTo (plotW, y0); fill.lineTo (0, y0); fill.closeSubPath();
        if (std::abs (yOf (ext) - y0) > 1.f)
        { g.setGradientFill (juce::ColourGradient (col.withAlpha (0.62f * mul), 0, yOf (ext), col.withAlpha (0.14f * mul), 0, y0, false)); g.fillPath (fill); }
        if (v.dyn)
        {
            Path range (base); const auto rg = curvePath (v.rg);
            for (int j = N - 1; j >= 0; --j) range.lineTo (xs[(size_t) j], yOf (v.rg[(size_t) j]));
            range.closeSubPath(); g.setColour (col.withAlpha (0.26f * mul)); g.fillPath (range);
            Path dashed; const float dl[] = { 3.f, 3.f }; juce::PathStrokeType (1.f).createDashedStroke (dashed, rg, dl, 2);
            g.setColour (col.withAlpha (0.75f * mul)); g.strokePath (dashed, juce::PathStrokeType (1.f));
            g.setColour (juce::Colours::white.withAlpha (0.55f * mul)); g.strokePath (curvePath (v.l), juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));
        }
        g.setColour (col.withAlpha (0.95f * mul)); g.strokePath (base, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
    }
    // collision highlight: where two bands push the same way over the same region
    for (auto& c : coll) c.clear();
    if (! showColl) return;
    std::vector<float> o ((size_t) N);
    for (int a = 0; a < kMaxBands; ++a)
        for (int b = a + 1; b < kMaxBands; ++b)
        {
            auto &A = views[(size_t) a], &B = views[(size_t) b];
            if (! A.p.on || ! B.p.on || isCut (A.p.type) || isCut (B.p.type) || A.p.type == FilterType::Notch || B.p.type == FilterType::Notch) continue;
            if (std::abs (std::log2 (A.p.freq / B.p.freq)) > 2.2) continue;
            const auto &la = A.dyn ? A.l : A.r, &lb = B.dyn ? B.l : B.r; float mx = 0;
            for (int i = 0; i < N; ++i) { const float p = la[(size_t) i], q = lb[(size_t) i]; o[(size_t) i] = p * q > 0 ? std::min (std::abs (p), std::abs (q)) * (p > 0 ? 1.f : -1.f) : 0.f; mx = std::max (mx, std::abs (o[(size_t) i])); }
            if (mx < 1.2f) continue;
            coll[(size_t) a].push_back (b); coll[(size_t) b].push_back (a);
            Path c = curvePath (o); Path f (c); f.lineTo (plotW, y0); f.lineTo (0, y0); f.closeSubPath();
            g.setColour (Colour (255, 140, 60).withAlpha (0.34f)); g.fillPath (f);
            g.setColour (Colour (255, 170, 90).withAlpha (0.7f)); g.strokePath (c, juce::PathStrokeType (1.f));
        }
}

void EqDisplay::drawNodes (juce::Graphics& g)
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& p = views[(size_t) i].p; if (! p.on && i != sel) { if (! p.on) continue; }
        const auto col = bandColour (i); const auto c = nodePos (p); const bool isSel = i == sel, isHov = (i == hover && hoverKind == 1) || (drag.kind == 1 && drag.i == i);
        if (views[(size_t) i].dyn)
        {
            const auto h = rangePos (p); const float s = 5.f; Path t;
            if (p.range >= 0) { t.addTriangle (h.x, h.y - s * 0.8f, h.x - s, h.y + s * 0.8f, h.x + s, h.y + s * 0.8f); }
            else              { t.addTriangle (h.x, h.y + s * 0.8f, h.x - s, h.y - s * 0.8f, h.x + s, h.y - s * 0.8f); }
            g.setColour (juce::Colours::white.withAlpha (hoverKind == 2 && hover == i ? 1.f : 0.85f)); g.fillPath (t);
        }
        const float r = isHov ? 8.f : 6.5f;
        if (isSel) { g.setColour (col.withAlpha (0.28f)); g.fillEllipse (c.x - r - 5, c.y - r - 5, (r + 5) * 2, (r + 5) * 2); }
        g.setColour (col); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (isSel ? juce::Colours::white : juce::Colours::white.withAlpha (0.8f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, isSel ? 2.2f : 1.4f);
        if (Bank::geti (bank.solo) == i + 1) { g.setFont (font (9.f, true)); g.setColour (juce::Colours::white); g.drawText ("S", juce::Rectangle<float> (c.x - 6, c.y - 6, 12, 12), juce::Justification::centred); }
    }
}

void EqDisplay::drawPeaks (juce::Graphics& g)
{
    if (! showPeaks || ! an) return;
    const auto& v = an >= 2 ? specPost : specPre; const auto f = font (12.f, true);
    for (int i : peaks)
    {
        const float y = sy (v[(size_t) i]), x = xs[(size_t) i]; const auto txt = fmtHz (fr[(size_t) i]);
        const float w = textWidth (f, txt) + 18.f, h = 22.f, tx = juce::jlimit (3.f, plotW - w - 3.f, x - w / 2), ty = std::max (4.f, y - h - 11.f);
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (tx, ty + 2, w, h, 6.f);
        g.setColour (amber); g.fillRoundedRectangle (tx, ty, w, h, 6.f);
        Path t; t.addTriangle (x - 5, ty + h - 0.5f, x, ty + h + 5, x + 5, ty + h - 0.5f); g.fillPath (t);
        g.setColour (Colour (0xff3a2a00)); g.setFont (f); g.drawText (txt, juce::Rectangle<float> (tx, ty, w, h), juce::Justification::centred);
        g.setColour (amber); g.fillEllipse (x - 4, y - 4, 8, 8); g.setColour (Colour (0xb33c2800)); g.drawEllipse (x - 4, y - 4, 8, 8, 1.f);
    }
}

void EqDisplay::drawScales (juce::Graphics& g)
{
    g.setFont (font (10.f)); const float W = (float) getWidth();
    const float step = R == 30 ? 10.f : R == 12 ? 3.f : R == 6 ? 2.f : 1.f;
    for (float v = R - step; v >= -R - 0.01f; v -= step)
    {
        g.setColour (v == 0 ? Colour (0xfff0c04a) : Colour (0xffd7a739));
        g.drawText ((v > 0 ? "+" : "") + juce::String ((int) std::round (v)), juce::Rectangle<float> (W - 76.f, yOf (v) - 6.f, 26.f, 12.f), juce::Justification::centredRight);
    }
    g.setColour (Colour (0xff808090));
    for (int d = 0; d >= -100; d -= 10) g.drawText (juce::String (d), juce::Rectangle<float> (W - 48.f, sy ((float) d) - 6.f, 28.f, 12.f), juce::Justification::centredRight);
    juce::ColourGradient grad (Colour (0xffff3b30), 0, 0, Colour (0xff16793a), 0, plotH, false);
    grad.addColour (0.12, Colour (0xffff9a2e)); grad.addColour (0.18, Colour (0xffe5d43a)); grad.addColour (0.24, Colour (0xff3ddc6a));
    const float mx[2] = { W - 14.f, W - 7.f }, lv[2] = { meterL, meterR }, hd[2] = { holdL, holdR };
    for (int k = 0; k < 2; ++k)
    {
        g.setColour (Colour (0xff0b0b0e)); g.fillRoundedRectangle (mx[k], 2, 5, plotH - 4, 2);
        const float y = sy (lv[k]); g.saveState(); g.reduceClipRegion (juce::Rectangle<int> ((int) mx[k], 2, 5, (int) plotH - 4));
        g.setGradientFill (grad); g.fillRect (mx[k], y, 5.f, plotH - y); g.setColour (Colour (0xffe8e8f0)); g.fillRect (mx[k], sy (hd[k]), 5.f, 1.5f); g.restoreState();
    }
}

void EqDisplay::drawBottom (juce::Graphics& g)
{
    const float H = (float) getHeight(); g.setFont (font (10.f)); g.setColour (Colour (0xff8c8c99));
    for (double f : { 20., 50., 100., 200., 500., 1e3, 2e3, 5e3, 1e4, 2e4 })
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (xOf (f) - 20, H - 21, 40, 12), juce::Justification::centred);
    const float y0 = H - 9; int hv = -1;
    if (mouseIn && mouse.x <= plotW && mouse.y <= plotH) hv = juce::roundToInt (12.0 * std::log2 (fOf (mouse.x) / 440.0) + 69);
    for (int n = 0; n <= 130; ++n)
    {
        const double f = 440.0 * std::pow (2.0, (n - 69) / 12.0); if (f < FMIN) continue; if (f > FMAX) break;
        const float x0 = xOf (f * std::pow (2.0, -0.5 / 12)), x1 = xOf (f * std::pow (2.0, 0.5 / 12)); const int k = n % 12; const bool blk = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
        g.setColour (n == hv ? amber : juce::Colours::white.withAlpha (0.15f)); g.fillRect (x0 + 0.4f, y0, std::max (0.5f, x1 - x0 - 0.8f), 9.f);
        if (blk && n != hv) { g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRect (x0, y0, x1 - x0, 5.5f); }
    }
    if (hv >= 0)
    {
        static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setFont (font (11.f)); g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (fmtHz ((float) fOf (mouse.x)) + "   " + nn[((hv % 12) + 12) % 12] + juce::String (hv / 12 - 1), 10, 6, 200, 14, juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- interaction ----------------------------
EqDisplay::Hit EqDisplay::hitTest (Point<float> pt) const
{
    for (int i = kMaxBands - 1; i >= 0; --i) if (views[(size_t) i].p.on && nodePos (views[(size_t) i].p).getDistanceFrom (pt) <= 11.f) return { 1, i };
    for (int i = kMaxBands - 1; i >= 0; --i) if (views[(size_t) i].dyn && rangePos (views[(size_t) i].p).getDistanceFrom (pt) <= 10.f) return { 2, i };
    const int idx = juce::roundToInt (pt.x / plotW * (float) (N - 1));
    if (idx >= 0 && idx < N && pt.x <= plotW)
    {
        int best = -1; float bd = 7.f;
        for (int i = 0; i < kMaxBands; ++i)
        {
            const auto& v = views[(size_t) i]; if (! v.p.on || v.r.size() != (size_t) N || std::abs (v.r[(size_t) idx]) < 0.5f) continue;
            const float d = std::abs (yOf (v.r[(size_t) idx]) - pt.y); if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) return { 3, best };
    }
    return {};
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position; mouseIn = true; const auto h = hitTest (e.position); hover = h.i; hoverKind = h.kind;
    setMouseCursor (h.kind == 1 ? juce::MouseCursor::PointingHandCursor : h.kind == 3 ? juce::MouseCursor::DraggingHandCursor : h.kind == 2 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}
void EqDisplay::mouseExit (const juce::MouseEvent&) { mouseIn = false; if (drag.kind == 0) { hover = -1; hoverKind = 0; } }

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (e.mods.isPopupMenu())
    {
        if (h.kind) bandMenu (h.i, e.getScreenPosition()); else emptyMenu (e.getScreenPosition(), (float) fOf (e.position.x), dbOf (e.position.y));
        return;
    }
    if (! h.kind) return;
    if (sel != h.i && onSelect) onSelect (h.i);
    const auto& q = bank.r[(size_t) h.i]; const auto p = views[(size_t) h.i].p;
    drag = { h.kind == 2 ? 2 : 1, h.i, e.position, p.freq, p.gain, p.range, false };
    if (drag.kind == 2) q.range->beginChangeGesture();
    else { q.freq->beginChangeGesture(); if (hasGain (p.type)) q.gain->beginChangeGesture(); }
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag.kind) return;
    if (! drag.moved) { if (e.position.getDistanceFrom (drag.start) < 4.f) return; drag.moved = true; }
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.f, dx = (e.position.x - drag.start.x) * fine, dy = (e.position.y - drag.start.y) * fine;
    const auto& q = bank.r[(size_t) drag.i]; const auto type = views[(size_t) drag.i].p.type;
    if (drag.kind == 2) Bank::set (q.range, juce::jlimit (-30.f, 30.f, std::round ((float) (drag.r0 - dy / ppdb()) * 10.f) / 10.f));
    else
    {
        Bank::set (q.freq, (float) juce::jlimit (FMIN, FMAX, fOf (xOf (drag.f0) + dx)));
        if (hasGain (type)) Bank::set (q.gain, juce::jlimit (-30.f, 30.f, std::round ((float) (drag.g0 - dy / ppdb() * (type == FilterType::Bell ? 1.f : 2.f)) * 20.f) / 20.f));
    }
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! drag.kind) return;
    const auto& q = bank.r[(size_t) drag.i]; const auto type = views[(size_t) drag.i].p.type;
    if (drag.kind == 2) q.range->endChangeGesture(); else { q.freq->endChangeGesture(); if (hasGain (type)) q.gain->endChangeGesture(); }
    const bool moved = drag.moved; drag = {}; if (moved && onEditEnd) onEditEnd();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (h.kind == 1) deleteBand (h.i);
    else if (! h.kind && e.position.x <= plotW && e.position.y <= plotH) addBandAt ((float) fOf (e.position.x), dbOf (e.position.y), FilterType::Bell);
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int i = hover >= 0 ? hover : sel; if (i < 0 || ! bank.isOn (i) || isCut (views[(size_t) i].p.type)) return;
    const float d = w.deltaY * (w.isReversed ? -1.f : 1.f), q = Bank::get (bank.r[(size_t) i].q);
    Bank::setG (bank.r[(size_t) i].q, juce::jlimit (0.025f, 40.f, q * std::exp (d * 1.2f)));
    if (onEditEnd) onEditEnd();
}

void EqDisplay::addBandAt (float f, float db, FilterType t)
{
    const int slot = bank.firstFree(); if (slot < 0) return;
    BandParams p; p.on = true; p.type = t; p.freq = juce::jlimit (FMIN, FMAX, (double) f);
    p.gain = hasGain (t) ? std::round (juce::jlimit (-30.f, 30.f, db) * 20.f) / 20.f : 0.0;
    p.q = t == FilterType::Notch ? 6.0 : t == FilterType::Bell ? 1.0 : 0.7;
    bank.write (slot, p);
    if (onSelect) onSelect (slot);
    if (onEditEnd) onEditEnd();
}

void EqDisplay::deleteBand (int i)
{
    if (i < 0) return;
    Bank::setG (bank.r[(size_t) i].on, 0.f); Bank::setG (bank.r[(size_t) i].dyn, 0.f);
    if (Bank::geti (bank.solo) == i + 1) Bank::setG (bank.solo, 0.f);
    int best = -1; for (int d = 1; d < kMaxBands && best < 0; ++d) { if (bank.isOn (i - d)) best = i - d; else if (bank.isOn (i + d)) best = i + d; }
    if (onSelect) onSelect (best);
    if (onEditEnd) onEditEnd();
}

void EqDisplay::bandMenu (int i, juce::Point<int> pos)
{
    if (onSelect) onSelect (i);
    const auto p = bank.read (i); juce::PopupMenu m; m.addSectionHeader ("Shape");
    for (int t = 0; t < 7; ++t) m.addItem (100 + t, typeName ((FilterType) t), true, (int) p.type == t);
    if (isCut (p.type)) { juce::PopupMenu s; for (int k = 0; k < 9; ++k) s.addItem (200 + k, slopeName (kSlopes[k]), true, p.slope == kSlopes[k]); m.addSubMenu ("Slope", s); }
    m.addSeparator(); m.addItem (300, "Dynamic EQ", canDyn (p.type), p.dynOn);
    if (p.dynOn) { m.addItem (301, "Compress", true, ! p.expand); m.addItem (302, "Expand", true, p.expand); m.addItem (303, p.wide ? "Narrow detection" : "Wide detection"); }
    m.addSeparator(); m.addItem (310, "Solo", true, Bank::geti (bank.solo) == i + 1); m.addItem (311, p.on ? "Disable band" : "Enable band"); m.addItem (312, "Delete band");
    juce::Component::SafePointer<EqDisplay> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (pos.x, pos.y, 1, 1)), [sp, i, p] (int r)
    {
        if (! sp || r == 0) return; auto& q = sp->bank.r[(size_t) i];
        if (r >= 100 && r < 107) sp->bank.setType (i, (FilterType) (r - 100));
        else if (r >= 200 && r < 209) Bank::setG (q.slope, (float) (r - 200));
        else if (r == 300) Bank::setG (q.dyn, p.dynOn ? 0.f : 1.f);
        else if (r == 301) { Bank::setG (q.dir, 0.f); if (Bank::get (q.range) > 0) Bank::setG (q.range, -Bank::get (q.range)); }
        else if (r == 302) Bank::setG (q.dir, 1.f);
        else if (r == 303) Bank::setG (q.wide, p.wide ? 0.f : 1.f);
        else if (r == 310) Bank::setG (sp->bank.solo, Bank::geti (sp->bank.solo) == i + 1 ? 0.f : (float) (i + 1));
        else if (r == 311) Bank::setG (q.on, p.on ? 0.f : 1.f);
        else if (r == 312) { sp->deleteBand (i); return; }
        if (sp->onEditEnd) sp->onEditEnd();
    });
}

void EqDisplay::emptyMenu (juce::Point<int> pos, float f, float db)
{
    juce::PopupMenu m; m.addSectionHeader ("Add band");
    for (int t = 0; t < 7; ++t) m.addItem (1 + t, typeName ((FilterType) t), bank.firstFree() >= 0);
    juce::Component::SafePointer<EqDisplay> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (pos.x, pos.y, 1, 1)), [sp, f, db] (int r) { if (sp && r > 0) sp->addBandAt (f, db, (FilterType) (r - 1)); });
}
}
