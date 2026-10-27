# Luna FX

> Eight-vertex octagonal XY performance instrument — VST3 + Standalone for Windows

## What it is

Luna FX is a serial multi-effects performance instrument built around an
eight-vertex octagonal XY field. Four instruments sit on the left half and four
on the right. Each vertex has a broad influence field, so three or four tools are
audible at once; the puck's position sets their relative live mixes. The field is
100% at a vertex, exactly dry at the centre, and still above 50% halfway to a
vertex. `MIX-MODEL.md` documents the exact mapping.

## Features

- **Eight-vertex octagonal field** — broad overlapping zones, smooth proximity,
  octagonal clamping, preset rings, keyboard support.
- **Per-tool DRY/WET knob** — every rack row has its own independent mix knob,
  editable even when the tool is powered off or another tool is selected.
- **Instrument rack** — power, per-tool mix, assignment, live proximity meter,
  selected/preset state for all eight slots, plus a larger master section.
- **Large contextual knobs** — three large cards per row, real APVTS parameters,
  unit-aware values, double-click reset and editable numbers.
- **Instrument library** — Harmony, Fuzz, Tape Distortion, Ring Mod, LFO Filter,
  Radio, Auto Wah, Stutter, Reverse, Tape Stop, Freeze, Scatter, Granular,
  Chorus, Flanger, Phaser, Tremolo, Vibrato, LFO Deathy, LFO Haunted Radio,
  LFO Tape Ghost, LFO Lunar Tremor, Delay, Reverb, Shimmer.
- **Preset library** — factory categories plus user presets.
- **Accent / Kill / Bypass** — performance controls; Kill is a lock-free tail
  reset.
- **Night and Day** lunar palettes; Night is the default.

## Build

### GitHub Actions (recommended)

1. Push the repository to GitHub.
2. **Actions → Build Windows installer and run checks → Run workflow**.
3. Download `LunaFX-Setup.exe` from the workflow artifacts.

### Local (Windows)

```bat
build_windows.bat
```

Requires Visual Studio 2022 (C++ workload), CMake ≥ 3.22, Python 3 and Inno
Setup 6. JUCE 8.0.6 is fetched by CMake if `JUCE_PATH` is not supplied.

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LunaFX accentfx_dsp_test accentfx_dsp_smoke accentfx_xy_test --parallel
```

## Project structure

```text
LunaFX/
  Source/
    Engine.h           DSP engine (C++17, no JUCE)
    XYRouting.h        documented 8-slot distance mapping (no JUCE)
    Params.h           parameter table, instrument library, presets
    PluginProcessor.*  APVTS, slot mix, state migration, panic request
    PluginEditor.*     layout, rack, context panel, preset overlay
    UiWidgets.h        lunar LookAndFeel, octagonal XY field, knobs, rack rows
  tests/
    test_engine.cpp
    test_engine_smoke.cpp
    test_xy_weights.cpp
    test_xy_model.py
    test_source_sanity.py
  installer/LunaFX.iss
  .github/workflows/build-windows.yml
  AUDIT.md
  MIX-MODEL.md
  BUILD-VERIFICATION.md
  CHANGES-1.3.0.md
```

## Status

Source-level checks and the independent Python XY model pass in the current
environment. Native MSVC/JUCE compilation, the VST3, the installer and Ableton
host testing could not be performed here; see
[`BUILD-VERIFICATION.md`](BUILD-VERIFICATION.md). Do not treat this package as
host-verified until that checklist is completed.

## User presets

Use **SAVE** in the top bar, or **PRESETS → + SAVE**, to store the current
sound. User presets are `.lfxpreset` files under
`LunaLabs/Luna FX/Presets` in the user application-data directory. Presets
restore the eight slot assignments, enable states, maximum mixes, output levels
and every effect parameter through normal APVTS notifications.
