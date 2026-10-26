from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
editor = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
editor_h = (ROOT / "Source/PluginEditor.h").read_text(encoding="utf-8")
widgets = (ROOT / "Source/UiWidgets.h").read_text(encoding="utf-8")
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
    "Setup navigation is accessible": ('navLabels[9]' in editor and 'setup.setVisible (idx == 8)' in editor),
    "Obsolete preset routing IDs removed": ('"padTL"' not in params and '"padTR"' not in params and '"padBL"' not in params and '"padBR"' not in params),
    "State restore bounds the program and releases momentary kill": (
        'jlimit (0, jmax (0, getNumPrograms() - 1)' in processor
        and 'killParam->convertTo0to1 (0.f)' in processor
    ),
    "Local Windows build script points at the actual installer script": (
        'installer\\LunaFX.iss' in (ROOT / "build_windows.bat").read_text(encoding="utf-8")
        and (ROOT / "installer/LunaFX.iss").exists()
    ),
}

failures = [name for name, passed in checks.items() if not passed]
for name, passed in checks.items():
    print(f"[{ 'PASS' if passed else 'FAIL' }] {name}")
if failures:
    raise SystemExit(f"{len(failures)} source sanity check(s) failed")
print(f"All {len(checks)} source sanity checks passed.")
