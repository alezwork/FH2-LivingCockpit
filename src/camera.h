/* camera.h -- the living cockpit camera.
   The game moves its cameras by "camera physics" (media\Camera.zip: CameraPhysics.xml, and CameraPhysicsSansEffects.xml
   when its camera-effects option is off): per camera, layers that take an input (g-forces, slips, crashes, the bumps
   under each wheel, speed, landings, Kinect head tracking...), map it through a curve (and noise, and a spring) and
   move or turn the camera by it (metres, degrees). The cockpit's own (DriverCam) barely reacts to turns: its sideways
   g-force layers are off. At the game's start both files get more layers after its own (which stay as they are):
   looking into turns (with a dead zone), the horizon tilting and the head pushed out by the sideways g-forces, looking
   where the car goes in a slide, a deeper dip under braking and push back under throttle (copies of the game's layers,
   scaled), a nod at gear changes, harder bumps, a shake with speed; and its field of view
   (CameraSettings.ini, Driver\FOV).
   The game's own units, measured: sideways g about 0..0.3 in turns, longitudinal 0..0.06 accelerating, to -0.75
   braking. Positive g sideways is a right turn, positive yaw a look to the right, positive x to the right, +z forward.
   The game reads the layers into vectors of 0x110-byte layers (Enabled at +0x40, Param x y z w at +0xE0) through one
   function for every camera's list (FH2 1.0.0.64: 0x103AA00, found by its "NumLayers"); it is hooked to keep where
   the cockpit's lists are, so the switch (the game's HUD settings, vm.h; [Camera] Enabled) turns ours on and off at
   once. A layer each frame (0x103E3E0): the input (+0x00; a "derivative" is its change per 1/60 s), clamped, through
   the curve, times Param (degrees -> radians for the YPR outputs), into +0x10 (x y z w) directly, or on a spring
   (velocity +0x20; K, damping SpringDPercent * 2 sqrt K, so 1 is critical) -- an impulse adds to the velocity. */
#pragma once

#define CAM_LAYER 0x110 /* a layer in the game's memory */
#define CAM_MAXOURS 24
enum { CAM_COCKPIT, CAM_KINDS };
static const char *const kCamName[CAM_KINDS] = {"DriverCam"};

static struct {
    volatile LONG on;
    float look;     /* degrees of yaw in a hard turn */
    float dead;     /* 0..0.8: the dead zone of the look, of a hard turn */
    float lean;     /* 0..2: the roll and the sway */
    float pitch;    /* 0..2: added to the game's dip and push back (1 = twice the game's) */
    float bumps;    /* 0..3: of the game's */
    float shake;    /* 0..2 */
    float drift;    /* degrees of yaw towards where the car goes in a slide */
    float kick;     /* 0..2: the gear change kick */
    float fov;      /* the cockpit's field of view, degrees (the game's 48); 0: the game's */
} Cam;

static float cam_pct(const WCHAR *sec, const WCHAR *key, int def, float max)
{
    float v = fh2_ini_int(sec, key, def) / 100.0f;
    return v < 0 ? 0 : v > max ? max : v;
}

