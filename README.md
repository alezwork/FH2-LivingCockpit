# FH2-LivingCockpit

**A living cockpit for Forza Horizon 2 (Xbox One) on the Azimuth 0.9.7 emulator.** In the cockpit view the driver's
head reacts to the car — it looks into turns, leans with the g-forces, dips under braking, looks where the car goes in
a slide, nods at gear changes, rides the bumps — and the speed is felt: the view opens up, the edges darken, the air
shakes the picture, the throttle pushes the view wide. Plus a switch for the game's motion blur. One DLL, both
switched in the game's own settings.

[Русская версия](README.ru.md)

---

## What it does
**The cockpit** (the other views stay the game's own)
| | |
|---|---|
| **Looks into turns** | The head turns towards the corner, smoothly (14° in a hard turn), with a dead zone for small steering. |
| **Leans** | The horizon tilts and the head is pushed to the outside of the turn. |
| **Slides** | In a slide the head looks where the car is going. |
| **Braking and throttle** | A deeper dip forward under braking and a push back under throttle. |
| **Gear changes** | A short nod with the jolt. |
| **Bumps and kerbs** | Every wheel's bumps felt harder. |
| **Field of view** | 65° instead of the game's 48°. |

**The speed** (in the cockpit, from 90 km/h, full at 260 km/h)
| | |
|---|---|
| **Wider view** | The field of view opens up by 12° at full speed, softly, breathing with the speed. |
| **Thrust** | The view opens 5° more under full throttle and closes 3° under hard braking; a gear change drops the pull for a moment, so the view kicks. |
| **Darker edges** | A vignette closing in at high speed. |
| **Wind** | A fine shake from the air at high speed. |

**In the game's settings** (*Settings > HUD*, right after the camera; both switch at once and are saved)
| | |
|---|---|
| **Living camera** | All of the above on or off. Off, the cockpit is exactly the game's. |
| **Motion blur** | The game's own motion blur in every view — the cockpit, the hood, the bumper, behind the car, the mirror, replays and cutscenes — on or off. The photo mode keeps its own shutter. |

## Requirements
- Forza Horizon 2 for Xbox One (1.0.0.64) running on **Azimuth 0.9.7**. Neither is included; use your own copy.
- The mod loader from the release (`azimuth-tls.dll`, the same as in [FH2-DualSense](https://github.com/alezwork/FH2-Dualsense)),
  or any loader that loads `mods\*.dll` once the game is mapped.

## Installation
1. Download `LivingCockpit.dll` (and `azimuth-tls.dll` if you have no mod loader yet) from [Releases](../../releases).
2. **The mod loader** (once): in `<Azimuth>\data\bin\` rename `azimuth-tls.dll` to `azimuth-tls.dll.original` and
   put the downloaded `azimuth-tls.dll` there.
3. Put `LivingCockpit.dll` in `<Azimuth>\mods\` (create the folder if there is none).
4. Start the game. `LivingCockpit.ini` is made in `mods\` on the first start; the log is `mods\LivingCockpit.log`.

Works together with [FH2-DualSense](https://github.com/alezwork/FH2-Dualsense): both change some of the same game
files at the start (the settings screens, the archive list) and each keeps the other's changes.

**Remove**: delete `LivingCockpit.dll` (and `LivingCockpit.ini`, `LivingCockpit.log`, `LivingCockpit.cache\`) from
`mods\`.

## Settings (`mods\LivingCockpit.ini`)
| `[Camera]` | |
|---|---|
| `Enabled` | 1 on, 0 off — the same as *Living camera* in the game. |
| `MotionBlur` | 1 the game's own motion blur, 0 none — the same as *Motion blur* in the game. |
| `FieldOfView` | The cockpit's field of view, degrees (the game's 48; 0 = the game's). |
| `LookIntoTurns` | Degrees in a hard turn. If the head looks out of the turns, make it negative. |
| `TurnDeadzone` | Percent of a hard turn below which the head stays straight. |
| `Lean` | The roll and the push to the outside, percent. |
| `Pitch` | How much deeper than the game's own the dip and push back are, percent (0 = the game's). |
| `Bumps` | The bumps and kerbs, percent of the game's own. |
| `Shake` | A light shake growing with speed, percent (0 = none). |
| `SlideLook` | Degrees the head turns towards where the car goes in a slide (negative for the other way). |
| `GearKick` | The nod at gear changes, percent (0 = none). |

| `[Speed]` | |
|---|---|
| `From`, `Full` | The speed the effects start from and the speed they are full at, km/h. |
| `FieldOfView` | The view wider at full speed, degrees (0 = none). |
| `Vignette` | Darker edges at full speed, percent (0 = none). |
| `WindShake` | The shake from the air, percent (0 = none). |
| `Thrust` | The view opening under throttle (5° at 100) and closing under braking (3°), percent (0 = none). |

The amounts are read at the game's start; the two switches work at once.

## How it works
- **The camera.** The game moves its cameras by data, `media\Camera.zip\CameraPhysics.xml`: for every camera a list
  of layers, each taking an input (g-forces, slips, crashes, the bumps under each wheel, speed...) through a curve,
  noise and a spring into the camera's position, rotation, field of view, vignette... The driver's camera barely
  reacts to turns: its sideways g-force layers are off. At the game's start LivingCockpit makes `Camera.zip` again
  with 27 more layers after the cockpit's own (which stay as they are), in both the file with effects and the one
  without, sets its field of view in `CameraSettings.ini`, and serves the new archive in place of the original (the
  engine's file calls are hooked; `zipmanifest.xml` gets the new archive directory). The vignette is the camera's own,
  the photo mode's.
- **The switch.** The game reads every camera's layer list through one function (FH2 1.0.0.64: `0x103AA00`, found
  by its `NumLayers`); it is hooked to keep where the cockpit's lists are, so *Living camera* turns our layers on and
  off at once. The switches are new properties of the HUD settings' view model, made like the game's own camera
  setting, and rows in `A_HUDOptions.xaml`.
- **The motion blur.** Each view's camera class gives the renderer its motion blur amount through one virtual method
  (the cockpit and the chase cameras 1.0, the hood 0.6). LivingCockpit finds the classes by their RTTI and puts in
  their vtables a method giving the game's amount, or 0 when *Motion blur* is off.

Everything is found by code signatures, RTTI and the files' contents; when something is not as expected, the log
says so and that part stays the game's own.

## Building
```
build.cmd            builds out\mods\LivingCockpit.dll
build.cmd install    also copies it into ..\Azimuth\mods and ..\Azimuth FF\mods
```
Visual Studio Build Tools (C++) or zig (`python -m pip install ziglang`); Python 3 for `tools\ini2h.py`.

| File | |
|---|---|
| `src\LivingCockpit.c` | the start, the files to make |
| `src\camera.h` | the cockpit's layers, the hook on the game's layer reading, *Living camera* |
| `src\blur.h` | *Motion blur* |
| `src\vm.h` | the switches in the game's HUD settings |
| `src\overlay.h` | the game's files changed without touching them |
| `src\game.h`, `src\fh2mod.h`, `src\inflate.h` | the game's image, detours and RTTI, the engine's imports, log, ini, deflate |

## Disclaimer
Not affiliated with or endorsed by Microsoft, Xbox Game Studios, Playground Games or Turn 10 Studios. Forza Horizon
and Xbox are trademarks of their respective owners. No game files are included.
