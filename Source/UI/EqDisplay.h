#pragma once
#include "Theme.h"
#include "ParamBank.h"
#include "../PluginProcessor.h"

namespace afoq::ui
{
/** The main curve view: analyzer, band curves with gradient fills, live dynamic shading,
    collision highlight, draggable nodes, peak labels, dB scales, meters and the piano ruler. */
class EqDisplay : public juce::Component, private juce::Timer
{
public:
    EqDisplay (AfoQ4Processor&, Bank&);
    ~EqDisplay() override { stopTimer(); }

    std::function<void (int)> onSelect;
    std::function<void()> onEditEnd;
    bool showPeaks = true, showColl = true;

    void setSelected (int i) { sel = i; }
    void setRange (int r) { R = r; }
    void deleteBand (int i);
    void bandMenu (int i, juce::Point<int> screenPos);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct View { BandParams p; bool natural = false, valid = false; std::vector<float> r, rg, l; bool dyn = false; };
    struct Hit { int kind = 0; int i = -1; };                       // 0 none, 1 node, 2 range handle, 3 curve
    struct Drag { int kind = 0, i = -1; juce::Point<float> start; double f0 = 0, g0 = 0, r0 = 0; bool moved = false; };

    void timerCallback() override;
    void computeViews();
    void updateSpectrum();
    void findPeaks();
    void rebuildBins();
    float sampleSpectrum (const std::vector<float>& db, int i) const;
    Hit hitTest (juce::Point<float>) const;
    juce::Point<float> nodePos (const BandParams&) const;
    juce::Point<float> rangePos (const BandParams&) const;
    void addBandAt (float f, float db, FilterType t);
    void emptyMenu (juce::Point<int> screenPos, float f, float db);
    void drawGrid (juce::Graphics&);
    void drawSpectrum (juce::Graphics&);
    void drawBands (juce::Graphics&);
    void drawNodes (juce::Graphics&);
    void drawPeaks (juce::Graphics&);
    void drawScales (juce::Graphics&);
    void drawBottom (juce::Graphics&);

    float xOf (double f) const;
    double fOf (float x) const;
    float topDb() const { return R * 1.05f; }
    float ppdb() const { return plotH / (R * 2.95f); }
    float yOf (float db) const { return (topDb() - db) * ppdb(); }
    float dbOf (float y) const { return topDb() - y / ppdb(); }
    float sy (float d) const { return plotH * (0.07f + 0.88f * juce::jlimit (0.f, 100.f, -d) / 100.f); }
    juce::Path curvePath (const std::vector<float>& a) const;

    AfoQ4Processor& proc; Bank& bank;
    int sel = -1, hover = -1, hoverKind = 0, R = 12, N = 0, an = 3;
    float plotW = 100, plotH = 100; double lastFs = 0;
    Drag drag; juce::Point<float> mouse; bool mouseIn = false;
    std::array<View, kMaxBands> views;
    std::vector<float> fr, xs, tot, specPre, specPost, tmpPre, tmpPost, binLo, binHi;
    std::vector<int> peaks;
    juce::uint32 lastPeak = 0;
    float meterL = -100.f, meterR = -100.f, holdL = -100.f, holdR = -100.f;
    std::array<std::vector<int>, kMaxBands> coll;
};
}