static void camera_load(void)
{
    Cam.on = fh2_ini_int(L"Camera", L"Enabled", 1) != 0;
    Cam.look = fh2_ini_float(L"Camera", L"LookIntoTurns", 14);
    Cam.dead = cam_pct(L"Camera", L"TurnDeadzone", 15, 0.8f);
    Cam.lean = cam_pct(L"Camera", L"Lean", 160, 2);
    Cam.pitch = cam_pct(L"Camera", L"Pitch", 160, 2);
    Cam.bumps = cam_pct(L"Camera", L"Bumps", 200, 3);
    Cam.shake = cam_pct(L"Camera", L"Shake", 90, 2);
    Cam.drift = fh2_ini_float(L"Camera", L"SlideLook", 10);
    Cam.kick = cam_pct(L"Camera", L"GearKick", 100, 2);
    Cam.fov = fh2_ini_float(L"Camera", L"FieldOfView", 65);
    Cam.fov = Cam.fov <= 0 ? 0 : Cam.fov < 30 ? 30 : Cam.fov > 90 ? 90 : Cam.fov;
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
static char *cam_copy(const char *tag, float f, int on)
{
    static const char *const kP[4] = {"Param.x", "Param.y", "Param.z", "Enabled"};
    size_t len = strlen(tag);
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
   times (x, y, z), on a spring (k, damping) or without (k 0), with noise when noiseFreq > 0; deriv: of the input */
static char *cam_layer(const char *input, int deriv, const char *noise, const char *output, float cmin, float cmax,
                       int mirrored, const float curve[6], float x, float y, float z, float k, float d, float noiseFreq)
{
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, 1200);
    int n = _snprintf(out, 1199,
                      "<Layer Enabled=\"%d\" LocationType=\"All\" InputType=\"%s\" NoiseType=\"%s\" OutputType=\"%s\" "
                      "InputTakeDerivative=\"%d\" InputClampMin=\"%.6f\" InputClampMax=\"%.6f\" InputMagAttackDecay=\"0\" ",
                      (int)Cam.on, input, noise, output, deriv, cmin, cmax);
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

/* a pair of layers, one per side (the game's own way): the left one mirrored */
static int cam_pair(char **ours, const char *input, const char *output, float clamp, const float curve[6], float x, float y,
                    float z, float k, float d)
{
    ours[0] = cam_layer(input, 0, "None", output, -clamp, 0, 1, curve, x, y, z, k, d, 0);
    ours[1] = cam_layer(input, 0, "None", output, 0, clamp, 0, curve, x, y, z, k, d, 0);
    return 2;
}

/* a kick: (x, y, z) per second added to the spring's velocity when the input passes cut, at most once in delay s */
static char *cam_impulse(const char *input, int deriv, const char *output, float clamp, const float curve[6], float x,
                         float y, float z, float k, float d, float cut, float delay)
{
    char *l = cam_layer(input, deriv, "None", output, -clamp, clamp, 1, curve, x, y, z, k, d, 0);
    char *at = strstr(l, " Sprung="), tail[160];
    lstrcpynA(tail, at, sizeof tail);
    _snprintf(at, 1199 - (size_t)(at - l), " ImpulseInputSizeCutoff=\"%.6f\" ImpulseDelay=\"%.6f\"%s", cut, delay, tail);
    l[1199] = 0;
    return l;
}

/* a probe for the log: the input as it is, a thousandth of it as millimetres (unseen) */
static char *cam_probe(const char *input, int deriv)
{
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, 400);
    _snprintf(out, 399,
              "<Layer Enabled=\"%d\" LocationType=\"All\" InputType=\"%s\" NoiseType=\"None\" OutputType=\"CameraSpaceXYZOffset\" "
              "InputTakeDerivative=\"%d\" InputClampMin=\"-50.000000\" InputClampMax=\"50.000000\" InputMagAttackDecay=\"0\" "
              "InputToParamMapping=\"Linear\" Param.x=\"0.001000\" Param.y=\"0.000000\" Param.z=\"0.000000\" "
              "Param.w=\"0.000000\" Sprung=\"0\"/>",
              (int)Cam.on, input, deriv);
    out[399] = 0;
    return out;
}

/* a curve from the dead zone (a share of full) to full, eased in */
static void cam_curve(float out[6], float full, float dead)
{
    float d = full * dead;
    out[0] = d, out[1] = 0;
    out[2] = d + (full - d) * 0.4f, out[3] = 0.55f;
    out[4] = full, out[5] = 1;
}

/* ---- the layers ours copy (from CameraPhysics.xml's DriverCam): the dip and push back 0..3, the bumps 33..36, the
   shake 47 ---- */
static const int kCamSrc[9] = {0, 1, 2, 3, 33, 34, 35, 36, 47};
static char *g_camSrc[9];

/* the cockpit's layers; bumps: the factor of the game's bump layers (added to them, or alone where they are not) */
static int cam_cockpit(char **ours, float bumps)
{
    int k = 0;
    float c[6];
    /* looking into turns: yaw by the sideways g, smooth (a soft spring) */
    cam_curve(c, 0.3f, Cam.dead);
    k += cam_pair(ours + k, "Gs_Lateral", "CameraSpaceYPROffset", 1, c, Cam.look, 0, 0, 25, 1.0f);
    /* leaning: the horizon tilting (roll), the head pushed to the outside (sway) */
    static const float kLean[6] = {0.03f, 0, 0.15f, 0.6f, 0.35f, 1};
    k += cam_pair(ours + k, "Gs_Lateral", "CarSpaceYPROffset", 1, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f);
    k += cam_pair(ours + k, "Gs_Lateral", "CarSpaceXYZOffset", 1, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f);
    /* braking and throttle: the game's dip and push back once more, scaled */
    for (int i = 0; i < 4; i++)
        ours[k++] = cam_copy(g_camSrc[i], Cam.pitch, Cam.on);
    /* the bumps under each wheel */
    for (int i = 4; i < 8; i++)
        ours[k++] = cam_copy(g_camSrc[i], bumps, Cam.on);
    /* a light shake growing with speed (from 30 mph; the game's own, off, came only beyond 140) */
    static const float kShake[6] = {30, 0, 90, 0.45f, 200, 1};
    ours[k++] = cam_layer("SpeedMPH", 0, "Simplex", "CameraSpaceXYZOffset", 0, 250, 0, kShake, 0.0015f * Cam.shake,
                          0.0035f * Cam.shake, 0.0015f * Cam.shake, 0, 0, 18);
    ours[k++] = cam_layer("SpeedMPH", 0, "Simplex", "CameraSpaceYPROffset", 0, 250, 0, kShake, 0.12f * Cam.shake,
                          0.18f * Cam.shake, 0.10f * Cam.shake, 0, 0, 9);
    ours[k++] = cam_copy(g_camSrc[8], Cam.shake, Cam.on);
    /* in a slide: looking where the car goes (the sideways slip; a slow spring) */
    static const float kSlide[6] = {0.4f, 0, 1.2f, 0.55f, 3.0f, 1};
    k += cam_pair(ours + k, "Slip_Lateral", "CameraSpaceYPROffset", 10, kSlide, Cam.drift, 0, 0, 12, 1.0f);
    /* a gear change: the head nods with the jolt (the longitudinal g changing fast, per 1/60 s), a bouncy spring; down
       (+pitch, as under braking) when the pull drops. Measured: a jolt 0.1..0.66, small ones 0.01..0.04 */
    static const float kKick[6] = {0.05f, 0, 0.12f, 0.6f, 0.3f, 1};
    ours[k++] = cam_impulse("Gs_Longitudinal", 1, "CarSpaceYPRImpulse", 1, kKick, 0, -26 * Cam.kick, 0, 120, 0.45f, 0.05f, 0.3f);
    /* the longitudinal g's change, for the log */
    ours[k++] = cam_probe("Gs_Longitudinal", 1);
    return k;
}

/* the camera's layers: their tags; n, or -1 when there is no such camera */
static int cam_section(const char *xml, const char *name, const char **num, const char *tag[], size_t tl[], int max)
{
    char open[32], close[32];
    wsprintfA(open, "<%s ", name);
    wsprintfA(close, "</%s>", name);
    const char *sec = strstr(xml, open), *secEnd = sec ? strstr(sec, close) : NULL;
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

/* the text with ours after the n layers of a camera, NumLayers n -> n + count; the old text freed */
static char *cam_insert(char *xml, const char *num, const char *last, size_t lastLen, int n, char **ours, int count)
{
    size_t len = strlen(xml), total = len + 64;
    for (int i = 0; i < count; i++)
        total += strlen(ours[i]) + 8;
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, total), *o = out;
    const char *at = last + lastLen, *p = strchr(num + 11, '"') + 1;
    memcpy(o, xml, (size_t)(num - xml));
    o += num - xml;
    o += wsprintfA(o, "NumLayers=\"%d\"", n + count);
    memcpy(o, p, (size_t)(at - p));
    o += at - p;
    for (int i = 0; i < count; i++) {
        o += wsprintfA(o, "\r\n    ");
        size_t l = strlen(ours[i]);
        memcpy(o, ours[i], l);
        o += l;
        HeapFree(GetProcessHeap(), 0, ours[i]);
    }
    memcpy(o, at, len - (size_t)(at - xml));
    o += len - (size_t)(at - xml);
    *o = 0;
    HeapFree(GetProcessHeap(), 0, xml);
    return out;
}

static int g_camBase[CAM_KINDS][4], g_camBases[CAM_KINDS]; /* each camera's sizes ours went after */
static int g_camOurs[CAM_KINDS];                           /* and how many ours */

/* one physics file: ours into the cockpit; full: the one with effects */
static char *cam_patch_any(const char *xml, size_t len, size_t *outLen, int full)
{
    char *text = (char *)HeapAlloc(GetProcessHeap(), 0, len + 1);
    int done = 0;
    memcpy(text, xml, len);
    text[len] = 0;
    for (int c = 0; c < CAM_KINDS; c++) {
        const char *tag[129], *num;
        size_t tl[129];
        char *ours[CAM_MAXOURS];
        int n = cam_section(text, kCamName[c], &num, tag, tl, 129), count;
        if (n < 1 || n > 128 || g_camBases[c] >= 4)
            continue;
        count = cam_cockpit(ours, full ? Cam.bumps - 1 : Cam.bumps);
        g_camOurs[c] = count;
        g_camBase[c][g_camBases[c]++] = n;
        text = cam_insert(text, num, tag[n - 1], tl[n - 1], n, ours, count);
        fh2_say("camera: %s: %s (%d layers) gets %d", full ? "with effects" : "without effects", kCamName[c], n, count);
        done++;
    }
    if (!done) {
        HeapFree(GetProcessHeap(), 0, text);
        return NULL;
    }
    *outLen = strlen(text);
    return text;
}

/* CameraPhysics.xml: the cockpit's own 64 checked by their inputs first (ours copy some); NULL: as it is */
static char *camera_patch(const char *xml, size_t len, size_t *outLen)
{
    const char *tag[65], *num;
    size_t tl[65];
    static const struct {
        int i;
        const char *input;
    } kExpect[] = {{0, "Gs_Longitudinal"}, {1, "Gs_Longitudinal"}, {2, "Gs_Longitudinal"}, {3, "Gs_Longitudinal"},
                   {33, "SurfaceBumpOffset_FL"}, {34, "SurfaceBumpOffset_FR"}, {35, "SurfaceBumpOffset_RR"},
                   {36, "SurfaceBumpOffset_RL"}, {47, "SpeedMPH"}};
    int n = cam_section(xml, "DriverCam", &num, tag, tl, 65);
    if (n != 64) {
        fh2_say("camera: the driver's camera has %d layers, not 64 -- the game's own cameras", n);
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof kExpect / sizeof kExpect[0]); i++)
        if (!cam_is(tag[kExpect[i].i], tl[kExpect[i].i], kExpect[i].input)) {
            fh2_say("camera: the driver's camera layer %d is not %s -- the game's own cameras", kExpect[i].i, kExpect[i].input);
            return NULL;
        }
    for (int i = 0; i < 9; i++) {
        size_t l = tl[kCamSrc[i]];
        g_camSrc[i] = (char *)HeapAlloc(GetProcessHeap(), 0, l + 1);
        memcpy(g_camSrc[i], tag[kCamSrc[i]], l);
        g_camSrc[i][l] = 0;
    }
    fh2_say("camera: cockpit -- looks %.0f degrees into turns (dead zone %d%%), lean %d%%, pitch +%d%%, bumps %d%%, "
            "shake %d%%, slide look %.0f, gear kick %d%%",
            Cam.look, (int)(Cam.dead * 100), (int)(Cam.lean * 100), (int)(Cam.pitch * 100), (int)(Cam.bumps * 100),
            (int)(Cam.shake * 100), Cam.drift, (int)(Cam.kick * 100));
    return cam_patch_any(xml, len, outLen, 1);
}

