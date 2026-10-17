#pragma once
#include "FilterDesign.h"
#include "Lanes.h"
#include <atomic>
#include <juce_dsp/juce_dsp.h>

namespace afoq
{
/** Zero-latency / natural-phase engine: per-band biquad cascades with per-band dynamics.

    Real-time rules: no allocation, no locks, no exceptions. Work is done in 32-sample sub-blocks:
    parameters are smoothed, coefficients are redesigned only when something actually changed, and
    bands that are idle (disabled, or 0 dB with no dynamic movement) cost nothing. */
class EqEngine
{
public:
    static constexpr int kSub = 32;

    void prepare (double sampleRate) { fs = sampleRate; for (auto& b : bands) b.prepare (fs); }
    void setNatural (bool n) { natural = n; }
    void setParams (const std::array<BandParams, kMaxBands>& p) { for (int i = 0; i < kMaxBands; ++i) bands[(size_t) i].setTarget (p[(size_t) i]); }

    /** applyFilters=false runs only the dynamics detectors (used by the linear-phase path). */
    void process (float* l, float* r, int n, bool applyFilters)
    {
        for (int pos = 0; pos < n; pos += kSub)
        {
            const int m = std::min (kSub, n - pos);
            for (auto& b : bands) b.process (l + pos, r + pos, m, applyFilters, natural, fs);
        }
    }
    float dynamicAmount (int i) const { return bands[(size_t) i].kOut.load (std::memory_order_relaxed); }

private:
    struct Biquad
    {
        Coeffs c; double z1[2] {}, z2[2] {};
        inline double tick (int lane, double x) noexcept
        {
            const double y = c.b0 * x + z1[lane];
            z1[lane] = c.b1 * x - c.a1 * y + z2[lane];
            z2[lane] = c.b2 * x - c.a2 * y;
            return y;
        }
        void clear() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    };

    struct Band
    {
        BandParams p;
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> sFreq, sQ;
        juce::SmoothedValue<double> sGain;
        std::array<Biquad, 16> sec; int nSec = 0;
        Biquad det;                                    // band-pass detector (2 lanes)
        double mix = 0, env = 0, k = 0, fs = 48000;
        double lf = -1, lq = -1, lg = -1e9; int ls = -1; FilterType lt = FilterType::Bell; bool ln = false;
        bool init = false;
        std::atomic<float> kOut { 0.f };

        void prepare (double sr)
        {
            fs = sr; sFreq.reset (sr, 0.02); sQ.reset (sr, 0.02); sGain.reset (sr, 0.02);
            clearAll(); init = false; mix = 0; k = 0; env = 0; lf = -1;
        }
        void clearAll() { for (auto& s : sec) s.clear(); det.clear(); }

        void setTarget (const BandParams& np)
        {
            if (! init) { sFreq.setCurrentAndTargetValue (np.freq); sQ.setCurrentAndTargetValue (np.q); sGain.setCurrentAndTargetValue (np.gain); init = true; }
            else        { sFreq.setTargetValue (np.freq); sQ.setTargetValue (np.q); sGain.setTargetValue (np.gain); }
            p = np;
        }

        void design (double f, double q, double ge, bool natural)
        {
            const int old = nSec;
            switch (p.type)
            {
                case FilterType::Bell:      nSec = 1; sec[0].c = natural ? peakAnalogMatched (fs, f, ge, q) : peak (fs, f, ge, q); break;
                case FilterType::LowShelf:  nSec = 1; sec[0].c = lowShelf  (fs, f, ge, q); break;
                case FilterType::HighShelf: nSec = 1; sec[0].c = highShelf (fs, f, ge, q); break;
                case FilterType::Notch:     nSec = 1; sec[0].c = notch (fs, f, q); break;
                case FilterType::Tilt:      nSec = 2; sec[0].c = lowShelf (fs, f, -ge / 2, q); sec[1].c = highShelf (fs, f, ge / 2, q); break;
                case FilterType::LowCut: case FilterType::HighCut:
                {
                    auto cc = designCut (p.type == FilterType::LowCut, fs, f, p.slope);
                    nSec = cc.n; for (int i = 0; i < cc.n; ++i) sec[(size_t) i].c = cc.c[(size_t) i];
                    break;
                }
            }
            if (hasGain (p.type) && std::abs (ge) < 0.003) nSec = 0;     // idle: zero cost
            if (nSec != old) for (auto& s : sec) s.clear();
        }

