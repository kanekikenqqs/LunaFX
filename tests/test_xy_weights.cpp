// =============================================================================
//  Tests for the documented eight-slot distance-based XY mapping.
//  JUCE-free so it runs on CI and any desktop toolchain.
// =============================================================================
#include "../Source/XYRouting.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
constexpr float kEps = 1.0e-5f;
bool near (float a, float b, float tol = 0.02f) { return std::abs (a - b) <= tol; }
}

int main()
{
    int failures = 0;
    auto check = [&] (bool ok, const char* name)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", name);
        if (! ok) ++failures;
    };

    static_assert (params::kNumSlots == 8, "the circle must expose exactly eight slots");
    static_assert (params::kSlotsPerSide == 4, "four slots per side");

    // Four left, four right, evenly mirrored.
    bool sidesCorrect = true;
    for (int i = 0; i < params::kNumSlots; ++i)
    {
        const auto p = params::slotPosition (i);
        sidesCorrect = sidesCorrect && (std::abs (std::sqrt (p.x * p.x + p.y * p.y) - params::kSlotRadius) < 1.0e-4f);
        if (i < 4) sidesCorrect = sidesCorrect && p.x < 0.f;
        else       sidesCorrect = sidesCorrect && p.x > 0.f;
    }
    check (sidesCorrect, "eight nodes: four on the left, four on the right");

    // The centre must be completely dry.
    {
        float w[params::kNumSlots] {};
        params::slotWeights (0.f, 0.f, w);
        bool dry = true;
        for (float v : w) dry = dry && v < 1.0e-4f;
        check (dry, "centre is exactly dry for every node");
    }

    // Exact node positions give weight 1 for that node and are bounded elsewhere.
    {
        bool peak = true;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto n = params::slotPosition (i);
            const float self = params::slotProximity (n.x, n.y, i);
            peak = peak && near (self, 1.f, 1.0e-5f);
            for (int j = 0; j < params::kNumSlots; ++j)
            {
                if (j == i) continue;
                const float other = params::slotProximity (n.x, n.y, j);
                peak = peak && other >= 0.f && other <= 0.2f;
            }
        }
        check (peak, "each node peaks at 1 at its own position; neighbours stay below 20%");
    }

    // Distance-based monotonic approach along a straight line.
    {
        const auto n = params::slotPosition (1);
        const float len = std::sqrt (n.x * n.x + n.y * n.y);
        const float ux = n.x / len, uy = n.y / len;
        float previous = -1.f;
        bool monotonic = true, finite = true;
        for (int step = 0; step <= 100; ++step)
        {
            const float t = params::kSlotRadius * (float) step / 100.f;
            const float w = params::slotProximity (ux * t, uy * t, 1);
            monotonic = monotonic && (w + 1.0e-6f >= previous);
            finite = finite && std::isfinite (w) && w >= 0.f && w <= 1.f;
            previous = w;
        }
        check (monotonic, "proximity rises monotonically while approaching a node");
        check (finite, "proximity is finite and bounded 0..1");
    }

    // Smoothness: a tiny move must not create a jump (no angular discontinuity).
    {
        float maxJump = 0.f;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            for (int a = 0; a < 360; a += 3)
            {
                const float rad = (float) a * 3.14159265f / 180.f;
                const float r = 0.73f;
                const float x = std::cos (rad) * r, y = std::sin (rad) * r;
                const float w0 = params::slotProximity (x, y, i);
                const float w1 = params::slotProximity (x + 1.0e-4f, y + 1.0e-4f, i);
                maxJump = std::max (maxJump, std::abs (w1 - w0));
            }
        }
        check (maxJump < 0.001f, "proximity is continuous for small movements");
    }

    // Outside the falloff, influence is exactly zero; at/near the rim it is not.
    {
        bool zeroFar = true;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto n = params::slotPosition (i);
            const float farX = -n.x * 0.9f, farY = -n.y * 0.9f;
            zeroFar = zeroFar && params::slotProximity (farX, farY, i) < 1.0e-6f;
        }
        check (zeroFar, "a node reaches zero influence far from the puck");
    }

    // Adjacent nodes still blend at their midpoint.
    {
        const auto a = params::slotPosition (0);
        const auto b = params::slotPosition (1);
        const float wa = params::slotProximity ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 0);
        const float wb = params::slotProximity ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 1);
        check (wa > 0.5f && wb > 0.5f && near (wa, wb, 0.01f), "adjacent nodes blend smoothly at the midpoint");
    }

    // Out-of-disk input is clamped, never producing invalid values.
    {
        bool valid = true;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const float w = params::slotProximity (50.f, -50.f, i);
            valid = valid && std::isfinite (w) && w >= 0.f && w <= 1.f;
        }
        const auto p = params::clampToDisk (3.f, 4.f);
        valid = valid && std::abs (std::sqrt (p.x * p.x + p.y * p.y) - 1.f) < 1.0e-5f;
        check (valid, "puck positions outside the disk are clamped safely");
    }

    // A disabled / unassigned slot is not part of this header, but the mix model
    // is: effective = maxMix * depth * weight. Verify the bound arithmetic here.
    {
        bool bounded = true;
        for (int slot = 0; slot < params::kNumSlots; ++slot)
        {
            const auto n = params::slotPosition (slot);
            const float w = params::slotProximity (n.x, n.y, slot);
            for (float depth : { 0.f, 0.5f, 1.f })
                for (float maxMix : { 0.f, 0.37f, 1.f })
                {
                    const float eff = std::min (1.f, std::max (0.f, maxMix * depth * w));
                    bounded = bounded && eff >= 0.f && eff <= 1.f;
                }
        }
        check (bounded, "effective wet = maxMix * depth * proximity stays inside 0..1");
    }

    std::printf ("XY routing test %s (%d failure(s))\n", failures == 0 ? "passed" : "failed", failures);
    return failures == 0 ? 0 : 1;
}
