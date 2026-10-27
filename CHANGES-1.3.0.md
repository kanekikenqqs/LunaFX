# Luna FX 1.3.0

## Eight-slot XY circle

- Exactly eight instrument nodes: four on the left side, four on the right.
- Nodes are fixed positions; the rack and the circle share the same eight slots.
- Distance-based live mix documented in `MIX-MODEL.md`: Euclidean distance,
  smoothstep falloff, zero at the centre, one on a node.
- Maximum mix, live proximity mix, enable, output level and master dry/wet are
  separate controls. Unassigned and disabled instruments are forced dry.
- Node brightness, glow, connection line, rack meter and percentage all follow
  the smoothed proximity. Preset assignment is shown by a separate silver ring.
- Puck remains inside the disk, node clicks do not move it, arrow keys and Home
  work, and long gestures produce a bounded number of host notifications.

## Rack and editor

- New 1440×900 responsive layout: vector lunar circle, 350 px instrument rack,
  358 px contextual parameter panel, top preset/global bar, performance strip.
- Eight rack rows with enable, name/category, assignment, maximum-mix slider,
  live proximity meter, selected and preset states.
- Large parameter cards (three per row) replace the small 72 px controls. Each
  card has a track, progress arc, indicator, readable name, unit-aware value,
  double-click reset and editable numeric value.
- Context panel updates on slot selection, preset load, automation, enable
  change and state restore.
- Category tabs navigate the slots and act as the assignment palette.
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

- New 8-slot XY C++ tests and Python reference model (19 checks).
- 60 source-level regression checks: parameter IDs, presets, eight-slot
  invariants, null guards, audio-thread allocations, feedback clamps, formatting
  and build metadata.
- DSP tests extended for granular extremes, clearTails and delay freeze.

Native Windows compilation and Ableton validation were not possible in this
environment; see `BUILD-VERIFICATION.md` and `AUDIT.md`.
