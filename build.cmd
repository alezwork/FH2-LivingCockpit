@echo off
rem Builds CockpitCam.dll (the living cockpit camera for Forza Horizon 2 on Azimuth) into out\mods\ -- one file for
rem Azimuth\mods\. It writes its CockpitCam.ini with the defaults when there is none. Nothing else to install.
rem build.cmd install   also copies it into ..\Azimuth\mods\ and ..\Azimuth FF\mods\.
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
cd /d "%~dp0"
set B=%TEMP%\fh2cockpitcam_build
set OUT=%~dp0out\mods
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
python -I tools\ini2h.py CockpitCam.ini src\ini_default.h || exit /b 1

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if exist "%VCVARS%" call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul && goto msvc
python -m ziglang version >nul 2>nul && goto zig
echo no C compiler: python -m pip install ziglang (or Visual Studio Build Tools with C++)
exit /b 1

:msvc
set CL_FLAGS=/nologo /O1 /Gy /W3 /MT /GS- /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS
cl %CL_FLAGS% /LD src\CockpitCam.c /Fo"%B%\\" /link /OPT:REF /OPT:ICF /OUT:"%B%\CockpitCam.dll" /IMPLIB:"%B%\CockpitCam.lib" kernel32.lib user32.lib || exit /b 1
goto built

:zig
set ZCC=python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections
%ZCC% src\CockpitCam.c -o "%B%\CockpitCam.dll" -luser32 || exit /b 1

:built
copy /y "%B%\CockpitCam.dll" "%OUT%\" >nul || exit /b 1
for %%F in ("%OUT%\CockpitCam.dll") do echo built "%OUT%\CockpitCam.dll" (%%~zF bytes)
if /i not "%~1"=="install" exit /b 0

for %%E in ("%~dp0..\Azimuth" "%~dp0..\Azimuth FF") do (
  if exist "%%~E\mods" (
    copy /y "%OUT%\CockpitCam.dll" "%%~E\mods\" >nul || exit /b 1
    echo installed into "%%~E\mods"
  )
)
