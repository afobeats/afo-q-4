// Minimal stand-in for the two JUCE helpers EqEngine uses, so the engine can be tested without JUCE.
#pragma once
#include <algorithm>
#include <cmath>
namespace juce {
template <class T> T jlimit (T lo, T hi, T v) { return std::min (hi, std::max (lo, v)); }
namespace ValueSmoothingTypes { struct Linear {}; struct Multiplicative {}; }
template <class T, class Type = ValueSmoothingTypes::Linear> struct SmoothedValue
{
    T cur {}, tgt {}, step {}; int steps = 0, total = 1;
    void reset (double sr, double sec) { total = (int) (sr * sec); }
    void setCurrentAndTargetValue (T v) { cur = tgt = v; steps = 0; }
    void setTargetValue (T v) { if (v == tgt) return; tgt = v; steps = total; }
    T skip (int n) { for (int i = 0; i < n && steps > 0; ++i, --steps) cur += (tgt - cur) / (T) (steps); return cur; }
    T getCurrentValue() const { return cur; }
};
}
