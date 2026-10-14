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
    float fov;   /* the cockpit's field of view, degrees (the game's 48); 0: the game's */
} Cam;

static void camera_load(void)
{
    Cam.on = fh2_ini_int(L"Camera", L"Enabled", 1) != 0;
    Cam.look = fh2_ini_float(L"Camera", L"LookIntoTurns", 14);
    Cam.lean = fh2_ini_int(L"Camera", L"Lean", 160) / 100.0f;
    Cam.pitch = fh2_ini_int(L"Camera", L"Pitch", 160) / 100.0f;
    Cam.bumps = fh2_ini_int(L"Camera", L"Bumps", 200) / 100.0f;
    Cam.shake = fh2_ini_int(L"Camera", L"Shake", 90) / 100.0f;
    Cam.fov = fh2_ini_float(L"Camera", L"FieldOfView", 60);
    Cam.fov = Cam.fov <= 0 ? 0 : Cam.fov < 30 ? 30 : Cam.fov > 90 ? 90 : Cam.fov;
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

/* the DriverCam's layers: their tags; n, or -1 when there is no DriverCam */
static int cam_section(const char *xml, const char **num, const char *tag[], size_t tl[], int max)
{
    const char *sec = strstr(xml, "<DriverCam"), *secEnd = sec ? strstr(sec, "</DriverCam>") : NULL;
    int n = 0;
    if (!secEnd)
        return -1;
    *num = strstr(sec, "NumLayers=\"");
    for (const char *p = sec; n < max && (p = strstr(p, "<Layer ")) != NULL && p < secEnd; n++) {
        const char *e = strstr(p, "/>");
        if (!e || e > secEnd)
            return -1;
        tag[n] = p, tl[n] = (size_t)(e + 2 - p);
        p = e + 2;
    }
    return *num && n && *num < tag[0] ? n : -1;
}

/* the text with ours after the n layers of its DriverCam, NumLayers n -> n + 17 */
static char *cam_insert(const char *xml, size_t len, const char *num, const char *last, size_t lastLen, int n,
                        char *ours[CAM_OURS], size_t *outLen)
{
    size_t total = len + 64;
    for (int i = 0; i < CAM_OURS; i++)
        total += strlen(ours[i]) + 8;
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, total), *o = out;
    const char *at = last + lastLen, *p = strchr(num + 11, '"') + 1;
    memcpy(o, xml, (size_t)(num - xml));
    o += num - xml;
    o += wsprintfA(o, "NumLayers=\"%d\"", n + CAM_OURS);
    memcpy(o, p, (size_t)(at - p));
    o += at - p;
    for (int i = 0; i < CAM_OURS; i++) {
        o += wsprintfA(o, "\r\n    ");
        size_t l = strlen(ours[i]);
        memcpy(o, ours[i], l);
        o += l;
    }
    memcpy(o, at, len - (size_t)(at - xml));
    o += len - (size_t)(at - xml);
    *o = 0;
    *outLen = (size_t)(o - out);
    return out;
}

/* the game's layers ours copy (from CameraPhysics.xml): the dip and push back 0..3, the bumps 33..36, the shake 47 */
static const int kCamSrc[9] = {0, 1, 2, 3, 33, 34, 35, 36, 47};
static char *g_camSrc[9];
static int g_camBase[4], g_camBases; /* the DriverCam sizes ours went after (64; 27 without effects) */

/* our 17 layers; bumps: the factor of the game's bump layers (added to them, or alone where they are not) */
static void cam_make(char *ours[CAM_OURS], float bumps)
{
    int on = Cam.on, k = 0;
    /* looking into turns: yaw by the sideways g, smooth (a soft spring), one layer per side as the game's own. The
       game's sideways g runs about 0..0.3 in turns (measured: its own units, not g), so full at 0.3 */
    static const float kLook[6] = {0.02f, 0, 0.12f, 0.55f, 0.3f, 1};
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CameraSpaceYPROffset", -1.0f, 0, 1, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CameraSpaceYPROffset", 0, 1.0f, 0, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    /* leaning: the horizon tilting with the body (roll), the head pushed to the outside (sway) */
    static const float kLean[6] = {0.03f, 0, 0.15f, 0.6f, 0.35f, 1};
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceYPROffset", -1.0f, 0, 1, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceYPROffset", 0, 1.0f, 0, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceXYZOffset", -1.0f, 0, 1, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    ours[k++] = cam_layer(on, "Gs_Lateral", "None", "CarSpaceXYZOffset", 0, 1.0f, 0, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    /* braking and throttle: the game's dip and push back once more, scaled */
    for (int i = 0; i < 4; i++)
        ours[k++] = cam_copy(g_camSrc[i], strlen(g_camSrc[i]), Cam.pitch, on);
    /* the bumps under each wheel */
    for (int i = 4; i < 8; i++)
        ours[k++] = cam_copy(g_camSrc[i], strlen(g_camSrc[i]), bumps, on);
    /* a light shake growing with speed (from 30 mph; the game's own, off, came only beyond 140) */
    static const float kShake[6] = {30, 0, 90, 0.45f, 200, 1};
    ours[k++] = cam_layer(on, "SpeedMPH", "Simplex", "CameraSpaceXYZOffset", 0, 250, 0, kShake, 0.0015f * Cam.shake,
                          0.0035f * Cam.shake, 0.0015f * Cam.shake, 0, 0, 18);
    ours[k++] = cam_layer(on, "SpeedMPH", "Simplex", "CameraSpaceYPROffset", 0, 250, 0, kShake, 0.12f * Cam.shake,
                          0.18f * Cam.shake, 0.10f * Cam.shake, 0, 0, 9);
    ours[k++] = cam_copy(g_camSrc[8], strlen(g_camSrc[8]), Cam.shake, on);
}

