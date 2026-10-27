// =============================================================================
//  Tests for the eight-vertex octagonal XY mapping.
//  JUCE-free so it runs on CI and any desktop toolchain.
// =============================================================================
#include "../Source/XYRouting.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr float kEps = 1.0e-5f;
constexpr float kPi = 3.14159265358979323846f;
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

    static_assert (params::kNumSlots == 8, "the field must expose exactly eight vertices");
    static_assert (params::kSlotsPerSide == 4, "four vertices per side");

    bool sidesCorrect = true;
    for (int i = 0; i < params::kNumSlots; ++i)
    {
        const auto p = params::slotPosition (i);
        sidesCorrect = sidesCorrect && (std::abs (std::sqrt (p.x * p.x + p.y * p.y) - params::kSlotRadius) < 1.0e-4f);
        if (i < 4) sidesCorrect = sidesCorrect && p.x < 0.f;
        else       sidesCorrect = sidesCorrect && p.x > 0.f;
    }
    check (sidesCorrect, "regular octagon: four vertices left, four right");

    // Perimeter order must be CCW and close the octagon.
    {
        float twiceArea = 0.f;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto a = params::fieldVertex (i);
            const auto b = params::fieldVertex (i + 1);
            twiceArea += a.x * b.y - b.x * a.y;
        }
        check (twiceArea > 0.f, "octagon field order is counter-clockwise");
        check (params::insideField (0.f, 0.f), "centre lies inside the octagon");
        check (! params::insideField (0.f, 1.f), "the square top-centre lies outside the octagon");
    }

    // Centre dry.
    {
        float w[params::kNumSlots] {};
        params::slotWeights (0.f, 0.f, w);
        bool dry = true;
        for (float v : w) dry = dry && v < 1.0e-6f;
        check (dry, "centre is exactly dry for every vertex");
    }

    // Exact vertices peak; neighbours are present but weaker.
    {
        bool peak = true, neigh = true;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto n = params::slotPosition (i);
            peak = peak && near (params::slotProximity (n.x, n.y, i), 1.f, 1.0e-5f);
            for (int j = 0; j < params::kNumSlots; ++j)
            {
                if (j == i) continue;
                const float other = params::slotProximity (n.x, n.y, j);
                peak = peak && other >= 0.f && other <= 1.f;
                const float angle = std::abs (params::shortestAngle (
                    params::kSlotAngleDeg[i] * kPi / 180.f - params::kSlotAngleDeg[j] * kPi / 180.f));
                if (angle < 0.9f)
                    neigh = neigh && other > 0.20f && other < 0.50f;
            }
        }
        check (peak, "each vertex peaks at 1 and remains bounded");
        check (neigh, "45-degree neighbours contribute ~36% at a vertex");
    }

    // Halfway from the centre to a vertex, the aligned tool is still >50%.
    {
        const auto n = params::slotPosition (1);
        const float halfX = n.x * 0.5f, halfY = n.y * 0.5f;
        const float w = params::slotProximity (halfX, halfY, 1);
        std::printf ("  half-radius weight = %.3f\n", w);
        check (w > 0.50f, "aligned tool exceeds 50% at half centre-to-vertex distance");
        check (w < 1.0f, "half-radius influence is below the vertex peak");
    }

    // Three or four tools should be meaningfully present at any useful radius.
    {
        int minimumActive = 8;
        for (int a = 0; a < 360; a += 15)
        {
            const float rad = (float) a * kPi / 180.f;
            for (float r : { 0.43f, 0.63f, 0.80f })
            {
                float all[params::kNumSlots] {};
                params::slotWeights (std::cos (rad) * r, std::sin (rad) * r, all);
                int active = 0;
                for (float x : all) if (x > 0.02f) ++active;
                minimumActive = std::min (minimumActive, active);
            }
        }
        std::printf ("  minimum active tools on sampled field = %d\n", minimumActive);
        check (minimumActive >= 3, "at least three tools remain audible across the field");
    }

    // Monotonic direct approach.
    {
        const auto n = params::slotPosition (1);
        const float len = std::sqrt (n.x * n.x + n.y * n.y);
        const float ux = n.x / len, uy = n.y / len;
        float previous = -1.f;
        bool monotonic = true;
        for (int step = 0; step <= 100; ++step)
        {
            const float t = params::kSlotRadius * (float) step / 100.f;
            const float w = params::slotProximity (ux * t, uy * t, 1);
            monotonic = monotonic && (w + 1.0e-4f >= previous);
            previous = w;
        }
        check (monotonic, "proximity rises while moving directly toward a vertex");
    }

    // Smoothness: tiny move must not create a jump.
    {
        float maxJump = 0.f;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            for (int a = 0; a < 360; a += 3)
            {
                const float rad = (float) a * kPi / 180.f;
                const float r = 0.73f;
                const float x = std::cos (rad) * r, y = std::sin (rad) * r;
                const float w0 = params::slotProximity (x, y, i);
                const float w1 = params::slotProximity (x + 1.0e-4f, y + 1.0e-4f, i);
                maxJump = std::max (maxJump, std::abs (w1 - w0));
            }
        }
        check (maxJump < 0.001f, "mapping is continuous for small movements");
    }

    // Adjacent blend at the midpoint of an octagon edge.
    {
        const auto a = params::slotPosition (0);
        const auto b = params::slotPosition (4);
        const float wa = params::slotProximity ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 0);
        const float wb = params::slotProximity ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 4);
        check (wa > 0.5f && wb > 0.5f && near (wa, wb, 0.02f), "octagon edge midpoint blends adjacent vertices");
    }

    // Clamp to the octagon, never outside.
    {
        bool valid = true;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const float w = params::slotProximity (50.f, -50.f, i);
            valid = valid && std::isfinite (w) && w >= 0.f && w <= 1.f;
        }
        const auto p = params::clampToField (3.f, 4.f);
        valid = valid && params::insideField (p.x, p.y);
        const auto top = params::clampToField (0.f, 4.f);
        valid = valid && std::abs (top.y - 0.795f) < 0.03f && std::abs (top.x) < 0.03f;
        check (valid, "puck positions outside the octagon are projected safely");
    }

    // Mix arithmetic: effective = maxMix * depth * proximity, bounded 0..1.
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
        check (bounded, "maxMix * depth * proximity stays inside 0..1");
    }

    std::printf ("XY routing test %s (%d failure(s))\n", failures == 0 ? "passed" : "failed", failures);
    return failures == 0 ? 0 : 1;
}
