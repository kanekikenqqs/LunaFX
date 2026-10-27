# Luna FX 1.3.0

## Eight-vertex octagonal field

- Exactly eight instrument vertices on a regular octagon: four on the left,
  four on the right.
- Influence is now broad radial × Gaussian angular: 100% at a vertex, exactly
  dry at the centre, 53.7% at half the centre-to-vertex distance and around 36%
  on each 45-degree neighbour. Three or four tools remain active across the
  field instead of a single narrow "hot spot".
- The puck is clamped to the octagon; automation outside it is projected safely.
- Each vertex has its own large DRY/WET knob in the rack, always visible and
  editable, even when the slot is powered off or another tool is selected.
  The knob sets that tool's maximum wet level independently.
- Broad overlapping colour zones, vertex halos, weighted connection lines,
  travelling dots, an octagonal rim/echo, silver preset rings and a dry-centre
  vignette.
- Maximum mix, live proximity mix, enable, output level and master dry/wet are
  separate. Unassigned and disabled instruments are forced dry.
- Puck remains inside the octagon, node clicks do not move it, arrow keys and
  Home work, and long gestures produce a bounded number of host notifications.

## Rack and editor

- New 1600×1000 responsive layout: larger octagonal field, larger rack rows,
  larger context knobs and larger type throughout.
- Removed the non-interactive SIGNAL FLOW menu. Its space now gives the rack
  eight taller rows, eight per-tool knobs and three larger master knobs.
- Eight rack rows with power, per-tool DRY/WET knob, name/category, assignment,
  live proximity meter, selected and preset states.
- Context panel shows Output plus the selected effect's controls (three per row,
  substantially larger cards).
- Category tabs navigate the slots and stay synchronized with preset/automation
  changes.
- Night is the default; Day is a full alternate palette.
- Preset library overlay grouped by category, with factory and user presets.

## Instruments and DSP

- Fuzz: symmetric/asymmetric clipping, bias and output compensation.
- Tape Distortion: drive, tone, wear/wow, bias and output compensation.
- Harmony: independent −24…0 and 0…+24 semitone voices, fine tuning, voice
  levels, stereo spread.
- Granular: new 12-voice preallocated Hann grain cloud with size, density,
  pitch, spray, position, reverse, spread and safe feedback.
- LFO Filter: sine/triangle/S&H/drift shapes plus LP/BP/HP.
- Radio: Telephone, Vintage AM, Hollow, Resonant, Broadcast, Overdrive modes
  with resonance and noise.
- Delay: Clean/Tape/Analog/Ping-Pong/Diffused/Modulated/Rhythm, freeze,
  sync/free time, damping, modulation, ducking and feedback safety.
- Reverb: ten spaces with distinct length/damping/diffusion/early-reflection
  behaviour, plus diffusion, early level, width, modulation and freeze.
- Shimmer: high and low octave layers, tone, feedback, width, modulation and
  protected feedback.
- New LFO Tape Ghost and LFO Lunar Tremor effects.
- Kill Switch: lock-free panic request clears all tails and fades back in
  without changing user parameters.

## Removed / cleaned

- No Sub Drop, Riser, Trans Gate/Trance Gate or bit-crusher instrument.
- Retired 16-node routing; old `n0..n7` session values keep working, `n8..n15`
  are dropped during state migration. Unknown PARAM children are removed.
- Momentary kill and accent parameters are reset on state restore.

## Tests

- New eight-vertex octagonal XY C++ tests and Python reference model (25 checks).
- 65 source-level regression checks: parameter IDs, presets, eight-slot
  invariants, null guards, audio-thread allocations, feedback clamps, formatting
  and build metadata.
- DSP tests extended for granular extremes, clearTails and delay freeze.

Native Windows compilation and Ableton validation were not possible in this
environment; see `BUILD-VERIFICATION.md` and `AUDIT.md`.
