#pragma once
#include "FilterDesign.h"
#include <atomic>
#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>

namespace afoq
{
/** Linear-phase path.

    A background thread builds a 2x2 matrix of zero-phase FIRs (LL, LR, RL, RR) from the bands, in band
    order, so every channel mode is exact: Left/Right/Stereo bands are diagonal, Mid/Side bands have
    cross terms. The cross-term convolver only runs when a Mid/Side band exists.

    Each band's response is cached and only re-evaluated when that band (or its dynamic amount)
    changes, so a handful of moving dynamic bands costs a few FFTs per update, not a full rebuild.     */
class LinearPhase : private juce::Thread
{
public:
    struct Snapshot { std::array<BandParams, kMaxBands> p {}; std::array<float, kMaxBands> k {}; };

    LinearPhase() : juce::Thread ("AfoQ4 IR design") {}
    ~LinearPhase() override { stopThread (3000); }

    void prepare (double sr, int maxBlock)
    {
        fs = sr;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        convMain.prepare (spec); convCross.prepare (spec);
        crossBuf.setSize (2, maxBlock);
        ready.store (false); forceRebuild.store (true);
        if (! isThreadRunning()) startThread (juce::Thread::Priority::low);
    }
    void reset()                   { convMain.reset(); convCross.reset(); crossWas = false; }
    void setOrder (int o)          { order.store (juce::jlimit (11, 15, o)); }
    void setActive (bool a)        { active.store (a); }
    bool isReady() const           { return ready.load(); }
    int  latencySamples() const    { return (1 << order.load()) / 2; }

    void setSnapshot (const Snapshot& s)
    {
        const juce::SpinLock::ScopedTryLockType g (lock);
        if (g.isLocked()) snap = s;
    }

    /** Audio thread, in place. */
    void process (float* l, float* r, int n)
    {
        const bool useCross = needCross.load();
        if (useCross && ! crossWas) convCross.reset();
        crossWas = useCross;
        if (useCross)
        {
            crossBuf.copyFrom (0, 0, r, n); crossBuf.copyFrom (1, 0, l, n);          // R feeds L-out, L feeds R-out
            juce::dsp::AudioBlock<float> cb (crossBuf); auto sub = cb.getSubBlock (0, (size_t) n);
            convCross.process (juce::dsp::ProcessContextReplacing<float> (sub));
        }
        float* ptrs[2] = { l, r }; juce::AudioBuffer<float> st (ptrs, 2, n);
        juce::dsp::AudioBlock<float> blk (st);
        convMain.process (juce::dsp::ProcessContextReplacing<float> (blk));
        if (useCross)
        {
            const float* c0 = crossBuf.getReadPointer (0); const float* c1 = crossBuf.getReadPointer (1);
            for (int i = 0; i < n; ++i) { l[i] += c0[i]; r[i] += c1[i]; }
        }
    }

private:
    struct Cache { BandParams p; float k = 0; bool valid = false; std::vector<float> db; };

    static bool sameBand (const BandParams& x, const BandParams& y)
    {
        return x.type == y.type && x.freq == y.freq && x.gain == y.gain && x.q == y.q && x.slope == y.slope && x.dynOn == y.dynOn && x.range == y.range;
    }
    static bool differs (const Snapshot& a, const Snapshot& b)
    {
        for (size_t i = 0; i < (size_t) kMaxBands; ++i)
        {
            if (a.p[i].on != b.p[i].on) return true;
            if (! a.p[i].on) continue;
            if (! sameBand (a.p[i], b.p[i]) || a.p[i].ch != b.p[i].ch) return true;
            if (a.p[i].dynOn && std::abs (a.k[i] - b.k[i]) > 0.004f) return true;
        }
        return false;
    }

    void run() override
    {
        Snapshot last; int lastOrder = -1; bool have = false;
        while (! threadShouldExit())
        {
            if (! active.load()) { wait (100); continue; }
            Snapshot s; { const juce::SpinLock::ScopedLockType g (lock); s = snap; }
            const int ord = order.load();
            if (! have || ord != lastOrder || forceRebuild.exchange (false) || differs (s, last))
            { build (s, ord); last = s; lastOrder = ord; have = true; }
            wait (12);                                   // up to ~80 updates / second
        }
    }

    void writeIr (const std::vector<float>& re, float* dst, int L)
    {
        const int half = L / 2;
        for (int k = 0; k <= half; ++k) { spec[(size_t) k] = { re[(size_t) k], 0.f }; if (k > 0 && k < half) spec[(size_t) (L - k)] = { re[(size_t) k], 0.f }; }
        fft->perform (spec.data(), out.data(), true);
        for (int n = 0; n < L; ++n) dst[n] = out[(size_t) ((n + half) % L)].real() * scale * win[(size_t) n];
    }