/* CameraPhysicsSansEffects.xml (the game's "camera effects" off): ours too, the bumps whole (it has none) */
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

/* ---- the game's memory: ours on and off ---- */
typedef void *(__fastcall *LayerListFn)(BYTE *vec, void *visitor, const char *name, void *r9);
static LayerListFn g_layerList;
typedef struct {
    BYTE *vec;
    int kind, base, count;
    float param[CAM_MAXOURS][4];
} CamVec;
static CamVec g_camVec[16];
static int g_camVecs;
static SRWLOCK g_camLock = SRWLOCK_INIT;

static int cam_layers(BYTE *vec, BYTE **begin)
{
    BYTE *b = *(BYTE **)vec, *e = *(BYTE **)(vec + 8);
    *begin = b;
    return b && e > b ? (int)((e - b) / CAM_LAYER) : 0;
}

/* ours at base in a list of n: the first looks into turns (Gs_Lateral -> CameraSpaceYPROffset), the last is the probe
   (Gs_Longitudinal, Linear -> CameraSpaceXYZOffset); other mods' layers may come after (SpeedFX.dll) */
static int cam_ours_at(BYTE *b, int n, int base, int count)
{
    BYTE *f = b + base * CAM_LAYER, *l = b + (base + count - 1) * CAM_LAYER;
    return count > 0 && base + count <= n && *(int *)(f + 0x48) == 1 && *(int *)(f + 0x50) == 4 && *(int *)(l + 0x48) == 2 &&
           *(int *)(l + 0x50) == 6 && *(int *)(l + 0xAC) == 1;
}

