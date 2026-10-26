# Luna FX 1.2.1

## XY / rack behaviour
- Changed the XY ring routing response to use Euclidean proximity to the 16 actual rim nodes. An effect's live mix now rises as the puck approaches its assigned node and falls away by distance; the centre remains dry for rim-routed effects.
- Added a compact hint to the right rack: the instrument slider is its maximum mix, and the second meter shows the live XY-scaled mix.
- Preset changes highlight assigned nodes in the ring and active instruments/categories in the right rack.
- Clarified the 16-node design as eight nodes per half-circle. Node assignment and enable controls remain available in SET.

## Instruments
- Sub Drop and Riser are removed from the user-facing instrument set. Octave down/up are combined in Harmony with separate -24..0 and 0..+24 semitone voice controls.
- Tape Distortion replaces the old 8-bit/crush slot. LFO Deathy and LFO Haunted Radio are audio filters, not generators.
- Delay, Reverb and Shimmer expose their advanced controls in the detail panel; every visible instrument can be enabled/disabled and have a maximum Mix set from the rack. Select an instrument name or pad node to edit its effect-specific parameters.

## Verification
- Source regression checks and standalone C++ DSP smoke tests passed in the current environment.
- Native JUCE/VST3 Windows compilation and Ableton Live verification still need to be performed on a Windows runner/host.