    void build (const Snapshot& s, int ord)
    {
        const int L = 1 << ord, half = L / 2, bins = half + 1;
        if (ord != fftOrder)
        {
            fft = std::make_unique<juce::dsp::FFT> (ord); fftOrder = ord;
            win.resize ((size_t) L);
            for (int n = 0; n < L; ++n)
            { const double x = 2.0 * kPi * n / (L - 1); win[(size_t) n] = (float) (0.42 - 0.5 * std::cos (x) + 0.08 * std::cos (2 * x)); }
            spec.assign ((size_t) L, {}); out.assign ((size_t) L, {});
            for (auto& c : spec) c = { 1.f, 0.f };       // calibrate the FFT's inverse scaling once
            fft->perform (spec.data(), out.data(), true);
            scale = std::abs (out[0].real()) > 1e-12f ? 1.0f / out[0].real() : 1.0f;
            for (auto& c : cache) c.valid = false;
            for (auto* v : { &LL, &LR, &RL, &RR }) v->assign ((size_t) bins, 0.f);
        }
        for (size_t b = 0; b < (size_t) bins; ++b) { LL[b] = RR[b] = 1.f; LR[b] = RL[b] = 0.f; }

        bool cross2 = false;
        for (size_t i = 0; i < (size_t) kMaxBands; ++i)
        {
            const auto& p = s.p[i]; auto& c = cache[i];
            if (! p.on) { c.valid = false; continue; }
            const bool dyn = p.dynOn && canDyn (p.type);
            const float k = dyn ? s.k[i] : 0.f;
            if (! (c.valid && sameBand (c.p, p) && std::abs (c.k - k) <= 0.004f && (int) c.db.size() == bins))
            {
                const auto e = BandEval::make (p, p.gain + (dyn ? p.range * k : 0.0), fs, false);
                c.db.resize ((size_t) bins);
                for (int b = 0; b < bins; ++b) c.db[(size_t) b] = (float) juce::jlimit (-150.0, 60.0, e.db (std::max (1.0, b * fs / L), fs));
                c.p = p; c.k = k; c.valid = true;
            }
            if (p.ch == ChannelMode::Mid || p.ch == ChannelMode::Side) cross2 = true;

            for (int b = 0; b < bins; ++b)               // T = B * T   (band order = signal order)
            {
                const float H = std::pow (10.f, c.db[(size_t) b] / 20.f);
                float b00 = H, b01 = 0, b10 = 0, b11 = H;
                switch (p.ch)
                {
                    case ChannelMode::Left:  b11 = 1; break;
                    case ChannelMode::Right: b00 = 1; break;
                    case ChannelMode::Mid:   b00 = b11 = (H + 1) * .5f; b01 = b10 = (H - 1) * .5f; break;
                    case ChannelMode::Side:  b00 = b11 = (H + 1) * .5f; b01 = b10 = (1 - H) * .5f; break;
                    default: break;
                }
                const size_t u = (size_t) b;
                const float ll = b00 * LL[u] + b01 * RL[u], lr = b00 * LR[u] + b01 * RR[u];
                const float rl = b10 * LL[u] + b11 * RL[u], rr = b10 * LR[u] + b11 * RR[u];
                LL[u] = ll; LR[u] = lr; RL[u] = rl; RR[u] = rr;
            }
        }

        juce::AudioBuffer<float> irMain (2, L);
        writeIr (LL, irMain.getWritePointer (0), L); writeIr (RR, irMain.getWritePointer (1), L);
        if (cross2)
        {
            juce::AudioBuffer<float> irCross (2, L);
            writeIr (LR, irCross.getWritePointer (0), L); writeIr (RL, irCross.getWritePointer (1), L);
            convCross.loadImpulseResponse (std::move (irCross), fs, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        }
        convMain.loadImpulseResponse (std::move (irMain), fs, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        needCross.store (cross2);
        ready.store (true);
    }

    juce::dsp::Convolution convMain, convCross;
    juce::AudioBuffer<float> crossBuf; bool crossWas = false;
    juce::SpinLock lock; Snapshot snap;
    std::atomic<int> order { 13 };
    std::atomic<bool> active { false }, ready { false }, forceRebuild { false }, needCross { false };
    double fs = 48000;
    std::unique_ptr<juce::dsp::FFT> fft; int fftOrder = -1; float scale = 1.f;
    std::vector<float> win, LL, LR, RL, RR; std::vector<juce::dsp::Complex<float>> spec, out;
    std::array<Cache, kMaxBands> cache;
};
}
