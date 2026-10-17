#pragma once
#include "../Types.h"
namespace afoq
{
// Per-band channel routing: a band works on up to two "lanes" (L/R, or M/S).
inline void getLanes (ChannelMode m, float l, float r, float& a0, float& a1) noexcept
{
    switch (m) {
        case ChannelMode::Stereo: a0 = l; a1 = r; break;
        case ChannelMode::Left:   a0 = l; a1 = 0; break;
        case ChannelMode::Right:  a0 = 0; a1 = r; break;
        case ChannelMode::Mid:    a0 = (l + r) * 0.5f; a1 = 0; break;
        case ChannelMode::Side:   a0 = 0; a1 = (l - r) * 0.5f; break;
    }
}
inline void putLanes (ChannelMode m, float& l, float& r, float y0, float y1) noexcept
{
    switch (m) {
        case ChannelMode::Stereo: l = y0; r = y1; break;
        case ChannelMode::Left:   l = y0; break;
        case ChannelMode::Right:  r = y1; break;
        case ChannelMode::Mid:    { const float s = (l - r) * 0.5f; l = y0 + s; r = y0 - s; break; }
        case ChannelMode::Side:   { const float mid = (l + r) * 0.5f; l = mid + y1; r = mid - y1; break; }
    }
}
}
