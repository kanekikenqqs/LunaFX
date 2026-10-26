from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
editor = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
editor_h = (ROOT / "Source/PluginEditor.h").read_text(encoding="utf-8")
widgets = (ROOT / "Source/UiWidgets.h").read_text(encoding="utf-8")
xy_routing = (ROOT / "Source/XYRouting.h").read_text(encoding="utf-8")
processor = (ROOT / "Source/PluginProcessor.cpp").read_text(encoding="utf-8")
params = (ROOT / "Source/Params.h").read_text(encoding="utf-8")

checks = {
    "ParamControl rejects missing APVTS parameters before range dereference": (
        "auto* p = s.getParameter (id);" in widgets
        and widgets.index("if (p == nullptr)", widgets.index("class ParamControl"))
            < widgets.index("p->getNormalisableRange()", widgets.index("class ParamControl"))
    ),
    "Setup page creates 16 real node choices and enable parameters": (
        "for (int i = 0; i < 16; ++i)" in editor
        and 'const String id = "n" + String (i);' in editor
        and 'const String enabledId = id + "e";' in editor
    ),
    "Setup layout sizes all vectors rather than indexing fixed nonexistent entries": (
        "i < (int) combos.size()" in editor
        and "i < (int) zoneCombos.size()" not in editor  # fixed 16 by the explicit loop below
        and "for (int i = 0; i < 16; ++i)" in editor
        and "const char* zIds[8]  = {};" not in editor
    ),
    "Setup navigation is accessible": ('navLabels[8]' in editor and 'setup.setVisible (idx == 7)' in editor),
    "Removed generator tab is not shown": ('"GEN"' not in editor[editor.index('navLabels[8]'):editor.index('for (int i = 0; i < 8; ++i)', editor.index('navLabels[8]'))]),
    "Circle maps compact choices to visible instruments": ('params::fxFromChoice' in widgets and 'params::isVisibleFx (fx)' in widgets),
    "XY proximity controls routed mix instead of adding a permanent wet floor": ('manualMix[(size_t) fx] * spatialWeight[(size_t) fx]' in processor and 'nw[ni] * depth' in processor),
    "Preset change triggers visible instrument feedback": ('uiPresetRevision' in processor and 'flashPreset' in widgets and 'presetFlash' in editor and 'row.onPad ? 0.68f' in editor and 'getRawParameterValue ("n" + String (ni) + "e")' in editor),
    "Removed Sub Drop, Riser, and bit-crush parameter IDs are gone": (all(term not in (ROOT / 'Source/PluginProcessor.h').read_text(encoding='utf-8') + params + (ROOT / 'Source/Engine.h').read_text(encoding='utf-8') for term in ['subFreq', 'risTime', 'crushBits', 'Fx::Riser', 'Fx::SubDrop'])),
    "Obsolete preset routing IDs removed": ('"padTL"' not in params and '"padTR"' not in params and '"padBL"' not in params and '"padBR"' not in params),
    "State restore bounds the program and releases momentary kill": (
        'jlimit (0, jmax (0, getNumPrograms() - 1)' in processor
        and 'killParam->convertTo0to1 (0.f)' in processor
    ),
    "Local Windows build script points at the actual installer script": (
        'installer\\LunaFX.iss' in (ROOT / "build_windows.bat").read_text(encoding="utf-8")
        and (ROOT / "installer/LunaFX.iss").exists()
    ),
    "XY mix uses direct distance to each of the 16 rim nodes": (
        "kFalloffRadius = 0.34f" in xy_routing
        and "const float distance = std::sqrt (dx * dx + dy * dy);" in xy_routing
        and "std::pow (proximity, kFalloffCurve)" in xy_routing
    ),
    "Right rack explains max mix versus live pad mix": (
        "PAD PROXIMITY -> LIVE MIX" in editor
        and "SLIDER = MAX" in editor
        and "r.getX() + 188" in editor
        and "proc.uiAmt[row.fxIdx].load" in editor
    ),
    "Every visible effect detail control references a defined parameter": (
        not (set(detail_id for block in re.findall(r'\{\s*"[A-Za-z0-9_]+",\s*\{([^}]*)\}\s*\}', params[params.index("inline const std::vector<FxInfo>& fxInfo()") : params.index("inline std::string amtId")])
                 for detail_id in re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', block))
             - set(re.findall(r'\b(?:F|I|C|B)\s*\(\s*"([A-Za-z][A-Za-z0-9_]*)"', params)))
    ),
    "Circle exposes exactly 16 slots, corresponding to 8 per half-circle": (
        "for (int ni = 0; ni < 16; ++ni)" in processor
        and "for (int i = 0; i < 16; ++i)" in widgets
        and "16 rim nodes (8 per half-circle)" in (ROOT / "Source/PluginEditor.h").read_text(encoding="utf-8")
    ),
}

failures = [name for name, passed in checks.items() if not passed]
for name, passed in checks.items():
    print(f"[{ 'PASS' if passed else 'FAIL' }] {name}")
if failures:
    raise SystemExit(f"{len(failures)} source sanity check(s) failed")
print(f"All {len(checks)} source sanity checks passed.")