static void cam_set(CamVec *v, int on)
{
    static const float kZero[4] = {0, 0, 0, 0};
    BYTE *b;
    if (cam_layers(v->vec, &b) < v->base + v->count)
        return;
    for (int i = 0; i < v->count; i++) {
        BYTE *L = b + (v->base + i) * CAM_LAYER;
        L[0x40] = (BYTE)(on != 0);
        memcpy(L + 0xE0, on ? v->param[i] : kZero, 16);
    }
}

/* while driving, for the first 24 times 10 s a camera moved (in a quarter of an hour at most): the most some of our
   layers reached (their place in +0x10 x y z, times unit: radians -> degrees, metres, a probe's thousandths) -- to
   tune by */
static const struct {
    const char *what;
    signed char layer[2], comp;
    float unit;
} kCamWatch[4] = {
    {"look into turns", {0, 1}, 0, 57.29578f}, {"slide look", {17, 18}, 0, 57.29578f}, {"gear nod", {19, 19}, 1, 57.29578f},
    {"g change", {20, 20}, 0, 1000},
};

static DWORD WINAPI cam_watch(void *arg)
{
    (void)arg;
    for (int t = 0, said = 0; t < 90 && said < 24; t++) {
        float most[16][4], first[16][4][2];
        int moved[16];
        ZeroMemory(most, sizeof most);
        ZeroMemory(moved, sizeof moved);
        for (int s = 0; s < 50; s++) {
            Sleep(200);
            AcquireSRWLockShared(&g_camLock);
            for (int v = 0; v < g_camVecs; v++) {
                CamVec *c = &g_camVec[v];
                BYTE *b;
                if (cam_layers(c->vec, &b) < c->base + c->count)
                    continue;
                for (int w = 0; w < 4; w++)
                    for (int side = 0; side < 2; side++) {
                        int i = kCamWatch[w].layer[side];
                        if (i >= c->count)
                            continue;
                        const float *L = (const float *)(b + (c->base + i) * CAM_LAYER);
                        float x = L[4 + kCamWatch[w].comp];
                        if (!s)
                            first[v][w][side] = x;
                        else if (x != first[v][w][side])
                            moved[v] = 1;
                        x = x < 0 ? -x : x;
                        if (x > most[v][w] && x < 1e6f)
                            most[v][w] = x;
                    }
            }
            ReleaseSRWLockShared(&g_camLock);
        }
        for (int v = 0; v < g_camVecs; v++)
            if (moved[v]) {
                fh2_say("camera: %s, the most in 10 s: %s %.3f, %s %.3f, %s %.3f, %s %.3f", kCamName[g_camVec[v].kind],
                        kCamWatch[0].what, most[v][0] * kCamWatch[0].unit, kCamWatch[1].what, most[v][1] * kCamWatch[1].unit,
                        kCamWatch[2].what, most[v][2] * kCamWatch[2].unit, kCamWatch[3].what, most[v][3] * kCamWatch[3].unit);
                said++;
            }
    }
    return 0;
}

