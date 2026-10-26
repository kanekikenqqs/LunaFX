#include "../Source/Engine.h"
#include <cstdio>
#include <vector>
#include <complex>
using namespace djx;

static double sr = 48000.0;
#include <cstdlib>

static constexpr double kTestPi = 3.14159265358979323846;

// naive DFT magnitude at frequency f
static double mag(const std::vector<float>& x, int start, int len, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < len; ++i) { double w = 0.5 - 0.5 * std::cos(2 * kTestPi * i / len); double ph = 2 * kTestPi * f * (start + i) / sr; re += x[start + i] * w * std::cos(ph); im -= x[start + i] * w * std::sin(ph); }
    return std::sqrt(re * re + im * im) / len * 4;
}
static double peakFreq(const std::vector<float>& x, int start, int len, double lo, double hi)
{
    double best = 0, bf = 0;
    for (double f = lo; f < hi; f += 1.0) { double m = mag(x, start, len, f); if (m > best) { best = m; bf = f; } }
    return bf;
}

struct Res { double peak, rms; int bad; };
static Res run(Engine& e, Settings& S, std::vector<float>& in, std::vector<float>& out, int block = 256, bool bpmPlaying = false, int engageAt = 0)
{
    int n = (int) in.size(); out.assign(n, 0.f);
    std::vector<float> L(block), R(block);
    Res r { 0, 0, 0 }; double beat = 0;
    for (int p = 0; p < n; p += block)
    {
        int m = std::min(block, n - p);
        for (int i = 0; i < m; ++i) L[i] = R[i] = in[p + i];
        Tempo T; T.bpm = 140; T.beat = beat; T.playing = bpmPlaying;
        Settings Sx = S; if (p < engageAt) Sx.engaged = false;
        e.process(L.data(), R.data(), m, Sx, T);
        beat += m * T.bpm / (60.0 * sr);
        for (int i = 0; i < m; ++i) { float y = 0.5f * (L[i] + R[i]); out[p + i] = y; if (!std::isfinite(y)) r.bad++; r.peak = std::max(r.peak, (double) std::fabs(y)); r.rms += y * y; if (std::fabs(L[i]) > 3.99f) r.bad++; }
    }
    r.rms = std::sqrt(r.rms / n);
    return r;
}

