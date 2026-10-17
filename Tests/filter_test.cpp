// Stand-alone sanity checks for the filter maths:  g++ -std=c++17 -I../Source Tests/filter_test.cpp
#include "DSP/FilterDesign.h"
#include <cstdio>
using namespace afoq;
static int fails = 0;
static void expect (const char* n, double got, double want, double tol)
{ const bool ok = std::abs (got - want) <= tol; if (! ok) ++fails; std::printf ("%s %-34s got %9.4f want %9.4f\n", ok ? "ok  " : "FAIL", n, got, want); }

int main()
{
    const double fs = 48000;
    auto at = [&] (const Coeffs& c, double f) { return magDb (c, 2 * kPi * f / fs); };
    expect ("bell centre gain +9 dB",        at (peak (fs, 1000, 9, 2), 1000), 9, 0.01);
    expect ("bell far-away is flat",         at (peak (fs, 1000, 9, 2), 20),   0, 0.2);
    expect ("analog bell centre -12 dB",     at (peakAnalogMatched (fs, 1000, -12, 1.5), 1000), -12, 0.01);
    expect ("analog bell 16k keeps gain",    at (peakAnalogMatched (fs, 16000, 12, 1), 16000), 12, 0.01);
    expect ("low shelf +6 at DC",            at (lowShelf (fs, 200, 6, 0.707), 5), 6, 0.1);
    expect ("high shelf +6 at 20k",          at (highShelf (fs, 2000, 6, 0.707), 20000), 6, 0.3);
    expect ("notch deeper than -60 dB",       at (notch (fs, 1000, 8), 1000) < -60.0 ? 1.0 : 0.0, 1.0, 0.0);
    for (int s : { 6, 12, 18, 24, 36, 48, 72, 96, 0 })
    {
        auto cc = designCut (true, fs, 1000, s); double tot = 0, tot1 = 0, tot2 = 0;
        for (int i = 0; i < cc.n; ++i) { tot += at (cc.c[(size_t) i], 1000); tot1 += at (cc.c[(size_t) i], 500); tot2 += at (cc.c[(size_t) i], 250); }
        char n[64]; std::snprintf (n, 64, "HP %d dB/oct: -3 dB at corner", s);
        expect (n, tot, -3.0103, 0.05);
        const double oct = (s == 0 ? 192 : s);            // brickwall = order 32
        std::snprintf (n, 64, "HP %d dB/oct: slope 1->2 oct below", s);
        if (s != 0 && s <= 48) expect (n, tot2 - tot1, -oct, oct * 0.12);
    }
    auto lc = designCut (false, fs, 5000, 24); double t = 0; for (int i = 0; i < lc.n; ++i) t += at (lc.c[(size_t) i], 5000);
    expect ("LP 24 dB/oct -3 dB at corner", t, -3.0103, 0.05);
    BandEval e = BandEval::make (BandParams { true, FilterType::Tilt, 1000, 6, 0.5 }, 6, fs, false);
    expect ("tilt pivot ~0 dB",              e.db (1000, fs), 0, 0.1);
    std::printf (fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails;
}
