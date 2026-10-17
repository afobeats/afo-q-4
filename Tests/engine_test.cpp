// g++ -std=c++17 -O2 -ITests/stub -ISource Tests/engine_test.cpp
#include "DSP/EqEngine.h"
#include <cstdio>
#include <vector>
using namespace afoq;
static double rms (const std::vector<float>& v, size_t a) { double s = 0; for (size_t i = a; i < v.size(); ++i) s += v[i] * v[i]; return std::sqrt (s / (v.size() - a)); }
static std::vector<float> sine (double f, double fs, size_t n, double amp) { std::vector<float> v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2 * kPi * f * i / fs)); return v; }
static int fails = 0;
static void expect (const char* n, double got, double want, double tol) { const bool ok = std::abs (got - want) <= tol; if (! ok) ++fails; std::printf ("%s %-40s got %8.3f want %8.3f\n", ok ? "ok  " : "FAIL", n, got, want); }

int main()
{
    const double fs = 48000; const size_t N = 96000;
    auto run = [&] (BandParams bp, double tone, double amp, bool natural = false)
    {
        static EqEngine* e; delete e; e = new EqEngine; e->prepare (fs); e->setNatural (natural);
        std::array<BandParams, kMaxBands> p {}; bp.on = true; p[0] = bp;
        auto l = sine (tone, fs, N, amp), r = l;
        for (size_t pos = 0; pos < N; pos += 512) { e->setParams (p); e->process (l.data() + pos, r.data() + pos, (int) std::min<size_t> (512, N - pos), true); }
        return std::make_pair (20 * std::log10 (rms (l, N / 2) / (amp / std::sqrt (2.0))), e);
    };
    BandParams bell; bell.freq = 1000; bell.gain = 9; bell.q = 2;
    expect ("bell +9 dB at 1 kHz (stereo)", run (bell, 1000, .25).first, 9.0, .05);
    BandParams hp; hp.type = FilterType::LowCut; hp.freq = 1000; hp.slope = 24;
    expect ("24 dB/oct low cut: -3 dB at corner", run (hp, 1000, .25).first, -3.01, .1);
    expect ("24 dB/oct low cut: -24 dB one oct below", run (hp, 500, .25).first, -24.7, 1.0);
    BandParams side = bell; side.ch = ChannelMode::Side;
    expect ("Side band leaves a mono signal untouched", run (side, 1000, .25).first, 0.0, .01);
    BandParams dyn = bell; dyn.gain = 0; dyn.dynOn = true; dyn.range = -9; dyn.thr = -30; dyn.atk = 5; dyn.rel = 50;
    expect ("dynamic: loud tone ducked by ~range", run (dyn, 1000, .5).first, -9.0, 1.0);
    expect ("dynamic: quiet tone untouched",       run (dyn, 1000, .005).first, 0.0, .05);
    BandParams ex = dyn; ex.expand = true; ex.range = 6; ex.thr = -30;
    expect ("expand: quiet tone boosted",          run (ex, 1000, .005).first, 6.0, 1.0);
    expect ("natural bell +9 dB at 1 kHz",         run (bell, 1000, .25, true).first, 9.0, .05);

    EqEngine e; e.prepare (fs);   // idle bands cost nothing & never alter audio
    std::array<BandParams, kMaxBands> p {}; auto l = sine (1000, fs, 4096, .3), r = l, l0 = l;
    e.setParams (p); e.process (l.data(), r.data(), 4096, true);
    double d = 0; for (size_t i = 0; i < l.size(); ++i) d = std::max (d, (double) std::abs (l[i] - l0[i]));
    expect ("all bands off = bit-exact passthrough", d, 0.0, 0.0);
    std::printf (fails ? "\n%d FAILED\n" : "\nall passed\n", fails); return fails;
}
