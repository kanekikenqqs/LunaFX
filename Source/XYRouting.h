// =============================================================================
//  Luna FX - eight-vertex octagonal XY field.
//
//  The control surface is a regular octagon. Its eight vertices are the eight
//  instrument points, four on the left half and four on the right half.
//
//  Node layout (angles counter-clockwise from +x, y points UP):
//
//        slot 4        slot 0
//             \      /
//     slot 5   \    /   slot 1
//               \  /
//               centre
//               /  \
//     slot 6   /    \   slot 2
//             /      \
//        slot 7        slot 3
//
//  Influence model
//  ---------------
//  Each vertex has a broad angular field and a radial gate:
//
//      r        = length(puck)
//      radial   = smoothstep(clamp01(r / kRadialGate))      0 at centre, 1 at rim
//      dAngle   = shortest angular distance to the vertex
//      angular  = exp(-0.5 * (dAngle / kAngularSigma)^2)     1 at the vertex
//      w_i      = radial * angular
//
//  Properties:
//    - w = 1 at the vertex.
//    - w = 0 at the centre.
//    - at half the centre-to-vertex distance, the aligned vertex is ~54%.
//    - at a vertex, the two 45-degree neighbours are ~36% each, so three or
//      four tools are always meaningfully active across the field.
//    - the Gaussian angular field overlaps smoothly; there are no sectors,
//      hard borders, or angular jumps.
//
//  The puck is clamped to the octagon. Only this header defines the mapping;
//  the engine and the UI both use it.
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
constexpr float kRadialGate        = 0.82f;   // smooth radial fade-in
constexpr float kAngularSigmaRad   = 0.55f;   // ~32 degrees, broad overlap

struct SlotPosition { float x = 0.f, y = 0.f; };

// Left side top -> bottom, then right side top -> bottom. This is a regular
// octagon rotated by 22.5 degrees, which gives exactly four vertices on each
// half of the field.
inline constexpr float kSlotAngleDeg[kNumSlots] = {
    112.5f, 157.5f, 202.5f, 247.5f,
     67.5f,  22.5f, -22.5f, -67.5f
};

// Perimeter order around the octagon (counter-clockwise).
inline constexpr int kFieldOrder[kNumSlots] = { 4, 0, 1, 2, 3, 7, 6, 5 };

inline float clamp01 (float v) noexcept { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

inline float smoothstep01 (float v) noexcept
{
    v = clamp01 (v);
    return v * v * (3.f - 2.f * v);
}

inline SlotPosition slotPosition (int slot) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return { 0.f, 0.f };
    constexpr float degToRad = 3.14159265358979323846f / 180.f;
    const float a = kSlotAngleDeg[slot] * degToRad;
    return { kSlotRadius * std::cos (a), kSlotRadius * std::sin (a) };
}

inline SlotPosition fieldVertex (int perimeterIndex) noexcept
{
    return slotPosition (kFieldOrder[(size_t) (perimeterIndex % kNumSlots)]);
}

inline bool insideField (float x, float y) noexcept
{
    // Convex polygon; the field order is counter-clockwise.
    constexpr float eps = -1.0e-6f;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto a = fieldVertex (i);
        const auto b = fieldVertex (i + 1);
        const float cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        if (cross < eps) return false;
    }
    return true;
}

// Projects a point onto the convex octagon. Used by the UI so the puck can
// never leave the field, and by the audio mapping so automation is safe.
inline SlotPosition clampToField (float x, float y) noexcept
{
    if (insideField (x, y)) return { x, y };

    float bestX = 0.f, bestY = 0.f;
    float bestD = 3.4e38f;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto a = fieldVertex (i);
        const auto b = fieldVertex (i + 1);
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        float t = len2 > 0.f ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 0.f;
        t = std::max (0.f, std::min (1.f, t));
        const float qx = a.x + t * dx, qy = a.y + t * dy;
        const float d = (x - qx) * (x - qx) + (y - qy) * (y - qy);
        if (d < bestD) { bestD = d; bestX = qx; bestY = qy; }
    }
    return { bestX, bestY };
}

inline float shortestAngle (float a) noexcept
{
    constexpr float twoPi = 6.28318530717958647692f;
    constexpr float pi = 3.14159265358979323846f;
    a = std::fmod (a + pi, twoPi);
    if (a < 0.f) a += twoPi;
    return a - pi;
}

// Influence for an already-clamped point. The UI and the processor call the
// public overload below; this one exists to avoid clamping eight times.
inline float slotProximityAt (const SlotPosition& p, int slot) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return 0.f;
    const auto n = slotPosition (slot);
    const float r = std::sqrt (p.x * p.x + p.y * p.y);
    const float radial = smoothstep01 (r / kRadialGate);

    const float nodeAngle = std::atan2 (n.y, n.x);
    const float puckAngle = std::atan2 (p.y, p.x);
    const float dAngle = std::abs (shortestAngle (puckAngle - nodeAngle));
    const float angular = std::exp (-0.5f * (dAngle / kAngularSigmaRad) * (dAngle / kAngularSigmaRad));

    return clamp01 (radial * angular);
}

inline float slotProximity (float x, float y, int slot) noexcept
{
    const auto p = clampToField (x, y);
    return slotProximityAt (p, slot);
}

inline void slotWeights (float x, float y, float* w) noexcept
{
    if (w == nullptr) return;
    const auto p = clampToField (x, y);
    for (int i = 0; i < kNumSlots; ++i)
        w[i] = slotProximityAt (p, i);
}

inline std::array<float, kNumSlots> slotWeights (float x, float y) noexcept
{
    std::array<float, kNumSlots> w {};
    slotWeights (x, y, w.data());
    return w;
}
} // namespace params
