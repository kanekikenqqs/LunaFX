#include "../Source/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

int main()
{
    using namespace djx;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int blocksPerEffect = 16; // 2048 samples per effect

    Engine engine;
    engine.prepare (sampleRate);
    Tempo tempo;
    tempo.bpm = 137.0;
    tempo.playing = true;

    float left[blockSize] {}, right[blockSize] {};
    int failures = 0;
    for (int fx = 0; fx < kNumFx; ++fx)
    {
        engine.reset();
        Settings settings;
        settings.engaged = true;
        settings.amt[(size_t) fx] = 1.0f;
        settings.revDecay = 12.0f;
        settings.dlyFb = 0.9f;
        settings.shFb = 0.8f;
        double beat = 0.0;
        bool good = true;
        for (int block = 0; block < blocksPerEffect; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const double t = (block * blockSize + i) / sampleRate;
                left[i] = (float) (0.25 * std::sin (2.0 * 3.141592653589793 * 73.0 * t));
                right[i] = (float) (0.20 * std::sin (2.0 * 3.141592653589793 * 110.0 * t));
            }
            tempo.beat = beat;
            engine.process (left, right, blockSize, settings, tempo);
            beat += blockSize * tempo.bpm / (60.0 * sampleRate);
            for (int i = 0; i < blockSize; ++i)
            {
                if (! std::isfinite (left[i]) || ! std::isfinite (right[i])
                    || std::abs (left[i]) > 4.001f || std::abs (right[i]) > 4.001f)
                    good = false;
            }
        }
        std::printf ("[%s] %s\n", good ? "PASS" : "FAIL", kFxNames[fx]);
        if (! good) ++failures;
    }

    engine.reset();
    Settings all;
    all.engaged = true;
    all.dlyFb = 0.95f;
    all.shFb = 0.85f;
    all.revDecay = 20.0f;
    for (auto& amount : all.amt) amount = 1.0f;
    bool allGood = true;
    for (int block = 0; block < 32; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float v = (i % 48 < 24) ? 0.25f : -0.25f;
            left[i] = v;
            right[i] = -v * 0.8f;
        }
        tempo.beat = block * blockSize * tempo.bpm / (60.0 * sampleRate);
        engine.process (left, right, blockSize, all, tempo);
        for (int i = 0; i < blockSize; ++i)
            if (! std::isfinite (left[i]) || ! std::isfinite (right[i])
                || std::abs (left[i]) > 4.001f || std::abs (right[i]) > 4.001f)
                allGood = false;
    }
    std::printf ("[%s] all-effects stress combination\n", allGood ? "PASS" : "FAIL");
    if (! allGood) ++failures;

    std::printf ("Smoke test %s (%d failure(s))\n", failures == 0 ? "passed" : "failed", failures);
    return failures == 0 ? 0 : 1;
}
