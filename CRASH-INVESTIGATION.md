# Luna FX startup crash investigation

## Historical root cause (1.1.1 source)

The uploaded 1.1.1 source still constructed `SetupPage` from `LunaFXEditor`
even though the old routing UI had been removed. `SetupPage` passed empty/null
parameter IDs to `ui::ParamControl`; `ParamControl` then read
`p->getNormalisableRange()` while constructing its formatter, dereferencing a
null `RangedAudioParameter*`. In addition, `SetupPage::resized()` indexed four
combo boxes when only three existed, then indexed empty routing vectors.
`LunaFXEditor::resized()` called `setup.setBounds()` while the editor was being
constructed, so those invalid accesses could occur as soon as the editor opened.

## State of the supplied 1.2.1 source

The supplied 1.2.1 tree **does not contain `SetupPage`**. `ui::ParamControl`
already returns early when `s.getParameter(id)` is null, and the setup routing
grid is gone. The exact 1.1.1 crash path therefore cannot be reproduced from
this source. The previous report does not prove that Ableton was faulting on
that path in a later build.

## What 1.3.0 changes

The eight-slot rewrite removes the whole class of failure rather than relying on
one guard:

- `KnobCard` checks the parameter before any dereference and renders an
  “unavailable” card if an ID is missing.
- `ContextPanel` validates every effect-detail ID with `params::find` before
  creating a control.
- All UI raw-parameter reads are null-checked; a source test fails on a chained
  `getRawParameterValue(...)->load()`.
- State restore validates XML tag and size, removes unknown/removed PARAM
  children, resets absent preset parameters, de-duplicates slot assignments,
  clamps the program index and resets the momentary KILL.
- Slot arrays are fixed eight-entry arrays with bounds checks.
- The preset-save editor is not deleted from inside its own callback; deletion is
  deferred and SafePointer-checked.

The full list of reviewed defects and residual risks is in
[`AUDIT.md`](AUDIT.md).

## What still has to be verified on Windows

The 1.3.0 native VST3 was not compiled in this environment (no MSVC, CMake, JUCE
or Inno Setup; direct HTTPS blocked). Open the plug-in in Ableton Live 12 Suite,
then run the checklist in [`BUILD-VERIFICATION.md`](BUILD-VERIFICATION.md). If a
crash remains, capture the Ableton crash report and the Windows Event Viewer
faulting module; do not assume the historical `SetupPage` defect applies.
