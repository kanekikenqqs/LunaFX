# Luna FX

> DJ-style live effects processor — VST3 + Standalone for Windows

## Features

- **XY Ring Pad** — 16 configurable FX routing points around the circle (8 per half-circle), with live mix driven by proximity
- **26 visible instruments** across Pitch · FX · Glitch · Mod · Time (two internal legacy slots are hidden)
- **Right-side FX rack** — each instrument has power, maximum Mix, live XY mix meter, and effect-specific controls when selected
- **Preset browser** — factory presets plus persistent user presets
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
    XYRouting.h       JUCE-independent XY proximity mixer
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

See `CHANGES-1.2.1.md`, `CHANGES-1.1.1.md`, and `CRASH-INVESTIGATION.md`

## User presets

Use **PRE → + SAVE** to save the current sound. User presets are stored in the user's application-data directory under `LunaLabs/Luna FX/Presets` as `.lfxpreset` files. The **SET** page exposes all 16 routing-node assignments and enable switches. In the right-side rack, each FX slider sets the maximum mix; moving the XY puck closer to its assigned ring node increases the live effect mix, while moving away reduces it. Select an instrument row or ring node to open its dedicated parameters. Preset changes briefly illuminate the assigned nodes and active rack rows.

If the plug-in editor previously crashed during opening, see `CRASH-INVESTIGATION.md` for the source-level diagnosis and Windows validation steps.
