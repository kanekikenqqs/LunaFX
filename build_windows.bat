@echo off
REM Requires: Visual Studio 2022 (C++ workload), CMake, Inno Setup 6 (iscc in PATH or default location)
cmake -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release -j || exit /b 1
set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% set ISCC=iscc
%ISCC% installer\AccentFX.iss || exit /b 1
echo.
echo Done: dist\AccentFX-Setup.exe
