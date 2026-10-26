// Parameter table: single source of truth for the APVTS layout, the UI, the randomizer and presets.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include <string>
#include <vector>
#include <cmath>

namespace params
{
enum Kind { kFloat = 0, kInt, kChoice, kBool };

struct PDef
{
    std::string id, name, unit, choices;
    int   kind = kFloat;
    float mn = 0.f, mx = 1.f, def = 0.f, skew = 1.f;
    int   group = -1;        // 0..5 effect category (used by the randomizer), -1 = global
    bool  inPreset = false;  // reset to default when a preset is loaded
};

inline const char* const kCatNames[6] = { "Pitch", "FX", "Glitch", "Mod", "Gen", "Time" };

inline juce::Colour catColour (int cat)
{
    // vivid per-category: purple, orange, cyan, green, yellow, pink
    static const juce::uint32 c[6] = { 0xff8c64ff, 0xffff7837, 0xff32ceff, 0xff37dc82, 0xffd8c832, 0xffe14896 };
    return juce::Colour (c[juce::jlimit (0, 5, cat)]);
}

struct FxInfo { std::string key; std::vector<std::string> details; };

inline const std::vector<FxInfo>& fxInfo()
{
    static const std::vector<FxInfo> v = {
        { "octdn", { "pitchWin" } }, { "octup", {} }, { "harm", { "harmInt" } }, { "wham", { "whamPos" } },
        { "dive", { "diveDepth", "diveTime", "diveCurve" } }, { "det", { "detCents" } },
        { "fuzz", { "fuzzDrive", "fuzzTone" } }, { "crush", { "crushBits", "crushRate" } }, { "ring", { "ringFreq" } },
        { "filt", { "filtMode", "filtFreq", "filtRes", "filtRate", "filtDepth" } }, { "radio", { "radioLow", "radioHigh", "radioDrive" } }, { "wah", { "wahSens" } },
        { "stut", { "glDiv" } }, { "rev", { "glDiv" } }, { "tstop", { "tsTime" } },
        { "frz", { "frzSize" } }, { "scat", { "glDiv" } }, { "gran", { "grainSize", "grainDensity", "grainPitch", "grainJitter" } },
        { "cho", { "choRate", "choDepth" } }, { "fla", { "flRate", "flFb" } }, { "pha", { "phRate", "phFb" } },
        { "trem", { "trDiv", "trDepth" } }, { "vib", { "vibRate", "vibDepth" } },
        { "sub", { "subFreq", "subDecay" } }, { "ris", { "risTime", "risMode", "risStart", "risEnd", "risNoise", "risTone" } },
        { "dly", { "dlyDiv", "dlyMs", "dlyFb", "dlyTone", "dlyPing", "dlyWidth", "dlySat", "dlyMod" } },
        { "rvb", { "revDecay", "revDamp", "revPre", "revSize", "revRoom" } }, { "shim", { "shDecay", "shFb", "shInt" } } };
    return v;
}
inline std::string amtId (int fx) { return "a_" + fxInfo()[(size_t) fx].key; }

inline std::string padChoices()
{
    std::string s = "None";
    for (int i = 0; i < djx::kNumFx; ++i) s += std::string ("|") + djx::kFxNames[i];
    return s;
}

inline const std::vector<PDef>& defs()
{
    static const std::vector<PDef> v = []
    {
        std::vector<PDef> d;
        auto F = [&] (const char* id, const char* name, float mn, float mx, float def, float skew, const char* unit, int grp, bool pre = true)
        { PDef p; p.id = id; p.name = name; p.kind = kFloat; p.mn = mn; p.mx = mx; p.def = def; p.skew = skew; p.unit = unit; p.group = grp; p.inPreset = pre && grp >= 0; d.push_back (p); };
        auto I = [&] (const char* id, const char* name, int mn, int mx, int def, const char* unit, int grp, bool pre = true)
        { PDef p; p.id = id; p.name = name; p.kind = kInt; p.mn = (float) mn; p.mx = (float) mx; p.def = (float) def; p.unit = unit; p.group = grp; p.inPreset = pre && grp >= 0; d.push_back (p); };
        auto C = [&] (const char* id, const char* name, const std::string& ch, int def, int grp, bool pre = true)
        { PDef p; p.id = id; p.name = name; p.kind = kChoice; p.choices = ch; p.def = (float) def; p.group = grp; p.inPreset = pre && grp >= 0; d.push_back (p); };
        auto B = [&] (const char* id, const char* name, bool def)
        { PDef p; p.id = id; p.name = name; p.kind = kBool; p.def = def ? 1.f : 0.f; d.push_back (p); };

        // ---- globals
        B ("bypass", "Bypass", false);
        B ("kill", "Kill Switch", false);
        F ("inGain", "Input", -24, 12, 0, 1, "dB", -1);
        F ("outGain", "Output", -24, 12, 0, 1, "dB", -1);
        F ("dryWet", "Dry / Wet", 0, 1, 1, 1, "%", -1);
        d.back().inPreset = true; // presets may override Dry/Wet; otherwise reset it to 100%
        C ("uiTheme", "Theme", "Night|Day|Moonlight", 0, -1, false);
        B ("engage", "Accent", false);
        C ("engMode", "Footswitch", "Latch|Momentary", 0, -1);
        F ("fade", "Fade", 1, 200, 8, 0.5f, "ms", -1);
        B ("midi", "MIDI Hold", false);
        C ("autoRand", "Auto Random", "Off|On Engage|On Release", 0, -1);
        F ("chaos", "Chaos", 0, 1, 0.5f, 1, "", -1);
        const char* rn[8] = { "Pitch", "FX", "Glitch", "Mod", "Gen", "Time", "Pad", "Zones" };
        for (int i = 0; i < 8; ++i) { const std::string rid = "rnd" + std::to_string (i); B (rid.c_str(), rn[i], true); }

        // ---- XY pad
        F ("padX", "Pad X", 0, 1, 0.5f, 1, "", -1);
        F ("padY", "Pad Y", 0, 1, 0.5f, 1, "", -1);
        F ("padDepth", "Depth", 0, 1, 1, 1, "", -1);
        B ("padTouch", "Touch = Accent", true);
        B ("padSpring", "Spring", true);
        // 16 nodes around the rim: node 0 = top (12 o'clock), clockwise.
        // Each node has a FX-choice param "n<i>" and an enable bool "n<i>e".
        static const int kNodeDefFx[16] = {
            djx::Fx::OctDown,  djx::Fx::Shimmer,   djx::Fx::Scatter,   djx::Fx::Stutter,
            djx::Fx::Reverb,   djx::Fx::Delay,     djx::Fx::Chorus,    djx::Fx::Filter,
            djx::Fx::Fuzz,     djx::Fx::Riser,     djx::Fx::Granular,  djx::Fx::TapeStop,
            djx::Fx::RingMod,  djx::Fx::OctUp,     djx::Fx::Dive,      djx::Fx::Crush };
        {
            const std::string pc2 = padChoices();
            for (int i = 0; i < 16; ++i)
            {
                PDef p; p.kind = kChoice; p.choices = pc2; p.inPreset = true;
                p.id   = "n" + std::to_string (i);
                p.name = "Node " + std::to_string (i);
                p.def  = (float) (kNodeDefFx[i] + 1); d.push_back (p);
                const std::string eid = "n" + std::to_string (i) + "e";
                B (eid.c_str(), p.name.c_str(), true);
            }
        }

        // ---- amounts
        for (int i = 0; i < djx::kNumFx; ++i)
            { const std::string aid = amtId (i); F (aid.c_str(), djx::kFxNames[i], 0, 1, 0, 1, "", djx::kFxCategory[i]); }

        // ---- details
        const std::string divs = djx::kDivNames;
        // pitch
        F ("pitchWin", "Window", 20, 120, 50, 1, "ms", 0);
        I ("harmInt", "Interval", -24, 24, 7, "st", 0);
        F ("whamPos", "Position", -24, 24, -12, 1, "st", 0);
        F ("diveDepth", "Depth", 1, 36, 24, 1, "st", 0);
        F ("diveTime", "Time", 0.1f, 4, 1.2f, 0.5f, "s", 0);
        C ("diveCurve", "Curve", "Linear|Ease|Snap|Smooth", 1, 0);
        F ("detCents", "Cents", 1, 50, 18, 1, "ct", 0);
        // fx
        F ("fuzzDrive", "Drive", 0, 1, 0.6f, 1, "", 1);
        F ("fuzzTone", "Tone", 0, 1, 0.72f, 1, "", 1);
        I ("crushBits", "Bits", 2, 12, 6, "bit", 1);
        F ("crushRate", "Down", 1, 40, 8, 0.5f, "x", 1);
        F ("ringFreq", "Freq", 20, 4000, 220, 0.3f, "Hz", 1);
        C ("filtMode", "Type", "LP|BP|HP", 1, 1);
        F ("filtFreq", "Cutoff", 40, 12000, 800, 0.3f, "Hz", 1);
        F ("filtRes", "Reso", 0, 1, 0.5f, 1, "", 1);
        F ("filtRate", "Rate", 0.1f, 20, 2, 0.4f, "Hz", 1);
        F ("filtDepth", "Sweep", 0, 1, 0.5f, 1, "", 1);
        F ("wahSens", "Sens", 0, 1, 0.6f, 1, "", 1);
        F ("radioLow", "Low Cut", 180, 1800, 650, 0.35f, "Hz", 1);
        F ("radioHigh", "High Cut", 1200, 16000, 3200, 0.35f, "Hz", 1);
        F ("radioDrive", "Drive", 2, 16, 8, 0.5f, "x", 1);
        // glitch
        C ("glDiv", "Slice", divs, 6, 2);
        F ("tsTime", "Time", 0.05f, 3, 0.6f, 0.5f, "s", 2);
        F ("frzSize", "Size", 40, 400, 150, 1, "ms", 2);
        F ("grainSize", "Grain", 20, 500, 120, 0.5f, "ms", 2);
        F ("grainDensity", "Density", 2, 40, 12, 0.5f, "Hz", 2);
        F ("grainPitch", "Pitch", 0.5f, 2, 1, 0.5f, "x", 2);
        F ("grainJitter", "Jitter", 0, 1, 0.15f, 1, "", 2);
        // mod
        F ("choRate", "Rate", 0.1f, 5, 0.6f, 0.5f, "Hz", 3);
        F ("choDepth", "Depth", 0, 1, 0.5f, 1, "", 3);
        F ("flRate", "Rate", 0.05f, 5, 0.3f, 0.5f, "Hz", 3);
        F ("flFb", "Fdbk", -0.9f, 0.9f, 0.6f, 1, "", 3);
        F ("phRate", "Rate", 0.05f, 8, 0.4f, 0.5f, "Hz", 3);
        F ("phFb", "Fdbk", 0, 0.9f, 0.5f, 1, "", 3);
        C ("trDiv", "Rate", divs, 4, 3);
        F ("trDepth", "Depth", 0, 1, 1, 1, "", 3);
        F ("vibRate", "Rate", 0.5f, 12, 5, 0.5f, "Hz", 3);
        F ("vibDepth", "Depth", 0, 1, 0.5f, 1, "", 3);
        // gen
        F ("subFreq", "Pitch", 30, 90, 55, 1, "Hz", 4);
        F ("subDecay", "Decay", 0.2f, 3, 1.2f, 0.6f, "s", 4);
        F ("risTime", "Time", 0.25f, 8, 2, 0.5f, "s", 4);
        C ("risMode", "Mode", "Noise Sweep|Tone Rise|Hybrid", 2, 4);
        F ("risStart", "Start", 40, 2000, 180, 0.4f, "Hz", 4);
        F ("risEnd", "End", 1000, 16000, 10000, 0.4f, "Hz", 4);
        F ("risNoise", "Noise", 0, 1, 0.8f, 1, "", 4);
        F ("risTone", "Tone", 0, 1, 0.35f, 1, "", 4);
        // time
        C ("dlyDiv", "Time", divs + "|Free", 3, 5);
        F ("dlyMs", "Free ms", 20, 2000, 350, 0.4f, "ms", 5);
        F ("dlyFb", "Fdbk", 0, 0.95f, 0.45f, 1, "", 5);
        F ("dlyTone", "Tone", 500, 12000, 5000, 0.4f, "Hz", 5);
        F ("dlyPing", "Ping", 0, 1, 0.7f, 1, "", 5);
        F ("dlyWidth", "Width", 0, 1, 0.65f, 1, "", 5);
        F ("dlySat", "Saturation", 0, 1, 0.15f, 1, "", 5);
        F ("dlyMod", "Modulation", 0, 1, 0.15f, 1, "", 5);
        F ("revDecay", "Decay", 0.3f, 20, 3.5f, 0.4f, "s", 5);
        F ("revDamp", "Damp", 0, 1, 0.5f, 1, "", 5);
        F ("revPre", "Pre", 0, 200, 20, 1, "ms", 5);
        F ("revSize", "Size", 0.5f, 1.8f, 1, 0.5f, "x", 5);
        C ("revRoom", "Room", "Room|Hall|Plate|Chamber|Cathedral|Spring|Void", 1, 5);
        F ("shDecay", "Decay", 1, 20, 8, 0.4f, "s", 5);
        F ("shFb", "Fdbk", 0, 0.85f, 0.5f, 1, "", 5);
        I ("shInt", "Interval", -12, 12, 12, "st", 5);
        return d;
    }();
    return v;
}

inline const PDef* find (const std::string& id)
{
    for (auto& d : defs()) if (d.id == id) return &d;
    return nullptr;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    for (auto& d : defs())
    {
        const ParameterID pid { String (d.id.c_str()), 1 };
        switch (d.kind)
        {
            case kFloat:
            {
                NormalisableRange<float> r (d.mn, d.mx);
                r.skew = d.skew;
                const String unit (d.unit.c_str());
                const float mn = d.mn, mx = d.mx;
                auto attrs = AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction
                    ([unit, mn, mx] (float value, int maximumLength)
                    {
                        int decimals = (unit == "" || unit == "x" || unit == "%") ? 2 :
                                       (unit == "s" || unit == "dB") ? 2 : 1;
                        if (unit == "Hz" || unit == "st" || unit == "ct" || unit == "bit") decimals = 0;
                        if (mx - mn <= 1.0001f && unit.isEmpty()) decimals = 2;
                        const bool fractionPercent = unit == "%" && mn == 0.f && mx == 1.f;
                        const float shown = fractionPercent ? value * 100.f : value;
                        if (fractionPercent) decimals = 0;
                        String text (shown, decimals);
                        if (text.containsChar ('.')) text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
                        if (! fractionPercent && unit == "Hz" && value >= 1000.f)
                        {
                            text = String (value / 1000.f, 2);
                            if (text.containsChar ('.')) text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
                            text << " kHz";
                        }
                        else if (unit.isNotEmpty()) text << " " << unit;
                        if (maximumLength > 0) text = text.substring (0, maximumLength);
                        return text;
                    });
                ps.push_back (std::make_unique<AudioParameterFloat> (pid, d.name, r, d.def, attrs));
                break;
            }
            case kInt:
                ps.push_back (std::make_unique<AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def, AudioParameterIntAttributes().withLabel (d.unit)));
                break;
            case kChoice:
            {
                StringArray sa; sa.addTokens (String (d.choices), "|", "");
                ps.push_back (std::make_unique<AudioParameterChoice> (pid, d.name, sa, (int) d.def));
                break;
            }
            default:
                ps.push_back (std::make_unique<AudioParameterBool> (pid, d.name, d.def > 0.5f));
        }
    }
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& p : ps) layout.add (std::move (p));
    return layout;
}

