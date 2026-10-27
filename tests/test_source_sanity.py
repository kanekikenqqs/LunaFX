#!/usr/bin/env python3
"""Source-level regression checks for Luna FX 1.3.0.

These are not a substitute for compiling with MSVC/JUCE. They catch the defects
that caused the previous editor crash, verify the eight-slot parameter model and
check that no obvious allocation or unsafe raw-parameter access was introduced
into the audio path.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source"

editor = (SRC / "PluginEditor.cpp").read_text(encoding="utf-8")
editor_h = (SRC / "PluginEditor.h").read_text(encoding="utf-8")
widgets = (SRC / "UiWidgets.h").read_text(encoding="utf-8")
xy = (SRC / "XYRouting.h").read_text(encoding="utf-8")
processor = (SRC / "PluginProcessor.cpp").read_text(encoding="utf-8")
processor_h = (SRC / "PluginProcessor.h").read_text(encoding="utf-8")
params = (SRC / "Params.h").read_text(encoding="utf-8")
engine = (SRC / "Engine.h").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
installer = (ROOT / "installer/LunaFX.iss").read_text(encoding="utf-8")
workflow = (ROOT / ".github/workflows/build-windows.yml").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, ok: bool) -> None:
    checks.append((name, bool(ok)))


def section(text: str, start: str, end: str) -> str:
    i = text.find(start)
    if i < 0:
        return ""
    j = text.find(end, i + len(start))
    return text[i:j] if j >= 0 else text[i:]


def strip_cpp(text: str) -> str:
    """Remove comments and string/char literals, preserving structure."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            continue
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    brace = text.find('{', start)
    if brace < 0:
        return ""
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[brace:i + 1]
    return ""


# ── exact eight-slot model ───────────────────────────────────────────────────
check("XY header defines exactly 8 slots", re.search(r"kNumSlots\s*=\s*8\b", xy) is not None)
check("XY header defines four slots per side", re.search(r"kSlotsPerSide\s*=\s*4\b", xy) is not None)
check("XY mapping is a broad octagonal radial/angular field", all(t in xy for t in
      ["kRadialGate", "kAngularSigmaRad", "kFieldOrder", "smoothstep01", "std::exp", "clampToField"]))
check("XY mapping keeps the dry centre", "smoothstep01 (r / kRadialGate)" in xy)
check("no legacy nine/sixteen-node identifiers remain", not re.search(r"\bn(?:[89]|1[0-5])e?\b",
      strip_cpp(xy) + strip_cpp(processor) + strip_cpp(widgets) + strip_cpp(editor)))
check("processor drives eight slot choices", "params::slotChoiceId (s)" in processor
      and "s < params::kNumSlots" in processor)
check("UI circle and rack use params::kNumSlots", "params::kNumSlots" in widgets and "params::kNumSlots" in editor)
check("XY surface is an octagon with field clamping", "fieldPath" in widgets and "params::clampToField" in widgets)
check("signal-flow menu was removed from the rack", "SIGNAL FLOW" not in editor)
check("every rack row owns a per-tool mix knob", "rebuildMixKnob" in widgets and "mixKnob" in widgets)
check("larger 1600x1000 layout", "kDesignW = 1600" in editor_h and "kDesignH = 1000" in editor_h)
check("no sixteen-slot loops in UI/processor", not re.search(r"<\s*16\b", editor + widgets + processor))

# ── parameter table and IDs ──────────────────────────────────────────────────
defs_code = section(params, "inline const std::vector<PDef>& defs()", "inline const PDef* find")
literal_ids = re.findall(r'\b(?:F|I|C|B)\s*\(\s*"([A-Za-z][A-Za-z0-9_]*)"', defs_code)
slot_ids = []
for i in range(8):
    slot_ids += [f"n{i}", f"n{i}e", f"n{i}v"]
rnd_ids = [f"rnd{i}" for i in range(8)]
all_def_ids = literal_ids + slot_ids + rnd_ids

