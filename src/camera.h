/* camera.h -- the living cockpit camera.
   The game moves its cameras by "camera physics" (media\Camera.zip, Camera\CameraPhysics.xml): per camera, layers
   that take an input (g-forces, slips, crashes, the bumps under each wheel, speed, landings, Kinect head tracking...),
   map it through a curve (and noise, and a spring) and move or turn the camera by it (metres, degrees). The driver's
   camera (DriverCam, 64 layers) has its sideways g-force layers switched off, a light dip under braking and no look
   into turns.
   At the game's start CameraPhysics.xml gets 17 more DriverCam layers after the game's own (left as they are):
   looking into turns, the horizon tilting and the head pushed out by the sideways g-forces, a deeper dip under braking
   and push back under throttle (copies of the game's layers, scaled), harder bumps, a light shake with speed.
   The game reads the layers into a vector of 0x110-byte layers (Enabled at +0x40, Param x y z w at +0xE0) through one
   function for every camera's list (FH2 1.0.0.64: 0x103AA00, found by its "NumLayers"); it is hooked to keep where
   the DriverCam lists are, so the switch (the game's HUD settings, vm.h; [Camera] Enabled) turns our layers on and
   off at once. Positive g sideways is taken as a right turn and positive yaw as a look to the right. */
#pragma once

#define CAM_GAME 64      /* the driver camera's own layers */
#define CAM_LAYER 0x110  /* a layer in the game's memory */
#define CAM_OURS 17

static struct {
    volatile LONG on;
    float look;  /* degrees of yaw at 1 g sideways */
    float lean;  /* 0..2: the roll and the sway */
    float pitch; /* 0..2: added to the game's dip and push back (1 = twice the game's) */
    float bumps; /* 0..3: of the game's */
    float shake; /* 0..2 */
} Cam;

static void camera_load(void)
{
    Cam.on = fh2_ini_int(L"Camera", L"Enabled", 1) != 0;
    Cam.look = fh2_ini_float(L"Camera", L"LookIntoTurns", 10);
    Cam.lean = fh2_ini_int(L"Camera", L"Lean", 125) / 100.0f;
    Cam.pitch = fh2_ini_int(L"Camera", L"Pitch", 130) / 100.0f;
    Cam.bumps = fh2_ini_int(L"Camera", L"Bumps", 175) / 100.0f;
    Cam.shake = fh2_ini_int(L"Camera", L"Shake", 75) / 100.0f;
    Cam.lean = Cam.lean < 0 ? 0 : Cam.lean > 2 ? 2 : Cam.lean;
    Cam.pitch = Cam.pitch < 0 ? 0 : Cam.pitch > 2 ? 2 : Cam.pitch;
    Cam.bumps = Cam.bumps < 0 ? 0 : Cam.bumps > 3 ? 3 : Cam.bumps;
    Cam.shake = Cam.shake < 0 ? 0 : Cam.shake > 2 ? 2 : Cam.shake;
}

/* ---- the file: a layer's tag <Layer a="v" ... /> ---- */
static const char *cam_attr(const char *tag, size_t len, const char *name, size_t *vlen)
{
    char key[48];
    size_t kl = (size_t)wsprintfA(key, " %s=\"", name);
    for (size_t i = 0; i + kl <= len; i++)
        if (!memcmp(tag + i, key, kl)) {
            const char *v = tag + i + kl, *q = (const char *)memchr(v, '"', len - (size_t)(v - tag));
            if (!q)
                return NULL;
            *vlen = (size_t)(q - v);
            return v;
        }
    return NULL;
}

static int cam_is(const char *tag, size_t len, const char *input)
{
    size_t n;
    const char *v = cam_attr(tag, len, "InputType", &n);
    return v && n == strlen(input) && !memcmp(v, input, n);
}

