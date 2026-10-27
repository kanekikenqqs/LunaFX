// =============================================================================
//  Luna FX - DSP engine (pure C++17, no JUCE dependency, so it can be unit-tested)
//
//  Signal flow (stereo):
//    in -> [Pitch bus (parallel voices)] -> [FX inserts] -> [Glitch inserts]
//       -> [Modulation inserts, including vintage LFO filter instruments]
//       -> engage crossfade (dry <-> effected)
//       -> sends: Delay / Reverb / Shimmer (fed only while engaged => tails ring out)
//    out = crossfade + sends
// =============================================================================
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace djx
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

namespace Fx
{
enum : int
{
    // One combined harmonizer uses two independently tunable pitch voices.
    // The next two slots are retained internally for old session index stability,
    // but are hidden from the UI and no longer process audio.
    Harmony = 0, ReservedPitchA, ReservedPitchB, Whammy, Dive, Detune,
    // FX
    Fuzz, TapeDistortion, RingMod, Filter, Radio, AutoWah,
    // Glitch
    Stutter, Reverse, TapeStop, Freeze, Scatter, Granular,
    // Modulation
    Chorus, Flanger, Phaser, Tremolo, Vibrato, LfoDeathy, LfoHauntedRadio,
    // Time
    Delay, Reverb, Shimmer,
    // New vintage LFO instruments appended so every older index stays stable.
    LfoTapeGhost, LfoLunarTremor,
    Count
};
}
constexpr int kNumFx = Fx::Count;

inline constexpr const char* kFxNames[kNumFx] = {
    "Harmony", "Reserved", "Reserved", "Whammy", "Dive Bomb", "Detune",
    "Fuzz", "Tape Distortion", "Ring Mod", "LFO Filter", "Radio", "Auto Wah",
    "Stutter", "Reverse", "Tape Stop", "Freeze", "Scatter", "Granular",
    "Chorus", "Flanger", "Phaser", "Tremolo", "Vibrato",
    "LFO Deathy", "LFO Haunted Radio",
    "Delay", "Reverb", "Shimmer",
    "LFO Tape Ghost", "LFO Lunar Tremor" };

// -1 hides the two internal legacy pitch slots while preserving project parameter indices.
// category of visible effects: 0 pitch, 1 fx, 2 glitch, 3 mod, 5 time.
inline constexpr int kFxCategory[kNumFx] = { 0,-1,-1,0,0,0, 1,1,1,1,1,1, 2,2,2,2,2,2, 3,3,3,3,3, 3,3, 5,5,5, 3,3 };

constexpr int kNumDivs = 9;
inline constexpr float kDivBeats[kNumDivs] = { 2.f, 1.5f, 1.f, 0.75f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f };
inline constexpr const char* kDivNames = "1/2|1/4.|1/4|1/8.|1/8|1/8T|1/16|1/16T|1/32";

struct Tempo
{
    double bpm = 120.0;
    double beat = 0.0;      // beat position at start of block (host ppq or free running)
    bool   playing = false; // host transport running
};

struct Settings
{
    std::array<float, kNumFx> amt {};   // effective wet amounts 0..1 (slot max * proximity)
    std::array<float, kNumFx> outLevel = [] { std::array<float, kNumFx> a {}; a.fill (1.f); return a; }();
    bool  engaged = false;
    float fadeMs = 8.f, inGain = 1.f, outGain = 1.f;

    // Pitch: the Harmony instrument blends one down-shifted and one up-shifted voice.
    float pitchWinMs = 50.f, harmDownSemis = -12.f, harmUpSemis = 12.f;
    float harmDownFine = 0.f, harmUpFine = 0.f, harmDownLvl = 0.5f, harmUpLvl = 0.5f, harmSpread = 0.f;
    float whamSemis = -12.f, diveSemis = 24.f, diveTime = 1.2f, detCents = 18.f;
    int diveCurve = 1;
    // FX
    float fuzzDrive = 0.6f, fuzzTone = 0.72f, fuzzBias = 0.f, fuzzComp = 0.35f;
    int   fuzzMode = 0;
    float tapeDrive = 0.45f, tapeTone = 0.62f, tapeWear = 0.18f, tapeBias = 0.f, tapeComp = 0.4f;
    float ringFreq = 220.f, ringSpread = 0.25f;
    int   ringShape = 0;
    int   filtMode = 0, filtShape = 0;
    float filtFreq = 800.f, filtRes = 0.5f, filtRate = 2.f, filtDepth = 0.5f, wahSens = 0.6f;
    int   radMode = 0;
    float radioLow = 650.f, radioHigh = 3200.f, radioDrive = 8.f, radRes = 0.4f, radNoise = 0.08f;
    // glitch
    float glBeats = 0.25f, tsTime = 0.6f, frzMs = 150.f;
    // granular time manipulation
    float grainSizeMs = 120.f, grainDensity = 12.f, grainPitch = 1.f, grainJitter = 0.15f;
    float grainPos = 0.5f, grainReverse = 0.1f, grainSpread = 0.5f, grainFb = 0.1f;
    // modulation
    float choRate = 0.6f, choDepth = 0.5f, flRate = 0.3f, flFb = 0.6f, phRate = 0.4f, phFb = 0.5f;
    float trBeats = 0.5f, trDepth = 1.f, vibRate = 5.f, vibDepth = 0.5f;
    // Vintage LFO filter instruments (former generator slots, now purely audio effects).
    float deathyRate = 0.22f, deathyDepth = 0.78f, deathyTone = 0.38f, deathyRes = 0.72f;
    float hauntRate = 0.11f, hauntDepth = 0.68f, hauntTone = 0.58f, hauntRes = 0.62f;
    float ghostRate = 0.18f, ghostDepth = 0.72f, ghostTone = 0.48f, ghostRes = 0.62f;
    float tremorRate = 4.5f, tremorDepth = 0.7f, tremorRes = 0.5f;
    int   tremorShape = 1;
    // Time FX
    int   dlyMode = 0;
    bool  dlyFree = false, dlyFreeze = false;
    float dlyBeats = 0.75f, dlyMs = 350.f, dlyFb = 0.45f, dlyTone = 5000.f, dlyPing = 0.7f;
    float dlyWidth = 0.65f, dlySat = 0.15f, dlyMod = 0.15f, dlyDuck = 0.f;
    int revRoom = 2;
    float revDecay = 3.5f, revDamp = 0.5f, revPreMs = 20.f, revSize = 1.f;
    float revMod = 0.5f, revWidth = 0.5f, revDiff = 0.65f, revEarly = 0.f;
    bool revFreeze = false;
    float shDecay = 8.f, shFb = 0.5f, shSemis = 12.f, shLow = 0.f;
    float shTone = 7000.f, shWidth = 0.5f, shMod = 0.f;
};

namespace detail
{
inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf (float a, float b, float t)    { return a + (b - a) * t; }
inline float semiToRatio (float s)                { return std::exp2 (s / 12.f); }

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.f / 16777216.f); }
    inline float bi()  { return uni() * 2.f - 1.f; }
};

struct DelayLine
{
    std::vector<float> b;
    int mask = 0, w = 0;
    void init (int minSize) { int n = 16; while (n < minSize) n <<= 1; b.assign ((size_t) n, 0.f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.f); w = 0; }
    int  size() const { return mask + 1; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    // integer lookback, back >= 1 -> most recent sample
    inline float at (int back) const { return b[(size_t) ((w - back) & mask)]; }
    // fractional lookback, d >= 1
    inline float read (double d) const
    {
        const double p = (double) w - d;
        const double fl = std::floor (p);
        const float fr = (float) (p - fl);
        const int i0 = ((int) fl) & mask, i1 = (i0 + 1) & mask;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }
};

// classic two-tap crossfaded delay-line pitch shifter
struct PitchShifter
{
    DelayLine dl;
    float phase = 0.f;
    void init (double sr) { dl.init ((int) (sr * 0.4) + 16); phase = 0.f; }
    void clear() { dl.clear(); phase = 0.f; }
    inline void push (float x) { dl.push (x); }
    inline float read (float ratio, float win)
    {
        phase += (1.f - ratio) / win;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
        const float w1 = 0.5f - 0.5f * std::cos (kTwoPi * phase);
        const float w2 = 0.5f - 0.5f * std::cos (kTwoPi * p2);
        return dl.read (2.0 + (double) (phase * win)) * w1 + dl.read (2.0 + (double) (p2 * win)) * w2;
    }
};

// Cytomic / Zavalishin TPT state variable filter
struct SVF
{
    float s1 = 0.f, s2 = 0.f;
    inline void run (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { s1 = s2 = 0.f; }
};

// 8-line feedback delay network reverb with Hadamard mixing, modulated lines,
// a controllable diffusion stage and pre-delay early reflections.
struct FdnReverb
{
    static constexpr int N = 8;
    struct AP
    {
        DelayLine d; int len = 100; float g = 0.6f;
        inline float run (float x) { const float bd = d.at (len); const float w = x + g * bd; d.push (w); return bd - g * w; }
    };

