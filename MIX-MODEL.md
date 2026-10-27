# Luna FX mix model

This document is the contract between the XY surface, the rack and the DSP.
The implementation lives in `Source/XYRouting.h`,
`Source/PluginProcessor.cpp` (`buildSettings`) and `Source/Engine.h`.

## 1. Eight slots

| Slot | Side | Default instrument |
|---|---|---|
| 0 | left upper | Harmony |
| 1 | left mid-upper | Fuzz |
| 2 | left mid-lower | Tape Distortion |
| 3 | left lower | LFO Filter |
| 4 | right upper | Radio |
| 5 | right mid-upper | Granular |
| 6 | right mid-lower | Delay |
| 7 | right lower | Reverb |

A slot references one instrument from the library. An instrument may only be
assigned to one slot at a time; selecting an instrument elsewhere clears the old
slot. The library has more than eight instruments, so instruments that are not
currently assigned are available through **CHANGE** but do not process audio.

## 2. Distance-based proximity

Coordinates are normalised to the unit disk (`x,y ∈ [-1,1]`, `y` points up).
Slot positions use radius `kSlotRadius = 0.86` and the mirrored angles in
`kSlotAngleDeg`.

For slot `i` at `(nx, ny)` and puck at `(x, y)`:

```
d_i = sqrt((x - nx)^2 + (y - ny)^2)
s_i = clamp01(1 - d_i / kProximityFalloff)      kProximityFalloff = 0.62
w_i = s_i^2 * (3 - 2*s_i)                        smoothstep
```

Properties:

- `w_i = 1` exactly on the node.
- `w_i = 0` when the puck is farther than 0.62 unit-disk units.
- the centre is completely dry (nearest node is 0.86 away).
- adjacent nodes are about 11% influenced at a node and blend around 66% each
  at their midpoint.
- the function is radial and C1-continuous; no angular branch or on/off step.

`Source/XYRouting.h` is JUCE-free and `tests/test_xy_weights.cpp` plus
`tests/test_xy_model.py` test exactly this mapping.

## 3. Effective wet amount

```
assigned_i   = slot i has a valid visible instrument
enabled_i    = slot enable parameter n<i>e is on
proximity_i  = w_i
depth        = padDepth (0..1)
maxMix_i     = a_<instrument> maximum mix for the assigned instrument
effective_i  = assigned_i && enabled_i
               ? clamp01(maxMix_i * depth * proximity_i)
               : 0
```

The engine receives one amount per instrument. Duplicate assignments are
prevented by the rack and sanitised on state restore; if a malformed state still
contains a duplicate, the first slot wins.

`maxMix` is the ceiling and is shown in the rack. `effective` is the live mix and
is shown by the dedicated rack meter and the node percentage. They are not the
same value.

## 4. Enable, output and global mix

- **Enable** (`n<i>e`) gates the slot. A disabled slot is zero even if the puck
  is directly on its node.
- **Output level** (`n<i>v`, -24…+12 dB) scales the instrument’s wet
  contribution after its own wet/dry blend. In insert effects this is
  implemented by `Engine::blendWet`: the dry path is untouched, and an output of
  −∞ dB leaves the dry signal intact. For send effects it scales the send/return
  contribution.
- **Accent** gates the whole engine (`gEng`) with the configured fade. XY is
  independent of Accent.
- **Master Dry/Wet** (`dryWet`) is a final parallel crossfade between the
  original input and the engine output. It is not multiplied by the XY
  proximity again.

The chain is serial inside the engine:

```
PITCH -> FX -> GLITCH -> MOD -> DELAY/REVERB/SHIMMER sends -> MASTER DRY/WET
```

A slot does not multiply its wet amount twice: `maxMix`, `depth`,
`proximity`, enable and output are the only terms in the slot path. Master
dry/wet is applied once at the end.

## 5. Tails and Kill Switch

Delay, reverb and shimmer are sends. They keep ringing after Accent is released,
which is intentional. The Kill Switch (top bar, momentary) issues a lock-free
request counter consumed by the audio thread. On the next block:

1. `Engine::clearTails()` clears every delay, granular, stutter, freeze,
   pitch-shift and reverb buffer and resets filter/phase state;
2. the output fades back in over 8 ms;
3. user parameters, slot assignments, max mixes and output levels are not
   changed.

The KILL parameter is also reset to zero on state restore, so a saved project
cannot open with the panic button stuck on.
