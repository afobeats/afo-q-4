#pragma once
#include "../Types.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace afoq
{
constexpr double kPi = 3.14159265358979323846;

struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };   // a0 normalised to 1

inline Coeffs norm (double b0, double b1, double b2, double a0, double a1, double a2)
{ return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 }; }

inline double w0Of (double fs, double f) { return 2.0 * kPi * std::min (f, 0.45 * fs) / fs; }

// ---- RBJ cookbook ----------------------------------------------------------------------
inline Coeffs peak (double fs, double f, double gDb, double q)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q), A = std::pow (10.0, gDb / 40.0);
    return norm (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
}
inline Coeffs lowShelf (double fs, double f, double gDb, double q)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q), A = std::pow (10.0, gDb / 40.0), be = 2 * std::sqrt (A) * al;
    return norm (A * ((A + 1) - (A - 1) * cw + be), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - be),
                 (A + 1) + (A - 1) * cw + be, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - be);
}
inline Coeffs highShelf (double fs, double f, double gDb, double q)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q), A = std::pow (10.0, gDb / 40.0), be = 2 * std::sqrt (A) * al;
    return norm (A * ((A + 1) + (A - 1) * cw + be), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - be),
                 (A + 1) - (A - 1) * cw + be, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - be);
}
inline Coeffs notch (double fs, double f, double q)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return norm (1, -2 * cw, 1, 1 + al, -2 * cw, 1 - al);
}
inline Coeffs bandPass (double fs, double f, double q)           // constant 0 dB peak gain (detector)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return norm (al, 0, -al, 1 + al, -2 * cw, 1 - al);
}
inline Coeffs biquadPass (bool highpass, double fs, double f, double q)
{
    const double w = w0Of (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return highpass ? norm ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al)
                    : norm ((1 - cw) / 2,   1 - cw,  (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Coeffs firstOrder (bool highpass, double fs, double f)
{
    const double K = std::tan (w0Of (fs, f) / 2.0), d = 1.0 + K;
    return highpass ? Coeffs { 1 / d, -1 / d, 0, (K - 1) / d, 0 } : Coeffs { K / d, K / d, 0, (K - 1) / d, 0 };
}

// ---- "Natural" bell: Orfanidis analog-matched peaking EQ ---------------------------------
// Same centre gain and half-gain bandwidth as the analog prototype, but the response does not
// cramp towards Nyquist, so high-frequency bells keep their analog shape and phase behaviour.
inline Coeffs peakAnalogMatched (double fs, double f, double gDb, double q)
{
    if (std::abs (gDb) < 1e-4) return {};
    const double w = w0Of (fs, f), G = std::pow (10.0, gDb / 20.0);
    const double dw = std::min (w / q, kPi * 0.98);
    const double beta = std::tan (dw / 2.0) / std::sqrt (G);
    const double d = 1.0 + beta, c = -2.0 * std::cos (w) / d;
    return { (1 + G * beta) / d, c, (1 - G * beta) / d, c, (1 - beta) / d };
}

// ---- Cuts: Butterworth cascades (6 dB/oct per order; brickwall = order 32) ----------------
struct CutCascade { int n = 0; std::array<Coeffs, 16> c {}; };
inline int cutOrder (int slope) { return slope == 0 ? 32 : std::max (1, slope / 6); }

inline CutCascade designCut (bool highpass, double fs, double f, int slope)
{
    CutCascade o; const int N = cutOrder (slope);
    if (N & 1) o.c[(size_t) o.n++] = firstOrder (highpass, fs, f);
    for (int k = 1; k <= N / 2; ++k)
        o.c[(size_t) o.n++] = biquadPass (highpass, fs, f, 1.0 / (2.0 * std::sin ((2 * k - 1) * kPi / (2.0 * N))));
    return o;
}

// ---- Magnitude evaluation (used for the linear-phase IR design and for UI curves) --------
inline double magDb (const Coeffs& c, double w)
{
    const double c1 = std::cos (w), c2 = std::cos (2 * w), s1 = std::sin (w), s2 = std::sin (2 * w);
    const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = c.b1 * s1 + c.b2 * s2;
    const double dr = 1 + c.a1 * c1 + c.a2 * c2,    di = c.a1 * s1 + c.a2 * s2;
    return 10.0 * std::log10 ((nr * nr + ni * ni + 1e-30) / (dr * dr + di * di + 1e-30));
}

struct BandEval
{
    FilterType type = FilterType::Bell; Coeffs c1, c2; double fc = 1000, order = 2;

    static BandEval make (const BandParams& p, double gainDb, double fs, bool natural)
    {
        BandEval e; e.type = p.type; e.fc = p.freq; e.order = cutOrder (p.slope);
        switch (p.type)
        {
            case FilterType::Bell:      e.c1 = natural ? peakAnalogMatched (fs, p.freq, gainDb, p.q) : peak (fs, p.freq, gainDb, p.q); break;
            case FilterType::LowShelf:  e.c1 = lowShelf  (fs, p.freq, gainDb, p.q); break;
            case FilterType::HighShelf: e.c1 = highShelf (fs, p.freq, gainDb, p.q); break;
            case FilterType::Notch:     e.c1 = notch     (fs, p.freq, p.q); break;
            case FilterType::Tilt:      e.c1 = lowShelf (fs, p.freq, -gainDb / 2, p.q); e.c2 = highShelf (fs, p.freq, gainDb / 2, p.q); break;
            default: break;
        }
        return e;
    }
    double db (double f, double fs) const
    {
        const double w = 2.0 * kPi * f / fs;
        switch (type)
        {
            case FilterType::LowCut:  return std::max (-150.0, -10.0 * std::log10 (1.0 + std::pow (fc / f, 2.0 * order)));
            case FilterType::HighCut: return std::max (-150.0, -10.0 * std::log10 (1.0 + std::pow (f / fc, 2.0 * order)));
            case FilterType::Tilt:    return magDb (c1, w) + magDb (c2, w);
            default:                  return magDb (c1, w);
        }
    }
};
}