int main(int argc, char** argv)
{
    if (argc > 1) sr = atof(argv[1]);
    int fails = 0;
    auto check = [&](bool ok, const char* msg) { printf("  [%s] %s\n", ok ? "OK " : "FAIL", msg); if (!ok) ++fails; };

    // 1. transparent when all amounts are 0
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true;
        std::vector<float> in(48000), out;
        for (int i = 0; i < (int) in.size(); ++i) in[i] = 0.3f * std::sin(2 * kTestPi * 110.0 * i / sr);
        run(e, S, in, out);
        double err = 0; for (size_t i = 24000; i < in.size(); ++i) err = std::max(err, (double) std::fabs(in[i] - out[i]));
        printf("Transparency max err = %g\n", err);
        check(err < 1e-5, "all amounts 0 => bit transparent");
    }

    // 2. pitch accuracy
    {
        struct T { int fx; double f0; double expect; const char* name; float harm; } tests[] = {
            { Fx::OctDown, 440, 220, "OctDown 440->220", 7 }, { Fx::OctUp, 440, 880, "OctUp 440->880", 7 },
            { Fx::Harmony, 440, 659.3, "Harmony +7 440->659", 7 } };
        for (auto& t : tests)
        {
            Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[t.fx] = 1.f; S.harmSemis = t.harm;
            std::vector<float> in(96000), out;
            for (int i = 0; i < (int) in.size(); ++i) in[i] = 0.3f * std::sin(2 * kTestPi * t.f0 * i / sr);
            run(e, S, in, out);
            double pf = peakFreq(out, 48000, 8192, t.expect * 0.8, t.expect * 1.2);
            printf("  %s => detected %.1f Hz\n", t.name, pf);
            check(std::fabs(pf - t.expect) < t.expect * 0.02, t.name);
        }
    }

    // 3. ring mod sidebands
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[Fx::RingMod] = 1.f; S.ringFreq = 100;
        std::vector<float> in(48000), out;
        for (int i = 0; i < (int) in.size(); ++i) in[i] = 0.3f * std::sin(2 * kTestPi * 440.0 * i / sr);
        run(e, S, in, out);
        double m1 = mag(out, 20000, 8192, 340), m2 = mag(out, 20000, 8192, 540), m0 = mag(out, 20000, 8192, 440);
        printf("  ring: 340=%.3f 540=%.3f 440=%.3f\n", m1, m2, m0);
        check(m1 > 0.05 && m2 > 0.05 && m0 < 0.02, "ring mod sidebands");
    }

    // 4. every effect alone + all together, djent-like input
    std::vector<float> gtr(48000 * 10);
    {
        // palm-muted chugs on a drop-tuned low string, 1/16 notes @ 140bpm
        double step = 60.0 / 140 / 4 * sr;
        for (int i = 0; i < (int) gtr.size(); ++i)
        {
            double t = std::fmod(i, step) / sr; int n = (int) (i / step);
            bool on = ((0xB6B6 >> (n % 16)) & 1);
            double env = on ? std::exp(-t * 14.0) : 0.0;
            double f = 55.0, s = 0;
            for (int h = 1; h <= 12; ++h) s += std::sin(2 * kTestPi * f * h * i / sr) / h;
            gtr[i] = (float) (0.35 * env * s);
        }
    }
    printf("Per-effect stress (engaged, amount 1):\n");
    for (int fx = 0; fx < kNumFx; ++fx)
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[fx] = 1.f;
        std::vector<float> out;
        Res r = run(e, S, gtr, out, 200 + fx * 7, false, 96000);
        printf("  %-10s peak %.3f rms %.4f bad %d\n", kFxNames[fx], r.peak, r.rms, r.bad);
        if (r.bad) ++fails;
    }
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true;
        for (auto& a : S.amt) { a = 1.f; } S.shFb = 0.85f; S.dlyFb = 0.95f; S.revDecay = 20.f; S.shDecay = 20.f; S.flFb = 0.95f; S.filtRes = 1.f;
        std::vector<float> out; Res r = run(e, S, gtr, out, 512, true);
        printf("  ALL=1      peak %.3f rms %.4f bad %d\n", r.peak, r.rms, r.bad);
        check(r.bad == 0, "everything at 100%, max feedback: no NaN / no runaway clipping");
        // toggle engage rapidly + tails decay
        Settings S2 = S; S2.engaged = false; std::vector<float> o2;
        r = run(e, S2, gtr, o2, 512, true);
        printf("  ALL released: peak %.3f rms(last 2s) ", r.peak);
        double tail = 0; for (size_t i = o2.size() - 96000; i < o2.size(); ++i) tail += (o2[i] - gtr[i]) * (o2[i] - gtr[i]);
        printf("%.5f\n", std::sqrt(tail / 96000));
    }

    // 5. reverb/delay calibration + trails
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[Fx::Reverb] = 1.f; S.revDecay = 3.5f;
        std::vector<float> in(48000 * 8, 0.f), out;
        for (int i = 0; i < 4800; ++i) in[i] = 0.5f * std::sin(2 * kTestPi * 220.0 * i / sr);
        // engage for 0.1s then release -> tail must ring
        Settings Sr = S; Sr.engaged = false;
        std::vector<float> a(in.begin(), in.begin() + 9600), b(in.begin() + 9600, in.end()), oa, ob;
        run(e, S, a, oa); run(e, Sr, b, ob);
        auto rmsw = [&](const std::vector<float>& x, int s0, int s1) { double r = 0; for (int i = s0; i < s1; ++i) r += x[i] * x[i]; return std::sqrt(r / (s1 - s0)); };
        double early = rmsw(ob, 4800, 14400), late = rmsw(ob, 150000, 160000);
        printf("  reverb tail rms: 0.1-0.4s %.5f, 3.1-3.3s %.6f\n", early, late);
        check(early > 0.003 && late < early * 0.2, "reverb ring-out after release, then decays");
        // white-noise level calibration
        Engine e2; e2.prepare(sr); Settings S3; S3.engaged = true; S3.amt[Fx::Reverb] = 1.f;
        std::vector<float> nz(48000 * 4), o3; unsigned st = 1; for (auto& x : nz) { st = st * 1664525u + 1013904223u; x = ((st >> 8) / 8388608.f - 1.f) * 0.3f; }
        Settings Sd = S3; Sd.amt[Fx::Reverb] = 1.f;
        run(e2, Sd, nz, o3);
        double inr = rmsw(nz, 48000, 192000), outr = 0; { std::vector<float> d(o3.size()); for (size_t i = 0; i < d.size(); ++i) d[i] = o3[i] - nz[i]; outr = rmsw(d, 48000, 192000); }
        printf("  reverb level (wet rms / in rms) = %.2f\n", outr / inr);
    }

    // 6. tape stop ends silent after its ramp; stutter / granular are covered by per-effect stress
    {
        Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[Fx::TapeStop] = 1.f; S.tsTime = 0.5f;
        std::vector<float> in(48000 * 2), out; for (int i = 0; i < (int) in.size(); ++i) in[i] = 0.3f * std::sin(2 * kTestPi * 330.0 * i / sr);
        run(e, S, in, out);
        double endRms = 0; for (int i = 90000; i < 96000; ++i) endRms += out[i] * out[i]; endRms = std::sqrt(endRms / 6000);
        double f1 = peakFreq(out, 6000 + 4000, 4096, 100, 400);
        printf("  tape stop: tail rms %.5f, pitch ~%.0f Hz while slowing\n", endRms, f1);
        check(endRms < 0.01, "tape stop reaches silence");
    }
    // 7. shimmer / delay / reverb worst case on a sustained tone: bounded and decaying
    {
        const char* names[3] = { "Shimmer", "Delay", "Reverb" }; int ids[3] = { Fx::Shimmer, Fx::Delay, Fx::Reverb };
        for (int k = 0; k < 3; ++k)
        {
            Engine e; e.prepare(sr); Settings S; S.engaged = true; S.amt[ids[k]] = 1.f; S.shFb = 0.85f; S.shDecay = 20.f; S.dlyFb = 0.97f; S.revDecay = 20.f; S.dlyPing = 0.5f; S.revDamp = 0.f;
            std::vector<float> tone(48000 * 6), o; for (int i = 0; i < (int) tone.size(); ++i) tone[i] = 0.4f * std::sin(2 * kTestPi * 110.0 * i / sr);
            Res r = run(e, S, tone, o);
            S.engaged = false; std::vector<float> sil(48000 * 40, 0.f), o2; run(e, S, sil, o2);
            double endRms = 0; for (size_t i = o2.size() - 48000; i < o2.size(); ++i) endRms += o2[i] * o2[i]; endRms = std::sqrt(endRms / 48000);
            printf("  worst-case %-8s sustained peak %.2f | release 40s -> end rms %.5f\n", names[k], r.peak, endRms);
            check(r.peak < 3.0 && endRms < 0.05, names[k]);
        }
    }
    printf("\n%s (%d failures)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails;
}
