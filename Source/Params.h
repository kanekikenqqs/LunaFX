// =============================================================================
//  Luna FX - parameter table, instrument library and factory presets.
//
//  The table is the single source of truth for the APVTS layout, the UI, the
//  randomiser and the preset browser. Eight XY slots reference entries in the
//  instrument library; the library itself can be larger than eight because
//  slots are assignable.
// =============================================================================
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "XYRouting.h"
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace params
{
constexpr const char* kVersion = "1.3.0";

enum Kind { kFloat = 0, kInt, kChoice, kBool };

struct PDef
{
    std::string id, name, unit, choices;
    int   kind = kFloat;
    float mn = 0.f, mx = 1.f, def = 0.f, skew = 1.f;
    int   group = -1;        // 0..5 effect category, -1 = global / slot control
    bool  inPreset = false;  // reset to default when a preset is loaded
};

inline const char* const kCatNames[6] = { "Pitch", "FX", "Glitch", "Mod", "Internal", "Time" };
inline const char* const kVisibleCatNames[5] = { "PITCH", "FX", "GLITCH", "MOD", "TIME" };

// Restrained lunar accents. The UI maps these to the active Day/Night theme.
inline juce::Colour catColour (int cat)
{
    static const juce::uint32 c[6] = {
        0xffa99be8, // pitch  - moon violet
        0xffd9a066, // fx     - warm amber
        0xffd97b63, // glitch - controlled ember
        0xffb98bc9, // mod    - muted magenta
        0xff6f727c, // hidden / internal
        0xff86a9d6  // time   - restrained blue
    };
    return juce::Colour (c[juce::jlimit (0, 5, cat)]);
}

inline juce::Colour silver() { return juce::Colour (0xffcfd3dc); }

// ------------------------------------------------------------------ instrument library
struct FxInfo { std::string key; std::vector<std::string> details; };

inline const std::vector<FxInfo>& fxInfo()
{
    // Index positions 1 and 2 are retained only so old saved sessions keep
    // stable DSP-array positions. They are never shown as instruments.
    static const std::vector<FxInfo> v = {
        { "harm",  { "harmDown", "harmUp", "harmDownFine", "harmUpFine",
                     "harmDownLvl", "harmUpLvl", "harmSpread", "pitchWin" } },
        { "reservedA", {} },
        { "reservedB", {} },
        { "wham",  { "whamPos" } },
        { "dive",  { "diveDepth", "diveTime", "diveCurve" } },
        { "det",   { "detCents" } },
        { "fuzz",  { "fuzzDrive", "fuzzTone", "fuzzBias", "fuzzMode", "fuzzComp" } },
        { "tape",  { "tapeDrive", "tapeTone", "tapeWear", "tapeBias", "tapeComp" } },
        { "ring",  { "ringFreq", "ringShape", "ringSpread" } },
        { "filt",  { "filtMode", "filtShape", "filtFreq", "filtRes", "filtRate", "filtDepth" } },
        { "radio", { "radMode", "radioLow", "radioHigh", "radioDrive", "radRes", "radNoise" } },
        { "wah",   { "wahSens" } },
        { "stut",  { "glDiv" } },
        { "rev",   { "glDiv" } },
        { "tstop", { "tsTime" } },
        { "frz",   { "frzSize" } },
        { "scat",  { "glDiv" } },
        { "gran",  { "grainSize", "grainDensity", "grainPitch", "grainJitter",
                     "grainPos", "grainReverse", "grainSpread", "grainFb" } },
        { "cho",   { "choRate", "choDepth" } },
        { "fla",   { "flRate", "flFb" } },
        { "pha",   { "phRate", "phFb" } },
        { "trem",  { "trDiv", "trDepth" } },
        { "vib",   { "vibRate", "vibDepth" } },
        { "deathy",{ "deathyRate", "deathyDepth", "deathyTone", "deathyRes" } },
        { "haunt", { "hauntRate", "hauntDepth", "hauntTone", "hauntRes" } },
        { "dly",   { "dlyMode", "dlyDiv", "dlyMs", "dlyFb", "dlyTone", "dlyPing",
                     "dlyWidth", "dlySat", "dlyMod", "dlyDuck", "dlyFreeze" } },
        { "rvb",   { "revRoom", "revDecay", "revPre", "revSize", "revDamp",
                     "revDiff", "revEarly", "revMod", "revWidth", "revFreeze" } },
        { "shim",  { "shInt", "shDecay", "shFb", "shLow", "shTone", "shWidth", "shMod" } },
        { "ghost", { "ghostRate", "ghostDepth", "ghostTone", "ghostRes" } },
        { "tremor",{ "tremorRate", "tremorDepth", "tremorShape", "tremorRes" } }
    };
    jassert ((int) v.size() == djx::kNumFx);
    return v;
}

inline std::string amtId (int fx)
{
    if (fx < 0 || fx >= djx::kNumFx) return "a_invalid";
    return "a_" + fxInfo()[(size_t) fx].key;
}

inline bool isVisibleFx (int fx)
{
    return fx >= 0 && fx < djx::kNumFx && djx::kFxCategory[fx] >= 0;
}
inline int visibleFxCount()
{
    int n = 0;
    for (int i = 0; i < djx::kNumFx; ++i) if (isVisibleFx (i)) ++n;
    return n;
}

// Choice list: index 0 = None, then every visible instrument in DSP order.
inline int fxFromChoice (int choice)
{
    if (choice <= 0) return -1;
    for (int i = 0; i < djx::kNumFx; ++i)
        if (isVisibleFx (i) && --choice == 0) return i;
    return -1;
}
inline int choiceForFx (int fx)
{
    if (! isVisibleFx (fx)) return 0;
    int choice = 1;
    for (int i = 0; i < fx; ++i) if (isVisibleFx (i)) ++choice;
    return choice;
}
inline float normalisedChoiceForFx (int fx)
{
    const int n = visibleFxCount();
    return n > 0 ? (float) choiceForFx (fx) / (float) n : 0.f;
}
inline std::string padChoices()
{
    std::string s = "None";
    for (int i = 0; i < djx::kNumFx; ++i)
        if (isVisibleFx (i)) s += std::string ("|") + djx::kFxNames[i];
    return s;
}

inline std::string slotChoiceId (int slot)  { return "n" + std::to_string (slot); }
inline std::string slotEnableId (int slot)  { return slotChoiceId (slot) + "e"; }
inline std::string slotLevelId  (int slot)  { return slotChoiceId (slot) + "v"; }

// ------------------------------------------------------------------ value formatting
inline juce::String trimmedNumber (double value, int decimals)
{
    juce::String s (value, decimals);
    if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    if (s == "-0" || s == "-0.0") s = "0";
    return s;
}

inline juce::String signedNumber (double value, int decimals, const juce::String& suffix)
{
    juce::String s = trimmedNumber (value, decimals);
    if (value > 0.0000001) s = "+" + s;
    if (suffix.isNotEmpty()) s << " " << suffix;
    return s;
}

// Human-readable, unit-aware formatting. No meaningless trailing zeros.
inline juce::String formatParamValue (const PDef& d, float value)
{
    if (d.kind == kInt)
    {
        const int iv = (int) std::lround (value);
        if (d.unit == "st" || d.unit == "ct") return signedNumber ((double) iv, 0, d.unit.c_str());
        if (juce::String (d.unit.c_str()).isNotEmpty()) return juce::String (iv) + " " + d.unit.c_str();
        return juce::String (iv);
    }

    if (d.unit == "%" && d.mn == 0.f && d.mx == 1.f)
        return juce::String ((int) std::lround (juce::jlimit (0.f, 1.f, value) * 100.f)) + "%";

    if (d.unit == "dB")
        return signedNumber (value, 1, "dB");

    if (d.unit == "Hz")
    {
        if (value >= 1000.f) return trimmedNumber (value / 1000.f, 2) + " kHz";
        if (value < 10.f)    return trimmedNumber (value, 2) + " Hz";
        return juce::String ((int) std::lround (value)) + " Hz";
    }

    if (d.unit == "s")
    {
        if (value < 1.f) return juce::String ((int) std::lround (value * 1000.f)) + " ms";
        return trimmedNumber (value, 2) + " s";
    }

    if (d.unit == "ms") return juce::String ((int) std::lround (value)) + " ms";
    if (d.unit == "st") return signedNumber (value, 0, "st");
    if (d.unit == "ct") return signedNumber (value, 0, "ct");
    if (d.unit == "x")  return trimmedNumber (value, 2) + "x";

    // Unitless 0..1 values read as plain decimals (0.75, not 0.750000).
    if (d.mx - d.mn <= 1.0001f) return trimmedNumber (value, 2);
    juce::String out = trimmedNumber (value, 2);
    if (! d.unit.empty()) out << " " << d.unit.c_str();
    return out;
}

// ------------------------------------------------------------------ definitions
inline const std::vector<PDef>& defs()
{
    static const std::vector<PDef> v = []
    {
        std::vector<PDef> d;
        auto F = [&] (const char* id, const char* name, float mn, float mx, float def, float skew,
                      const char* unit, int grp, bool pre = true)
        {
            PDef p;
            p.id = id; p.name = name; p.kind = kFloat; p.mn = mn; p.mx = mx;
            p.def = juce::jlimit (mn, mx, def); p.skew = skew; p.unit = unit;
            p.group = grp; p.inPreset = pre && grp >= 0;
            d.push_back (std::move (p));
        };
        auto I = [&] (const char* id, const char* name, int mn, int mx, int def,
                      const char* unit, int grp, bool pre = true)
        {
            PDef p;
            p.id = id; p.name = name; p.kind = kInt; p.mn = (float) mn; p.mx = (float) mx;
            p.def = (float) juce::jlimit (mn, mx, def); p.unit = unit;
            p.group = grp; p.inPreset = pre && grp >= 0;
            d.push_back (std::move (p));
        };
        auto C = [&] (const char* id, const char* name, const std::string& ch, int def,
                      int grp, bool pre = true)
        {
            PDef p;
            p.id = id; p.name = name; p.kind = kChoice; p.choices = ch;
            p.def = (float) def; p.group = grp; p.inPreset = pre && grp >= 0;
            d.push_back (std::move (p));
        };
        auto B = [&] (const char* id, const char* name, bool def, bool pre = false)
        {
            PDef p;
            p.id = id; p.name = name; p.kind = kBool; p.def = def ? 1.f : 0.f;
            p.inPreset = pre;
            d.push_back (std::move (p));
        };

        // ---- globals -------------------------------------------------------
        B ("bypass", "Bypass", false);
        B ("kill", "Kill Switch", false);
        F ("inGain", "Input", -24, 12, 0, 1, "dB", -1);
        F ("outGain", "Output", -24, 12, 0, 1, "dB", -1);
        d.back().inPreset = true;
        F ("dryWet", "Dry / Wet", 0, 1, 1, 1, "%", -1);
        d.back().inPreset = true;
        C ("uiTheme", "Theme", "Night|Day", 0, -1, false);
        B ("engage", "Accent", false);
        C ("engMode", "Footswitch", "Latch|Momentary", 0, -1);
        F ("fade", "Fade", 1, 200, 8, 0.5f, "ms", -1);
        B ("midi", "MIDI Hold", false);
        C ("autoRand", "Auto Random", "Off|On Engage|On Release", 0, -1);
        F ("chaos", "Chaos", 0, 1, 0.5f, 1, "", -1);
        const char* rn[8] = { "Pitch", "FX", "Glitch", "Mod", "Time", "Slots", "Presets", "Reserved" };
        for (int i = 0; i < 8; ++i) B (("rnd" + std::to_string (i)).c_str(), rn[i], true);

        // ---- XY performance surface ---------------------------------------
        F ("padX", "Pad X", 0, 1, 0.5f, 1, "", -1);
        F ("padY", "Pad Y", 0, 1, 0.5f, 1, "", -1);
        F ("padDepth", "Depth", 0, 1, 1, 1, "%", -1);
        B ("padTouch", "Touch = Accent", true);
        B ("padSpring", "Spring Return", true);

        // ---- eight fixed XY slots ------------------------------------------
        // Slots 0..3 are left, 4..7 right. The assignment is a library choice.
        // Maximum mix lives on the assigned instrument's a_<fx> parameter so
        // existing automation keeps working. Output level is per slot.
        static const int kDefaultSlotFx[params::kNumSlots] = {
            djx::Fx::Harmony, djx::Fx::Fuzz, djx::Fx::TapeDistortion, djx::Fx::Filter,
            djx::Fx::Radio, djx::Fx::Granular, djx::Fx::Delay, djx::Fx::Reverb
        };
        {
            const std::string pc2 = padChoices();
            for (int i = 0; i < kNumSlots; ++i)
            {
                PDef p;
                p.kind = kChoice; p.choices = pc2; p.inPreset = true;
                p.id   = slotChoiceId (i);
                p.name = "Slot " + std::to_string (i + 1);
                p.def  = (float) choiceForFx (kDefaultSlotFx[i]);
                d.push_back (std::move (p));
                B (slotEnableId (i).c_str(), p.name.c_str(), true, true);
                F (slotLevelId (i).c_str(), "Slot Level", -24, 12, 0, 1, "dB", -1);
                d.back().inPreset = true;
            }
        }

        // ---- instrument maximum mix ----------------------------------------
        for (int i = 0; i < djx::kNumFx; ++i)
        {
            const int cat = djx::kFxCategory[i];
            const float defAmt = cat < 0 ? 0.f : (i == djx::Fx::Harmony ? 0.70f
                                                             : (i == djx::Fx::Delay || i == djx::Fx::Reverb || i == djx::Fx::Shimmer) ? 0.45f
                                                             : 0.55f);
            F (amtId (i).c_str(), djx::kFxNames[i], 0, 1, defAmt, 1, "", cat);
        }

        // ---- pitch ----------------------------------------------------------
        F ("pitchWin", "Window", 20, 120, 50, 1, "ms", 0);
        I ("harmDown", "Down", -24, 0, -12, "st", 0);
        I ("harmUp", "Up", 0, 24, 12, "st", 0);
        F ("harmDownFine", "Down Fine", -50, 50, 0, 1, "ct", 0);
        F ("harmUpFine", "Up Fine", -50, 50, 0, 1, "ct", 0);
        F ("harmDownLvl", "Down Level", 0, 1, 0.5f, 1, "%", 0);
        F ("harmUpLvl", "Up Level", 0, 1, 0.5f, 1, "%", 0);
        F ("harmSpread", "Stereo Spread", 0, 1, 0, 1, "%", 0);
        F ("whamPos", "Position", -24, 24, -12, 1, "st", 0);
        F ("diveDepth", "Depth", 1, 36, 24, 1, "st", 0);
        F ("diveTime", "Time", 0.1f, 4, 1.2f, 0.5f, "s", 0);
        C ("diveCurve", "Curve", "Linear|Ease|Snap|Smooth", 1, 0);
        F ("detCents", "Cents", 1, 50, 18, 1, "ct", 0);

        // ---- FX -------------------------------------------------------------
        F ("fuzzDrive", "Drive", 0, 1, 0.6f, 1, "", 1);
        F ("fuzzTone", "Tone", 0, 1, 0.72f, 1, "", 1);
        F ("fuzzBias", "Bias", -1, 1, 0, 1, "", 1);
        C ("fuzzMode", "Clipping", "Asymmetric|Symmetric", 0, 1);
        F ("fuzzComp", "Output Comp", 0, 1, 0.35f, 1, "", 1);
        F ("tapeDrive", "Drive", 0, 1, 0.45f, 1, "", 1);
        F ("tapeTone", "Tone", 0, 1, 0.62f, 1, "", 1);
        F ("tapeWear", "Wear / Wow", 0, 1, 0.18f, 1, "", 1);
        F ("tapeBias", "Bias", -1, 1, 0, 1, "", 1);
        F ("tapeComp", "Output Comp", 0, 1, 0.4f, 1, "", 1);
        F ("ringFreq", "Freq", 20, 4000, 220, 0.3f, "Hz", 1);
        C ("ringShape", "Waveform", "Sine|Triangle|Square|Sample & Hold", 0, 1);
        F ("ringSpread", "Stereo", 0, 1, 0.25f, 1, "%", 1);
        C ("filtMode", "Type", "Low Pass|Band Pass|High Pass", 1, 1);
        C ("filtShape", "LFO Shape", "Sine|Triangle|Sample & Hold|Drift", 0, 1);
        F ("filtFreq", "Cutoff", 40, 12000, 800, 0.3f, "Hz", 1);
        F ("filtRes", "Resonance", 0, 1, 0.5f, 1, "%", 1);
        F ("filtRate", "Rate", 0.1f, 20, 2, 0.4f, "Hz", 1);
        F ("filtDepth", "Sweep", 0, 1, 0.5f, 1, "%", 1);
        C ("radMode", "Mode", "Telephone|Vintage AM|Hollow|Resonant|Broadcast|Overdrive", 0, 1);
        F ("radioLow", "Low Cut", 180, 1800, 650, 0.35f, "Hz", 1);
        F ("radioHigh", "High Cut", 1200, 16000, 3200, 0.35f, "Hz", 1);
        F ("radioDrive", "Drive", 2, 16, 8, 0.5f, "x", 1);
        F ("radRes", "Resonance", 0, 1, 0.4f, 1, "%", 1);
        F ("radNoise", "Noise", 0, 1, 0.08f, 1, "%", 1);
        F ("wahSens", "Sensitivity", 0, 1, 0.6f, 1, "%", 1);

        // ---- glitch ---------------------------------------------------------
        C ("glDiv", "Slice", djx::kDivNames, 6, 2);
        F ("tsTime", "Time", 0.05f, 3, 0.6f, 0.5f, "s", 2);
        F ("frzSize", "Size", 40, 400, 150, 1, "ms", 2);
        F ("grainSize", "Grain", 20, 500, 120, 0.5f, "ms", 2);
        F ("grainDensity", "Density", 1, 40, 12, 0.5f, "Hz", 2);
        F ("grainPitch", "Pitch", 0.25f, 4, 1, 0.5f, "x", 2);
        F ("grainJitter", "Spray", 0, 1, 0.15f, 1, "%", 2);
        F ("grainPos", "Position", 0, 1, 0.5f, 1, "%", 2);
        F ("grainReverse", "Reverse", 0, 1, 0.1f, 1, "%", 2);
        F ("grainSpread", "Stereo", 0, 1, 0.5f, 1, "%", 2);
        F ("grainFb", "Feedback", 0, 0.6f, 0.1f, 1, "%", 2);

        // ---- modulation -----------------------------------------------------
        F ("choRate", "Rate", 0.1f, 5, 0.6f, 0.5f, "Hz", 3);
        F ("choDepth", "Depth", 0, 1, 0.5f, 1, "%", 3);
        F ("flRate", "Rate", 0.05f, 5, 0.3f, 0.5f, "Hz", 3);
        F ("flFb", "Feedback", -0.9f, 0.9f, 0.6f, 1, "", 3);
        F ("phRate", "Rate", 0.05f, 8, 0.4f, 0.5f, "Hz", 3);
        F ("phFb", "Feedback", 0, 0.9f, 0.5f, 1, "", 3);
        C ("trDiv", "Rate", djx::kDivNames, 4, 3);
        F ("trDepth", "Depth", 0, 1, 1, 1, "%", 3);
        F ("vibRate", "Rate", 0.5f, 12, 5, 0.5f, "Hz", 3);
        F ("vibDepth", "Depth", 0, 1, 0.5f, 1, "%", 3);
        F ("deathyRate", "Rate", 0.02f, 8, 0.22f, 0.45f, "Hz", 3);
        F ("deathyDepth", "Sweep", 0, 1, 0.78f, 1, "%", 3);
        F ("deathyTone", "Tone", 0, 1, 0.38f, 1, "%", 3);
        F ("deathyRes", "Resonance", 0, 1, 0.72f, 1, "%", 3);
        F ("hauntRate", "Rate", 0.02f, 5, 0.11f, 0.45f, "Hz", 3);
        F ("hauntDepth", "Sweep", 0, 1, 0.68f, 1, "%", 3);
        F ("hauntTone", "Tone", 0, 1, 0.58f, 1, "%", 3);
        F ("hauntRes", "Resonance", 0, 1, 0.62f, 1, "%", 3);
        F ("ghostRate", "Rate", 0.02f, 5, 0.18f, 0.45f, "Hz", 3);
        F ("ghostDepth", "Depth", 0, 1, 0.72f, 1, "%", 3);
        F ("ghostTone", "Tone", 0, 1, 0.48f, 1, "%", 3);
        F ("ghostRes", "Resonance", 0, 1, 0.62f, 1, "%", 3);
        F ("tremorRate", "Rate", 0.05f, 12, 4.5f, 0.4f, "Hz", 3);
        F ("tremorDepth", "Depth", 0, 1, 0.7f, 1, "%", 3);
        C ("tremorShape", "Shape", "Sine|Triangle|Sample & Hold", 1, 3);
        F ("tremorRes", "Resonance", 0, 1, 0.5f, 1, "%", 3);

        // ---- time -----------------------------------------------------------
        C ("dlyMode", "Mode", "Clean|Tape|Analog|Ping-Pong|Diffused|Modulated|Rhythm", 0, 5);
        C ("dlyDiv", "Time", std::string (djx::kDivNames) + "|Free", 3, 5);
        F ("dlyMs", "Free Time", 20, 2000, 350, 0.4f, "ms", 5);
        F ("dlyFb", "Feedback", 0, 0.95f, 0.45f, 1, "%", 5);
        F ("dlyTone", "Tone", 500, 12000, 5000, 0.4f, "Hz", 5);
        F ("dlyPing", "Ping-Pong", 0, 1, 0.7f, 1, "%", 5);
        F ("dlyWidth", "Width", 0, 1, 0.65f, 1, "%", 5);
        F ("dlySat", "Saturation", 0, 1, 0.15f, 1, "%", 5);
        F ("dlyMod", "Modulation", 0, 1, 0.15f, 1, "%", 5);
        F ("dlyDuck", "Duck", 0, 1, 0, 1, "%", 5);
        B ("dlyFreeze", "Freeze", false, true);
        C ("revRoom", "Space", "Room|Studio|Hall|Chamber|Cathedral|Plate|Spring|Dark|Shimmer|Void", 2, 5);
        F ("revDecay", "Decay", 0.3f, 20, 3.5f, 0.4f, "s", 5);
        F ("revDamp", "Damping", 0, 1, 0.5f, 1, "%", 5);
        F ("revPre", "Pre-delay", 0, 200, 20, 1, "ms", 5);
        F ("revSize", "Size", 0.5f, 1.8f, 1, 0.5f, "x", 5);
        F ("revDiff", "Diffusion", 0, 1, 0.65f, 1, "%", 5);
        F ("revEarly", "Early", 0, 1, 0, 1, "%", 5);
        F ("revMod", "Modulation", 0, 1, 0.5f, 1, "%", 5);
        F ("revWidth", "Width", 0, 1, 0.5f, 1, "%", 5);
        B ("revFreeze", "Freeze", false, true);
        F ("shDecay", "Decay", 1, 20, 8, 0.4f, "s", 5);
        F ("shFb", "Feedback", 0, 0.85f, 0.5f, 1, "%", 5);
        I ("shInt", "Pitch", -12, 12, 12, "st", 5);
        F ("shLow", "Low Octave", 0, 1, 0, 1, "%", 5);
        F ("shTone", "Tone", 1200, 16000, 7000, 0.35f, "Hz", 5);
        F ("shWidth", "Width", 0, 1, 0.5f, 1, "%", 5);
        F ("shMod", "Warp", 0, 1, 0, 1, "%", 5);

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
        const ParameterID pid { String (d.id), 1 };
        switch (d.kind)
        {
            case kFloat:
            {
                NormalisableRange<float> r (d.mn, d.mx);
                if (d.skew > 0.f) r.skew = d.skew;
                const PDef pd = d;
                auto attrs = AudioParameterFloatAttributes()
                    .withLabel (d.unit)
                    .withStringFromValueFunction ([pd] (float value, int) { return formatParamValue (pd, value); });
                ps.push_back (std::make_unique<AudioParameterFloat> (pid, d.name, r, d.def, attrs));
                break;
            }
            case kInt:
            {
                const PDef pd = d;
                auto attrs = AudioParameterIntAttributes()
                    .withLabel (d.unit)
                    .withStringFromValueFunction ([pd] (int value, int) { return formatParamValue (pd, (float) value); });
                ps.push_back (std::make_unique<AudioParameterInt> (pid, d.name,
                                                                   (int) d.mn, (int) d.mx, (int) d.def, attrs));
                break;
            }
            case kChoice:
            {
                StringArray sa;
                sa.addTokens (String (d.choices), "|", "");
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

// ------------------------------------------------------------------ factory presets
struct PVal { std::string id; float val; };
struct Preset { std::string name, category; std::vector<PVal> v; };

inline std::vector<PVal> withSlots (std::initializer_list<int> fxs, std::vector<PVal> extra = {})
{
    std::vector<PVal> out;
    int slot = 0;
    for (int fx : fxs)
    {
        if (slot >= kNumSlots) break;
        out.push_back ({ slotChoiceId (slot), (float) choiceForFx (fx) });
        out.push_back ({ slotEnableId (slot), 1.f });
        ++slot;
    }
    for (auto& e : extra) out.push_back (std::move (e));
    return out;
}

inline const std::vector<Preset>& presets()
{
    using namespace djx;
    static const std::vector<Preset> p = []
    {
        std::vector<Preset> v;
        auto add = [&] (const char* name, const char* cat, std::initializer_list<int> fxs,
                        std::vector<PVal> extra = {})
        {
            v.push_back ({ name, cat, withSlots (fxs, std::move (extra)) });
        };

        add ("Init", "Init",
             { Fx::Harmony, Fx::Fuzz, Fx::TapeDistortion, Fx::Filter,
               Fx::Radio, Fx::Granular, Fx::Delay, Fx::Reverb }, {});

        add ("Lunar Ambient", "Lunar Ambient",
             { Fx::Harmony, Fx::TapeDistortion, Fx::Filter, Fx::Granular,
               Fx::Chorus, Fx::Delay, Fx::Reverb, Fx::Shimmer },
             { { "a_harm", 0.55f }, { "harmDown", -12 }, { "harmUp", 12 }, { "harmSpread", 0.7f },
               { "a_tape", 0.38f }, { "a_filt", 0.55f }, { "filtMode", 0 }, { "filtFreq", 2200 },
               { "filtRate", 0.12f }, { "filtDepth", 0.35f }, { "a_cho", 0.35f },
               { "a_dly", 0.42f }, { "dlyMode", 4 }, { "dlyFb", 0.55f }, { "dlyDuck", 0.25f },
               { "a_rvb", 0.55f }, { "revRoom", 4 }, { "revDecay", 9.5f }, { "revSize", 1.35f },
               { "a_shim", 0.5f }, { "shDecay", 12 }, { "shFb", 0.58f }, { "shLow", 0.2f },
               { "dryWet", 0.9f }, { "outGain", -1.5f } });

        add ("Vintage Radio", "Vintage Radio",
             { Fx::Radio, Fx::Filter, Fx::TapeDistortion, Fx::Fuzz,
               Fx::Chorus, Fx::Delay, Fx::Reverb, Fx::AutoWah },
             { { "a_radio", 0.9f }, { "radMode", 1 }, { "radioLow", 520 }, { "radioHigh", 2800 },
               { "radioDrive", 11 }, { "radRes", 0.62f }, { "radNoise", 0.22f },
               { "a_filt", 0.5f }, { "filtMode", 1 }, { "filtFreq", 1100 }, { "filtRes", 0.4f },
               { "filtRate", 0.35f }, { "filtDepth", 0.5f }, { "a_tape", 0.35f }, { "tapeWear", 0.35f },
               { "a_fuzz", 0.2f }, { "a_cho", 0.3f }, { "choRate", 0.25f },
               { "a_dly", 0.28f }, { "dlyMode", 1 }, { "dlyFb", 0.38f },
               { "a_rvb", 0.3f }, { "revRoom", 0 }, { "revDecay", 2.2f }, { "outGain", -1.f } });

        add ("Tape Experiments", "Tape Experiments",
             { Fx::TapeDistortion, Fx::Fuzz, Fx::Granular, Fx::Filter,
               Fx::Delay, Fx::Reverb, Fx::Shimmer, Fx::Harmony },
             { { "a_tape", 0.72f }, { "tapeDrive", 0.68f }, { "tapeWear", 0.55f }, { "tapeBias", -0.15f },
               { "a_fuzz", 0.35f }, { "a_gran", 0.55f }, { "grainSize", 210 }, { "grainDensity", 7 },
               { "grainPitch", 0.75f }, { "grainJitter", 0.4f }, { "a_filt", 0.45f },
               { "a_dly", 0.38f }, { "dlyMode", 1 }, { "dlyFb", 0.62f }, { "dlySat", 0.4f },
               { "a_rvb", 0.35f }, { "revRoom", 2 }, { "a_shim", 0.3f },
               { "a_harm", 0.3f }, { "outGain", -2.f } });

        add ("Heavy & Industrial", "Heavy & Industrial",
             { Fx::Fuzz, Fx::TapeDistortion, Fx::RingMod, Fx::Filter,
               Fx::Radio, Fx::Delay, Fx::Reverb, Fx::Granular },
             { { "a_fuzz", 0.82f }, { "fuzzDrive", 0.85f }, { "fuzzTone", 0.5f }, { "fuzzMode", 1 },
               { "a_tape", 0.65f }, { "tapeDrive", 0.78f }, { "a_ring", 0.38f }, { "ringFreq", 90 },
               { "ringShape", 2 }, { "a_filt", 0.62f }, { "filtRes", 0.68f },
               { "a_radio", 0.4f }, { "radMode", 5 }, { "a_dly", 0.3f }, { "dlyFb", 0.5f },
               { "a_rvb", 0.38f }, { "revRoom", 3 }, { "revDecay", 5.5f },
               { "outGain", -4.f } });

        add ("Glitch & Rhythm", "Glitch & Rhythm",
             { Fx::Stutter, Fx::Scatter, Fx::Reverse, Fx::TapeStop,
               Fx::Granular, Fx::Delay, Fx::Filter, Fx::Reverb },
             { { "a_stut", 0.7f }, { "glDiv", 6 }, { "a_scat", 0.55f }, { "a_rev", 0.4f },
               { "a_tstop", 0.5f }, { "tsTime", 0.35f }, { "a_gran", 0.6f }, { "grainSize", 80 },
               { "grainDensity", 22 }, { "grainJitter", 0.35f }, { "a_dly", 0.45f },
               { "dlyMode", 6 }, { "dlyDiv", 6 }, { "dlyFb", 0.58f }, { "a_filt", 0.4f },
               { "a_rvb", 0.25f }, { "revRoom", 1 }, { "outGain", -2.f } });

        add ("Pitch & Harmony", "Pitch & Harmony",
             { Fx::Harmony, Fx::Whammy, Fx::Dive, Fx::Detune,
               Fx::RingMod, Fx::Shimmer, Fx::Delay, Fx::Reverb },
             { { "a_harm", 0.8f }, { "harmDown", -12 }, { "harmUp", 7 }, { "harmDownLvl", 0.8f },
               { "harmUpLvl", 0.75f }, { "harmSpread", 0.8f }, { "pitchWin", 42 },
               { "a_wham", 0.3f }, { "whamPos", 12 }, { "a_dive", 0.2f }, { "diveDepth", 24 },
               { "a_det", 0.25f }, { "detCents", 12 }, { "a_ring", 0.2f }, { "ringFreq", 180 },
               { "a_shim", 0.55f }, { "shInt", 12 }, { "shFb", 0.55f }, { "shLow", 0.25f },
               { "a_dly", 0.35f }, { "a_rvb", 0.4f }, { "revDecay", 7 }, { "outGain", -2.f } });

        add ("Granular Textures", "Granular Textures",
             { Fx::Granular, Fx::Shimmer, Fx::Reverb, Fx::Delay,
               Fx::Filter, Fx::Harmony, Fx::TapeDistortion, Fx::Chorus },
             { { "a_gran", 0.88f }, { "grainSize", 280 }, { "grainDensity", 9 }, { "grainPitch", 0.6f },
               { "grainJitter", 0.55f }, { "grainPos", 0.65f }, { "grainReverse", 0.35f },
               { "grainSpread", 0.85f }, { "grainFb", 0.22f }, { "a_shim", 0.5f },
               { "a_rvb", 0.6f }, { "revRoom", 8 }, { "revDecay", 11 }, { "a_dly", 0.35f },
               { "dlyMode", 4 }, { "a_filt", 0.35f }, { "a_harm", 0.2f }, { "a_tape", 0.25f },
               { "a_cho", 0.2f }, { "dryWet", 0.95f }, { "outGain", -2.f } });

        add ("Creative Space", "Creative Space",
             { Fx::Reverb, Fx::Shimmer, Fx::Delay, Fx::Granular,
               Fx::Harmony, Fx::Filter, Fx::Chorus, Fx::TapeDistortion },
             { { "a_rvb", 0.75f }, { "revRoom", 6 }, { "revDecay", 12 }, { "revDamp", 0.7f },
               { "revDiff", 0.8f }, { "revEarly", 0.2f }, { "revWidth", 0.9f },
               { "a_shim", 0.6f }, { "shDecay", 14 }, { "shFb", 0.62f }, { "shLow", 0.3f },
               { "a_dly", 0.4f }, { "dlyMode", 5 }, { "dlyFb", 0.5f }, { "a_gran", 0.3f },
               { "a_harm", 0.25f }, { "a_filt", 0.3f }, { "a_cho", 0.25f }, { "a_tape", 0.2f },
               { "dryWet", 0.92f }, { "outGain", -2.5f } });

        add ("Hybrid FX", "Hybrid FX",
             { Fx::Fuzz, Fx::RingMod, Fx::Filter, Fx::Granular,
               Fx::Delay, Fx::Shimmer, Fx::Reverb, Fx::Harmony },
             { { "a_fuzz", 0.55f }, { "a_ring", 0.35f }, { "ringFreq", 320 }, { "ringShape", 1 },
               { "a_filt", 0.55f }, { "filtMode", 1 }, { "filtRate", 0.8f }, { "filtDepth", 0.6f },
               { "a_gran", 0.45f }, { "a_dly", 0.4f }, { "dlyMode", 4 }, { "a_shim", 0.35f },
               { "a_rvb", 0.35f }, { "a_harm", 0.4f }, { "harmUp", 5 }, { "outGain", -2.f } });

        add ("Radio Ghost", "Vintage Radio",
             { Fx::LfoHauntedRadio, Fx::Filter, Fx::Radio, Fx::LfoDeathy,
               Fx::Reverb, Fx::Delay, Fx::Chorus, Fx::TapeDistortion },
             { { "a_haunt", 0.75f }, { "hauntRate", 0.07f }, { "hauntDepth", 0.85f }, { "hauntRes", 0.72f },
               { "a_filt", 0.55f }, { "filtMode", 2 }, { "filtFreq", 900 }, { "filtRate", 0.15f },
               { "a_radio", 0.5f }, { "radMode", 2 }, { "radNoise", 0.18f }, { "a_deathy", 0.4f },
               { "a_rvb", 0.45f }, { "revRoom", 3 }, { "revDecay", 6 }, { "a_dly", 0.3f },
               { "a_cho", 0.22f }, { "a_tape", 0.3f }, { "outGain", -2.f } });

        add ("Lunar Tremor", "Heavy & Industrial",
             { Fx::LfoLunarTremor, Fx::Filter, Fx::Fuzz, Fx::TapeDistortion,
               Fx::Delay, Fx::Reverb, Fx::Chorus, Fx::Shimmer },
             { { "a_tremor", 0.8f }, { "tremorRate", 5.5f }, { "tremorDepth", 0.82f },
               { "tremorShape", 2 }, { "tremorRes", 0.55f }, { "a_filt", 0.5f }, { "filtMode", 0 },
               { "filtRate", 0.4f }, { "filtDepth", 0.55f }, { "a_fuzz", 0.45f }, { "a_tape", 0.5f },
               { "a_dly", 0.35f }, { "dlyMode", 1 }, { "a_rvb", 0.35f }, { "a_cho", 0.2f },
               { "a_shim", 0.3f }, { "outGain", -3.f } });

        add ("Tape Ghost", "Tape Experiments",
             { Fx::LfoTapeGhost, Fx::TapeDistortion, Fx::Filter, Fx::Reverb,
               Fx::Delay, Fx::Granular, Fx::Shimmer, Fx::Harmony },
             { { "a_ghost", 0.75f }, { "ghostRate", 0.12f }, { "ghostDepth", 0.7f },
               { "ghostTone", 0.4f }, { "ghostRes", 0.65f }, { "a_tape", 0.6f }, { "tapeWear", 0.6f },
               { "a_filt", 0.4f }, { "a_rvb", 0.4f }, { "revRoom", 5 }, { "a_dly", 0.35f },
               { "a_gran", 0.3f }, { "a_shim", 0.35f }, { "a_harm", 0.25f }, { "outGain", -2.f } });

        add ("Deep Space", "Creative Space",
             { Fx::Reverb, Fx::Shimmer, Fx::Delay, Fx::Filter,
               Fx::Chorus, Fx::Harmony, Fx::Granular, Fx::TapeDistortion },
             { { "a_rvb", 0.82f }, { "revRoom", 9 }, { "revDecay", 18 }, { "revSize", 1.7f },
               { "revDiff", 0.9f }, { "revEarly", 0.15f }, { "revWidth", 1.f },
               { "a_shim", 0.7f }, { "shDecay", 16 }, { "shFb", 0.7f }, { "shLow", 0.35f },
               { "a_dly", 0.45f }, { "dlyMode", 4 }, { "dlyFb", 0.62f }, { "a_filt", 0.3f },
               { "a_cho", 0.2f }, { "a_harm", 0.2f }, { "a_gran", 0.25f }, { "a_tape", 0.15f },
               { "dryWet", 0.88f }, { "outGain", -3.f } });

        add ("Broken Broadcast", "Vintage Radio",
             { Fx::Radio, Fx::RingMod, Fx::TapeDistortion, Fx::Filter,
               Fx::Delay, Fx::Reverb, Fx::Stutter, Fx::Scatter },
             { { "a_radio", 0.85f }, { "radMode", 4 }, { "radNoise", 0.3f }, { "radioDrive", 13 },
               { "a_ring", 0.3f }, { "ringFreq", 700 }, { "ringShape", 3 }, { "a_tape", 0.4f },
               { "a_filt", 0.35f }, { "a_dly", 0.3f }, { "a_rvb", 0.25f }, { "a_stut", 0.25f },
               { "a_scat", 0.25f }, { "outGain", -2.f } });

        add ("Shimmer Bloom", "Lunar Ambient",
             { Fx::Shimmer, Fx::Reverb, Fx::Delay, Fx::Harmony,
               Fx::Filter, Fx::Chorus, Fx::TapeDistortion, Fx::Granular },
             { { "a_shim", 0.85f }, { "shInt", 12 }, { "shDecay", 15 }, { "shFb", 0.68f },
               { "shLow", 0.25f }, { "shTone", 11000 }, { "shWidth", 0.9f }, { "shMod", 0.35f },
               { "a_rvb", 0.6f }, { "revRoom", 8 }, { "revDecay", 13 }, { "revSize", 1.45f },
               { "a_dly", 0.35f }, { "dlyMode", 5 }, { "a_harm", 0.35f }, { "a_filt", 0.3f },
               { "a_cho", 0.2f }, { "a_tape", 0.2f }, { "a_gran", 0.2f },
               { "dryWet", 0.9f }, { "outGain", -2.5f } });

        add ("Moonlit Keys", "Lunar Ambient",
             { Fx::Harmony, Fx::Chorus, Fx::Delay, Fx::Reverb,
               Fx::Shimmer, Fx::Filter, Fx::TapeDistortion, Fx::Granular },
             { { "a_harm", 0.35f }, { "harmDown", -12 }, { "harmUp", 12 }, { "harmDownLvl", 0.6f },
               { "harmUpLvl", 0.55f }, { "harmSpread", 0.6f }, { "a_cho", 0.4f }, { "choRate", 0.35f },
               { "a_dly", 0.35f }, { "dlyMode", 1 }, { "dlyFb", 0.4f }, { "a_rvb", 0.45f },
               { "revRoom", 0 }, { "revDecay", 4.5f }, { "a_shim", 0.3f }, { "a_filt", 0.35f },
               { "filtFreq", 3500 }, { "a_tape", 0.25f }, { "a_gran", 0.15f }, { "outGain", -1.f } });

        add ("Industrial Impact", "Heavy & Industrial",
             { Fx::Fuzz, Fx::TapeDistortion, Fx::RingMod, Fx::Radio,
               Fx::Reverb, Fx::Delay, Fx::Filter, Fx::Granular },
             { { "a_fuzz", 0.7f }, { "fuzzDrive", 0.78f }, { "a_tape", 0.6f }, { "tapeBias", -0.2f },
               { "a_ring", 0.45f }, { "ringFreq", 73 }, { "ringShape", 2 }, { "a_radio", 0.5f },
               { "radMode", 5 }, { "a_rvb", 0.5f }, { "revRoom", 3 }, { "revDecay", 5.2f },
               { "a_dly", 0.25f }, { "a_filt", 0.5f }, { "a_gran", 0.25f }, { "outGain", -4.f } });

        return v;
    }();
    return p;
}
} // namespace params
