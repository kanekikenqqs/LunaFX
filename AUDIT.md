# Luna FX 1.3.0 — audit, repairs and redesign

## Scope

This package continues the existing `LunaFX` JUCE project (same product, plug-in
codes, build system and DSP engine). It does not introduce a second plug-in
identity. The work covers the startup-crash investigation, the eight-slot XY
model, the rack/editor redesign, new and revised DSP, state migration, presets
and source-level regression tests.

## Crash investigation

The original Ableton crash path documented for 1.1.1 was:

- `SetupPage` constructed old routing controls with empty parameter IDs.
- `ui::ParamControl` captured `p->getNormalisableRange()` before checking for a
  null `RangedAudioParameter*`.
- `SetupPage::resized()` indexed four combo boxes while only three had been
  created, then indexed empty routing vectors.
- `LunaFXEditor::resized()` called `setup.setBounds()` during editor creation,
  so those invalid accesses could happen as soon as the editor was opened.

**The current 1.2.1 source no longer contains `SetupPage` or that construction
path, and its `ParamControl` already has a null guard.** The exact 1.1.1 defect
therefore cannot be reproduced from this source. The published previous report
was not enough to prove what Ableton faulted on in the newer build.

The new source removes the class of defect rather than relying on one guard:

- `KnobCard` resolves the parameter once, checks `param == nullptr` before any
  parameter dereference, and renders an “unavailable” card if an ID is missing.
- `ContextPanel` validates every detail ID with `params::find` before creating a
  control.
- `LunarCircle` caches slot parameters through null-checked helpers and guards
  every `RangedAudioParameter*` before use.
- All raw parameter dereferences in the UI/processor are guarded; the source
  test fails if a chained `getRawParameterValue(...)->load()` returns.
- State restore validates the XML tag, drops unknown/removed PARAM children,
  resets absent preset parameters, clamps the program index, resets the
  momentary KILL, and de-duplicates slot assignments.
- The preset-save `TextEditor` is no longer deleted from inside its own return
  key callback; deletion is deferred and SafePointer-checked.
- The eight slots are a fixed array with bounds checks in every lookup.

## Eight-vertex octagonal field

The octagonal field and the rack now represent the same eight slots:

- slots 0–3 on the left arc,
- slots 4–7 on the right arc,
- one assignable instrument per slot,
- independent enable, maximum mix, output level, live proximity mix and
  preset/highlight state.

The documented distance mapping lives in `Source/XYRouting.h` and
`MIX-MODEL.md`. It is radial (Euclidean), C1-continuous (smoothstep), zero at
the centre, and exactly 1 on a node. Unassigned or disabled instruments are
forced to zero in `buildSettings`; the previous “enabled but unassigned still
processes” behaviour is gone.

## DSP changes

- **Tape Distortion** remains the replacement for the old bit-crusher, with
  drive, tone, wear/wow, bias and output compensation.
- **Fuzz** gained symmetric/asymmetric clipping, user bias and output comp.
- **Harmony** now has independent lower/upper semitones, fine tune, voice
  levels, stereo spread and window.
- **Granular** was rebuilt as a preallocated 12-voice Hann-window grain cloud
  with grain size, density, pitch, spray, position, reverse probability, stereo
  spread and safe feedback.
- **Delay** has Clean/Tape/Analog/Ping-Pong/Diffused/Modulated/Rhythm modes,
  freeze, tempo sync, damping, modulation, ducking and a protected feedback path.
- **Reverb** has ten distinct room/algorithm parameter sets, diffusion, early
  reflections, damping, modulation, width and freeze.
- **Shimmer** gained a low-octave layer in addition to the high shimmer voice;
  feedback is tone-limited and clamped.
- **Radio** has six genuinely different band-limit/resonance/drive modes.
- **Filter** has sine/triangle/S&H/drift LFO shapes; **LFO Tape Ghost** and
  **LFO Lunar Tremor** were added as distinct effects.
- **Kill Switch** clears delay, granular, reverb and shimmer state through a
  lock-free request counter and fades back in over 8 ms.

## UI

The editor is a 1600×1000 responsive layout with a Night/Day theme:

- vector octagonal field (no pixel texture) with eight vertices, broad overlapping
  influence zones, smoothed proximity glow, live percentage, distinct silver
  preset-assignment rings, keyboard support (arrow keys, Home, double-click
  centre) and accessibility titles/tooltips;
- 392 px rack with eight tall slot rows: power, a per-tool DRY/WET knob, name,
  assignment, live proximity meter and selected/preset states; the
  non-interactive signal-flow menu was removed so this section can be larger;
- 384 px context panel with large parameter cards (three per row) including
  Output; every control is an APVTS parameter;
- top bar preset navigation/save, global kill and bypass;
- performance strip with Accent, XY Depth, Fade, Chaos, Touch and Spring;
- preset library overlay grouped by category, factory and user presets;
- unit-aware value formatting (`250 ms`, `2 kHz`, `-12 dB`, `+12 st`, `85%`,
  `0.75`) with no meaningless trailing zeros.

## Sound preservation

Defaults were chosen to keep the historical character for existing controls:

- Harmony voice levels default to 50%/50% (the old shared-mix behaviour), fine
  tune 0, stereo spread 0.
- Shimmer's new low-octave layer defaults to 0%; the high shimmer voice is
  unchanged.
- The reverb default is Hall with the old 1.0 line-length scale and no added
  early-reflection level; its diffusion/early controls are available in the
  context panel.
- Delay defaults to Clean; the old parameter set and feedback/tone path are
  retained. The new modes are opt-in.
- Filter defaults to the old sine LFO and LP/BP/HP selection.

Intentional sonic changes, isolated to new controls or refinements: the small
asymmetric term in Fuzz, Tape output compensation, Radio's six modes (default
is the narrow Telephone band), the reverb diffusion/early network, and the new
granular voice architecture. Factory presets were rewritten around the new
defaults. Native A/B testing is still required on Windows.

## Tests executed in this environment

- `python tests/test_xy_model.py` — 25/25 passed. This parses the C++ constants
  and independently verifies the documented distance function: centre dry, node
  peak, monotonic approach, continuity, mirror symmetry, adjacent blend, bounds,
  and max-mix/depth/effective-wet arithmetic.
- `python tests/test_source_sanity.py` — 65/65 passed. Checks parameter-ID
  integrity, preset integrity, eight-slot invariants, crash-path guards,
  audio-thread allocation scans, feedback clamps, formatting, build metadata and
  balanced source structure.

## Not verified here

This environment has no MSVC/CMake/JUCE/Ableton/Inno Setup and its direct HTTPS
download is blocked (`schannel: SEC_E_NO_CREDENTIALS`), so the native Windows
VST3, the C++ test executables and the installer could **not** be built here.
No host-compatibility claim is made. See `BUILD-VERIFICATION.md` for the exact
commands and the remaining native validation checklist.
