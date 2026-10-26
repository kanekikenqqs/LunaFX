# Luna FX 1.1.1 — startup crash investigation

## Root cause found in the uploaded source

`SetupPage` was still being constructed by `LunaFXEditor`, even though its old routing UI had been removed. The old constructor passed empty/null parameter IDs to `ui::ParamControl`. `ParamControl` then captured `p->getNormalisableRange()` while constructing its slider formatter, which dereferenced the null pointer. In addition, `SetupPage::resized()` indexed four ComboBox controls when only three existed, then indexed empty routing vectors. `LunaFXEditor::resized()` calls `setup.setBounds()` during editor construction, so those out-of-range accesses could crash as soon as the editor was instantiated.

## Fixes in this package

- Added a release-build null guard to `ParamControl`, so stale/missing parameter IDs render a safe “unavailable” label instead of dereferencing a null pointer.
- Reconnected the setup routing controls to the actual `n0..n15` and `n0e..n15e` parameters and made the layout size-aware. There are now 16 configurable routing points in a four-column grid.
- Added a dedicated `SET` navigation page so the routing settings can actually be reached; `PRE` remains the preset browser.
- Guarded FX detail selection indices, validated incoming plug-in state, clamped restored program indices, and reset the momentary KILL parameter on state restore.
- Replaced invalid/non-finite sample-rate values with a safe 48 kHz fallback before allocating DSP buffers.
- Added a user preset save/load path under the user's application-data directory and removed nonfunctional empty placeholder folders.
- Fixed the Windows batch build script to use `installer\\LunaFX.iss`, fixed the invalid installer GUID, and synchronised the displayed version to 1.1.1.
- Made the central XY texture restrained monochrome. DSP algorithm coefficients and routing in `Engine.h` were not changed as part of the crash fix.

## Validation still required on Windows

This environment cannot resolve `github.com` and has no local JUCE SDK, MSVC, Ableton Live, or Inno Setup, so it cannot perform a full VST3/Standalone build or reproduce the host crash directly. The C++ DSP engine has a standalone test suite; run it on CI or Windows, then install the built VST3 into a clean test set and launch Ableton with the plug-in both visible and hidden. If the exact host crash persists after this patch, collect the Ableton crash report and Windows Event Viewer faulting-module details.
