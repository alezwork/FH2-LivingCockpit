/* vm.h -- "Living camera" and "Motion blur" On/Off in the game's HUD settings (Settings > HUD, after the camera).
   The game's UI is XAML bound to C++ view models; a view model type lists its properties once, when it is first used
   (FH2 1.0.0.64: 0xD453D0 adds the HUD options' descriptors with 0x155EE30). This adds two more, LivingCamera and
   MotionBlur, made like the game's CameraAngle descriptor (0xD57E60: base init 0x143EB80, the value type
   0x15C8B20(0), a getter thunk that boxes an int, the descriptor vtable), with our getters and setters: the values are
   [Camera] Enabled and MotionBlur in LivingCockpit.ini, and the setters switch at once (camera.h, blur.h). The rows go
   into Scenes\Anthem\A_HUDOptions.xaml (UI.zip, overlay.h). Everything is found by its code around the string
   "CameraAngle", not by fixed addresses. (DualSense.dll adds its switch the same way to another type, the controller
   options: the two do not meet.) */
#pragma once

typedef void *(__fastcall *BoxIntFn)(void **holder, const int *value);
typedef void(__fastcall *BaseInitFn)(void *desc);
typedef void *(__fastcall *ValueTypeFn)(int);
typedef void(__fastcall *AddPropFn)(void *type, void *pair);
typedef void(__fastcall *ClassInitFn)(void);
static BoxIntFn g_boxInt;
static BaseInitFn g_descInit;
static ValueTypeFn g_valueType;
static AddPropFn g_addProp;
static void *g_descVtable, *g_vmType;
static ClassInitFn g_classInitOrig;
#ifdef _MSC_VER
static __declspec(align(16)) BYTE g_desc[2][0x60];
#else
static BYTE g_desc[2][0x60] __attribute__((aligned(16)));
#endif
static void *g_pair[2][2];
static volatile LONG g_propAdded;
static int g_vmOk;

static void *vm_box(void **out, int v)
{
    void *holder = NULL;
    g_boxInt(&holder, &v);
    *out = holder;
    return out;
}

static void *__fastcall vm_get_camera(void **out, void *x, void *obj)
{
    (void)x, (void)obj;
    return vm_box(out, Cam.on ? 1 : 0);
}

static void __fastcall vm_set_camera(void *x, void *obj, BYTE *boxed)
{
    (void)x, (void)obj;
    int v = *(int *)(boxed + 0x28) != 0;
    if (v == Cam.on)
        return;
    camera_apply(v);
    WritePrivateProfileStringW(L"Camera", L"Enabled", v ? L"1" : L"0", g_modIni);
    fh2_say("living camera %s (the game's HUD settings)", v ? "on" : "off");
}

static void *__fastcall vm_get_blur(void **out, void *x, void *obj)
{
    (void)x, (void)obj;
    return vm_box(out, Cam.blur ? 1 : 0);
}

static void __fastcall vm_set_blur(void *x, void *obj, BYTE *boxed)
{
    (void)x, (void)obj;
    int v = *(int *)(boxed + 0x28) != 0;
    if (v == Cam.blur)
        return;
    blur_apply(v);
    WritePrivateProfileStringW(L"Camera", L"MotionBlur", v ? L"1" : L"0", g_modIni);
    fh2_say("motion blur %s (the game's HUD settings)", v ? "on" : "off");
}

/* a property of the HUD options: an int (0 Off, 1 On) by our getter and setter */
static void vm_add(int i, const char *name, void *get, void *set, void *valueType)
{
    BYTE *d = g_desc[i];
    g_descInit(d);
    *(void **)(d + 0x28) = get;
    *(void **)(d + 0x30) = set;
    *(void **)(d + 0x38) = NULL;
    *(const char **)(d + 0x40) = name;
    *(void **)(d + 0x48) = valueType;
    d[0x50] &= 0xF0;
    *(void **)d = g_descVtable;
    g_pair[i][0] = d, g_pair[i][1] = NULL;
    g_addProp(g_vmType, g_pair[i]);
}

static void __fastcall vm_class_init(void)
{
    g_classInitOrig();
    if (InterlockedExchange(&g_propAdded, 1))
        return;
    void *vt = g_valueType(0);
    vm_add(0, "LivingCamera", (void *)vm_get_camera, (void *)vm_set_camera, vt);
    vm_add(1, "MotionBlur", (void *)vm_get_blur, (void *)vm_set_blur, vt);
    fh2_say("settings: the switches are in the game's HUD settings");
}

