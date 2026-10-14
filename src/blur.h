/* blur.h -- the game's motion blur on or off ("Motion blur" in the game's HUD settings, vm.h; [Camera] MotionBlur).
   A view's camera class gives the renderer its motion blur amount through one virtual method (the vtable's +0xE0:
   a getter, vmovss xmm0, [rip + x] or [rcx + 0x170]; ret -- the cockpit and the chase cameras 1.0, the hood and the
   bumper 0.6, a replay or a cutscene camera its own; FH2 1.0.0.64). It is replaced, in the vtables of the views,
   the mirror, the replay and the cutscene cameras (found by RTTI; not the photo mode's, where the shutter is chosen),
   by one giving the game's amount, or 0 when it is off: at once, nothing else touched. */
#pragma once

typedef float(__fastcall *BlurFn)(BYTE *cam);
static const char *const kBlurClass[] = {
    ".?AVCCamDriver@Camera@@",     ".?AVCCamCockpit@Camera@@",   ".?AVCCamHood@Camera@@",       ".?AVCCamBumperHigh@Camera@@",
    ".?AVCCamFollow@Camera@@",     ".?AVCCamFollowLow@Camera@@", ".?AVCCamFollowHigh@Camera@@", ".?AVCCamRearView@Camera@@",
    ".?AVCCamSide@Camera@@",       ".?AVCCamWheel@Camera@@",     ".?AVCCamAnimPlayer@Camera@@", ".?AVCTSCCamera@Camera@@",
    ".?AVCMultiCam@Camera@@",      ".?AVForzaMultiCam@Camera@@", ".?AVCinematicGameCamera@Cinematics@@"};
#define BLUR_CLASSES ((int)(sizeof kBlurClass / sizeof kBlurClass[0]))
static void **g_blurVt[BLUR_CLASSES];
static BlurFn g_blurGame[BLUR_CLASSES];

static float __fastcall blur_amount(BYTE *cam)
{
    void **vt = *(void ***)cam;
    for (int i = 0; i < BLUR_CLASSES; i++)
        if (g_blurVt[i] == vt)
            return Cam.blur ? g_blurGame[i](cam) : 0.0f;
    return Cam.blur ? 1.0f : 0.0f;
}

/* at load: the getters through blur_amount */
static int blur_hook(GameImage *g)
{
    int n = 0;
    for (int i = 0; i < BLUR_CLASSES; i++) {
        DWORD old;
        void **vt = game_vtable(g, kBlurClass[i]);
        BYTE *f = vt ? (BYTE *)vt[0xE0 / 8] : NULL;
        if (!f || f < g->text || f >= g->textEnd || f[0] != 0xC5 || f[1] != 0xFA || f[2] != 0x10 ||
            (f[3] != 0x05 && f[3] != 0x81) || f[8] != 0xC3 || !VirtualProtect(&vt[0xE0 / 8], 8, PAGE_READWRITE, &old))
            continue;
        g_blurGame[i] = (BlurFn)f;
        g_blurVt[i] = vt;
        vt[0xE0 / 8] = (void *)blur_amount;
        VirtualProtect(&vt[0xE0 / 8], 8, old, &old);
        n++;
    }
    fh2_say("motion blur: %s, switchable in %d camera kinds of %d", Cam.blur ? "on" : "off", n, BLUR_CLASSES);
    return n;
}

static void blur_apply(int on)
{
    InterlockedExchange(&Cam.blur, on != 0);
}
