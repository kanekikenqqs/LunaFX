# Luna FX

> DJ-style live effects processor — VST3 + Standalone for Windows

## Features

- **XY Ring Pad** — 8 assignable FX nodes on the circle rim, drag to reposition
- **28 effects** across 6 categories: Pitch · FX · Glitch · Mod · Gen · Time
- **Per-FX detail panel** — full parameter knobs for every selected effect
- **Preset browser** — Factory / My Presets / DJ Drops / Templates folders
- **Accent / Kill / Bypass** — one-shot performance controls
- **Chaos knob** — randomise active effects on the fly

## Build

### GitHub Actions (recommended)

1. Fork / push this repo to GitHub
2. **Actions → Build Windows installer → Run workflow**
3. Download `LunaFX-Setup.exe` from the workflow artifacts

### Local (Windows)

```bat
build_windows.bat
```

Requires: Visual Studio 2022, CMake ≥ 3.22, Inno Setup 6

## Project structure

```
LunaFX/
  Source/
    Engine.h          DSP engine (pure C++17)
    Params.h          Parameter table + factory presets
    PluginProcessor.h/cpp
    PluginEditor.h/cpp   UI (JUCE)
    UiWidgets.h       Custom components & LookAndFeel
  installer/
    LunaFX.iss        Inno Setup script
  .github/workflows/
    build-windows.yml GitHub Actions CI
```

## Version history

See `CHANGES-1.1.0.md`