        void update (int m, bool natural)
        {
            const double f = sFreq.skip (m), q = sQ.skip (m), g = sGain.skip (m);
            const bool dyn = p.dynOn && canDyn (p.type);
            const double ge = g + (dyn ? p.range * k : 0.0);
            if (f != lf || q != lq || ge != lg || p.slope != ls || p.type != lt || natural != ln)
            {
                design (f, q, ge, natural);
                lf = f; lq = q; lg = ge; ls = p.slope; lt = p.type; ln = natural;
            }
        }

        void detect (const float* l, const float* r, int n)
        {
            const double f = sFreq.getCurrentValue(), q = sQ.getCurrentValue();
            det.c = bandPass (fs, f, p.wide ? std::max (0.25, q * 0.25) : std::max (0.5, q));
            const double ac = std::exp (-1.0 / (fs * 0.001)), rc = std::exp (-1.0 / (fs * 0.030));
            for (int i = 0; i < n; ++i)
            {
                float a0, a1; getLanes (p.ch, l[i], r[i], a0, a1);
                const double x = std::max (std::abs (det.tick (0, a0)), std::abs (det.tick (1, a1)));
                env = x > env ? ac * env + (1 - ac) * x : rc * env + (1 - rc) * x;
            }
            const double level = 20.0 * std::log10 (env + 1e-9);
            const double tgt = p.expand ? juce::jlimit (0.0, 1.0, (p.thr - level) / 8.0)     // soft 8 dB knee
                                        : juce::jlimit (0.0, 1.0, (level - p.thr) / 8.0);
            const double tau = std::max (1e-4, (tgt > k ? p.atk : p.rel) / 1000.0);
            k += (tgt - k) * (1.0 - std::exp (-(double) n / fs / tau));
            kOut.store ((float) k, std::memory_order_relaxed);
        }

        void process (float* l, float* r, int n, bool applyFilters, bool natural, double sr)
        {
            fs = sr;
            if (! p.on && mix <= 0.0)
            {
                if (k != 0.0) { k = 0; env = 0; kOut.store (0.f, std::memory_order_relaxed); }
                for (auto& s : sec) s.clear();
                return;
            }
            update (n, natural);
            if (p.on && p.dynOn && canDyn (p.type)) detect (l, r, n);
            else if (k != 0.0) { k = 0; kOut.store (0.f, std::memory_order_relaxed); }

            const double target = p.on ? 1.0 : 0.0, step = 1.0 / (0.010 * fs);
            if (! applyFilters || nSec == 0) { mix = target; return; }

            const bool use0 = p.ch != ChannelMode::Right && p.ch != ChannelMode::Side;
            const bool use1 = p.ch != ChannelMode::Left  && p.ch != ChannelMode::Mid;
            for (int i = 0; i < n; ++i)
            {
                mix += juce::jlimit (-step, step, target - mix);
                const float l0 = l[i], r0 = r[i];
                float a0, a1; getLanes (p.ch, l0, r0, a0, a1);
                double y0 = a0, y1 = a1;
                for (int s = 0; s < nSec; ++s) { if (use0) y0 = sec[(size_t) s].tick (0, y0); if (use1) y1 = sec[(size_t) s].tick (1, y1); }
                float ol = l0, or_ = r0; putLanes (p.ch, ol, or_, (float) y0, (float) y1);
                l[i] = l0 + (float) mix * (ol - l0);
                r[i] = r0 + (float) mix * (or_ - r0);
            }
        }
    };

    std::array<Band, kMaxBands> bands;
    double fs = 48000; bool natural = false;
};
}