static int vm_install(GameImage *g)
{
    static const BYTE kProlog[18] = {0x40, 0x53, 0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00,
                                     0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF};
    BYTE *str = game_string(g, "CameraAngle");
    /* the descriptor's init: lea rax,[name] with its known neighbours */
    BYTE *L = NULL;
    for (BYTE *p = g->text; str && p + 7 <= g->textEnd && !L; p++)
        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x05 && rel32(p, 3, 7) == str && p - 0x40 >= g->text &&
            p[-0x40] == 0x33 && p[-0x3F] == 0xC9 && p[-0x3E] == 0xE8 && !memcmp(p - 0x36, "\x48\x8D\x3D", 3) &&
            p[-0x2C] == 0xE8 && !memcmp(p - 0x27, "\x48\x8D\x05", 3) && !memcmp(p - 0x19, "\x48\x8D\x05", 3) &&
            !memcmp(p + 0x1C, "\x48\x8D\x05", 3))
            L = p;
    BYTE *descFn = L ? game_func(g, L) : NULL;
    if (!descFn) {
        fh2_say("settings: the game's settings code is not as expected -- no switches in its settings");
        return 0;
    }
    g_valueType = (ValueTypeFn)rel32(L - 0x3E, 1, 5);
    g_descInit = (BaseInitFn)rel32(L - 0x2C, 1, 5);
    BYTE *getter = rel32(L - 0x27, 3, 7);
    g_descVtable = rel32(L + 0x1C, 3, 7);
    /* the type's property list: the call of descFn followed by AddProperty(type, &pair) */
    BYTE *C = NULL;
    for (BYTE *p = g->text; p + 0x26 <= g->textEnd && !C; p++)
        if (p[0] == 0xE8 && rel32(p, 1, 5) == descFn && !memcmp(p + 5, "\x48\x89\x05", 3) &&
            !memcmp(p + 0x13, "\x48\x8D\x15", 3) && !memcmp(p + 0x1A, "\x48\x8D\x0D", 3) && p[0x21] == 0xE8)
            C = p;
    BYTE *classInit = C ? game_func(g, C) : NULL;
    /* the getter thunk's second call boxes the int */
    int calls = 0;
    for (BYTE *p = getter; p < getter + 0xA0 && calls < 2; p++)
        if (*p == 0xE8 && ++calls == 2)
            g_boxInt = (BoxIntFn)rel32(p, 1, 5);
    if (!classInit || !g_boxInt) {
        fh2_say("settings: the game's settings code is not as expected (property list %p, box %p) -- no switches",
                classInit, (void *)g_boxInt);
        return 0;
    }
    g_vmType = rel32(C + 0x1A, 3, 7);
    g_addProp = (AddPropFn)rel32(C + 0x21, 1, 5);
    g_classInitOrig = (ClassInitFn)game_detour(classInit, 18, kProlog, (void *)vm_class_init);
    if (!g_classInitOrig) {
        fh2_say("settings: the HUD settings' property list is not as expected -- no switches");
        return 0;
    }
    fh2_say("settings: the switches hooked (game+0x%lX, its list game+0x%lX)", (DWORD)(descFn - g->base),
            (DWORD)(classInit - g->base));
    return 1;
}

/* Scenes\Anthem\A_HUDOptions.xaml with the switches after the camera */
static char *vm_xaml(const char *text, size_t len, size_t *outLen)
{
    static const char kAnchor[] = "SelectedIndex=\"{Binding CameraAngle}\"";
    const char *a = strstr(text, kAnchor);
    if (!a) {
        fh2_say("settings: the HUD settings screen is not as expected -- no switches");
        return NULL;
    }
    const char *start = a, *end = strstr(a, "/>");
    while (start > text && *start != '<')
        start--;
    const char *ls = start;
    while (ls > text && ls[-1] != '\n')
        ls--;
    if (!end || strncmp(start, "<local:SpinnerControl", 21))
        return NULL;
    end += 2;
    static const char kRow[] = "\r\n%s<local:SpinnerControl Label=\"%s\" SelectedIndex=\"{Binding %s}\" "
                               "ItemsSource=\"{Binding OnOffOptions}\" Style=\"{StaticResource A_Style_SpinnerControl_Default}\"/>";
    char add[1400], indent[64];
    int il = (int)(start - ls) < 63 ? (int)(start - ls) : 63;
    memcpy(indent, ls, il);
    indent[il] = 0;
    int n = wsprintfA(add, kRow, indent,
                      g_langRu ? "\xD0\x96\xD0\x98\xD0\x92\xD0\x90\xD0\xAF \xD0\x9A\xD0\x90\xD0\x9C\xD0\x95\xD0\xA0\xD0\x90" : "LIVING CAMERA",
                      "LivingCamera");
    wsprintfA(add + n, kRow, indent,
              g_langRu ? "\xD0\xA0\xD0\x90\xD0\x97\xD0\x9C\xD0\xAB\xD0\xA2\xD0\x98\xD0\x95 \xD0\x92 "
                         "\xD0\x94\xD0\x92\xD0\x98\xD0\x96\xD0\x95\xD0\x9D\xD0\x98\xD0\x98"
                       : "MOTION BLUR",
              "MotionBlur");
    size_t al = strlen(add), pre = (size_t)(end - text);
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + al + 1);
    if (!out)
        return NULL;
    memcpy(out, text, pre);
    memcpy(out + pre, add, al);
    memcpy(out + pre + al, end, len - pre);
    *outLen = len + al;
    out[*outLen] = 0;
    return out;
}