    DelayLine pre, line[N];
    AP ap[4];
    float len[N] {}, baseLen[N] {}, maxLen[N] {}, lp[N] {}, gain[N] {}, ph[N] {}, inc[N] {};
    float dampC = 0.6f, preS = 0.f, inj = 0.5f;
    float modDepthSamples = 9.f, stereoWidth = 0.5f;
    float earlyLvl = 0.35f, diffG = 0.6f;
    bool freezeMode = false;
    double sr = 48000.0;

    void init (double sampleRate, float size)
    {
        sr = sampleRate;
        static const int base[N] = { 1327, 1559, 1823, 2111, 2377, 2693, 2999, 3329 };
        static const int apl[4]  = { 142, 107, 379, 277 };
        const float rate = (float) (sr / 48000.0);
        for (int i = 0; i < N; ++i)
        {
            baseLen[i] = (float) base[i] * rate;
            maxLen[i] = baseLen[i] * 2.4f;
            len[i] = baseLen[i] * size;
            line[i].init ((int) maxLen[i] + 64);
            ph[i] = (float) i / (float) N;
            inc[i] = (0.08f + 0.045f * (float) i) / (float) sr;
        }
        for (int j = 0; j < 4; ++j) { ap[j].len = std::max (8, (int) (apl[j] * sr / 48000.0)); ap[j].d.init (ap[j].len + 8); }
        pre.init ((int) (sr * 0.3) + 8);
        setParams (3.5f, 0.5f, 20.f);
        clear();
    }
    void clear()
    {
        pre.clear(); for (auto& a : ap) a.d.clear();
        for (int i = 0; i < N; ++i) { line[i].clear(); lp[i] = 0.f; }
    }
    void setParams (float decaySec, float damp, float preMs, int roomType = 1, float size = 1.f,
                    float modulation = 0.5f, float width = 0.5f, bool freeze = false,
                    float early = 0.35f, float diffusion = 0.65f)
    {
        freezeMode = freeze;
        modDepthSamples = 18.f * clampf (modulation, 0.f, 1.f); // 0.5 preserves the classic 9-sample motion
        stereoWidth = clampf (width, 0.f, 1.f);
        earlyLvl = clampf (early, 0.f, 1.f);
        diffG = 0.15f + 0.72f * clampf (diffusion, 0.f, 1.f);
        // Ten spaces with genuinely different line lengths, damping, diffusion
        // and early-reflection characters (the FDN topology itself is shared).
        static const float roomScale[10] = { 0.58f, 0.76f, 1.00f, 0.92f, 1.52f, 0.80f, 0.62f, 1.02f, 1.30f, 1.72f };
        static const float roomDamp[10]  = { 0.16f, 0.08f,  0.01f, 0.12f,  0.20f, -0.12f, -0.04f, 0.38f, -0.06f, 0.25f };
        static const float roomEarly[10] = { 0.62f, 0.48f, 0.28f, 0.42f, 0.22f, 0.14f, 0.20f, 0.30f, 0.18f, 0.12f };
        static const float roomDiff[10]  = { 0.48f, 0.62f, 0.72f, 0.74f, 0.82f, 0.95f, 0.34f, 0.70f, 0.88f, 0.92f };
        const int rt = std::max (0, std::min (9, roomType));
        decaySec = std::max (0.1f, decaySec);
        const float scale = clampf (size, 0.5f, 1.8f) * roomScale[rt];
        float gm = 0.f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = clampf (baseLen[i] * scale, 96.f, maxLen[i] - 12.f);
            gain[i] = freezeMode ? 0.9992f : std::pow (10.f, -3.f * len[i] / (decaySec * (float) sr));
            gm += gain[i];
        }
        gm /= (float) N;
        inj = freezeMode ? 0.018f : 1.6f * std::sqrt (std::max (0.02f, 1.f - gm * gm));
        dampC = freezeMode ? 0.995f : 1.f - 0.8f * clampf (damp + roomDamp[rt], 0.f, 1.f);
        dampC = clampf (dampC, 0.02f, 0.9995f);
        preS = clampf (preMs * 0.001f * (float) sr, 0.f, (float) (0.24 * sr));
        earlyLvl = clampf (earlyLvl * roomEarly[rt], 0.f, 1.f);
        diffG = clampf (diffG * (0.6f + 0.4f * roomDiff[rt]), 0.05f, 0.95f);
        for (auto& a : ap) a.g = diffG;
    }
    inline void process (float inL, float inR, float& oL, float& oR)
    {
        float x = 0.5f * (inL + inR);
        pre.push (x);

        // Early reflections are read from the pre-delay line so they stay
        // coherent with the chosen pre-delay and size.
        float earlyL = 0.f, earlyR = 0.f;
        if (earlyLvl > 0.0001f)
        {
            const float base = preS;
            const float tapScale = (float) sr * 0.001f;
            const float taps[4] = { base + 7.f * tapScale, base + 11.f * tapScale,
                                    base + 17.f * tapScale, base + 23.f * tapScale };
            for (int i = 0; i < 4; ++i)
            {
                const float d = clampf (taps[i], 1.f, (float) (pre.size() - 8));
                const float e = pre.read ((double) d) * (0.9f - 0.15f * (float) i);
                if (i == 0 || i == 3) earlyL += e;
                if (i == 1 || i == 2) earlyR += e;
            }
        }

        x = pre.read ((double) preS + 1.0);
        for (auto& a : ap) x = a.run (x);

        float y[N];
        for (int i = 0; i < N; ++i)
        {
            ph[i] += inc[i]; if (ph[i] >= 1.f) ph[i] -= 1.f;
            const float d = line[i].read ((double) (len[i] + 12.f + modDepthSamples * std::sin (kTwoPi * ph[i])));
            lp[i] += dampC * (d - lp[i]);
            y[i] = lp[i] * gain[i];
        }
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j) { const float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
        const float s = 0.35355339f;
        for (int i = 0; i < N; ++i) line[i].push (x * inj + y[i] * s);

        const float rawL = (lp[0] - lp[1] + lp[2] - lp[3] + lp[4] - lp[5] + lp[6] - lp[7]) * 0.45f;
        const float rawR = (lp[0] + lp[1] - lp[2] - lp[3] + lp[4] + lp[5] - lp[6] - lp[7]) * 0.45f;
        const float mid = 0.5f * (rawL + rawR);
        const float side = 0.5f * (rawL - rawR) * (0.3f + 1.4f * stereoWidth);
        oL = mid + side + earlyL * earlyLvl * 0.55f;
        oR = mid - side + earlyR * earlyLvl * 0.55f;
    }
};

struct StereoDelay
{
    DelayLine l, r, ap1, ap2;
    int apLen1 = 113, apLen2 = 197;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0, dt = 0, modPhase = 0, wowPhase = 0, duckEnv = 0.f;
    static inline float allpass (DelayLine& d, float x, int back, float g) noexcept
    {
        const float bd = d.at (back);
        const float w = x + g * bd;
        d.push (w);
        return bd - g * w;
    }
    void init (double sr)
    {
        l.init ((int) (sr * 3.0) + 16); r.init ((int) (sr * 3.0) + 16);
        ap1.init ((int) (sr * 0.02) + 16); ap2.init ((int) (sr * 0.02) + 16);
        apLen1 = std::max (8, (int) (0.0023 * sr));
        apLen2 = std::max (8, (int) (0.0041 * sr));
        clear();
    }
    void clear()
    {
        l.clear(); r.clear(); ap1.clear(); ap2.clear();
        lpL = lpR = hpL = hpR = 0.f; dt = 0.f; modPhase = 0.f; wowPhase = 0.f; duckEnv = 0.f;
    }
    inline void run (float inL, float inR, float dtTarget, float fb, float toneC, float hpC,
                     float ping, float width, float saturation, float modulation, float ducking, float sampleRate,
                     int mode, bool freeze, float& oL, float& oR)
    {
        if (dt < 1.f) dt = dtTarget;
        dt += (dtTarget - dt) * 0.0008f;
        const int m = std::max (0, std::min (6, mode));

        float fbEff = clampf (fb, 0.f, 0.97f);
        float satAmt = clampf (saturation, 0.f, 1.f);
        float modAmt = clampf (modulation, 0.f, 1.f);
        float pingAmt = clampf (ping, 0.f, 1.f);
        float tone = clampf (toneC, 0.0001f, 0.99f);
        float hp = clampf (hpC, 0.0001f, 0.99f);
        float lfoRate = 0.23f / std::max (8000.f, sampleRate);
        float wow = 0.f;

        switch (m)
        {
            case 1: tone *= 0.55f; satAmt = clampf (satAmt + 0.28f, 0.f, 1.f);
                    lfoRate *= 1.8f; modAmt = clampf (modAmt + 0.25f, 0.f, 1.f); break;   // Tape
            case 2: tone *= 0.78f; satAmt = clampf (satAmt + 0.38f, 0.f, 1.f); hp *= 1.6f; break; // Analog
            case 3: pingAmt = 1.f; break;                                                   // Ping-Pong
            case 4: break;                                                                  // Diffused (feedback AP)
            case 5: modAmt = clampf (modAmt * 1.8f + 0.2f, 0.f, 1.f); lfoRate *= 2.5f; break; // Modulated
            case 6: break;                                                                  // Rhythm (second tap)
            default: break;
        }
        if (freeze) { fbEff = 0.995f; inL = 0.f; inR = 0.f; }

        wowPhase += 0.37f / std::max (8000.f, sampleRate);
        if (wowPhase >= 1.f) wowPhase -= 1.f;
        if (m == 1) wow = 0.0025f * std::sin (kTwoPi * wowPhase) + 0.0008f * std::sin (kTwoPi * wowPhase * 5.1f);
        modPhase += lfoRate;
        if (modPhase >= 1.f) modPhase -= 1.f;
        const float modSamples = modAmt * sampleRate * 0.0035f;
        const float maxD = (float) (l.size() - 8);
        const float baseD = clampf (dt * (1.f + wow), 8.f, maxD);
        const float dl = l.read ((double) clampf (baseD + modSamples * std::sin (kTwoPi * modPhase), 8.f, maxD));
        const float dr = r.read ((double) clampf (baseD + modSamples * std::sin (kTwoPi * (modPhase + 0.25f)), 8.f, maxD));
        const float fL = lerpf (dl, dr, pingAmt), fR = lerpf (dr, dl, pingAmt);
        lpL += tone * (fL - lpL); lpR += tone * (fR - lpR);
        hpL += hp * (lpL - hpL);  hpR += hp * (lpR - hpR);
        const float sat = 1.f + 5.f * satAmt;
        float sL = std::tanh ((lpL - hpL) * sat) / std::sqrt (sat) * fbEff;
        float sR = std::tanh ((lpR - hpR) * sat) / std::sqrt (sat) * fbEff;
        if (m == 4)
        {
            // Short all-passes smear the repeats without changing the delay time.
            sL = allpass (ap1, sL, apLen1, 0.62f);
            sR = allpass (ap2, sR, apLen2, 0.58f);
        }
        const float mono = 0.5f * (inL + inR);
        l.push (lerpf (inL, mono, pingAmt) + sL);
        r.push (lerpf (inR, 0.f, pingAmt) + sR);
        const float mid = 0.5f * (dl + dr);
        const float side = 0.5f * (dl - dr) * (0.25f + 1.5f * clampf (width, 0.f, 1.f));
        oL = mid + side; oR = mid - side;
        if (m == 6)
        {
            // A half-time secondary tap gives the rhythm mode a clear identity.
            const float rl = l.read ((double) clampf (baseD * 0.5f, 8.f, maxD)) * 0.35f;
            const float rr = r.read ((double) clampf (baseD * 0.5f, 8.f, maxD)) * 0.35f;
            oL += rl; oR += rr;
        }
        const float inputEnv = 0.5f * (std::fabs (inL) + std::fabs (inR));
        const float duckCoef = 1.f - std::exp (-1.f / ((inputEnv > duckEnv ? 0.012f : 0.18f) * std::max (8000.f, sampleRate)));
        duckEnv += (inputEnv - duckEnv) * duckCoef;
        const float duckGain = 1.f - clampf (duckEnv * 3.f * clampf (ducking, 0.f, 1.f), 0.f, 0.88f);
        oL *= duckGain; oR *= duckGain;
    }
};
} // namespace detail

