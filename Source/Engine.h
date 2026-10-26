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
    Count
};
}
constexpr int kNumFx = Fx::Count;

inline constexpr const char* kFxNames[kNumFx] = {
    "Harmony", "Reserved", "Reserved", "Whammy", "Dive Bomb", "Detune",
    "Fuzz", "Tape Distortion", "Ring Mod", "Filter", "Radio", "Auto Wah",
    "Stutter", "Reverse", "Tape Stop", "Freeze", "Scatter", "Granular",
    "Chorus", "Flanger", "Phaser", "Tremolo", "Vibrato",
    "LFO Deathy", "LFO Haunted Radio",
    "Delay", "Reverb", "Shimmer" };

// -1 hides the two internal legacy pitch slots while preserving project parameter indices.
// category of visible effects: 0 pitch, 1 fx, 2 glitch, 3 mod, 5 time.
inline constexpr int kFxCategory[kNumFx] = { 0,-1,-1,0,0,0, 1,1,1,1,1,1, 2,2,2,2,2,2, 3,3,3,3,3, 3,3, 5,5,5 };

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
    std::array<float, kNumFx> amt {};   // effective amounts 0..1 (manual + pad)
    bool  engaged = false;
    float fadeMs = 8.f, inGain = 1.f, outGain = 1.f;

    // Pitch: the Harmony instrument blends one down-shifted and one up-shifted voice.
    float pitchWinMs = 50.f, harmDownSemis = -12.f, harmUpSemis = 12.f;
    float whamSemis = -12.f, diveSemis = 24.f, diveTime = 1.2f, detCents = 18.f;
    int diveCurve = 1;
    // FX
    float fuzzDrive = 0.6f, fuzzTone = 0.72f;
    float tapeDrive = 0.45f, tapeTone = 0.62f, tapeWear = 0.18f, tapeBias = 0.f;
    float ringFreq = 220.f;
    int   filtMode = 0;
    float filtFreq = 800.f, filtRes = 0.5f, filtRate = 2.f, filtDepth = 0.5f, wahSens = 0.6f;
    float radioLow = 650.f, radioHigh = 3200.f, radioDrive = 8.f;
    // glitch
    float glBeats = 0.25f, tsTime = 0.6f, frzMs = 150.f;
    // granular time manipulation
    float grainSizeMs = 120.f, grainDensity = 12.f, grainPitch = 1.f, grainJitter = 0.15f;
    // modulation
    float choRate = 0.6f, choDepth = 0.5f, flRate = 0.3f, flFb = 0.6f, phRate = 0.4f, phFb = 0.5f;
    float trBeats = 0.5f, trDepth = 1.f, vibRate = 5.f, vibDepth = 0.5f;
    // Vintage LFO filter instruments (former generator slots, now purely audio effects).
    float deathyRate = 0.22f, deathyDepth = 0.78f, deathyTone = 0.38f, deathyRes = 0.72f;
    float hauntRate = 0.11f, hauntDepth = 0.68f, hauntTone = 0.58f, hauntRes = 0.62f;
    // Time FX
    bool  dlyFree = false;
    float dlyBeats = 0.75f, dlyMs = 350.f, dlyFb = 0.45f, dlyTone = 5000.f, dlyPing = 0.7f;
    float dlyWidth = 0.65f, dlySat = 0.15f, dlyMod = 0.15f, dlyDuck = 0.f;
    int revRoom = 1;
    float revDecay = 3.5f, revDamp = 0.5f, revPreMs = 20.f, revSize = 1.f;
    float revMod = 0.5f, revWidth = 0.5f;
    bool revFreeze = false;
    float shDecay = 8.f, shFb = 0.5f, shSemis = 12.f;
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

// 8-line feedback delay network reverb with Hadamard mixing, modulated lines
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
                    float modulation = 0.5f, float width = 0.5f, bool freeze = false)
    {
        freezeMode = freeze;
        modDepthSamples = 18.f * clampf (modulation, 0.f, 1.f); // 0.5 preserves the classic 9-sample motion
        stereoWidth = clampf (width, 0.f, 1.f);
        static const float roomScale[7] = { 0.72f, 1.00f, 0.86f, 0.92f, 1.42f, 0.64f, 1.72f };
        static const float roomDamp[7]  = { 0.08f, 0.00f, -0.12f, 0.04f, 0.18f, -0.18f, 0.25f };
        const int rt = std::max (0, std::min (6, roomType));
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
        preS = clampf (preMs * 0.001f * (float) sr, 0.f, (float) (0.29 * sr));
    }
    inline void process (float inL, float inR, float& oL, float& oR)
    {
        float x = 0.5f * (inL + inR);
        pre.push (x);
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
        oL = mid + side; oR = mid - side;
    }
};

