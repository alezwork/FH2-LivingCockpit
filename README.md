# FH2-CockpitCam

**A living cockpit camera for Forza Horizon 2 (Xbox One) on the Azimuth 0.9.7 emulator.** The driver's head reacts
to the car: it looks into turns, leans with the g-forces, dips under braking, looks where the car goes in a slide,
nods at gear changes, rides the bumps. One DLL, switched on and off in the game's own settings.

[Русская версия](README.ru.md)

---

## What it does
| | |
|---|---|
| **Looks into turns** | The head turns towards the corner, smoothly (14° in a hard turn), with a dead zone for small steering. |
| **Leans** | The horizon tilts and the head is pushed to the outside of the turn. |
| **Slides** | In a slide the head looks where the car is going. |
| **Braking and throttle** | A deeper dip forward under braking and a push back under throttle. |
| **Gear changes** | A short nod with the jolt. |
| **Bumps and kerbs** | Every wheel's bumps felt harder. |
| **Speed** | A light shake growing with speed. |
| **Field of view** | 65° instead of the game's 48°. |

**On and off in the game**: *Settings > HUD > Living camera* (right after the camera). It switches at once and is
saved to `CockpitCam.ini`.

## Requirements
- Forza Horizon 2 for Xbox One (1.0.0.64) running on **Azimuth 0.9.7** (not included; use your own copy).
- A mod loader that loads `Azimuth\mods\*.dll`: `azimuth-tls.dll` from the
  [FH2-DualSense release](https://github.com/alezwork/FH2-Dualsense/releases), or any other.

## Installation
1. Download `CockpitCam.dll` from [Releases](../../releases) and put it in `<Azimuth>\mods\`.
2. Start the game. `CockpitCam.ini` is made in `mods\` on the first start; the log is `mods\CockpitCam.log`.

Works together with [FH2-DualSense](https://github.com/alezwork/FH2-Dualsense) and SpeedFX (the feel of speed in
every view: a wider view, motion blur, darker edges, a shake): they change some of the same game files at the start
(the settings screens, the camera file, the archive list) and each keeps the others' changes.

## Settings (`mods\CockpitCam.ini`)
| `[Camera]` | |
|---|---|
| `Enabled` | 1 on, 0 off (the same as the switch in the game). |
| `FieldOfView` | Degrees; more = a wider, farther view (the game's 48; 0 = the game's). |
| `LookIntoTurns` | Degrees in a hard turn. If the head looks out of the turns, make it negative. |
| `TurnDeadzone` | Percent of a hard turn below which the head stays straight. |
| `Lean` | The roll and the push to the outside, percent. |
| `Pitch` | How much deeper than the game's own the dip and push back are, percent (0 = the game's). |
| `Bumps` | The bumps and kerbs, percent of the game's own. |
| `Shake` | The shake with speed, percent (0 = none). |
| `SlideLook` | Degrees the head turns towards where the car goes in a slide (negative for the other way). |
| `GearKick` | The nod at gear changes, percent. |

The amounts are read at the game's start; the on/off switch works at once.

## How it works
The game moves its cameras by data, `media\Camera.zip\CameraPhysics.xml`: for every camera a list of layers, each
taking an input (g-forces, slips, crashes, the bumps under each wheel, speed, landings, head tracking ...) through a
curve, noise and a spring into a camera offset or rotation. The driver's camera barely reacts to turns: its sideways g-force
layers are off. At the game's start CockpitCam makes `Camera.zip` again with 21 more layers after its own (which stay
as they are), in both the file with effects and the one without, and sets its field of view in `CameraSettings.ini`; the new archive is served to the game in place of the original (the engine's file calls are hooked;
`zipmanifest.xml` gets the new archive directory).

The game reads every camera's list through one function (FH2 1.0.0.64: `0x103AA00`, found by its `NumLayers`);
it is hooked to keep where the driver camera's lists are in memory, so the switch turns ours on and off at once (`Enabled` at +0x40, `Param` at +0xE0 of each 0x110-byte layer). The switch itself is a new property of the
HUD settings' view model, made like the game's own camera setting, and a row in `A_HUDOptions.xaml`. Everything is
found by code signatures; when something is not as expected, the log says so and the game's camera stays.

## Building
```
build.cmd            builds out\mods\CockpitCam.dll
build.cmd install    also copies it into ..\Azimuth\mods and ..\Azimuth FF\mods
```
Visual Studio Build Tools (C++) or zig (`python -m pip install ziglang`); Python 3 for `tools\ini2h.py`.

| File | |
|---|---|
| `src\CockpitCam.c` | the start, the files to make |
| `src\camera.h` | the layers, the hook on the game's layer reading, the switch |
| `src\vm.h` | the switch in the game's HUD settings |
| `src\overlay.h` | the game's files changed without touching them |
| `src\game.h`, `src\fh2mod.h`, `src\inflate.h` | the game's image and detours, the engine's imports, log, ini, deflate |

## Disclaimer
Not affiliated with or endorsed by Microsoft, Xbox Game Studios, Playground Games or Turn 10 Studios. Forza Horizon
and Xbox are trademarks of their respective owners. No game files are included.