static char *cam_patch_any(const char *xml, size_t len, size_t *outLen, int full)
{
    const char *tag[CAM_GAME + 1], *num;
    size_t tl[CAM_GAME + 1];
    int n = cam_section(xml, &num, tag, tl, CAM_GAME + 1);
    char *ours[CAM_OURS];
    if (n < 1 || n > CAM_GAME || g_camBases >= 4)
        return NULL;
    cam_make(ours, full ? Cam.bumps - 1 : Cam.bumps);
    char *out = cam_insert(xml, len, num, tag[n - 1], tl[n - 1], n, ours, outLen);
    for (int i = 0; i < CAM_OURS; i++)
        HeapFree(GetProcessHeap(), 0, ours[i]);
    g_camBase[g_camBases++] = n;
    fh2_say("camera: %s: the driver's camera (%d layers) gets %d -- looks %.0f degrees into turns, lean %d%%, "
            "pitch +%d%%, bumps %d%%, shake %d%%", full ? "with effects" : "without effects", n, CAM_OURS, Cam.look,
            (int)(Cam.lean * 100), (int)(Cam.pitch * 100), (int)(Cam.bumps * 100), (int)(Cam.shake * 100));
    return out;
}

/* CameraPhysics.xml: ours after the DriverCam's own 64 (checked by their inputs); NULL: as it is */
static char *camera_patch(const char *xml, size_t len, size_t *outLen)
{
    const char *tag[CAM_GAME + 1], *num;
    size_t tl[CAM_GAME + 1];
    static const struct {
        int i;
        const char *input;
    } kExpect[] = {{0, "Gs_Longitudinal"}, {1, "Gs_Longitudinal"}, {2, "Gs_Longitudinal"}, {3, "Gs_Longitudinal"},
                   {33, "SurfaceBumpOffset_FL"}, {34, "SurfaceBumpOffset_FR"}, {35, "SurfaceBumpOffset_RR"},
                   {36, "SurfaceBumpOffset_RL"}, {47, "SpeedMPH"}};
    int n = cam_section(xml, &num, tag, tl, CAM_GAME + 1);
    if (n != CAM_GAME) {
        fh2_say("camera: the driver's camera has %d layers, not 64 -- the game's own", n);
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof kExpect / sizeof kExpect[0]); i++)
        if (!cam_is(tag[kExpect[i].i], tl[kExpect[i].i], kExpect[i].input)) {
            fh2_say("camera: the driver's camera layer %d is not %s -- the game's own", kExpect[i].i, kExpect[i].input);
            return NULL;
        }
    for (int i = 0; i < 9; i++) {
        size_t l = tl[kCamSrc[i]];
        g_camSrc[i] = (char *)HeapAlloc(GetProcessHeap(), 0, l + 1);
        memcpy(g_camSrc[i], tag[kCamSrc[i]], l);
        g_camSrc[i][l] = 0;
    }
    return cam_patch_any(xml, len, outLen, 1);
}

/* CameraPhysicsSansEffects.xml (the game's "camera effects" off -- the one it uses when that option is off): ours
   too, the bumps whole (it has none of its own) */
static char *camera_patch_sans(const char *xml, size_t len, size_t *outLen)
{
    if (!g_camSrc[0])
        return NULL;
    return cam_patch_any(xml, len, outLen, 0);
}

