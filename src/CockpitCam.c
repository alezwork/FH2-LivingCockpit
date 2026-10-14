/* CockpitCam.dll -- the living cockpit camera for Forza Horizon 2 on Azimuth, one file in Azimuth\mods\.
   The driver's head reacts to the car: it looks into turns, leans and is pushed out by the sideways g-forces, dips
   under braking and sinks back under throttle, rides the bumps and kerbs, shakes a little at speed -- the game's own
   camera physics with more layers (camera.h). On and off in the game's HUD settings ("Living camera", vm.h) or in
   mods\CockpitCam.ini; the files it changes (Camera.zip, UI.zip, zipmanifest.xml) are made at the game's start into
   mods\CockpitCam.cache\ (overlay.h), together with what other mods (DualSense.dll) change in them. Log:
   mods\CockpitCam.log. */
#include "fh2mod.h"
#include <stdlib.h>
#include "ini_default.h"
#include "game.h"
#include "overlay.h"
#include "camera.h"
#include "vm.h"

static ULONGLONG g_t0;

/* at the game's first file open: Camera.zip with the camera's layers, UI.zip with the switch, the manifest */
static BOOL CALLBACK build(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    static const char *const kRel[2] = {"Camera.zip", "UI.zip"};
    DWORD dirs[2][3];
    int made[2] = {0, 0};
    (void)once, (void)param, (void)ctx;
    if (!ov_find_game()) {
        fh2_say("files: the game folder is not in data\\azimuth.ini -- the game's own camera");
        return TRUE;
    }
    CreateDirectoryW(g_cache, NULL);
    /* the camera with effects first: ours are made from its layers */
    static const char *const kCamFiles[2] = {"CameraPhysics.xml", "CameraPhysicsSansEffects.xml"};
    static const OvPatchFn kCamPatch[2] = {camera_patch, camera_patch_sans};
    static const char *const kUiFiles[1] = {"Scenes/Anthem/A_HUDOptions.xaml"};
    static const OvPatchFn kUiPatch[1] = {vm_xaml};
    made[0] = ov_zip(kRel[0], kCamFiles, kCamPatch, 2, dirs[0]);
    if (g_vmOk)
        made[1] = ov_zip(kRel[1], kUiFiles, kUiPatch, 1, dirs[1]);
    if ((made[0] || made[1]) && !ov_manifest(kRel, dirs, made, 2)) {
        fh2_say("files: zipmanifest.xml could not be changed -- the game's own camera");
        g_ovN = 0;
        return TRUE;
    }
    fh2_say("files: made in %.2f s (camera %s, the switch %s)", (GetTickCount64() - g_t0) / 1000.0,
            made[0] ? "yes" : "no", made[1] ? "yes" : "no");
    return TRUE;
}

static void install(void)
{
    Fh2Engine e;
    GameImage g;
    g_t0 = GetTickCount64();
    fh2_mod_open(L"CockpitCam");
    fh2_ini_default(kDefaultIni);
    camera_load();
    if (!fh2_engine(&e) || !game_image(&g)) {
        fh2_say("CockpitCam: the engine or the game is not where it should be -- off");
        return;
    }
    camera_hook(&g);
    g_vmOk = vm_install(&g);
    if (ov_install(&e, build))
        fh2_say("CockpitCam: the living camera %s; the files are made when the game starts reading", Cam.on ? "on" : "off");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        install();
    }
    return TRUE;
}
