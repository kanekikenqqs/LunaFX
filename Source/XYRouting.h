// =============================================================================
//  Luna FX - eight-slot XY proximity routing.
//
//  This header is intentionally independent of JUCE so the exact same mapping
//  can be unit-tested on CI and reasoned about without a host.
//
//  Layout (normalised unit disk, y points UP):
//
//        slot 0        slot 4
//     slot 1  \      /  slot 5
//              \    /
//     slot 2  /    \  slot 6
//        slot 3        slot 7
//
//  Slots 0..3 are on the left half, slots 4..7 on the right half. Every slot is
//  a fixed instrument node on the rim at radius kSlotRadius.
//
//  Influence model
//  ---------------
//  For a puck at (x, y) and node i at (nx, ny):
//
//      d_i = sqrt((x - nx)^2 + (y - ny)^2)          Euclidean distance
//      s_i = clamp01(1 - d_i / kProximityFalloff)   linear distance window
//      w_i = smoothstep(s_i) = s_i^2 * (3 - 2*s_i)  C1-continuous falloff
//
//  w_i is exactly 1 when the puck sits on the node and exactly 0 when the puck
//  is farther than kProximityFalloff away. smoothstep gives a zero slope at
//  both ends, so there is no sudden jump or angular discontinuity as the puck
//  crosses a zone boundary or orbits the centre. The function is radial, not
//  angular: it depends only on physical distance, never on the angle of the
//  node or on which side of the circle the puck is.
//
//  kProximityFalloff = 0.62 (in unit-disk units) was chosen so that:
//    - the centre is completely dry (nearest node is 0.86 away),
//    - a single node dominates near the rim (adjacent node gets ~11%),
//    - neighbours still crossfade smoothly at their midpoint (~66% each).
//
//  The audio engine multiplies this proximity by the slot's maximum mix and by
//  the XY depth control; the result is clamped to 0..1. An unassigned or
//  disabled slot is forced to zero and therefore stays effectively dry.
// =============================================================================
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace params
{
constexpr int   kNumSlots          = 8;
constexpr int   kSlotsPerSide      = 4;
constexpr float kSlotRadius        = 0.86f;
constexpr float kProximityFalloff  = 0.62f;

struct SlotPosition { float x = 0.f, y = 0.f; };

// Angles in degrees measured counter-clockwise from +x, with +y pointing up.
// Left side runs top -> bottom; left/right are mirrored.
inline constexpr float kSlotAngleDeg[kNumSlots] = {
    130.0f, 163.33333f, 196.66667f, 230.0f,
     50.0f,  16.66667f, -16.66667f, -50.0f
};

inline float clamp01 (float v) noexcept { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

inline SlotPosition slotPosition (int slot) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return { 0.f, 0.f };
    constexpr float degToRad = 3.14159265358979323846f / 180.f;
    const float a = kSlotAngleDeg[slot] * degToRad;
    return { kSlotRadius * std::cos (a), kSlotRadius * std::sin (a) };
}

// Clamps a puck position into the unit disk while preserving its direction.
inline SlotPosition clampToDisk (float x, float y) noexcept
{
    const float len = std::sqrt (x * x + y * y);
    if (len > 1.f && len > 0.f) { x /= len; y /= len; }
    return { x, y };
}

// Distance-based influence for one slot, in 0..1. Continuous in x and y.
inline float slotProximity (float x, float y, int slot,
                            float falloff = kProximityFalloff) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return 0.f;
    const auto p = clampToDisk (x, y);
    const auto n = slotPosition (slot);
    const float dx = p.x - n.x, dy = p.y - n.y;
    const float d = std::sqrt (dx * dx + dy * dy);
    const float f = std::max (1.0e-4f, falloff);
    const float s = clamp01 (1.f - d / f);
    return s * s * (3.f - 2.f * s);
}

// Fills w[8] with the proximity of every slot. w may not be null.
inline void slotWeights (float x, float y, float* w,
                         float falloff = kProximityFalloff) noexcept
{
    if (w == nullptr) return;
    const auto p = clampToDisk (x, y);
    for (int i = 0; i < kNumSlots; ++i)
        w[i] = slotProximity (p.x, p.y, i, falloff);
}

// Convenience helper used by the UI and by documentation examples.
inline std::array<float, kNumSlots> slotWeights (float x, float y) noexcept
{
    std::array<float, kNumSlots> w {};
    slotWeights (x, y, w.data());
    return w;
}
} // namespace params
