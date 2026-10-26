# AccentFX 1.1.0 — changes and validation

## Included
- Momentary `KILL` control: clears the current audio block on a press and resets persistent DSP state (delay, reverb, shimmer, granular, pitch, and other effect buffers).
- Master Dry/Wet control with host parameter automation.
- More compact numeric readouts; trailing decimal zeroes are removed, frequencies at 1 kHz and above use kHz.
- Trance Gate UI/effect removed; granular processing replaces that slot in the glitch category.
- Granular cloud effect with grain size, density, pitch and jitter controls.
- Expanded stereo delay: sync/free time, width, saturation and modulation.
- Expanded reverb room choices: Room, Hall, Plate, Chamber, Cathedral, Spring, Void, with size, decay, damping and pre-delay.
- More aggressive radio filtering/drive; more analog-style asymmetric fuzz with tone control.
- New Riser source modes and sweep controls; selectable Dive Bomb curves.
- XY routing: four corner selectors, four optional cardinal edge slots and four optional centre slots with independent enable switches.
- Night, Day and Moonlight themes; smoother monochrome lunar XY surface and more restrained category colours.
- Additional factory presets and improved Windows Actions workflow with DSP test and build-artifact checks.

## Validation performed in this environment
- DSP unit/integration executable built with g++ and passed: `ALL TESTS PASSED (0 failures)`.
- Per-effect stress test included all 28 effects, including Granular, Riser, Delay, Reverb and Shimmer; no NaN/runaway failures were reported.
- Static consistency checks: 28 effect metadata entries match 28 DSP/UI effect names; no remaining Trance Gate UI/source references.

## Not verified here
- Full JUCE VST3/Standalone build and Inno Setup Windows installer were not compiled in this environment because JUCE's GitHub dependency could not be fetched (network DNS unavailable). Run the included GitHub Actions workflow or build on a Windows machine to validate the full plugin UI/toolchain. DSP tests do not replace that final host-level validation.