// =============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < kPitchVoices; ++i)
            for (int c = 0; c < 2; ++c) ps[i][c].init (sr);
        for (int c = 0; c < 2; ++c)
        {
            stBuf[c].init ((int) (sr * 4.0));  rvBuf[c].init ((int) (sr * 8.0));
            scBuf[c].init ((int) (sr * 8.0));  tsBuf[c].init ((int) (sr * 4.0));
            fzBuf[c].init ((int) (sr * 0.6));  grBuf[c].init ((int) (sr * 4.0));
            stCap[c].assign ((size_t) stBuf[c].size(), 0.f);
            fzCap[c].assign ((size_t) fzBuf[c].size(), 0.f);
            choD[c].init ((int) (sr * 0.06)); flD[c].init ((int) (sr * 0.03)); vibD[c].init ((int) (sr * 0.03));
        }
        dly.init (sr); rev.init (sr, 1.5f); shRev.init (sr, 1.8f); shPs.init (sr); shPsLow.init (sr);
        hpG = std::tan (kPi * 450.f / (float) sr);
        lpG = std::tan (kPi * 3400.f / (float) sr);
        wahAtk = 1.f - std::exp (-1.f / (0.004f * (float) sr));
        wahRel = 1.f - std::exp (-1.f / (0.12f * (float) sr));
        reset();
    }

    // Clears every delay line, granular voice and filter state without touching
    // user parameters. Called by the Kill Switch and by bypass transitions.
    void clearTails()
    {
        for (auto& row : ps) for (auto& p : row) p.clear();
        for (int c = 0; c < 2; ++c)
        {
            stBuf[c].clear(); rvBuf[c].clear(); scBuf[c].clear(); tsBuf[c].clear(); fzBuf[c].clear(); grBuf[c].clear();
            choD[c].clear(); flD[c].clear(); vibD[c].clear();
            fltSvf[c].reset(); radHp[c].reset(); radLp[c].reset(); wahSvf[c].reset();
            deathySvf[c].reset(); hauntSvf[c].reset(); ghostSvf[c].reset(); tremorSvf[c].reset();
            tapeMem[c] = tapeLp[c] = flFbS[c] = phFbS[c] = fuzzLp[c] = fuzzHp[c] = 0.f;
            for (auto& z : phZ[c]) z = 0.f;
            std::fill (stCap[c].begin(), stCap[c].end(), 0.f);
            std::fill (fzCap[c].begin(), fzCap[c].end(), 0.f);
            grLast[c] = 0.f;        }
        for (auto& g : grains) g.active = false;
        grainAcc = 0.f;
        dly.clear(); rev.clear(); shRev.clear(); shPs.clear(); shPsLow.clear();
        prevAct.fill (false); prevEngaged = false;
        shFbS = shLp = shHp = 0.f; wahEnv = 0.f;
        stActive = tsActive = fzActive = diveAct = false;
        stMix = tsMix = fzMix = 0.f; diveEnv = 0.f; tsRate = 1.f; tsLag = 0.f;
        tapePhase = deathyPhase = hauntPhase = ghostPhase = tremorPhase = shPhase = 0.f;
        tremorSH = 0.f;
        rvIdx = scIdx = -1; whamR = 1.f; winS = 0.f;
        rmPh = fltPh = choPh = flPh = phPh = vibPh = 0.f;
        rmSH[0] = rmSH[1] = 0.f; fltSH = 0.f; radPh = 0.f;
        stPos = rvPos = scPos = fzPos = 0;
    }

    void reset()
    {
        clearTails();
        am.fill (0.f); og.fill (1.f);
        gEng = 0.f; inG = outG = 1.f;
    }

    float amount (int i) const { return am[(size_t) i]; }
    float engageGain() const   { return gEng; }

    // Process in place. L and R must be distinct buffers.
    void process (float* L, float* R, int n, const Settings& S, const Tempo& Tin)
    {
        using namespace detail;
        s = &S;
        Tempo T = Tin; T.bpm = std::max (20.0, std::min (400.0, T.bpm));
        const float fsr = (float) sr;
        const float amtC = 1.f - std::exp (-1.f / (0.006f * fsr));
        const float engC = 1.f - std::exp (-1.f / (std::max (0.5f, S.fadeMs) * 0.001f * fsr));
        const float gC   = 1.f - std::exp (-1.f / (0.02f * fsr));
        const double beatInc = T.bpm / (60.0 * sr);
        mixC  = 1.f - std::exp (-1.f / (0.004f * fsr));
        fzC   = 1.f - std::exp (-1.f / (0.015f * fsr));
        fadeSamples = std::max (8, (int) (0.003 * sr));

        // ---- block constants
        detUp = std::exp2 (S.detCents / 1200.f); detDn = 1.f / detUp;
        whamTarget = semiToRatio (S.whamSemis);
        winTarget = clampf (S.pitchWinMs * 0.001f * fsr, 64.f, 0.15f * fsr);
        if (winS < 1.f) winS = winTarget;
        filtK = 2.f * std::pow (0.025f, clampf (S.filtRes, 0.f, 1.f));
        const float dlyTarget = S.dlyFree ? S.dlyMs * 0.001f * fsr
                                          : (float) (S.dlyBeats * 60.0 / T.bpm * sr);
        const float toneC = 1.f - std::exp (-kTwoPi * clampf (S.dlyTone, 200.f, 18000.f) / fsr);
        const float hpC   = 1.f - std::exp (-kTwoPi * 140.f / fsr);
        rev.setParams (S.revDecay, S.revDamp, S.revPreMs, S.revRoom, S.revSize,
                       S.revMod, S.revWidth, S.revFreeze, S.revEarly, S.revDiff);
        shRev.setParams (S.shDecay, 0.55f, 30.f, 4, 1.15f, S.shMod, S.shWidth, false, 0.15f, 0.8f);
        const float shWin = 0.05f * fsr;
        const float shLpC = 1.f - std::exp (-kTwoPi * clampf (S.shTone, 1200.f, 16000.f) / fsr);
        const float shFb = clampf (S.shFb, 0.f, 0.85f) * 0.8f;
        const float shHpC = 1.f - std::exp (-kTwoPi * 220.f / fsr);
        const float shLowMix = clampf (S.shLow, 0.f, 1.f);

        // ---- trigger edges (evaluated once per block)
        bool act[kNumFx];
        for (int i = 0; i < kNumFx; ++i) act[i] = S.engaged && S.amt[(size_t) i] > 0.02f;
        prevEngaged = S.engaged;

        diveAct = act[Fx::Dive];
        if (! diveAct) diveEnv = 0.f;

        if (act[Fx::Stutter])
        {
            stActive = true;
            if (! prevAct[Fx::Stutter])
            {
                stLen = sliceSamples (S.glBeats, T.bpm, stBuf[0].size() - 16);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < stLen; ++i) stCap[c][(size_t) i] = stBuf[c].at (stLen - i);
                stPos = 0;
            }
        }
        else stActive = false;

        if (act[Fx::TapeStop])
        {
            if (! prevAct[Fx::TapeStop]) { tsRate = 1.f; tsLag = 0.f; }
            tsActive = true;
        }
        else tsActive = false;

        if (act[Fx::Freeze])
        {
            fzActive = true;
            if (! prevAct[Fx::Freeze])
            {
                fzLen = std::max (64, std::min ((int) (S.frzMs * 0.001f * fsr), fzBuf[0].size() - 16));
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < fzLen; ++i) fzCap[c][(size_t) i] = fzBuf[c].at (fzLen - i);
                fzPos = 0;
            }
        }
        else fzActive = false;

        for (int i = 0; i < kNumFx; ++i) prevAct[(size_t) i] = act[i];

        // ---- sample loop
        for (int k = 0; k < n; ++k)
        {
            for (int i = 0; i < kNumFx; ++i)
            {
                am[(size_t) i] += (S.amt[(size_t) i] - am[(size_t) i]) * amtC;
                og[(size_t) i] += (clampf (S.outLevel[(size_t) i], 0.f, 4.f) - og[(size_t) i]) * gC;
            }
            gEng += ((S.engaged ? 1.f : 0.f) - gEng) * engC;
            inG  += (S.inGain  - inG)  * gC;
            outG += (S.outGain - outG) * gC;
            const double beat = T.beat + (double) k * beatInc;

            float dry[2] = { L[k] * inG, R[k] * inG };
            float v[2]   = { dry[0], dry[1] };

            pitchStage (v);
            fxStage (v);
            glitchStage (v, beat, T.bpm);
            modStage (v, beat);

            float core[2], send[2];
            for (int c = 0; c < 2; ++c)
            {
                core[c] = dry[c] * (1.f - gEng) + v[c] * gEng;
                send[c] = v[c] * gEng;
            }

            // time based sends (outputs are NOT gated => trails)
            float dL, dR, rL, rR, sL, sR;
            dly.run (send[0], send[1], dlyTarget, clampf (S.dlyFb, 0.f, 0.97f), toneC, hpC,
                    clampf (S.dlyPing, 0.f, 1.f), S.dlyWidth, S.dlySat, S.dlyMod, S.dlyDuck, fsr,
                    S.dlyMode, S.dlyFreeze, dL, dR);
            rev.process (send[0], send[1], rL, rR);

            const float shIn = 0.5f * (send[0] + send[1]);
            shRev.process (shIn + shFbS, shIn + shFbS, sL, sR);
            const float shMid = 0.5f * (sL + sR);
            const float shSide = 0.5f * (sL - sR) * (0.3f + 1.4f * clampf (S.shWidth, 0.f, 1.f));
            sL = shMid + shSide; sR = shMid - shSide;
            const float shMono = 0.5f * (sL + sR);
            shPs.push (shMono); shPsLow.push (shMono);
            shPhase += (0.08f + 0.22f * clampf (S.shMod, 0.f, 1.f)) / fsr;
            if (shPhase >= 1.f) shPhase -= 1.f;
            const float warp = std::sin (kTwoPi * shPhase) * 1.5f * clampf (S.shMod, 0.f, 1.f);
            const float shRatio = semiToRatio (S.shSemis + warp);
            // The low voice adds a darker sub-octave layer while the high voice
            // keeps the classic shimmer. Both feed the same protected feedback path.
            const float shRatioLow = semiToRatio (S.shSemis - 12.f + warp);
            const float shHigh = shPs.read (shRatio, shWin);
            const float shLow = shPsLow.read (shRatioLow, shWin);
            const float shifted = lerpf (shHigh, shLow, shLowMix);
            shLp += shLpC * (shifted - shLp);   // tone control prevents HF feedback build-up
            shHp += shHpC * (shLp - shHp);      // remove DC / sub build-up from the loop
            shFbS = std::tanh (shLp - shHp) * shFb;

            const float aD = am[Fx::Delay] * 0.8f * og[Fx::Delay];
            const float aR = am[Fx::Reverb] * 0.8f * og[Fx::Reverb];
            const float aS = am[Fx::Shimmer] * 0.9f * og[Fx::Shimmer];
            float o0 = core[0] + dL * aD + rL * aR + sL * aS;
            float o1 = core[1] + dR * aD + rR * aR + sR * aS;
            o0 *= outG; o1 *= outG;
            if (! std::isfinite (o0)) o0 = 0.f;
            if (! std::isfinite (o1)) o1 = 0.f;
            L[k] = clampf (o0, -4.f, 4.f);
            R[k] = clampf (o1, -4.f, 4.f);
        }
    }

