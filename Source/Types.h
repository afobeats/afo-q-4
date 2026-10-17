#pragma once
// JUCE-free types shared by the DSP code (so the DSP can be unit-tested stand-alone).
namespace afoq
{
constexpr int kMaxBands = 24;
constexpr int kSlopes[9] = { 6, 12, 18, 24, 36, 48, 72, 96, 0 };   // 0 = brickwall

enum class FilterType  { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, Tilt };
enum class ChannelMode { Stereo, Left, Right, Mid, Side };
enum class PhaseMode   { ZeroLatency, Natural, Linear };

inline bool isCut  (FilterType t) { return t == FilterType::LowCut || t == FilterType::HighCut; }
inline bool hasGain(FilterType t) { return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf || t == FilterType::Tilt; }
inline bool canDyn (FilterType t) { return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf; }

// One band's parameters, snapshotted from the parameter tree once per block.
struct BandParams
{
    bool        on = false;
    FilterType  type = FilterType::Bell;
    double      freq = 1000.0, gain = 0.0, q = 1.0;
    int         slope = 12;                       // dB/oct, 0 = brickwall (cuts only)
    ChannelMode ch = ChannelMode::Stereo;
    bool        dynOn = false, expand = false, wide = false;
    double      thr = -30.0, range = -6.0;        // dBFS, dB (signed)
    double      atk = 15.0, rel = 180.0;          // ms
};
}