static void *__fastcall hk_layer_list(BYTE *vec, void *visitor, const char *name, void *r9)
{
    void *r = g_layerList(vec, visitor, name, r9);
    static LONG watching;
    int kind = -1, base = -1, n = 0;
    BYTE *b;
    for (int c = 0; name && c < CAM_KINDS && kind < 0; c++)
        if (name[0] == kCamName[c][0] && !strcmp(name, kCamName[c]))
            kind = c;
    if (kind >= 0) {
        n = cam_layers(vec, &b);
        for (int i = 0; i < g_camBases[kind]; i++)
            if (cam_ours_at(b, n, g_camBase[kind][i], g_camOurs[kind]))
                base = g_camBase[kind][i];
    }
    if (base >= 0) {
        AcquireSRWLockExclusive(&g_camLock);
        int v = 0;
        while (v < g_camVecs && g_camVec[v].vec != vec)
            v++;
        if (v < 16) {
            CamVec *c = &g_camVec[v];
            if (v == g_camVecs)
                g_camVecs++;
            c->vec = vec, c->kind = kind, c->base = base, c->count = g_camOurs[kind];
            for (int i = 0; i < c->count; i++)
                memcpy(c->param[i], b + (base + i) * CAM_LAYER + 0xE0, 16);
            cam_set(c, Cam.on);
            fh2_say("camera: the game read %s (%d + %d layers) -- the living camera %s", kCamName[kind], base, c->count,
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

/* the switch: ours on or off in every camera read */
static void camera_apply(int on)
{
    AcquireSRWLockExclusive(&g_camLock);
    InterlockedExchange(&Cam.on, on != 0);
    for (int v = 0; v < g_camVecs; v++)
        cam_set(&g_camVec[v], on);
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
