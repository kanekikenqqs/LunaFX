# Luna FX mix model

This is the contract between the octagonal XY field, the rack and the DSP.
Implementation: `Source/XYRouting.h`, `Source/PluginProcessor.cpp`
(`buildSettings`) and `Source/Engine.h`.

## 1. Eight vertices

The control surface is a regular octagon. Its eight vertices are the eight
instrument points:

| Slot | Side | Default instrument |
|---|---|---|
| 0 | left upper | Harmony |
| 1 | left | Fuzz |
| 2 | left | Tape Distortion |
| 3 | left lower | LFO Filter |
| 4 | right upper | Radio |
| 5 | right | Granular |
| 6 | right | Delay |
| 7 | right lower | Reverb |

Each slot has its own **DRY/WET (MIX)** knob in the rack. It is always visible
and editable, including for a slot that is not currently the closest tool and
including while the slot is powered off. Changing it stores that tool's maximum
wet level; when the tool is enabled and receives proximity, that stored level is
what it can contribute. The knob is independent per tool, not a shared master.

The library is larger than eight instruments. Unassigned library instruments do
not process audio. Assigning an instrument to a slot is done with **CHANGE** in
that row.

## 2. Octagonal field influence

Coordinates are normalised to the unit disk (`x,y ∈ [-1,1]`, `y` points up).
The eight vertices are on radius `kSlotRadius = 0.86` at the regular-octagon
angles:

```
left  (top -> bottom): 112.5, 157.5, 202.5, 247.5 degrees
right (top -> bottom):  67.5,  22.5, -22.5, -67.5 degrees
```

The puck is clamped to the octagon (`clampToField`). Influence for vertex `i`:

```
r        = length(puck)
radial   = smoothstep(clamp01(r / kRadialGate))          kRadialGate = 0.82
dAngle   = abs(shortestAngle(puckAngle - vertexAngle_i))
angular  = exp(-0.5 * (dAngle / kAngularSigmaRad)^2)      kAngularSigmaRad = 0.55

w_i = radial * angular
```

Properties:

- `w_i = 1` at the vertex.
- `w_i = 0` at the centre. The centre is physically dry.
- Directly halfway from centre to a vertex, the aligned tool is **0.537** (more
  than 50%).
- At a vertex, each 45-degree neighbour is around **0.36**, so two adjacent
  tools also contribute, more weakly. Across the sampled field at least three
  tools are meaningfully active (above 2%) and typically four are present.
- The Gaussian angular field overlaps smoothly; there are no sector boundaries
  or angular on/off steps.
- The radial gate means automation outside the octagon is safe and the dry
  centre is preserved.

`Source/XYRouting.h` is JUCE-free. `tests/test_xy_weights.cpp` and
`tests/test_xy_model.py` verify the constants, geometry and the 50%-at-half
requirement.

## 3. Effective wet amount

```
assigned_i   = slot i has a valid visible instrument
enabled_i    = n<i>e is on
proximity_i  = w_i
depth        = padDepth (0..1)
mix_i        = a_<instrument> (the per-tool DRY/WET knob)

effective_i  = assigned_i && enabled_i
               ? clamp01(mix_i * depth * proximity_i)
               : 0
```

`mix_i` is the ceiling and is shown by the rack knob. `effective_i` is the live
mix and is shown by the rack LIVE meter and the vertex percentage. They are
separate values.

## 4. Enable, output and global mix

- **Enable** (`n<i>e`) gates the slot. A disabled slot is zero even at its
  vertex.
- **Output** (`n<i>v`, −24…+12 dB) scales that instrument's wet contribution
  after its own wet/dry blend. In insert effects `Engine::blendWet` leaves the
  dry path untouched; for send effects it scales the send/return.
- **Accent** gates the whole engine (`gEng`) with the configured fade.
- **Master Dry/Wet** is the final parallel blend between the original input and
  the engine output. It is applied once; XY proximity is not multiplied into it
  again.

Serial engine path:

```
PITCH -> FX -> GLITCH -> MOD -> DELAY / REVERB / SHIMMER sends -> MASTER DRY/WET
```

## 5. Visual model

- Each active vertex paints a broad soft colour wedge from the centre to its
  vertex; the wedge gradient is transparent at the centre, so the dry centre is
  visible and physically accurate.
- Vertex halo and wedge opacity follow the smoothed proximity weight.
- Connection lines from the puck to active vertices scale with weight; a small
  travelling dot marks the strongest connections.
- Selected vertex: white ring. Preset-assigned and enabled vertex: silver pulse
  ring. Disabled vertex: muted with a slash.
- Three or four tools remain visibly lit across most of the field; the centre is
  dark.

## 6. Tails and Kill Switch

Delay, reverb and shimmer are sends. They keep ringing after Accent is released.
The Kill Switch (top bar) issues a lock-free request counter consumed by the
audio thread. On the next block:

1. `Engine::clearTails()` clears delay, granular, stutter, freeze, pitch-shift
   and reverb buffers and resets filter/phase state;
2. the output fades back in over 8 ms;
3. user parameters, slot assignments, per-tool mixes and output levels are not
   changed.

The KILL parameter is reset on state restore.