/* CameraSettings.ini: the cockpit's field of view (Driver\FOV, the game's 48) */
static char *camera_settings(const char *text, size_t len, size_t *outLen)
{
    static const char kKey[] = "Driver\\FOV ";
    const char *a = NULL;
    if (Cam.fov <= 0)
        return NULL;
    for (const char *p = text; (p = strstr(p, kKey)) != NULL; p++)
        if (p == text || p[-1] == '\n') {
            a = p + sizeof kKey - 1;
            break;
        }
    if (!a) {
        fh2_say("camera: no Driver\\FOV in CameraSettings.ini -- the game's field of view");
        return NULL;
    }
    const char *e = a;
    while (*e && *e != '\r' && *e != '\n')
        e++;
    char num[32];
    int nl = _snprintf(num, 31, "%.1f", Cam.fov);
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + 40);
    memcpy(out, text, (size_t)(a - text));
    memcpy(out + (a - text), num, nl);
    memcpy(out + (a - text) + nl, e, len - (size_t)(e - text));
    *outLen = len - (size_t)(e - a) + nl;
    out[*outLen] = 0;
    fh2_say("camera: the cockpit's field of view %.0f degrees (the game's %.*s)", Cam.fov, (int)(e - a), a);
    return out;
}

/* ---- the game's memory: our layers on and off ---- */
typedef void *(__fastcall *LayerListFn)(BYTE *vec, void *visitor, const char *name, void *r9);
static LayerListFn g_layerList;
static BYTE *g_camVec[8];
static int g_camVecBase[8]; /* where ours start in it */
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
    if (cam_layers(g_camVec[v], &b) != g_camVecBase[v] + CAM_OURS)
        return;
    for (int i = 0; i < CAM_OURS; i++) {
        BYTE *L = b + (g_camVecBase[v] + i) * CAM_LAYER;
        L[0x40] = (BYTE)(on != 0);
        memcpy(L + 0xE0, on ? g_camParam[v][i] : kZero, 16);
    }
}

/* the first two minutes after a driver camera is read: every 2 s, when they changed, the live state the game keeps in
   the first 0x40 bytes of our look-into-turns and roll layers, per camera set -- which set it drives, and how far */
static DWORD WINAPI cam_watch(void *arg)
{
    static float last[8][2][8];
    (void)arg;
    for (int t = 0; t < 60; t++) {
        Sleep(2000);
        AcquireSRWLockShared(&g_camLock);
        for (int v = 0; v < g_camVecs; v++) {
            BYTE *b;
            if (cam_layers(g_camVec[v], &b) != g_camVecBase[v] + CAM_OURS)
                continue;
            const float *look = (const float *)(b + g_camVecBase[v] * CAM_LAYER);
            const float *roll = (const float *)(b + (g_camVecBase[v] + 2) * CAM_LAYER);
            if (!memcmp(last[v][0], look, 32) && !memcmp(last[v][1], roll, 32))
                continue;
            memcpy(last[v][0], look, 32);
            memcpy(last[v][1], roll, 32);
            fh2_say("camera: set %d (%d + %d) live: look %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f | roll %.3f %.3f %.3f %.3f "
                    "%.3f %.3f %.3f %.3f", v, g_camVecBase[v], CAM_OURS, look[0], look[1], look[2], look[3], look[4],
                    look[5], look[6], look[7], roll[0], roll[1], roll[2], roll[3], roll[4], roll[5], roll[6], roll[7]);
        }
        ReleaseSRWLockShared(&g_camLock);
    }
    return 0;
}

static void *__fastcall hk_layer_list(BYTE *vec, void *visitor, const char *name, void *r9)
{
    void *r = g_layerList(vec, visitor, name, r9);
    BYTE *b;
    int n = name && name[0] == 'D' && !strcmp(name, "DriverCam") ? cam_layers(vec, &b) : 0, base = -1;
    static LONG watching;
    if (n)
        fh2_say("camera: the game reads a driver camera of %d layers", n);
    for (int i = 0; n && i < g_camBases; i++)
        if (n == g_camBase[i] + CAM_OURS)
            base = g_camBase[i];
    if (base >= 0) {
        AcquireSRWLockExclusive(&g_camLock);
        int v = 0;
        while (v < g_camVecs && g_camVec[v] != vec)
            v++;
        if (v < 8) {
            if (v == g_camVecs)
                g_camVecs++;
            g_camVec[v] = vec;
            g_camVecBase[v] = base;
            for (int i = 0; i < CAM_OURS; i++)
                memcpy(g_camParam[v][i], b + (base + i) * CAM_LAYER + 0xE0, 16);
            cam_set(v, Cam.on);
            fh2_say("camera: the driver's camera read (%d + %d layers) -- the living camera %s", base, CAM_OURS,
                    Cam.on ? "on" : "off");
        }
        ReleaseSRWLockExclusive(&g_camLock);
        if (!InterlockedExchange(&watching, 1)) {
            HANDLE h = CreateThread(NULL, 0, cam_watch, NULL, 0, NULL);
            if (h)
                CloseHandle(h);
        }
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
