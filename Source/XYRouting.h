// =============================================================================
// Luna FX - XY proximity routing, kept independent of JUCE for deterministic tests.
#pragma once
#include <algorithm>
#include <cmath>

namespace params
{
// x/y are 0..1, with y=0 at the top. Each of 16 fixed nodes is a point on the
// ring. The closest node gets the strongest mix; influence falls off by actual
// Euclidean distance, so the centre stays clean and adjacent nodes blend smoothly.
inline void nodeWeights (float x, float y, float* w) // w[16], node 0=top, clockwise
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kFalloffRadius = 0.34f;
    constexpr float kFalloffCurve = 1.35f;
    x = std::max (0.f, std::min (1.f, x));
    y = std::max (0.f, std::min (1.f, y));

    for (int i = 0; i < 16; ++i)
    {
        const float angle = kPi * 0.5f - (float) i * (kTwoPi / 16.f);
        const float nx = 0.5f + 0.5f * std::cos (angle);
        const float ny = 0.5f - 0.5f * std::sin (angle);
        const float dx = x - nx, dy = y - ny;
        const float distance = std::sqrt (dx * dx + dy * dy);
        const float proximity = std::max (0.f, std::min (1.f, 1.f - distance / kFalloffRadius));
        w[i] = std::pow (proximity, kFalloffCurve);
    }
}
} // namespace params