/* a copy of a game's layer, its Param.x/y/z times f, on or off */
static char *cam_copy(const char *tag, size_t len, float f, int on)
{
    static const char *const kP[4] = {"Param.x", "Param.y", "Param.z", "Enabled"};
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + 96), *o = out;
    const char *p = tag;
    for (;;) {
        const char *best = NULL;
        size_t bl = 0;
        int which = 0;
        for (int i = 0; i < 4; i++) {
            size_t n;
            const char *v = cam_attr(p, len - (size_t)(p - tag), kP[i], &n);
            if (v && (!best || v < best))
                best = v, bl = n, which = i;
        }
        if (!best)
            break;
        memcpy(o, p, (size_t)(best - p));
        o += best - p;
        if (which == 3)
            *o++ = on ? '1' : '0';
        else
            o += _snprintf(o, 31, "%.6f", strtod(best, NULL) * f);
        p = best + bl;
    }
    memcpy(o, p, len - (size_t)(p - tag));
    o += len - (size_t)(p - tag);
    *o = 0;
    return out;
}

/* a new layer: input -> output through a three-point curve (mirrored: negative inputs give the negated output),
   times (x, y, z), on a spring (k, damping) or without (k 0), with noise when noiseFreq > 0 */
static char *cam_layer(int on, const char *input, const char *noise, const char *output, float cmin, float cmax,
                       int mirrored, const float curve[6], float x, float y, float z, float k, float d, float noiseFreq)
{
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, 1200);
    int n = _snprintf(out, 1199,
                      "<Layer Enabled=\"%d\" LocationType=\"All\" InputType=\"%s\" NoiseType=\"%s\" OutputType=\"%s\" "
                      "InputTakeDerivative=\"0\" InputClampMin=\"%.6f\" InputClampMax=\"%.6f\" InputMagAttackDecay=\"0\" ",
                      on, input, noise, output, cmin, cmax);
    if (noiseFreq > 0)
        n += _snprintf(out + n, 1199 - n,
                       "InputToNoiseFreqMapping=\"Linear\" NoiseFreq=\"%.6f\" NoiseSeed=\"%.6f\" NoiseOutputRangeMin=\"-1.000000\" "
                       "NoiseOutputRangeMax=\"1.000000\" ",
                       noiseFreq, noiseFreq * 1.37f);
    n += _snprintf(out + n, 1199 - n,
                   "InputToParamMapping=\"ThreePoint\" InputToParamCurveInput0=\"%.6f\" InputToParamCurveOutput0=\"%.6f\" "
                   "InputToParamCurveInput1=\"%.6f\" InputToParamCurveOutput1=\"%.6f\" InputToParamCurveInput2=\"%.6f\" "
                   "InputToParamCurveOutput2=\"%.6f\" InputToParamCurveMirrored=\"%d\" Param.x=\"%.6f\" Param.y=\"%.6f\" "
                   "Param.z=\"%.6f\" Param.w=\"0.000000\" ",
                   curve[0], curve[1], curve[2], curve[3], curve[4], curve[5], mirrored, x, y, z);
    if (k > 0)
        _snprintf(out + n, 1199 - n, "Sprung=\"1\" SpringK=\"%.6f\" SpringDPercent=\"%.6f\"/>", k, d);
    else
        _snprintf(out + n, 1199 - n, "Sprung=\"0\"/>");
    out[1199] = 0;
    return out;
}