fx_code = section(params, "inline const std::vector<FxInfo>& fxInfo()", "inline std::string amtId")
fx_entries = re.findall(r'\{\s*"([A-Za-z][A-Za-z0-9_]*)",\s*\{([^}]*)\}', fx_code)
fx_keys = [k for k, _ in fx_entries]
detail_ids = []
for _, block in fx_entries:
    detail_ids += re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', block)
amt_ids = [f"a_{k}" for k in fx_keys]
all_def_ids += amt_ids

raw_block = section(processor_h, "#define DJX_RAW_PARAMS(X)", "class LunaFXProcessor")
raw_ids = re.findall(r"X\(([A-Za-z][A-Za-z0-9_]*)\)", raw_block)

check("fxInfo has one entry per DSP effect (30)", len(fx_entries) == 30)
check("every detail control is a defined parameter", set(detail_ids) <= set(all_def_ids))
check("every instrument has an a_ maximum-mix parameter", all(f"a_{k}" in all_def_ids for k in fx_keys))
check("every raw parameter pointer has a definition", set(raw_ids) <= set(all_def_ids))
check("no duplicate parameter IDs", len(all_def_ids) == len(set(all_def_ids)))
check("every preset parameter ID exists", True)  # replaced by the stronger preset check below

names_block = section(engine, "kFxNames[kNumFx] = {", "};")
category_block = section(engine, "kFxCategory[kNumFx] = {", "};")
name_count = len(re.findall(r'"[^"]+"', names_block))
cat_count = len(re.findall(r"-?\d+", category_block.split("=", 1)[-1]))
check("kFxNames matches the 30-entry effect table", name_count == 30)
check("kFxCategory matches the 30-entry effect table", cat_count == 30)
check("removed instruments are absent from the library", all(term not in engine + params + processor + editor
      for term in ["Sub Drop", "Riser", "Trans Gate", "Trance Gate", "8-Bit", "Bit Crush", "crushBits", "subFreq", "risTime"]))
check("Tape Distortion replaced the old bit-crusher", "Tape Distortion" in engine and "TapeDistortion" in params)

# presets
preset_code = section(params, "inline const std::vector<Preset>& presets()", "} // namespace params")
add_blocks = list(re.finditer(r'add\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*\{([^}]*)\}', preset_code))
preset_extra_ids = set(re.findall(r'\{\s*"([A-Za-z][A-Za-z0-9_]*)"\s*,\s*-?[0-9.]+f?\s*\}', preset_code))
check("factory preset library has at least 15 real presets", len(add_blocks) >= 15)
check("every preset assigns exactly eight unique instruments", all(
    len(re.findall(r"Fx::(\w+)", m.group(3))) == 8 and
    len(set(re.findall(r"Fx::(\w+)", m.group(3)))) == 8 for m in add_blocks))
check("every preset parameter value references a real ID", preset_extra_ids <= set(all_def_ids))
check("preset categories cover the requested range", len({m.group(2) for m in add_blocks}) >= 8)

# ── crash-path guards ────────────────────────────────────────────────────────
knob = section(widgets, "class KnobCard", "void setAccent (Colour c)")
first_null = knob.find("if (param == nullptr)")
first_param_deref = knob.find("param->")
check("KnobCard checks for a missing parameter before dereferencing it",
      first_null >= 0 and (first_param_deref < 0 or first_null < first_param_deref))
check("ContextPanel validates detail IDs before creating controls", "if (pd == nullptr) continue;" in editor)
check("state restore rejects null, empty and wrong-tag data",
      all(t in processor for t in ["data == nullptr || size <= 0", "getXmlFromBinary", "hasTagName", "replaceState"]))
check("state restore resets the momentary kill and clamps the program", all(t in processor for t in
      ["killParam->convertTo0to1 (0.f)", "jlimit (0, jmax (0, getNumPrograms() - 1)"]))
check("buildSettings clamps every slot mix to 0..1", "jlimit (0.f, 1.f" in processor and "slotWeights" in processor)
check("no chained raw-parameter dereference remains", "getRawParameterValue (params::amtId (fx))->load" not in editor
      and "getRawParameterValue (\"rnd\" + String (i))->load" not in processor
      and "getRawParameterValue (\"uiTheme\")->load" not in editor)
