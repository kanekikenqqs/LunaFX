@echo off
REM Requires Visual Studio 2022 (C++ workload), CMake, Python 3, and Inno Setup 6.
REM JUCE 8.0.6 is fetched by CMake when JUCE_PATH is not supplied.
python tests\test_source_sanity.py || exit /b 1
cmake -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release --target LunaFX accentfx_dsp_smoke accentfx_xy_test --parallel || exit /b 1
build\Release\accentfx_dsp_smoke.exe || exit /b 1
build\Release\accentfx_xy_test.exe || exit /b 1
set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% set ISCC=iscc
if not exist dist mkdir dist
%ISCC% /O"dist" installer\LunaFX.iss || exit /b 1
if not exist dist\LunaFX-Setup.exe exit /b 1
echo.
echo Done: dist\LunaFX-Setup.exe