/* CameraPhysics.xml with our layers after the DriverCam's own; NULL: as it is */
static char *camera_patch(const char *xml, size_t len, size_t *outLen)
{
    const char *sec = strstr(xml, "<DriverCam"), *secEnd = sec ? strstr(sec, "</DriverCam>") : NULL;
    const char *tag[CAM_GAME + 1];
    size_t tl[CAM_GAME + 1];
    int n = 0;
    if (!secEnd)
        return NULL;
    for (const char *p = sec; n <= CAM_GAME && (p = strstr(p, "<Layer ")) != NULL && p < secEnd; n++) {
        const char *e = strstr(p, "/>");
        if (!e || e > secEnd)
            return NULL;
        tag[n] = p, tl[n] = (size_t)(e + 2 - p);
        p = e + 2;
    }
    static const struct {
        int i;
        const char *input;
    } kExpect[] = {{0, "Gs_Longitudinal"}, {1, "Gs_Longitudinal"}, {2, "Gs_Longitudinal"}, {3, "Gs_Longitudinal"},
                   {33, "SurfaceBumpOffset_FL"}, {34, "SurfaceBumpOffset_FR"}, {35, "SurfaceBumpOffset_RR"},
                   {36, "SurfaceBumpOffset_RL"}, {47, "SpeedMPH"}};
    const char *num = strstr(sec, "NumLayers=\"64\"");
    if (n != CAM_GAME || !num || num > tag[0]) {
        fh2_say("camera: the driver's camera has %d layers, not 64 -- the game's own", n);
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof kExpect / sizeof kExpect[0]); i++)
        if (!cam_is(tag[kExpect[i].i], tl[kExpect[i].i], kExpect[i].input)) {
            fh2_say("camera: the driver's camera layer %d is not %s -- the game's own", kExpect[i].i, kExpect[i].input);
            return NULL;
        }
    char *ours[CAM_OURS];
    int on = Cam.on, k = 0;
    /* looking into turns: yaw by the sideways g, smooth (a soft spring), one layer per side as the game's own */
    static const float kLook[6] = {0.05f, 0, 0.5f, 0.45f, 1.2f, 1};
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CameraSpaceYPROffset", -1.5f, 0, 1, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CameraSpaceYPROffset", 0, 1.5f, 0, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    /* leaning: the horizon tilting with the body (roll), the head pushed to the outside (sway) */
    static const float kLean[6] = {0.1f, 0, 0.6f, 0.55f, 1.3f, 1};
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceYPROffset", -1.5f, 0, 1, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceYPROffset", 0, 1.5f, 0, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceXYZOffset", -1.5f, 0, 1, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceXYZOffset", 0, 1.5f, 0, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    /* braking and throttle: the game's dip and push back once more, scaled */
    for (int i = 0; i <= 3; i++)
        ours[k++] = cam_copy(tag[i], tl[i], Cam.pitch, on);
    /* the bumps under each wheel: added to the game's to make them Bumps times its own */
    for (int i = 33; i <= 36; i++)
        ours[k++] = cam_copy(tag[i], tl[i], Cam.bumps - 1, on);
    /* a light shake growing with speed (from 30 mph; the game's own, off, came only beyond 140) */
    static const float kShake[6] = {30, 0, 90, 0.45f, 200, 1};
    ours[k++] = cam_layer(on, "SpeedMPH", "Simplex", "CameraSpaceXYZOffset", 0, 250, 0, kShake, 0.0015f * Cam.shake,
                          0.0035f * Cam.shake, 0.0015f * Cam.shake, 0, 0, 18);
    ours[k++] = cam_layer(on, "SpeedMPH", "Simplex", "CameraSpaceYPROffset", 0, 250, 0, kShake, 0.12f * Cam.shake,
                          0.18f * Cam.shake, 0.10f * Cam.shake, 0, 0, 9);
    ours[k++] = cam_copy(tag[47], tl[47], Cam.shake, on);
    /* the new text: NumLayers 64 -> 81, ours after the last of the game's */
    size_t total = len + 64;
    for (int i = 0; i < CAM_OURS; i++)
        total += strlen(ours[i]) + 8;
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, total), *o = out;
    const char *at = tag[CAM_GAME - 1] + tl[CAM_GAME - 1];
    memcpy(o, xml, (size_t)(num - xml));
    o += num - xml;
    o += wsprintfA(o, "NumLayers=\"%d\"", CAM_GAME + CAM_OURS);
    const char *p = num + 14;
    memcpy(o, p, (size_t)(at - p));
    o += at - p;
    for (int i = 0; i < CAM_OURS; i++) {
        o += wsprintfA(o, "\r\n    ");
        size_t l = strlen(ours[i]);
        memcpy(o, ours[i], l);
        o += l;
        HeapFree(GetProcessHeap(), 0, ours[i]);
    }
    memcpy(o, at, len - (size_t)(at - xml));
    o += len - (size_t)(at - xml);
    *o = 0;
    *outLen = (size_t)(o - out);
    fh2_say("camera: the driver's camera gets %d layers -- looks %.0f degrees into turns, lean %d%%, pitch +%d%%, "
            "bumps %d%%, shake %d%%", CAM_OURS, Cam.look, (int)(Cam.lean * 100), (int)(Cam.pitch * 100),
            (int)(Cam.bumps * 100), (int)(Cam.shake * 100));
    return out;
}