check("removed SetupPage null-ID path is gone", "SetupPage" not in editor + editor_h)
check("slot menu validates row and program bounds", all(t in editor for t in ["row < 0 || row >= (int) rows.size()", "r.program < (int) ps.size()"]))
check("state restore sanitises duplicate slot assignments", "used[fx] = true" in processor and "setValueNotifyingHost (0.f)" in processor)

# ── real audio-thread safety ────────────────────────────────────────────────
pb = function_body(processor, "void LunaFXProcessor::processBlock")
eng_process = function_body(engine, "void process (float* L, float* R, int n")
for label, body in [("processBlock", pb), ("Engine::process", eng_process)]:
    check(f"{label} contains no heap allocation", body != "" and not re.search(
        r"\b(new|delete|malloc|calloc|realloc|make_unique|make_shared|push_back|resize|assign|std::vector)\b", body))
check("processBlock has no filesystem or GUI work", not re.search(r"\bFile|MessageManager|Component|Timer|Logger\b", pb))
check("kill requests are lock-free and non-blocking", "killRequest.fetch_add" in processor_h
      and "killRequest.load" in processor and "clearTails" in processor)
check("clearTails resets delay, granular, reverb and shimmer state", all(t in engine for t in
      ["stBuf[c].clear()", "grBuf[c].clear()", "dly.clear()", "rev.clear()", "shRev.clear()"]))
check("per-instrument output gain is smoothed and bounded", "outLevel" in engine and "clampf (S.outLevel" in engine)
check("feedback paths are clamped or saturated", all(t in engine for t in
      ["clampf (S.dlyFb, 0.f, 0.97f)", "clampf (S.shFb, 0.f, 0.85f)", "std::tanh (shLp - shHp)"]))
check("granular voices are fixed-size and preallocated", "kMaxGrains" in engine and "grains[kMaxGrains]" in engine
      and "grBuf[c].init ((int) (sr * 4.0))" in engine)
check("reverb modes are backed by distinct parameter tables", "roomScale[10]" in engine and "roomEarly[10]" in engine
      and "roomDiff[10]" in engine)
check("delay modes are backed by real processing branches", "m == 4" in engine and "m == 6" in engine and "allpass" in engine)

# ── formatting ──────────────────────────────────────────────────────────────
fmt = section(params, "inline juce::String trimmedNumber", "// ------------------------------------------------------------------ definitions")
check("value formatting trims meaningless zeros", "trimCharactersAtEnd (\"0\")" in fmt)
check("value formatting has ms, kHz, dB, percent and semitone units", all(t in fmt for t in
      ["\" ms\"", "\" kHz\"", "\"dB\"", "\"%\"", "\"st\""]))
check("no six-decimal default formatting remains", "0.750000" not in strip_cpp(params + editor + widgets))

# ── build / docs ────────────────────────────────────────────────────────────
check("CMake version is 1.3.0", "VERSION 1.3.0" in cmake)
check("installer version is 1.3.0", 'AppVer  "1.3.0"' in installer)
check("all DSP test targets are declared", all(t in cmake for t in
      ["accentfx_dsp_test", "accentfx_dsp_smoke", "accentfx_xy_test"]))
check("CI runs the source checks and DSP tests", "test_source_sanity.py" in workflow
      and "accentfx_dsp_test.exe" in workflow and "accentfx_xy_test.exe" in workflow)
check("version string matches the build", 'kVersion = "1.3.0"' in params)

# ── structural sanity (helps catch truncated edits) ────────────────────────
for name, text in [("Params.h", params), ("Engine.h", engine), ("PluginProcessor.cpp", processor),
                   ("PluginEditor.cpp", editor), ("UiWidgets.h", widgets), ("XYRouting.h", xy)]:
    s = strip_cpp(text)
    check(f"{name} has balanced braces", s.count("{") == s.count("}"))
    check(f"{name} has balanced parentheses", s.count("(") == s.count(")"))

failures = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failures:
    print(f"\n{len(failures)} source sanity check(s) failed:", file=sys.stderr)
    for f in failures:
        print("  -", f, file=sys.stderr)
    raise SystemExit(1)
print(f"All {len(checks)} source sanity checks passed.")
