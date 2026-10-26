#include "../Source/XYRouting.h"
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.f * pi;
constexpr float eps = 1.0e-4f;

bool near (float a, float b, float tolerance = 0.015f) { return std::abs (a - b) <= tolerance; }

void nodePosition (int i, float& x, float& y)
{
    const float a = pi * 0.5f - (float) i * (twoPi / 16.f);
    x = 0.5f + 0.5f * std::cos (a);
    y = 0.5f - 0.5f * std::sin (a);
}
}

int main()
{
    int failures = 0;
    auto check = [&] (bool ok, const char* name)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", name);
        if (! ok) ++failures;
    };

    std::array<float, 16> weights {};
    params::nodeWeights (0.5f, 0.5f, weights.data());
    bool centreSilent = true;
    for (float w : weights) centreSilent = centreSilent && std::abs (w) < eps;
    check (centreSilent, "pad centre leaves rim-routed effects dry");

    bool allNodesPeakCorrectly = true;
    for (int i = 0; i < 16; ++i)
    {
        float x, y; nodePosition (i, x, y);
        params::nodeWeights (x, y, weights.data());
        allNodesPeakCorrectly = allNodesPeakCorrectly && near (weights[(size_t) i], 1.f);
        for (int j = 0; j < 16; ++j)
            if (j != i) allNodesPeakCorrectly = allNodesPeakCorrectly && weights[(size_t) j] < 0.55f;
    }
    check (allNodesPeakCorrectly, "each of 16 nodes peaks at its exact position");

    params::nodeWeights (0.5f, 0.10f, weights.data());
    const float nearTop = weights[0];
    params::nodeWeights (0.5f, 0.20f, weights.data());
    const float fartherFromTop = weights[0];
    check (nearTop > fartherFromTop && fartherFromTop > 0.f, "mix falls smoothly as puck moves away from a node");

    float x0, y0, x1, y1; nodePosition (0, x0, y0); nodePosition (1, x1, y1);
    params::nodeWeights ((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, weights.data());
    check (weights[0] > 0.5f && weights[1] > 0.5f, "adjacent nodes blend smoothly between positions");

    check (std::all_of (weights.begin(), weights.end(), [] (float w) { return std::isfinite (w) && w >= 0.f && w <= 1.f; }),
           "weights are finite and bounded to 0..1");
    std::printf ("XY routing test %s (%d failure(s))\n", failures == 0 ? "passed" : "failed", failures);
    return failures == 0 ? 0 : 1;
}