/* ---- the game's memory: our layers on and off ---- */
typedef void *(__fastcall *LayerListFn)(BYTE *vec, void *visitor, const char *name, void *r9);
static LayerListFn g_layerList;
static BYTE *g_camVec[8];
static float g_camParam[8][CAM_OURS][4];
static int g_camVecs;
static SRWLOCK g_camLock = SRWLOCK_INIT;

static int cam_layers(BYTE *vec, BYTE **begin)
{
    BYTE *b = *(BYTE **)vec, *e = *(BYTE **)(vec + 8);
    *begin = b;
    return b && e > b ? (int)((e - b) / CAM_LAYER) : 0;
}

static void cam_set(int v, int on)
{
    static const float kZero[4] = {0, 0, 0, 0};
    BYTE *b;
    if (cam_layers(g_camVec[v], &b) != CAM_GAME + CAM_OURS)
        return;
    for (int i = 0; i < CAM_OURS; i++) {
        BYTE *L = b + (CAM_GAME + i) * CAM_LAYER;
        L[0x40] = (BYTE)(on != 0);
        memcpy(L + 0xE0, on ? g_camParam[v][i] : kZero, 16);
    }
}

static void *__fastcall hk_layer_list(BYTE *vec, void *visitor, const char *name, void *r9)
{
    void *r = g_layerList(vec, visitor, name, r9);
    BYTE *b;
    if (name && name[0] == 'D' && !strcmp(name, "DriverCam") && cam_layers(vec, &b) == CAM_GAME + CAM_OURS) {
        AcquireSRWLockExclusive(&g_camLock);
        int v = 0;
        while (v < g_camVecs && g_camVec[v] != vec)
            v++;
        if (v < 8) {
            if (v == g_camVecs)
                g_camVecs++;
            g_camVec[v] = vec;
            for (int i = 0; i < CAM_OURS; i++)
                memcpy(g_camParam[v][i], b + (CAM_GAME + i) * CAM_LAYER + 0xE0, 16);
            cam_set(v, Cam.on);
            fh2_say("camera: the driver's camera read (%d layers) -- the living camera %s", CAM_GAME + CAM_OURS,
                    Cam.on ? "on" : "off");
        }
        ReleaseSRWLockExclusive(&g_camLock);
    }
    return r;
}

/* the switch: ours on or off in every driver camera read */
static void camera_apply(int on)
{
    AcquireSRWLockExclusive(&g_camLock);
    InterlockedExchange(&Cam.on, on != 0);
    for (int v = 0; v < g_camVecs; v++)
        cam_set(v, on);
    ReleaseSRWLockExclusive(&g_camLock);
}

/* at load: the hook on the game's layer lists */
static int camera_hook(GameImage *g)
{
    static const BYTE kProlog[16] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x57, 0x41,
                                     0x56, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x02};
    BYTE *lea = game_lea(g, game_string(g, "NumLayers"), 0x05); /* lea r8, [rip + "NumLayers"] */
    BYTE *fn = lea ? game_func(g, lea) : NULL;
    g_layerList = (LayerListFn)game_detour(fn, 16, kProlog, (void *)hk_layer_list);
    if (!g_layerList) {
        fh2_say("camera: the game's layer reading is not as expected (%p) -- the switch needs a restart of the game",
                (void *)fn);
        return 0;
    }
    fh2_say("camera: the game's layer reading hooked (game+0x%lX)", (DWORD)(fn - g->base));
    return 1;
}
