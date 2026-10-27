# Build and host verification status

## What was actually run here

Environment inspected: Windows, PowerShell 5.1, Ableton Live installed, Python
3 and Node from the DSH runtime. The following were **not** available:

- Visual Studio 2022 / MSVC (`cl.exe` absent, no `vcvarsall.bat`)
- CMake, Ninja, make
- a local JUCE checkout or JUCE cache
- Inno Setup 6
- a working package manager (`winget.exe` is a 0-byte WindowsApps stub)
- direct HTTPS downloads (Schannel returns
  `SEC_E_NO_CREDENTIALS`, so JUCE/Build Tools cannot be fetched)

The shell was searched for `cl.exe`, `g++`, `clang++`, `zig` and `cmake`; none
were found. There is therefore **no native VST3 or installer artifact** in this
package and Ableton compatibility is **not** claimed.

The tests that could run in this environment are the Python checks:

```text
python tests/test_xy_model.py       -> 19/19 PASS
python tests/test_source_sanity.py  -> 60/60 PASS
```

The C++ DSP tests (`tests/test_engine.cpp`, `tests/test_engine_smoke.cpp`,
`tests/test_xy_weights.cpp`) have been updated for the new model but were not
compiled here. They are wired into CMake and CI.

## Required Windows build

```bat
python tests\test_source_sanity.py
python tests\test_xy_model.py
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LunaFX accentfx_dsp_test accentfx_dsp_smoke accentfx_xy_test --parallel
build\Release\accentfx_dsp_test.exe
build\Release\accentfx_dsp_smoke.exe
build\Release\accentfx_xy_test.exe
```

Expected artefacts:

- `build\LunaFX_artefacts\Release\VST3\Luna FX.vst3`
- `build\LunaFX_artefacts\Release\Standalone\Luna FX.exe`

Installer (Inno Setup 6):

```bat
ISCC.exe /Odist installer\LunaFX.iss
```

The CI workflow `.github/workflows/build-windows.yml` does the same on a
`windows-2022` runner with JUCE 8.0.6 and uploads `LunaFX-Setup.exe`.

## Host checklist still required

1. Build with MSVC in Release x64.
2. Install the VST3 into a clean plug-in folder.
3. Open Ableton Live 12 Suite; insert Luna FX on audio and MIDI tracks.
4. Open/close the editor repeatedly, resize it, toggle Day/Night.
5. Save, close and reopen the project; confirm the eight assignments, enables,
   maximum mixes, output levels and effect knobs restore.
6. Load every factory preset and every saved user preset.
7. Automate pad X/Y, a maximum mix, slot enable, output level, a mode choice and
   one detail knob; confirm automation lanes and host notifications.
8. Move the puck to each node and verify the corresponding instrument becomes
   audible; move to the centre and verify the dry signal.
9. Exercise Kill repeatedly with long reverb/delay/granular tails; confirm no
   permanent mute and no click.
10. Run the VST3 validator (`pluginval`) at strictness 10 if available.

If Ableton still crashes, collect the `.dmp` and Event Viewer faulting module
and compare against the candidate list in `AUDIT.md`. Do not assume the 1.1.1
`SetupPage` defect is the cause of a different build's crash.
