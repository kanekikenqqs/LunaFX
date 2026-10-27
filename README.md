# Luna FX

> Eight-slot XY performance instrument — VST3 + Standalone for Windows

## What it is

Luna FX is a serial multi-effects performance instrument built around an
eight-node XY circle. Four instruments sit on the left arc and four on the
right. The puck's **physical distance** to a node sets that instrument's live wet
mix; the rack's **MAX** slider is the ceiling, and the **LIVE** meter shows what
the puck is currently contributing. The full model is in
[`MIX-MODEL.md`](MIX-MODEL.md).

## Features

- **Eight-slot XY circle** — smooth vector lunar surface, distance-based live
  mix, separate preset-assignment rings, keyboard support.
- **Instrument rack** — enable, assignment, maximum mix, live proximity meter,
  selected/preset state for all eight slots.
- **Large contextual knobs** — three per row, real APVTS parameters, unit-aware
  values, double-click reset and editable numbers.
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
    UiWidgets.h        lunar LookAndFeel, XY circle, knobs, rack rows
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
