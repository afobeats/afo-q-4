#pragma once
#include <array>
#include <atomic>
#include <vector>
#include <juce_dsp/juce_dsp.h>

namespace afoq
{
/** Lock-free spectrum tap. The audio thread only writes samples into a ring buffer; the UI thread
    windows and transforms the most recent 8192 samples (~170 ms @ 48 kHz). Costs the audio thread
    one multiply-add per sample. */
class Analyzer
{
public:
    static constexpr int kOrder = 13, kSize = 1 << kOrder;

    Analyzer() : fft (kOrder)
    {
        for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.f * 3.14159265f * (float) i / (float) (kSize - 1));
        ring.fill (0.f);
    }
    void push (const float* l, const float* r, int n) noexcept
    {
        int w = wp.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) ring[(size_t) ((w + i) & (kSize - 1))] = 0.5f * (l[i] + r[i]);
        wp.store (w + n, std::memory_order_release);
    }
    /** UI thread. Fills db[0..kSize/2] with dBFS (full-scale sine = 0 dB). */
    void getSpectrum (std::vector<float>& db)
    {
        const int w = wp.load (std::memory_order_acquire);
        for (int i = 0; i < kSize; ++i) work[(size_t) i] = ring[(size_t) ((w - kSize + i) & (kSize - 1))] * window[(size_t) i];
        std::fill (work.begin() + kSize, work.end(), 0.f);
        fft.performFrequencyOnlyForwardTransform (work.data());
        db.resize (kSize / 2 + 1);
        for (int i = 0; i <= kSize / 2; ++i) db[(size_t) i] = juce::Decibels::gainToDecibels (work[(size_t) i] * 4.f / (float) kSize, -120.f);
    }
private:
    juce::dsp::FFT fft; std::array<float, kSize> window {}, ring {}; std::array<float, 2 * kSize> work {};
    std::atomic<int> wp { 0 };
};
}
