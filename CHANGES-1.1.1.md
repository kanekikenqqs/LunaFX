# Luna FX 1.1.1 — crash and usability fixes

- Fixed a confirmed null parameter dereference in the editor's setup controls.
- Fixed out-of-bounds access to missing ComboBox and XY routing controls during editor resize.
- Connected all 16 XY rim-node choice/enable controls and exposed the setup page in the navigation.
- Added a defensive parameter-binding fallback for future UI/parameter mismatches.
- Added state restore validation, program-index clamping, KILL reset on load, and sample-rate validation.
- Added functional user preset save/load using `.lfxpreset` files in the user's application data directory.
- Removed inert preset folder tabs and corrected folder/button layout.
- Updated the centre XY visual to restrained monochrome; DSP engine algorithms and coefficients remain unchanged in this patch.
- Corrected Windows build script installer path and fixed the installer AppId GUID.
