/* LivingCockpit.dll -- the living cockpit for Forza Horizon 2 on Azimuth, one file in Azimuth\mods\.
   In the cockpit view the driver's head reacts to the car: it looks into turns, leans and is pushed out by the
   sideways g-forces, dips under braking and sinks back under throttle, looks where the car goes in a slide, nods at
   gear changes, rides the bumps and kerbs; and the speed is felt: a wider view, darker edges, a shake from the air,
   the view opening under throttle -- the game's own camera physics with more layers (camera.h). The game's motion
   blur on or off in every view (blur.h). Both switch in the game's HUD settings ("Living camera", "Motion blur",
   vm.h) or in mods\LivingCockpit.ini; the files it changes (Camera.zip, UI.zip, zipmanifest.xml) are made at the
   game's start into mods\LivingCockpit.cache\ (overlay.h), together with what other mods (DualSense.dll) change in
   them. Log: mods\LivingCockpit.log. */
#include "fh2mod.h"
#include <stdlib.h>
#include <tlhelp32.h>
#include "ini_default.h"
#include "game.h"
#define OV_CACHE L"LivingCockpit.cache\\"
#include "overlay.h"
#include "camera.h"
#include "blur.h"
#include "strtab.h"
#include "labels.h"
#include "vm.h"

static ULONGLONG g_t0;

/* at the game's first file open: every language's string table with the switches' names, Camera.zip with the
   cockpit's layers, UI.zip with the switches, the manifest */
static BOOL CALLBACK build(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    enum { NREL = 2 + LOC_LANGS };
    static char relBuf[LOC_LANGS][48];
    const char *rel[NREL];
    DWORD dirs[NREL][3];
    int made[NREL], tables = 0;
    (void)once, (void)param, (void)ctx;
    ZeroMemory(made, sizeof made);
    /* the mods this one replaces would put their layers in a second time */
    static const WCHAR *const kOld[2] = {L"CockpitCam.dll", L"SpeedFX.dll"};
    for (int i = 0; i < 2; i++)
        if (GetModuleHandleW(kOld[i]))
            fh2_say_w("LivingCockpit: take this out of mods (LivingCockpit has it all): ", kOld[i]);
    if (!ov_find_game()) {
        fh2_say("files: the game folder is not in data\\azimuth.ini -- the game's own camera");
        return TRUE;
    }
    CreateDirectoryW(g_cache, NULL);
    /* ExtraOptions (loaded before the mods: its own Fh2TweaksStatus) sets the cockpit's field of view with its slider */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    MODULEENTRY32W me;
    me.dwSize = sizeof me;
    for (BOOL more = snap != INVALID_HANDLE_VALUE && Module32FirstW(snap, &me); more && !g_extraOptions;
         more = Module32NextW(snap, &me))
        g_extraOptions = GetProcAddress(me.hModule, "Fh2TweaksStatus") != NULL;
    if (snap != INVALID_HANDLE_VALUE)
        CloseHandle(snap);
    if (g_extraOptions)
        fh2_say("LivingCockpit: ExtraOptions is there");
    /* the switches' names in every language first: the screen says them by key only when every table has them */
    static const char *const kStrFiles[1] = {"InGame.str"};
    static const OvPatchFn kStrPatch[1] = {vm_strtab};
    for (int i = 0; g_vmOk && i < LOC_LANGS; i++) {
        wsprintfA(relBuf[i], "stripped\\stringtables\\%s.zip", kLocLang[i]);
        rel[2 + i] = relBuf[i];
        g_locLang = i;
        made[2 + i] = ov_zip(rel[2 + i], kStrFiles, kStrPatch, 1, dirs[2 + i]);
        tables += made[2 + i];
    }
    g_locOk = g_vmOk && g_locSeen == LOC_LANGS && !g_locFail;
    if (g_vmOk)
        fh2_say("settings: the switches' names in %d of %d languages' string tables", g_locSeen - g_locFail, LOC_LANGS);
    /* the camera with effects first: ours are made from its layers */
    static const char *const kCamFiles[3] = {"CameraPhysics.xml", "CameraPhysicsSansEffects.xml", "CameraSettings.ini"};
    static const OvPatchFn kCamPatch[3] = {camera_patch, camera_patch_sans, camera_settings};
    static const char *const kUiFiles[1] = {"Scenes/Anthem/A_HUDOptions.xaml"};
    static const OvPatchFn kUiPatch[1] = {vm_xaml};
    rel[0] = "Camera.zip", rel[1] = "UI.zip";
    made[0] = ov_zip(rel[0], kCamFiles, kCamPatch, 3, dirs[0]);
    if (g_vmOk)
        made[1] = ov_zip(rel[1], kUiFiles, kUiPatch, 1, dirs[1]);
    if ((made[0] || made[1] || tables) && !ov_manifest(rel, dirs, made, g_vmOk ? NREL : 2)) {
        fh2_say("files: zipmanifest.xml could not be changed -- the game's own camera");
        g_ovN = 0;
        return TRUE;
    }
    fh2_say("files: made in %.2f s (camera %s, the switches %s, %d string tables)", (GetTickCount64() - g_t0) / 1000.0,
            made[0] ? "yes" : "no", made[1] ? "yes" : "no", tables);
    return TRUE;
}

static void install(void)
{
    Fh2Engine e;
    GameImage g;
    g_t0 = GetTickCount64();
    fh2_mod_open(L"LivingCockpit");
    fh2_ini_default(kDefaultIni);
    camera_load();
    if (!fh2_engine(&e) || !game_image(&g)) {
        fh2_say("LivingCockpit: the engine or the game is not where it should be -- off");
        return;
    }
    camera_hook(&g);
    blur_hook(&g);
    g_vmOk = vm_install(&g);
    if (ov_install(&e, build))
        fh2_say("LivingCockpit: the living camera %s; the files are made when the game starts reading",
                Cam.on ? "on" : "off");
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