// XY pad -> weights of the 4 corners (TL, TR, BL, BR). x,y in 0..1, y = 1 is the top.
inline void nodeWeights (float x, float y, float* w) // w[16], node 0=top, clockwise
{
    static constexpr float kPi    = 3.14159265358979f;
    static constexpr float kTwoPi = 6.28318530717959f;
    const float cx = x - 0.5f, cy = 0.5f - y;          // centred, y-up
    const float len = std::sqrt (cx * cx + cy * cy);
    const float radial = std::min (1.f, len * 2.f);
    if (len < 1e-4f) { for (int i = 0; i < 16; ++i) w[i] = 0.f; return; }
    const float pAngle = std::atan2 (cy, cx);
    for (int i = 0; i < 16; ++i)
    {
        const float nAngle = kPi * 0.5f - i * (kTwoPi / 16.f);
        float diff = pAngle - nAngle;
        while (diff >  kPi) diff -= kTwoPi;
        while (diff < -kPi) diff += kTwoPi;
        w[i] = radial * std::max (0.f, std::pow (std::cos (diff), 3.f));
    }
}
// ------------------------------------------------------------------ factory presets
struct PVal  { const char* id; float val; };
struct Preset { const char* name; std::vector<PVal> v; };

inline const std::vector<Preset>& presets()
{
    using namespace djx;
    static const std::vector<Preset> p = {
        { "Init", {} },
        { "Octave Stab", { { "a_octdn", 0.85f }, { "a_rvb", 0.25f }, { "revDecay", 1.8f }, { "a_dly", 0.2f }, { "dlyDiv", 4 }, { "dlyFb", 0.3f } } },
        { "Dive Bomb", { { "a_dive", 1.f }, { "diveDepth", 30 }, { "diveTime", 0.9f }, { "a_sub", 0.5f }, { "subFreq", 48 }, { "a_rvb", 0.2f } } },
        { "Breakdown Shimmer", { { "a_shim", 0.85f }, { "shDecay", 10 }, { "shFb", 0.6f }, { "a_rvb", 0.45f }, { "revDecay", 5 }, { "a_octup", 0.25f }, { "a_dly", 0.4f }, { "dlyDiv", 3 }, { "dlyFb", 0.5f } } },
        { "Granular Chops", { { "a_gran", 0.8f }, { "grainSize", 70 }, { "grainDensity", 18 }, { "grainPitch", 1.25f }, { "grainJitter", 0.32f }, { "a_crush", 0.25f }, { "crushBits", 7 }, { "crushRate", 4 }, { "a_dly", 0.3f }, { "dlyDiv", 6 }, { "dlyFb", 0.35f } } },
        { "Tape Stop Kill", { { "a_tstop", 1.f }, { "tsTime", 0.45f }, { "a_sub", 0.6f }, { "a_rvb", 0.15f } } },
        { "Riser + Impact", { { "a_ris", 1.f }, { "risTime", 3 }, { "a_sub", 1.f }, { "subDecay", 1.8f }, { "a_rvb", 0.45f }, { "revDecay", 4 }, { "a_fuzz", 0.15f } } },
        { "Glitch Scatter", { { "a_scat", 0.9f }, { "glDiv", 6 }, { "a_crush", 0.4f }, { "crushBits", 5 }, { "crushRate", 6 }, { "a_ring", 0.25f }, { "ringFreq", 330 }, { "a_dly", 0.35f }, { "dlyPing", 1.f }, { "dlyDiv", 3 } } },
        { "Stutter Freeze", { { "a_stut", 1.f }, { "glDiv", 7 }, { "a_frz", 0.5f }, { "frzSize", 120 }, { "a_rvb", 0.3f } } },
        { "Ambient Freeze", { { "a_frz", 1.f }, { "frzSize", 220 }, { "a_shim", 0.6f }, { "a_rvb", 0.55f }, { "revDecay", 8 }, { "a_filt", 0.5f }, { "filtMode", 0 }, { "filtFreq", 2500 }, { "filtRes", 0.3f }, { "filtDepth", 0.3f }, { "filtRate", 0.5f } } },
        { "Robot Fifth", { { "a_harm", 0.55f }, { "harmInt", 7 }, { "a_ring", 0.5f }, { "ringFreq", 150 }, { "a_pha", 0.5f }, { "phRate", 0.3f } } },
        { "Wobble Filter", { { "a_filt", 1.f }, { "filtMode", 1 }, { "filtFreq", 600 }, { "filtRes", 0.7f }, { "filtRate", 4 }, { "filtDepth", 0.9f }, { "a_trem", 0.4f }, { "a_rvb", 0.15f } } },
        { "Whammy Octave Up", { { "a_wham", 1.f }, { "whamPos", 12 }, { "a_det", 0.3f }, { "a_dly", 0.25f } } },
        { "Underwater", { { "a_cho", 0.7f }, { "choRate", 0.3f }, { "choDepth", 1.f }, { "a_vib", 0.4f }, { "a_filt", 0.6f }, { "filtMode", 0 }, { "filtFreq", 1200 }, { "filtRes", 0.2f }, { "filtDepth", 0.2f }, { "filtRate", 0.3f }, { "a_rvb", 0.5f } } },
        { "Pad: Cinematic", { { "padTL", (float) (Fx::Shimmer + 1) }, { "padTR", (float) (Fx::Riser + 1) }, { "padBL", (float) (Fx::OctDown + 1) }, { "padBR", (float) (Fx::TapeStop + 1) }, { "a_rvb", 0.2f } } },
        { "Pad: Chaos Glitch", { { "padTL", (float) (Fx::Scatter + 1) }, { "padTR", (float) (Fx::Stutter + 1) }, { "padBL", (float) (Fx::Fuzz + 1) }, { "padBR", (float) (Fx::Crush + 1) }, { "a_dly", 0.25f } } },
        { "Radio Saw", { { "a_radio", 0.95f }, { "radioLow", 780 }, { "radioHigh", 2600 }, { "radioDrive", 13 }, { "a_fuzz", 0.2f }, { "fuzzDrive", 0.38f } } },
        { "Analog Fuzz Lead", { { "a_fuzz", 0.9f }, { "fuzzDrive", 0.72f }, { "fuzzTone", 0.58f }, { "a_det", 0.18f }, { "detCents", 8 }, { "a_rvb", 0.18f } } },
        { "Void Cathedral", { { "a_rvb", 0.85f }, { "revRoom", 6 }, { "revSize", 1.7f }, { "revDecay", 15 }, { "revDamp", 0.75f }, { "revPre", 42 }, { "a_shim", 0.38f } } },
        { "Wide Mod Delay", { { "a_dly", 0.62f }, { "dlyDiv", 4 }, { "dlyFb", 0.58f }, { "dlyWidth", 0.95f }, { "dlySat", 0.3f }, { "dlyMod", 0.55f }, { "a_cho", 0.22f }, { "a_rvb", 0.15f } } },
        { "Granular Moon", { { "a_gran", 0.95f }, { "grainSize", 260 }, { "grainDensity", 7 }, { "grainPitch", 0.72f }, { "grainJitter", 0.6f }, { "a_rvb", 0.52f }, { "revRoom", 4 }, { "revSize", 1.4f }, { "revDecay", 11 } } },
        { "Riser: Hybrid", { { "a_ris", 1.f }, { "risTime", 5 }, { "risMode", 2 }, { "risStart", 90 }, { "risEnd", 14500 }, { "risNoise", 0.9f }, { "risTone", 0.62f }, { "a_rvb", 0.24f } } },
        { "Dive: Snap", { { "a_dive", 1.f }, { "diveDepth", 36 }, { "diveTime", 0.55f }, { "diveCurve", 2 }, { "a_fuzz", 0.25f } } },
        { "Industrial Impact", { { "a_ring", 0.42f }, { "ringFreq", 90 }, { "a_fuzz", 0.6f }, { "fuzzDrive", 0.8f }, { "a_crush", 0.28f }, { "crushBits", 4 }, { "a_rvb", 0.45f }, { "revRoom", 2 }, { "revDecay", 4.8f } } },
        { "Moonlight Shimmer", { { "a_shim", 0.76f }, { "shInt", 12 }, { "shDecay", 14 }, { "shFb", 0.62f }, { "a_rvb", 0.42f }, { "revRoom", 6 }, { "revSize", 1.4f }, { "dryWet", 0.78f } } },
        { "Tape Bloom", { { "a_tstop", 0.55f }, { "tsTime", 1.1f }, { "a_gran", 0.5f }, { "grainSize", 180 }, { "grainPitch", 0.88f }, { "a_dly", 0.3f }, { "dlyFb", 0.52f }, { "dlySat", 0.35f } } },
        { "Clean Stereo Space", { { "a_dly", 0.28f }, { "dlyWidth", 1.f }, { "dlySat", 0.03f }, { "dlyMod", 0.08f }, { "a_rvb", 0.3f }, { "revRoom", 0 }, { "revSize", 0.7f }, { "revDecay", 1.7f }, { "dryWet", 0.35f } } },
    };
    return p;
}
} // namespace params