private:
    static constexpr int kPitchVoices = 6;
    using DelayLine = detail::DelayLine;

    int sliceSamples (double beats, double bpm, int maxLen) const
    {
        const int n = (int) (beats * 60.0 / bpm * sr);
        return std::max (64, std::min (n, maxLen));
    }

    // ------------------------------------------------------------------ PITCH
    void pitchStage (float* v)
    {
        using namespace detail;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kPitchVoices; ++i) ps[i][c].push (v[c]);

        winS += (winTarget - winS) * 0.0015f;
        whamR += (whamTarget - whamR) * 0.002f;
        if (diveAct) diveEnv = std::min (1.f, diveEnv + 1.f / (std::max (0.05f, s->diveTime) * (float) sr));
        float e = diveEnv;
        switch (s->diveCurve)
        {
            case 1: e = 1.f - (1.f - diveEnv) * (1.f - diveEnv); break; // gradual then fast
            case 2: e = std::pow (diveEnv, 0.42f); break;                // quick initial drop
            case 3: e = diveEnv * diveEnv * (3.f - 2.f * diveEnv); break; // smoothstep
            default: break; // linear
        }
        const float diveRatio = semiToRatio (-s->diveSemis * e);

        // Harmony is one instrument with two independent voices. The voice
        // levels, fine tuning and stereo spread are all real DSP controls; the
        // shared Maximum Mix still scales the whole Harmony slot.
        const float hAmt = am[Fx::Harmony];
        const float hOut = og[Fx::Harmony];
        float a[kPitchVoices] = { hAmt * clampf (s->harmDownLvl, 0.f, 1.f),
                                  hAmt * clampf (s->harmUpLvl, 0.f, 1.f), 0.f,
                                  am[Fx::Whammy], am[Fx::Dive], am[Fx::Detune] };
        float S = 0.f; for (int i = 0; i < kPitchVoices; ++i) S += a[i];
        if (S < 0.0005f) return;

        const float downRatio = semiToRatio (s->harmDownSemis + s->harmDownFine * 0.01f);
        const float upRatio   = semiToRatio (s->harmUpSemis + s->harmUpFine * 0.01f);
        const float ratios[kPitchVoices][2] = {
            { downRatio, downRatio }, { upRatio, upRatio },
            { 1.f, 1.f }, { whamR, whamR }, { diveRatio, diveRatio }, { detUp, detDn } };
        const float scale = S > 1.f ? 1.f / std::sqrt (S) : 1.f;
        const float wantSpread = clampf (s->harmSpread, 0.f, 1.f);
        const float outv[kPitchVoices] = { hOut, hOut, hOut, og[Fx::Whammy], og[Fx::Dive], og[Fx::Detune] };
        const float win = clampf (winS, 64.f, 0.15f * (float) sr);
        for (int c = 0; c < 2; ++c)
        {
            float y = v[c];
            for (int i = 0; i < kPitchVoices; ++i)
            {
                if (a[i] <= 0.0005f) continue;
                // Down voice leans left, up voice leans right; the other voices
                // stay centred. Scaling is applied to the wet term only so an
                // output level of zero leaves the dry signal intact.
                const float pan = (i == 0) ? (c == 0 ? 1.f + 0.35f * wantSpread : 1.f - 0.35f * wantSpread)
                                  : (i == 1) ? (c == 0 ? 1.f - 0.35f * wantSpread : 1.f + 0.35f * wantSpread)
                                  : 1.f;
                const float voice = ps[i][c].read (ratios[i][c], win) * scale * pan;
                y += (voice - v[c]) * a[i] * outv[i];
            }
            v[c] = y;
        }
    }

    // --------------------------------------------------------------------- FX
    void fxStage (float* v)
    {
        using namespace detail;
        const float fsr = (float) sr;
        float a;

        a = am[Fx::Fuzz];
        if (a > 0.0005f)
        {
            // Stateful soft clipping with user bias, symmetric/asymmetric modes,
            // tone shaping, DC restoration and output compensation. Lightweight
            // and deliberately not a copy of any commercial pedal circuit.
            const float d = clampf (s->fuzzDrive, 0.f, 1.f);
            const float drive = 1.f + 34.f * d * d;
            const bool symmetric = s->fuzzMode == 1;
            const float bias = symmetric ? 0.f
                                         : 0.10f * d + clampf (s->fuzzBias, -1.f, 1.f) * 0.28f;
            const float biasRef = std::tanh (bias);
            const float comp = lerpf (0.80f, 1.35f, clampf (s->fuzzComp, 0.f, 1.f));
            const float toneHz = 700.f + 10500.f * clampf (s->fuzzTone, 0.f, 1.f);
            const float toneC = 1.f - std::exp (-kTwoPi * toneHz / fsr);
            const float dcC = 1.f - std::exp (-kTwoPi * 24.f / fsr);
            for (int c = 0; c < 2; ++c)
            {
                const float shaped = std::tanh (drive * v[c] + bias) - biasRef;
                // A little second-order term gives the asymmetric mode its
                // even-harmonic "fuzz" edge without unbounded output.
                const float clipped = symmetric ? shaped : shaped + 0.08f * shaped * shaped;
                fuzzLp[c] += toneC * (clipped - fuzzLp[c]);
                fuzzHp[c] += dcC * (fuzzLp[c] - fuzzHp[c]);
                const float analogish = (fuzzLp[c] - fuzzHp[c] * 0.18f) * (0.78f - 0.24f * d) * comp;
                blendWet (v[c], analogish, a, og[Fx::Fuzz]);
            }
        }

        // Tape Distortion: smooth magnetic-style saturation, mild hysteresis,
        // controllable high-frequency loss and subtle wear modulation. At 0 Mix
        // it is fully transparent; no bit reduction is performed.
        a = am[Fx::TapeDistortion];
        if (a > 0.0005f)
        {
            const float drive = 1.f + 19.f * std::pow (clampf (s->tapeDrive, 0.f, 1.f), 1.35f);
            const float bias = clampf (s->tapeBias, -1.f, 1.f) * 0.18f;
            const float comp = lerpf (0.86f, 1.30f, clampf (s->tapeComp, 0.f, 1.f));
            const float toneHz = 1200.f + 11800.f * clampf (s->tapeTone, 0.f, 1.f);
            const float toneC = 1.f - std::exp (-kTwoPi * toneHz / fsr);
            tapePhase += (0.18f + 0.9f * clampf (s->tapeWear, 0.f, 1.f)) / fsr;
            if (tapePhase >= 1.f) tapePhase -= 1.f;
            const float wow = 1.f + clampf (s->tapeWear, 0.f, 1.f) *
                              (0.035f * std::sin (kTwoPi * tapePhase) + 0.012f * std::sin (kTwoPi * tapePhase * 6.3f));
            for (int c = 0; c < 2; ++c)
            {
                const float x = v[c] * drive * wow + bias;
                const float magnet = std::tanh (x + 0.08f * tapeMem[c]) - std::tanh (bias);
                tapeMem[c] += 0.035f * (magnet - tapeMem[c]);
                tapeLp[c] += toneC * (magnet - tapeLp[c]);
                const float saturated = tapeLp[c] * (0.96f - 0.12f * s->tapeDrive) * comp;
                blendWet (v[c], saturated, a, og[Fx::TapeDistortion]);
            }
        }

        a = am[Fx::RingMod];
        {
            const float inc = clampf (s->ringFreq, 20.f, 4000.f) / fsr;
            rmPh += inc;
            const bool wrapped = rmPh >= 1.f;
            rmPh -= std::floor (rmPh);
            if (wrapped) { rmSH[0] = rng.bi(); rmSH[1] = rng.bi(); }
        }
        if (a > 0.0005f)
        {
            const float inc = clampf (s->ringFreq, 20.f, 4000.f) / fsr;
            const float spread = clampf (s->ringSpread, 0.f, 1.f) * 0.5f;
            for (int c = 0; c < 2; ++c)
            {
                const float ph = rmPh + (c == 0 ? -spread : spread);
                const float p = ph - std::floor (ph);
                float car = 0.f;
                switch (std::max (0, std::min (3, s->ringShape)))
                {
                    case 0: car = std::sin (kTwoPi * p); break;
                    case 1: car = 1.f - 4.f * std::fabs (p - 0.5f); break;
                    case 2: car = std::sin (kTwoPi * p) >= 0.f ? 1.f : -1.f; break;
                    default: car = rmSH[c]; break;
                }
                (void) inc;
                blendWet (v[c], v[c] * car, a, og[Fx::RingMod]);
            }
        }

        a = am[Fx::Filter];
        fltPh += clampf (s->filtRate, 0.02f, 20.f) / fsr;
        if (fltPh >= 1.f)
        {
            fltPh -= 1.f;
            fltSH = rng.bi();
        }
        if (a > 0.0005f)
        {
            const int shape = std::max (0, std::min (3, s->filtShape));
            float lfo = 0.f;
            switch (shape)
            {
                case 0: lfo = std::sin (kTwoPi * fltPh); break;
                case 1: lfo = 4.f * std::fabs (fltPh - 0.5f) - 1.f; break;
                case 2: lfo = fltSH; break;
                default:
                    lfo = 0.62f * std::sin (kTwoPi * fltPh)
                        + 0.38f * std::sin (kTwoPi * (fltPh * 0.37f + 0.23f)); break;
            }
            const float fc = clampf (s->filtFreq * std::exp2 (s->filtDepth * 2.5f * lfo), 20.f, 0.45f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp;
                fltSvf[c].run (v[c], g, filtK, lp, bp, hp);
                const float y = s->filtMode == 0 ? lp : (s->filtMode == 1 ? bp * filtK : hp);
                blendWet (v[c], y, a, og[Fx::Filter]);
            }
        }

        a = am[Fx::Radio];
        if (a > 0.0005f)
        {
            const int mode = std::max (0, std::min (5, s->radMode));
            float lo = clampf (s->radioLow, 180.f, 1800.f);
            float hi = clampf (s->radioHigh, lo + 250.f, 16000.f);
            float drive = clampf (s->radioDrive, 2.f, 16.f);
            float res = clampf (s->radRes, 0.f, 1.f);
            float noise = clampf (s->radNoise, 0.f, 1.f) * 0.03f;
            switch (mode)
            {
                case 0: lo = std::max (lo, 480.f); hi = std::min (hi, 3200.f); drive *= 0.8f; res *= 0.4f; break;
                case 1: lo = std::max (lo, 300.f); hi = std::min (hi, 2800.f); noise *= 1.5f; break;
                case 2: lo = std::max (lo, 700.f); hi = std::min (hi, 2400.f); res = std::max (res, 0.7f); break;
                case 3: res = std::max (res, 0.82f); drive *= 0.7f; break;
                case 4: lo *= 0.55f; hi = std::min (hi * 1.6f, 12000.f); drive *= 0.55f; noise *= 0.35f; break;
                case 5: drive *= 1.9f; hi = std::min (hi * 0.9f, 8000.f); break;
                default: break;
            }
            radPh += 3.0f / fsr; if (radPh >= 1.f) radPh -= 1.f;
            const float loG = std::tan (kPi * clampf (lo, 120.f, 0.35f * fsr) / fsr);
            const float hiG = std::tan (kPi * clampf (hi, lo + 200.f, 0.43f * fsr) / fsr);
            const float k = clampf (1.6f - 1.45f * res, 0.06f, 1.6f);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp, lp2, bp2, hp2;
                radHp[c].run (v[c], loG, k, lp, bp, hp);
                radLp[c].run (hp, hiG, 0.72f, lp2, bp2, hp2);
                float y = lp2;
                if (mode == 1) y *= 0.72f + 0.28f * std::sin (kTwoPi * radPh);           // hollow AM movement
                if (mode == 2) y = lp2 - bp2 * 0.75f;                                     // hollow mid scoop
                if (mode == 3) y = bp2 * 1.6f + lp2 * 0.25f;                              // resonant band-pass
                if (mode == 4) y = std::tanh (y * 1.2f) * 0.9f;                           // broadcast limiting
                y += rng.bi() * noise;
                const float driven = std::tanh (drive * y) / std::sqrt (drive) * (mode == 5 ? 1.15f : 0.95f);
                blendWet (v[c], driven, a, og[Fx::Radio]);
            }
        }

        a = am[Fx::AutoWah];
        {
            const float e = std::max (std::fabs (v[0]), std::fabs (v[1]));
            wahEnv += (e - wahEnv) * (e > wahEnv ? wahAtk : wahRel);
            if (a > 0.0005f)
            {
                const float ev = clampf (wahEnv * s->wahSens * 10.f, 0.f, 1.f);
                const float fc = clampf (300.f * std::exp2 (ev * 3.2f), 20.f, 0.45f * fsr);
                const float g = std::tan (kPi * fc / fsr), k = 0.3f;
                for (int c = 0; c < 2; ++c)
                {
                    float lp, bp, hp;
                    wahSvf[c].run (v[c], g, k, lp, bp, hp);
                    blendWet (v[c], bp * k * 3.2f, a, og[Fx::AutoWah]);
                }
            }
        }
    }

    // ----------------------------------------------------------------- GLITCH
    void glitchStage (float* v, double beat, double bpm)
    {
        using namespace detail;
        const float fsr = (float) sr;
        const int F = fadeSamples;
        float a;

        // ---- Scatter: every slice randomly passes / repeats an older slice / reverses / mini-stutters
        for (int c = 0; c < 2; ++c) scBuf[c].push (v[c]);
        {
            const double sb = s->glBeats;
            const long long idx = (long long) std::floor (beat / sb);
            if (idx != scIdx)
            {
                scIdx = idx; scPos = 0;
                scLen = sliceSamples (sb, bpm, (scBuf[0].size() - 16) / 4);
                const float r = rng.uni();
                if (r < 0.25f) scType = 0;
                else if (r < 0.55f) { scType = 1; scD = ((int) (rng.uni() * 4.f) + 1) * scLen; }
                else if (r < 0.75f) scType = 2;
                else { scType = 3; scD = std::max (32, scLen / (rng.uni() < 0.5f ? 2 : 4)); }
                if (scType == 1 && scD > scBuf[0].size() - 16) scType = 0;
            }
        }
        a = am[Fx::Scatter];
        if (a > 0.0005f && scType != 0)
        {
            const int pos = std::min (scPos, scLen);
            const float w = std::max (0.f, std::min ({ 1.f, (float) pos / (float) F, (float) (scLen - pos) / (float) F }));
            for (int c = 0; c < 2; ++c)
            {
                float y;
                if (scType == 1)      y = scBuf[c].at (scD);
                else if (scType == 2) y = scBuf[c].at (2 * pos + 1);
                else                  y = scBuf[c].at (scD + (pos * scD) / std::max (1, scLen));
                blendWet (v[c], y * w, a, og[Fx::Scatter]);
            }
        }
        ++scPos;

        // ---- Granular cloud -------------------------------------------------
        // A preallocated stereo history feeds up to 12 overlapping Hann grains.
        // Each grain has its own pitch-scaled read trajectory, direction, pan and
        // lifetime; no allocation, no locks, no unbounded loops in process().
        {
            const float fb = clampf (s->grainFb, 0.f, 0.6f);
            for (int c = 0; c < 2; ++c)
                grBuf[c].push (std::tanh (v[c] + grLast[c] * fb));
        }
        a = am[Fx::Granular];
        if (a > 0.0005f)
        {
            const float fsrLocal = fsr;
            const int maxBack = grBuf[0].size() - 16;
            const float density = clampf (s->grainDensity, 1.f, 40.f);
            const float pitch = clampf (s->grainPitch, 0.25f, 4.f);
            const float pos = clampf (s->grainPos, 0.f, 1.f);
            const float spray = clampf (s->grainJitter, 0.f, 1.f);
            const float spread = clampf (s->grainSpread, 0.f, 1.f);
            const float revProb = clampf (s->grainReverse, 0.f, 1.f);
            const int lenBase = std::max (32, std::min ((int) (clampf (s->grainSizeMs, 20.f, 500.f) * 0.001f * fsrLocal),
                                                         maxBack - 256));

            grainAcc += density / fsrLocal;
            int spawns = 0;
            while (grainAcc >= 1.f && spawns < 4)
            {
                grainAcc -= 1.f;
                ++spawns;
                int idx = 0;
                int oldest = -1;
                for (int i = 0; i < kMaxGrains; ++i)
                {
                    if (! grains[i].active) { idx = i; oldest = -1; break; }
                    if (oldest < 0 || grains[i].age > grains[oldest].age) oldest = i;
                }
                if (oldest >= 0) idx = oldest;
                Grain& g = grains[idx];
                g.len = std::max (32, std::min ((int) (lenBase * (0.6f + 0.8f * rng.uni())), maxBack - 128));
                const float hist = (0.015f + (1.f - pos) * 1.9f) * fsrLocal;
                const float base = clampf (hist, 64.f, (float) (maxBack - 96));
                const float jit = rng.bi() * spray * 0.5f * fsrLocal;
                g.rev = rng.uni() < revProb;
                g.back = clampf (g.rev ? base + jit : base + (float) g.len * pitch + jit,
                                 8.f, (float) (maxBack - 8));
                g.inc = g.rev ? pitch : -pitch;
                g.age = 0;
                const float pan = rng.bi() * spread;
                g.gl = std::sqrt (0.5f * (1.f - pan));
                g.gr = std::sqrt (0.5f * (1.f + pan));
                g.active = true;
            }

            for (int c = 0; c < 2; ++c)
            {
                float cloud = 0.f, norm = 0.f;
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    g.back += g.inc;
                    if (g.back < 4.f || g.back > (float) (maxBack - 4)) { g.active = false; continue; }
                    const float phase = (float) g.age / (float) std::max (1, g.len);
                    const float env = 0.5f - 0.5f * std::cos (kTwoPi * phase);
                    cloud += grBuf[c].read ((double) g.back) * env * (c == 0 ? g.gl : g.gr);
                    norm += env;
                    if (c == 1)
                    {
                        if (++g.age >= g.len) g.active = false;
                    }
                }
                if (norm > 0.05f) cloud /= norm;
                grLast[c] = cloud * 0.9f;
                blendWet (v[c], grLast[c], a, og[Fx::Granular]);
            }
        }
        else
        {
            grLast[0] = grLast[1] = 0.f;
        }

        // ---- Stutter: loops the slice captured at the moment the effect was engaged
        for (int c = 0; c < 2; ++c) stBuf[c].push (v[c]);
        stMix += ((stActive ? 1.f : 0.f) - stMix) * mixC;
        a = am[Fx::Stutter];
        if (stMix * a > 0.0005f)
        {
            const float w = std::max (0.f, std::min (1.f, std::min ((float) stPos / (float) F, (float) (stLen - stPos) / (float) F)));
            for (int c = 0; c < 2; ++c) blendWet (v[c], stCap[c][(size_t) stPos] * w, a * stMix, og[Fx::Stutter]);
            if (++stPos >= stLen) stPos = 0;
        }

        // ---- Reverse: plays the previous slice backwards, grid-locked
        for (int c = 0; c < 2; ++c) rvBuf[c].push (v[c]);
        {
            const double sb = s->glBeats;
            const long long idx = (long long) std::floor (beat / sb);
            if (idx != rvIdx) { rvIdx = idx; rvPos = 0; rvLen = sliceSamples (sb, bpm, (rvBuf[0].size() - 16) / 2); }
        }
        a = am[Fx::Reverse];
        if (a > 0.0005f)
        {
            const int pos = std::min (rvPos, rvLen);
            const float w = std::max (0.f, std::min ({ 1.f, (float) pos / (float) F, (float) (rvLen - pos) / (float) F }));
            for (int c = 0; c < 2; ++c) blendWet (v[c], rvBuf[c].at (2 * pos + 1) * w, a, og[Fx::Reverse]);
        }
        ++rvPos;

        // ---- Tape stop
        for (int c = 0; c < 2; ++c) tsBuf[c].push (v[c]);
        tsMix += ((tsActive ? 1.f : 0.f) - tsMix) * (mixC * 0.7f);
        if (tsActive)
        {
            tsRate = std::max (0.f, tsRate - 1.f / (std::max (0.05f, s->tsTime) * (float) sr));
            tsLag = std::min (tsLag + (1.f - tsRate), (float) (tsBuf[0].size() - 16));
        }
        else if (tsMix < 0.001f) { tsRate = 1.f; tsLag = 0.f; }
        a = am[Fx::TapeStop];
        if (a * tsMix > 0.0005f)
        {
            const float gain = std::min (1.f, tsRate * 12.f);
            for (int c = 0; c < 2; ++c)
                blendWet (v[c], tsBuf[c].read (std::max (1.0, (double) tsLag + 1.0)) * gain,
                          a * tsMix, og[Fx::TapeStop]);
        }

        // ---- Freeze (two overlapping hann grains looping the captured audio)
        for (int c = 0; c < 2; ++c) fzBuf[c].push (v[c]);
        fzMix += ((fzActive ? 1.f : 0.f) - fzMix) * fzC;
        a = am[Fx::Freeze];
        if (fzMix * a > 0.0005f)
        {
            const int p2 = (fzPos + fzLen / 2) % fzLen;
            const float w1 = 0.5f - 0.5f * std::cos (kTwoPi * (float) fzPos / (float) fzLen);
            const float w2 = 0.5f - 0.5f * std::cos (kTwoPi * (float) p2 / (float) fzLen);
            for (int c = 0; c < 2; ++c)
                blendWet (v[c], fzCap[c][(size_t) fzPos] * w1 + fzCap[c][(size_t) p2] * w2,
                          a * fzMix, og[Fx::Freeze]);
            fzPos = (fzPos + 1) % fzLen;
        }

    }

    // ------------------------------------------------------------- MODULATION
    void modStage (float* v, double beat)
    {
        using namespace detail;
        const float fsr = (float) sr, msS = fsr / 1000.f;
        float a;

        // Chorus
        for (int c = 0; c < 2; ++c) choD[c].push (v[c]);
        choPh += s->choRate / fsr; choPh -= std::floor (choPh);
        a = am[Fx::Chorus];
        if (a > 0.0005f)
            for (int c = 0; c < 2; ++c)
            {
                const float p = choPh + 0.25f * (float) c;
                const float d1 = (11.f + 7.f * s->choDepth * std::sin (kTwoPi * p)) * msS;
                const float d2 = (14.f + 7.f * s->choDepth * std::sin (kTwoPi * (p + 0.5f))) * msS;
                const float wet = 0.5f * (choD[c].read ((double) d1) + choD[c].read ((double) d2));
                const float mixed = (v[c] + a * wet) / (1.f + 0.5f * a);
                v[c] += (mixed - v[c]) * og[Fx::Chorus];
            }

        // Flanger
        flPh += s->flRate / fsr; flPh -= std::floor (flPh);
        a = am[Fx::Flanger];
        for (int c = 0; c < 2; ++c)
        {
            float wet = 0.f;
            if (a > 0.0005f)
            {
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (flPh + 0.25f * (float) c));
                wet = flD[c].read ((double) ((0.5f + 3.5f * lfo) * msS));
            }
            flD[c].push (v[c] + clampf (s->flFb, -0.9f, 0.9f) * wet);
            if (a > 0.0005f)
            {
                const float mixed = (v[c] + a * wet) / (1.f + 0.5f * a);
                v[c] += (mixed - v[c]) * og[Fx::Flanger];
            }
        }

        // Phaser (4 allpass stages)
        phPh += s->phRate / fsr; phPh -= std::floor (phPh);
        a = am[Fx::Phaser];
        if (a > 0.0005f)
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (phPh + 0.25f * (float) c));
                const float fc = clampf (250.f * std::exp2 (lfo * 3.6f), 20.f, 0.45f * fsr);
                const float t = std::tan (kPi * fc / fsr), co = (t - 1.f) / (t + 1.f);
                float x = v[c] + clampf (s->phFb, 0.f, 0.9f) * phFbS[c];
                for (int j = 0; j < 4; ++j) { const float y = co * x + phZ[c][j]; phZ[c][j] = x - co * y; x = y; }
                phFbS[c] = x;
                const float mixed = v[c] * (1.f - 0.5f * a) + x * 0.5f * a;
                v[c] += (mixed - v[c]) * og[Fx::Phaser];
            }

        // Vibrato
        for (int c = 0; c < 2; ++c) vibD[c].push (v[c]);
        vibPh += s->vibRate / fsr; vibPh -= std::floor (vibPh);
        a = am[Fx::Vibrato];
        if (a > 0.0005f)
            for (int c = 0; c < 2; ++c)
            {
                const float d = (4.f + 3.5f * s->vibDepth * std::sin (kTwoPi * (vibPh + 0.1f * (float) c))) * msS;
                blendWet (v[c], vibD[c].read ((double) d), a, og[Fx::Vibrato]);
            }

        // LFO-DEATHY: dark, resonant vintage sweep with asymmetric movement.
        a = am[Fx::LfoDeathy];
        deathyPhase += clampf (s->deathyRate, 0.02f, 8.f) / fsr;
        if (deathyPhase >= 1.f) deathyPhase -= 1.f;
        if (a > 0.0005f)
        {
            const float wave = 0.72f * std::sin (kTwoPi * deathyPhase) +
                               0.28f * std::sin (kTwoPi * deathyPhase * 0.5f + 0.8f);
            const float fc = clampf (260.f * std::exp2 (wave * 3.9f * clampf (s->deathyDepth, 0.f, 1.f)), 35.f, 0.42f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            const float k = 1.6f - 1.45f * clampf (s->deathyRes, 0.f, 1.f);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp; deathySvf[c].run (v[c], g, k, lp, bp, hp);
                const float tone = clampf (s->deathyTone, 0.f, 1.f);
                const float wet = (0.96f - 0.42f * tone) * lp
                                + (0.28f + 0.58f * tone) * bp
                                - (0.18f * tone) * hp;
                blendWet (v[c], wet, a, og[Fx::LfoDeathy]);
            }
        }

        // LFO-HAUNTED RADIO: moving band-pass / radio tone with slow, woozy drift.
        a = am[Fx::LfoHauntedRadio];
        hauntPhase += clampf (s->hauntRate, 0.02f, 5.f) / fsr;
        if (hauntPhase >= 1.f) hauntPhase -= 1.f;
        if (a > 0.0005f)
        {
            const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * hauntPhase + 0.35f * std::sin (kTwoPi * hauntPhase * 0.25f));
            const float fc = clampf (180.f * std::exp2 (lfo * (3.f + 5.f * clampf (s->hauntDepth, 0.f, 1.f))), 50.f, 0.40f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            const float k = 1.8f - 1.62f * clampf (s->hauntRes, 0.f, 1.f);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp; hauntSvf[c].run (v[c], g, k, lp, bp, hp);
                const float darkness = 0.18f + 0.72f * clampf (s->hauntTone, 0.f, 1.f);
                const float wet = bp * (1.25f - darkness * 0.35f) + lp * darkness * 0.48f;
                blendWet (v[c], wet, a, og[Fx::LfoHauntedRadio]);
            }
        }

        // LFO-TAPE GHOST: slow tape-like pitch drift plus a saturated, resonant
        // low-pass. Distinct from Deathy (dark asymmetric sweep) and Haunted
        // Radio (radio band-pass) in both filter topology and modulation curve.
        a = am[Fx::LfoTapeGhost];
        ghostPhase += clampf (s->ghostRate, 0.02f, 5.f) / fsr;
        if (ghostPhase >= 1.f) ghostPhase -= 1.f;
        if (a > 0.0005f)
        {
            const float drift = 0.55f * std::sin (kTwoPi * ghostPhase)
                              + 0.30f * std::sin (kTwoPi * (ghostPhase * 0.37f + 0.21f))
                              + 0.15f * std::sin (kTwoPi * ghostPhase * 2.3f);
            const float fc = clampf (420.f * std::exp2 (drift * 3.3f * clampf (s->ghostDepth, 0.f, 1.f)),
                                     45.f, 0.40f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            const float k = clampf (1.55f - 1.40f * clampf (s->ghostRes, 0.f, 1.f), 0.08f, 1.55f);
            const float tone = clampf (s->ghostTone, 0.f, 1.f);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp; ghostSvf[c].run (v[c], g, k, lp, bp, hp);
                const float saturated = std::tanh ((lp * (0.8f + 0.35f * tone) + bp * 0.35f) * 1.4f);
                const float wet = saturated * 0.85f + hp * tone * 0.06f;
                blendWet (v[c], wet, a, og[Fx::LfoTapeGhost]);
            }
        }

        // LFO-LUNAR TREMOR: rhythmic amplitude movement combined with a resonant
        // sweep. S&H and triangle shapes give it a very different motion from
        // Deathy's asymmetric sine drift.
        a = am[Fx::LfoLunarTremor];
        tremorPhase += clampf (s->tremorRate, 0.05f, 12.f) / fsr;
        if (tremorPhase >= 1.f) { tremorPhase -= 1.f; tremorSH = rng.bi(); }
        if (a > 0.0005f)
        {
            float lfo = 0.f;
            switch (std::max (0, std::min (2, s->tremorShape)))
            {
                case 0: lfo = std::sin (kTwoPi * tremorPhase); break;
                case 1: lfo = 4.f * std::fabs (tremorPhase - 0.5f) - 1.f; break;
                default: lfo = tremorSH; break;
            }
            const float depth = clampf (s->tremorDepth, 0.f, 1.f);
            const float amp = 1.f - depth * 0.55f * (0.5f + 0.5f * lfo);
            const float fc = clampf (700.f * std::exp2 (lfo * 2.6f * depth), 60.f, 0.42f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            const float k = clampf (1.5f - 1.35f * clampf (s->tremorRes, 0.f, 1.f), 0.08f, 1.5f);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp; tremorSvf[c].run (v[c], g, k, lp, bp, hp);
                blendWet (v[c], (lp * 0.7f + bp * 0.45f) * amp, a, og[Fx::LfoLunarTremor]);
            }
        }
    }

    // ------------------------------------------------------------------ state
    double sr = 48000.0;
    const Settings* s = nullptr;
    detail::Rng rng;

    std::array<float, kNumFx> am {};
    std::array<float, kNumFx> og {};
    std::array<bool, kNumFx> prevAct {};
    bool prevEngaged = false;
    float gEng = 0.f, inG = 1.f, outG = 1.f;
    float mixC = 0.01f, fzC = 0.01f;
    int fadeSamples = 144;

    // Scales the wet contribution of one instrument by its output level while
    // leaving the dry path untouched. out=1 reproduces the historical mix.
    static inline void blendWet (float& v, float wet, float a, float out) noexcept
    {
        const float mixed = v + (wet - v) * a;
        v += (mixed - v) * out;
    }

    // pitch
    detail::PitchShifter ps[kPitchVoices][2];
    float detUp = 1.01f, detDn = 0.99f, whamTarget = 0.5f, whamR = 1.f, winTarget = 2400.f, winS = 0.f;
    float diveEnv = 0.f; bool diveAct = false;
    // fx
    float tapeMem[2] {}, tapeLp[2] {}, tapePhase = 0.f;
    float fuzzLp[2] {}, fuzzHp[2] {}, rmPh = 0.f, fltPh = 0.f, filtK = 1.f, wahEnv = 0.f, wahAtk = 0.01f, wahRel = 0.001f;
    float rmSH[2] {}, fltSH = 0.f, radPh = 0.f;
    float hpG = 0.03f, lpG = 0.2f;
    detail::SVF fltSvf[2], radHp[2], radLp[2], wahSvf[2];
    // glitch
    DelayLine stBuf[2], rvBuf[2], scBuf[2], tsBuf[2], fzBuf[2], grBuf[2];
    std::vector<float> stCap[2], fzCap[2];
    bool stActive = false, tsActive = false, fzActive = false;
    int stLen = 64, stPos = 0, rvPos = 0, rvLen = 64, scPos = 0, scLen = 64, scType = 0, scD = 64, fzLen = 64, fzPos = 0;
    long long rvIdx = -1, scIdx = -1;
    float stMix = 0.f, tsMix = 0.f, fzMix = 0.f, tsRate = 1.f, tsLag = 0.f;

    // Granular voices. All storage is preallocated; only the small fixed array
    // below is touched by the audio thread.
    static constexpr int kMaxGrains = 12;
    struct Grain
    {
        bool  active = false, rev = false;
        float back = 0.f, inc = 0.f, gl = 0.7f, gr = 0.7f;
        int   age = 0, len = 1;
    };
    Grain grains[kMaxGrains];
    float grainAcc = 0.f, grLast[2] {};

    // mod
    DelayLine choD[2], flD[2], vibD[2];
    float choPh = 0.f, flPh = 0.f, phPh = 0.f, vibPh = 0.f, phZ[2][4] {}, phFbS[2] {}, flFbS[2] {};
    float deathyPhase = 0.f, hauntPhase = 0.f, ghostPhase = 0.f;
    float tremorPhase = 0.f, tremorSH = 0.f;
    detail::SVF deathySvf[2], hauntSvf[2], ghostSvf[2], tremorSvf[2];
    // time
    detail::StereoDelay dly;
    detail::FdnReverb rev, shRev;
    detail::PitchShifter shPs, shPsLow;
    float shFbS = 0.f, shLp = 0.f, shHp = 0.f, shPhase = 0.f;
};

} // namespace djx