struct StereoDelay
{
    DelayLine l, r;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0, dt = 0, modPhase = 0, duckEnv = 0.f;
    void init (double sr) { l.init ((int) (sr * 3.0) + 16); r.init ((int) (sr * 3.0) + 16); clear(); }
    void clear() { l.clear(); r.clear(); lpL = lpR = hpL = hpR = 0.f; dt = 0.f; modPhase = 0.f; duckEnv = 0.f; }
    inline void run (float inL, float inR, float dtTarget, float fb, float toneC, float hpC,
                     float ping, float width, float saturation, float modulation, float ducking, float sampleRate,
                     float& oL, float& oR)
    {
        if (dt < 1.f) dt = dtTarget;
        dt += (dtTarget - dt) * 0.0008f;
        modPhase += 0.23f / std::max (8000.f, sampleRate);
        if (modPhase >= 1.f) modPhase -= 1.f;
        const float modSamples = clampf (modulation, 0.f, 1.f) * sampleRate * 0.0035f;
        const float maxD = (float) (l.size() - 8);
        const float dl = l.read ((double) clampf (dt + modSamples * std::sin (kTwoPi * modPhase), 8.f, maxD));
        const float dr = r.read ((double) clampf (dt + modSamples * std::sin (kTwoPi * (modPhase + 0.25f)), 8.f, maxD));
        const float fL = lerpf (dl, dr, ping), fR = lerpf (dr, dl, ping);
        lpL += toneC * (fL - lpL); lpR += toneC * (fR - lpR);
        hpL += hpC * (lpL - hpL);  hpR += hpC * (lpR - hpR);
        const float sat = 1.f + 5.f * clampf (saturation, 0.f, 1.f);
        const float sL = std::tanh ((lpL - hpL) * sat) / std::sqrt (sat) * fb;
        const float sR = std::tanh ((lpR - hpR) * sat) / std::sqrt (sat) * fb;
        const float mono = 0.5f * (inL + inR);
        l.push (lerpf (inL, mono, ping) + sL);
        r.push (lerpf (inR, 0.f, ping) + sR);
        const float mid = 0.5f * (dl + dr);
        const float side = 0.5f * (dl - dr) * (0.25f + 1.5f * clampf (width, 0.f, 1.f));
        oL = mid + side; oR = mid - side;
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
            fzBuf[c].init ((int) (sr * 0.6));  grBuf[c].init ((int) (sr * 2.5));
            stCap[c].assign ((size_t) stBuf[c].size(), 0.f);
            fzCap[c].assign ((size_t) fzBuf[c].size(), 0.f);
            choD[c].init ((int) (sr * 0.06)); flD[c].init ((int) (sr * 0.03)); vibD[c].init ((int) (sr * 0.03));
        }
        dly.init (sr); rev.init (sr, 1.5f); shRev.init (sr, 1.8f); shPs.init (sr);
        hpG = std::tan (kPi * 450.f / (float) sr);
        lpG = std::tan (kPi * 3400.f / (float) sr);
        wahAtk = 1.f - std::exp (-1.f / (0.004f * (float) sr));
        wahRel = 1.f - std::exp (-1.f / (0.12f * (float) sr));
        reset();
    }

    void reset()
    {
        for (auto& row : ps) for (auto& p : row) p.clear();
        for (int c = 0; c < 2; ++c)
        {
            stBuf[c].clear(); rvBuf[c].clear(); scBuf[c].clear(); tsBuf[c].clear(); fzBuf[c].clear(); grBuf[c].clear();
            choD[c].clear(); flD[c].clear(); vibD[c].clear();
            fltSvf[c].reset(); radHp[c].reset(); radLp[c].reset(); wahSvf[c].reset();
            deathySvf[c].reset(); hauntSvf[c].reset();
            tapeMem[c] = tapeLp[c] = flFbS[c] = phFbS[c] = fuzzLp[c] = fuzzHp[c] = 0.f;
            for (auto& z : phZ[c]) z = 0.f;
        }
        dly.clear(); rev.clear(); shRev.clear(); shPs.clear();
        am.fill (0.f); prevAct.fill (false); prevEngaged = false;
        gEng = 0.f; inG = outG = 1.f;
        shFbS = shLp = shHp = 0.f; wahEnv = 0.f;
        stActive = tsActive = fzActive = diveAct = false;
        stMix = tsMix = fzMix = 0.f; diveEnv = 0.f;
        tapePhase = deathyPhase = hauntPhase = shPhase = 0.f;
        grPhase[0] = 0.f; grPhase[1] = 0.5f; grOffset[0] = grOffset[1] = 0.f;
        rvIdx = scIdx = -1; whamR = 1.f; winS = 0.f;
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
                       S.revMod, S.revWidth, S.revFreeze);
        shRev.setParams (S.shDecay, 0.55f, 30.f, 4, 1.15f, S.shMod, S.shWidth, false);
        const float shWin = 0.05f * fsr;
        const float shLpC = 1.f - std::exp (-kTwoPi * clampf (S.shTone, 1200.f, 16000.f) / fsr);
        const float shFb = clampf (S.shFb, 0.f, 0.85f) * 0.8f;
        const float shHpC = 1.f - std::exp (-kTwoPi * 220.f / fsr);

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
            for (int i = 0; i < kNumFx; ++i) am[(size_t) i] += (S.amt[(size_t) i] - am[(size_t) i]) * amtC;
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
                    clampf (S.dlyPing, 0.f, 1.f), S.dlyWidth, S.dlySat, S.dlyMod, S.dlyDuck, fsr, dL, dR);
            rev.process (send[0], send[1], rL, rR);

            const float shIn = 0.5f * (send[0] + send[1]);
            shRev.process (shIn + shFbS, shIn + shFbS, sL, sR);
            const float shMid = 0.5f * (sL + sR);
            const float shSide = 0.5f * (sL - sR) * (0.3f + 1.4f * clampf (S.shWidth, 0.f, 1.f));
            sL = shMid + shSide; sR = shMid - shSide;
            shPs.push (0.5f * (sL + sR));
            shPhase += (0.08f + 0.22f * clampf (S.shMod, 0.f, 1.f)) / fsr;
            if (shPhase >= 1.f) shPhase -= 1.f;
            const float shRatio = semiToRatio (S.shSemis + std::sin (kTwoPi * shPhase) * 1.5f * clampf (S.shMod, 0.f, 1.f));
            shLp += shLpC * (shPs.read (shRatio, shWin) - shLp);   // tone control prevents HF feedback build-up
            shHp += shHpC * (shLp - shHp);                          // remove DC / sub build-up from the loop
            shFbS = std::tanh (shLp - shHp) * shFb;

            const float aD = am[Fx::Delay] * 0.8f, aR = am[Fx::Reverb] * 0.8f, aS = am[Fx::Shimmer] * 0.9f;
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

        // The first UI instrument is a single Harmony section with two voices.
        // Its one Mix amount is shared by the down/up voices; the next two array
        // positions are legacy-reserved and intentionally silent.
        const float hAmt = am[Fx::Harmony];
        float a[kPitchVoices] = { hAmt * 0.5f, hAmt * 0.5f, 0.f,
                                  am[Fx::Whammy], am[Fx::Dive], am[Fx::Detune] };
        float S = 0.f; for (int i = 0; i < kPitchVoices; ++i) S += a[i];
        if (S < 0.0005f) return;

        const float ratios[kPitchVoices][2] = {
            { semiToRatio (s->harmDownSemis), semiToRatio (s->harmDownSemis) },
            { semiToRatio (s->harmUpSemis),   semiToRatio (s->harmUpSemis)   },
            { 1.f, 1.f }, { whamR, whamR }, { diveRatio, diveRatio }, { detUp, detDn } };
        const float scale = S > 1.f ? 1.f / std::sqrt (S) : 1.f;
        const float dryW = std::max (0.f, 1.f - S);
        const float win = clampf (winS, 64.f, 0.15f * (float) sr);
        for (int c = 0; c < 2; ++c)
        {
            float y = v[c] * dryW;
            for (int i = 0; i < kPitchVoices; ++i)
                if (a[i] > 0.0005f) y += ps[i][c].read (ratios[i][c], win) * a[i] * scale;
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
            // Stateful, asymmetrical soft clipping with tone and DC restoration.  This is
            // deliberately lightweight rather than a copy of any commercial pedal circuit.
            const float d = clampf (s->fuzzDrive, 0.f, 1.f);
            const float drive = 1.f + 34.f * d * d;
            const float bias = 0.10f * d;
            const float biasRef = std::tanh (bias);
            const float toneHz = 700.f + 10500.f * clampf (s->fuzzTone, 0.f, 1.f);
            const float toneC = 1.f - std::exp (-kTwoPi * toneHz / fsr);
            const float dcC = 1.f - std::exp (-kTwoPi * 24.f / fsr);
            for (int c = 0; c < 2; ++c)
            {
                const float clipped = std::tanh (drive * v[c] + bias) - biasRef;
                fuzzLp[c] += toneC * (clipped - fuzzLp[c]);
                fuzzHp[c] += dcC * (fuzzLp[c] - fuzzHp[c]);
                const float analogish = (fuzzLp[c] - fuzzHp[c] * 0.18f) * (0.78f - 0.24f * d);
                v[c] = lerpf (v[c], analogish, a);
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
                const float saturated = tapeLp[c] * (0.96f - 0.12f * s->tapeDrive);
                v[c] = lerpf (v[c], saturated, a);
            }
        }

        a = am[Fx::RingMod];
        rmPh += s->ringFreq / fsr; rmPh -= std::floor (rmPh);
        if (a > 0.0005f)
        {
            const float car = std::sin (kTwoPi * rmPh);
            for (int c = 0; c < 2; ++c) v[c] = lerpf (v[c], v[c] * car, a);
        }

        a = am[Fx::Filter];
        fltPh += s->filtRate / fsr; fltPh -= std::floor (fltPh);
        if (a > 0.0005f)
        {
            const float lfo = std::sin (kTwoPi * fltPh);
            const float fc = clampf (s->filtFreq * std::exp2 (s->filtDepth * 2.5f * lfo), 20.f, 0.45f * fsr);
            const float g = std::tan (kPi * fc / fsr);
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp;
                fltSvf[c].run (v[c], g, filtK, lp, bp, hp);
                const float y = s->filtMode == 0 ? lp : (s->filtMode == 1 ? bp * filtK : hp);
                v[c] = lerpf (v[c], y, a);
            }
        }

        a = am[Fx::Radio];
        if (a > 0.0005f)
        {
            for (int c = 0; c < 2; ++c)
            {
                float lp, bp, hp, lp2, bp2, hp2;
                const float loG = std::tan (kPi * clampf (s->radioLow, 180.f, 1800.f) / fsr);
                const float hiG = std::tan (kPi * clampf (s->radioHigh, s->radioLow + 300.f, 0.43f * fsr) / fsr);
                radHp[c].run (v[c], loG, 0.72f, lp, bp, hp);
                radLp[c].run (hp, hiG, 0.72f, lp2, bp2, hp2);
                const float drive = clampf (s->radioDrive, 2.f, 16.f);
                v[c] = lerpf (v[c], std::tanh (drive * lp2) / std::sqrt (drive) * 0.95f, a);
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
                    v[c] = lerpf (v[c], bp * k * 3.2f, a);
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
                else                  y = scBuf[c].at (scD + (pos / scD) * scD);
                v[c] = lerpf (v[c], y * w, a);
            }
        }
        ++scPos;

        // ---- Granular cloud: two overlapping Hann-windowed read heads over a short ring buffer.
        // Pitch and jitter change the read trajectory; all state is preallocated in prepare().
        for (int c = 0; c < 2; ++c) grBuf[c].push (v[c]);
        a = am[Fx::Granular];
        if (a > 0.0005f)
        {
            const float gSize = clampf (s->grainSizeMs * 0.001f * fsr, 32.f, 0.45f * fsr);
            const float density = clampf (s->grainDensity, 2.f, 40.f);
            for (int h = 0; h < 2; ++h)
            {
                const float oldPhase = grPhase[h];
                grPhase[h] += density / fsr;
                if (grPhase[h] >= 1.f)
                {
                    grPhase[h] -= 1.f;
                    grOffset[h] = rng.bi() * clampf (s->grainJitter, 0.f, 1.f) * 0.18f * fsr;
                }
                if (oldPhase < grPhase[h]) { /* wrap is handled above; keep branch predictable */ }
            }
            for (int c = 0; c < 2; ++c)
            {
                float cloud = 0.f, norm = 0.f;
                for (int h = 0; h < 2; ++h)
                {
                    const float phase = grPhase[h];
                    const float win = 0.5f - 0.5f * std::cos (kTwoPi * phase);
                    const float pitch = clampf (s->grainPitch, 0.5f, 2.f);
                    const float delay = clampf (32.f + (1.f - phase) * gSize * pitch + grOffset[h], 8.f, (float) (grBuf[c].size() - 16));
                    cloud += grBuf[c].read ((double) delay) * win;
                    norm += win;
                }
                if (norm > 0.05f) cloud /= norm;
                v[c] = lerpf (v[c], cloud * 0.88f, a);
            }
        }

        // ---- Stutter: loops the slice captured at the moment the effect was engaged
        for (int c = 0; c < 2; ++c) stBuf[c].push (v[c]);
        stMix += ((stActive ? 1.f : 0.f) - stMix) * mixC;
        a = am[Fx::Stutter];
        if (stMix * a > 0.0005f)
        {
            const float w = std::max (0.f, std::min (1.f, std::min ((float) stPos / (float) F, (float) (stLen - stPos) / (float) F)));
            for (int c = 0; c < 2; ++c) v[c] = lerpf (v[c], stCap[c][(size_t) stPos] * w, a * stMix);
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
            for (int c = 0; c < 2; ++c) v[c] = lerpf (v[c], rvBuf[c].at (2 * pos + 1) * w, a);
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
                v[c] = lerpf (v[c], tsBuf[c].read (std::max (1.0, (double) tsLag + 1.0)) * gain, a * tsMix);
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
                v[c] = lerpf (v[c], fzCap[c][(size_t) fzPos] * w1 + fzCap[c][(size_t) p2] * w2, a * fzMix);
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
                v[c] = (v[c] + a * wet) / (1.f + 0.5f * a);
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
            if (a > 0.0005f) v[c] = (v[c] + a * wet) / (1.f + 0.5f * a);
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
                v[c] = v[c] * (1.f - 0.5f * a) + x * 0.5f * a;
            }

        // Vibrato
        for (int c = 0; c < 2; ++c) vibD[c].push (v[c]);
        vibPh += s->vibRate / fsr; vibPh -= std::floor (vibPh);
        a = am[Fx::Vibrato];
        if (a > 0.0005f)
            for (int c = 0; c < 2; ++c)
            {
                const float d = (4.f + 3.5f * s->vibDepth * std::sin (kTwoPi * (vibPh + 0.1f * (float) c))) * msS;
                v[c] = lerpf (v[c], vibD[c].read ((double) d), a);
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
                v[c] = lerpf (v[c], wet, a);
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
                v[c] = lerpf (v[c], wet, a);
            }
        }
    }

    // ------------------------------------------------------------------ state
    double sr = 48000.0;
    const Settings* s = nullptr;
    detail::Rng rng;

    std::array<float, kNumFx> am {};
    std::array<bool, kNumFx> prevAct {};
    bool prevEngaged = false;
    float gEng = 0.f, inG = 1.f, outG = 1.f;
    float mixC = 0.01f, fzC = 0.01f;
    int fadeSamples = 144;

    // pitch
    detail::PitchShifter ps[kPitchVoices][2];
    float detUp = 1.01f, detDn = 0.99f, whamTarget = 0.5f, whamR = 1.f, winTarget = 2400.f, winS = 0.f;
    float diveEnv = 0.f; bool diveAct = false;
    // fx
    float tapeMem[2] {}, tapeLp[2] {}, tapePhase = 0.f;
    float fuzzLp[2] {}, fuzzHp[2] {}, rmPh = 0.f, fltPh = 0.f, filtK = 1.f, wahEnv = 0.f, wahAtk = 0.01f, wahRel = 0.001f;
    float hpG = 0.03f, lpG = 0.2f;
    detail::SVF fltSvf[2], radHp[2], radLp[2], wahSvf[2];
    // glitch
    DelayLine stBuf[2], rvBuf[2], scBuf[2], tsBuf[2], fzBuf[2], grBuf[2];
    float grPhase[2] { 0.f, 0.5f }, grOffset[2] {};
    std::vector<float> stCap[2], fzCap[2];
    bool stActive = false, tsActive = false, fzActive = false;
    int stLen = 64, stPos = 0, rvPos = 0, rvLen = 64, scPos = 0, scLen = 64, scType = 0, scD = 64, fzLen = 64, fzPos = 0;
    long long rvIdx = -1, scIdx = -1;
    float stMix = 0.f, tsMix = 0.f, fzMix = 0.f, tsRate = 1.f, tsLag = 0.f;
    // mod
    DelayLine choD[2], flD[2], vibD[2];
    float choPh = 0.f, flPh = 0.f, phPh = 0.f, vibPh = 0.f, phZ[2][4] {}, phFbS[2] {}, flFbS[2] {};
    float deathyPhase = 0.f, hauntPhase = 0.f;
    detail::SVF deathySvf[2], hauntSvf[2];
    // time
    detail::StereoDelay dly;
    detail::FdnReverb rev, shRev;
    detail::PitchShifter shPs;
    float shFbS = 0.f, shLp = 0.f, shHp = 0.f, shPhase = 0.f;
};

} // namespace djx
