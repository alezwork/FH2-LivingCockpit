/* overlay.h -- the game's files changed without touching them. At the game's first file open the changed copies are
   made in mods\<the mod>.cache\ (OV_CACHE) and the engine's file calls (its CreateFileW, GetFileAttributesW,
   GetFileAttributesExW, FindFirstFileExW imports) are pointed there for them. The game's archives: zips with its own
   central directory extra field 0x1123 (where an entry's data is); a changed entry is stored; media\zipmanifest.xml
   lists every archive's directory (start, size, entries) and is changed with them.
   Other mods may change the same files (DualSense.dll: UI.zip; LivingCockpit.dll: Camera.zip, UI.zip; all of them
   zipmanifest.xml): this one reads the game's files
   through the hook it replaced, so whatever a mod hooked in before it made is the base and stays; a mod hooked in
   after it does the same with this one's. */
#pragma once
#include "inflate.h"

#ifndef OV_CACHE
#define OV_CACHE L"LivingCockpit.cache\\"
#endif

typedef HANDLE(WINAPI *CreateFileWFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI *GetFileAttributesWFn)(LPCWSTR);
typedef BOOL(WINAPI *GetFileAttributesExWFn)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef HANDLE(WINAPI *FindFirstFileExWFn)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
static CreateFileWFn g_CreateFileW;
static GetFileAttributesWFn g_GetFileAttributesW;
static GetFileAttributesExWFn g_GetFileAttributesExW;
static FindFirstFileExWFn g_FindFirstFileExW;

typedef struct {
    WCHAR suffix[96];      /* media\<the game's path> */
    WCHAR file[MAX_PATH];  /* ours */
} OvFile;
static OvFile g_ov[4];
static int g_ovN;
static WCHAR g_media[MAX_PATH], g_cache[MAX_PATH]; /* <game>\Mount\media\, mods\<the mod>.cache\ */
static int g_langRu;
static INIT_ONCE g_ovOnce = INIT_ONCE_STATIC_INIT;
static PINIT_ONCE_FN g_ovBuild;

/* ---- reading: our own files plainly, the game's through the hook below ours ---- */
static BYTE *ov_load(const WCHAR *path, DWORD *len, int game)
{
    CreateFileWFn open = game && g_CreateFileW ? g_CreateFileW : CreateFileW;
    HANDLE h = open(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *p = NULL;
    DWORD n = 0;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    *len = GetFileSize(h, NULL);
    if (*len != INVALID_FILE_SIZE && (p = (BYTE *)HeapAlloc(GetProcessHeap(), 0, *len + 2)) != NULL &&
        (!ReadFile(h, p, *len, &n, NULL) || n != *len)) {
        HeapFree(GetProcessHeap(), 0, p);
        p = NULL;
    }
    CloseHandle(h);
    if (p)
        p[*len] = p[*len + 1] = 0;
    return p;
}

static int ov_save(const WCHAR *path, const void *data, DWORD len)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    BOOL ok = WriteFile(h, data, len, &n, NULL) && n == len;
    CloseHandle(h);
    return ok;
}

/* the engine's data\azimuth.ini (UTF-16): game_folder (relative to the engine), language */
static int ov_find_game(void)
{
    WCHAR ini[MAX_PATH], dir[MAX_PATH], folder[MAX_PATH] = L"", lang[32] = L"";
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH), len = 0;
    while (n && dir[n - 1] != '\\' && dir[n - 1] != '/')
        n--;
    dir[n] = 0;
    lstrcpyW(ini, dir);
    lstrcatW(ini, L"data\\azimuth.ini");
    WCHAR *text = (WCHAR *)ov_load(ini, &len, 0);
    if (!text)
        return 0;
    text[len / 2] = 0;
    for (WCHAR *p = text; *p;) {
        WCHAR *e = p;
        while (*e && *e != '\n' && *e != '\r')
            e++;
        WCHAR save = *e;
        *e = 0;
        if (*p == 0xFEFF)
            p++;
        if (!folder[0] && CompareStringOrdinal(p, 12, L"game_folder=", 12, TRUE) == CSTR_EQUAL)
            lstrcpynW(folder, p + 12, MAX_PATH);
        if (!lang[0] && CompareStringOrdinal(p, 9, L"language=", 9, TRUE) == CSTR_EQUAL)
            lstrcpynW(lang, p + 9, 32);
        *e = save;
        p = *e ? e + 1 : e;
    }
    HeapFree(GetProcessHeap(), 0, text);
    if (!folder[0])
        return 0;
    if (folder[1] == ':' || (folder[0] == '\\' && folder[1] == '\\'))
        lstrcpyW(ini, folder);
    else {
        lstrcpyW(ini, dir);
        lstrcatW(ini, folder);
    }
    if (!GetFullPathNameW(ini, MAX_PATH, g_media, NULL))
        return 0;
    n = lstrlenW(g_media);
    if (n && g_media[n - 1] != '\\')
        lstrcatW(g_media, L"\\");
    lstrcatW(g_media, L"Mount\\media\\");
    g_langRu = lang[0] ? ((lang[0] | 32) == 'r' && (lang[1] | 32) == 'u') : PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    return 1;
}

/* ---- zips ---- */
static DWORD g_crcTab[256];
static DWORD ov_crc(const BYTE *p, size_t n)
{
    DWORD c = 0xFFFFFFFF;
    if (!g_crcTab[1])
        for (DWORD i = 0; i < 256; i++) {
            DWORD k = i;
            for (int j = 0; j < 8; j++)
                k = k & 1 ? 0xEDB88320 ^ (k >> 1) : k >> 1;
            g_crcTab[i] = k;
        }
    while (n--)
        c = g_crcTab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}

typedef struct {
    char name[160];
    WORD method;
    DWORD crc, csize, usize, dataOff;
} ZEnt;

static int zip_parse(const BYTE *z, DWORD len, ZEnt **out)
{
    if (len < 22 || *(const DWORD *)(z + len - 22) != 0x06054B50)
        return -1;
    WORD n = *(const WORD *)(z + len - 22 + 10);
    DWORD cd = *(const DWORD *)(z + len - 22 + 16);
    ZEnt *e = (ZEnt *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 1) * sizeof *e);
    if (!e)
        return -1;
    for (WORD k = 0; k < n; k++) {
        if (cd + 46 > len || *(const DWORD *)(z + cd) != 0x02014B50)
            goto bad;
        const BYTE *c = z + cd;
        WORD nl = *(const WORD *)(c + 28), xl = *(const WORD *)(c + 30), cl = *(const WORD *)(c + 32);
        DWORD lho = *(const DWORD *)(c + 42);
        e[k].method = *(const WORD *)(c + 10);
        e[k].crc = *(const DWORD *)(c + 16);
        e[k].csize = *(const DWORD *)(c + 20);
        e[k].usize = *(const DWORD *)(c + 24);
        memcpy(e[k].name, c + 46, nl < 159 ? nl : 159);
        if (lho + 30 > len)
            goto bad;
        e[k].dataOff = lho + 30 + *(const WORD *)(z + lho + 26) + *(const WORD *)(z + lho + 28);
        if (e[k].dataOff + e[k].csize > len)
            goto bad;
        cd += 46 + nl + xl + cl;
    }
    *out = e;
    return n;
bad:
    HeapFree(GetProcessHeap(), 0, e);
    return -1;
}

static BYTE *zip_read(const BYTE *z, const ZEnt *e)
{
    BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), 0, e->usize + 1);
    if (!b)
        return NULL;
    if (e->method == 0 && e->csize == e->usize)
        memcpy(b, z + e->dataOff, e->usize);
    else if (e->method != 8 || inflate_raw(z + e->dataOff, e->csize, b, e->usize) != (long long)e->usize) {
        HeapFree(GetProcessHeap(), 0, b);
        return NULL;
    }
    b[e->usize] = 0;
    return b;
}

typedef char *(*OvPatchFn)(const char *text, size_t len, size_t *outLen);

/* an entry's name as the game writes it (either slash), case aside */
static int ov_name_is(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a == '\\' ? '/' : *a, y = *b == '\\' ? '/' : *b;
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return 0;
        if (!x)
            return 1;
    }
}

/* media\<rel> again with entries changed by their patches, in the order given (its other entries as they are);
   dir: its new directory. Nothing changed: 0. */
static int ov_zip(const char *rel, const char *const *entries, const OvPatchFn *patches, int np, DWORD dir[3])
{
    WCHAR path[MAX_PATH], wrel[96], out[MAX_PATH];
    DWORD len = 0;
    int at[4], changed = 0;
    char *t[4];
    size_t tl[4];
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 96);
    lstrcpyW(path, g_media);
    lstrcatW(path, wrel);
    BYTE *z = ov_load(path, &len, 1);
    ZEnt *e = NULL;
    int n = z ? zip_parse(z, len, &e) : -1;
    if (n < 0) {
        fh2_say("files: %s: not readable -- left as it is", rel);
        return 0;
    }
    for (int j = 0; j < np && j < 4; j++) {
        at[j] = -1, t[j] = NULL, tl[j] = 0;
        for (int k = 0; k < n && at[j] < 0; k++)
            if (ov_name_is(e[k].name, entries[j]))
                at[j] = k;
        if (at[j] < 0) {
            fh2_say("files: %s: no %s in it", rel, entries[j]);
            continue;
        }
        BYTE *raw = zip_read(z, &e[at[j]]);
        t[j] = raw ? patches[j]((const char *)raw, e[at[j]].usize, &tl[j]) : NULL;
        if (raw)
            HeapFree(GetProcessHeap(), 0, raw);
        changed += t[j] != NULL;
    }
    if (!changed) {
        HeapFree(GetProcessHeap(), 0, z);
        HeapFree(GetProcessHeap(), 0, e);
        return 0;
    }
    /* the new zip: the entries as they were, the changed ones stored */
    size_t size = 22;
    for (int j = 0; j < np; j++)
        size += tl[j];
    for (int i = 0; i < n; i++)
        size += 30 + 46 + 8 + 2 * strlen(e[i].name) + e[i].csize;
    BYTE *o = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size), *p = o;
    DWORD *offs = (DWORD *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2 * sizeof(DWORD));
    DWORD crcs[4];
    for (int j = 0; j < np; j++)
        crcs[j] = t[j] ? ov_crc((BYTE *)t[j], tl[j]) : 0;
    for (int pass = 0; pass < 2; pass++) {
        DWORD cd = (DWORD)(p - o);
        for (int i = 0; i < n; i++) {
            WORD L = (WORD)strlen(e[i].name);
            int mine = -1;
            for (int j = 0; j < np; j++)
                if (at[j] == i && t[j])
                    mine = j;
            DWORD cs = mine >= 0 ? (DWORD)tl[mine] : e[i].csize, us = mine >= 0 ? (DWORD)tl[mine] : e[i].usize;
            DWORD crc = mine >= 0 ? crcs[mine] : e[i].crc;
            WORD method = mine >= 0 ? 0 : e[i].method;
            if (pass == 0) { /* the local headers and the data */
                offs[2 * i] = (DWORD)(p - o);
                *(DWORD *)p = 0x04034B50, *(WORD *)(p + 4) = 10, *(WORD *)(p + 8) = method;
                *(DWORD *)(p + 14) = crc, *(DWORD *)(p + 18) = cs, *(DWORD *)(p + 22) = us;
                *(WORD *)(p + 26) = L;
                memcpy(p + 30, e[i].name, L);
                p += 30 + L;
                offs[2 * i + 1] = (DWORD)(p - o);
                memcpy(p, mine >= 0 ? (const BYTE *)t[mine] : z + e[i].dataOff, cs);
                p += cs;
            } else { /* the central directory */
                *(DWORD *)p = 0x02014B50, *(WORD *)(p + 4) = 10, *(WORD *)(p + 6) = 10, *(WORD *)(p + 10) = method;
                *(DWORD *)(p + 16) = crc, *(DWORD *)(p + 20) = cs, *(DWORD *)(p + 24) = us;
                *(WORD *)(p + 28) = L, *(WORD *)(p + 30) = 8;
                *(DWORD *)(p + 42) = offs[2 * i];
                memcpy(p + 46, e[i].name, L);
                p += 46 + L;
                *(WORD *)p = 0x1123, *(WORD *)(p + 2) = 4, *(DWORD *)(p + 4) = offs[2 * i + 1]; /* the game's: the data */
                p += 8;
            }
        }
        if (pass == 1) {
            DWORD cdSize = (DWORD)(p - o) - cd;
            *(DWORD *)p = 0x06054B50, *(WORD *)(p + 8) = (WORD)n, *(WORD *)(p + 10) = (WORD)n;
            *(DWORD *)(p + 12) = cdSize, *(DWORD *)(p + 16) = cd;
            p += 22;
            dir[0] = cd, dir[1] = cdSize + 22, dir[2] = (DWORD)n;
        }
    }
    const WCHAR *base = wrel;
    for (const WCHAR *q = wrel; *q; q++)
        if (*q == '\\')
            base = q + 1;
    lstrcpyW(out, g_cache);
    lstrcatW(out, base);
    int ok = ov_save(out, o, (DWORD)(p - o));
    HeapFree(GetProcessHeap(), 0, o);
    HeapFree(GetProcessHeap(), 0, offs);
    for (int j = 0; j < np; j++)
        if (t[j])
            HeapFree(GetProcessHeap(), 0, t[j]);
    HeapFree(GetProcessHeap(), 0, z);
    HeapFree(GetProcessHeap(), 0, e);
    if (!ok || g_ovN >= 4)
        return 0;
    OvFile *f = &g_ov[g_ovN++];
    lstrcpyW(f->suffix, L"media\\");
    lstrcatW(f->suffix, wrel);
    lstrcpyW(f->file, out);
    return 1;
}

/* media\zipmanifest.xml with the new directories of the archives made */
static int ov_manifest(const char *const *rels, DWORD dirs[][3], const int *made, int n)
{
    WCHAR path[MAX_PATH], out[MAX_PATH];
    DWORD len = 0;
    lstrcpyW(path, g_media);
    lstrcatW(path, L"zipmanifest.xml");
    char *t = (char *)ov_load(path, &len, 1);
    if (!t)
        return 0;
    size_t tl = len;
    char *text = (char *)HeapAlloc(GetProcessHeap(), 0, len + 1024);
    memcpy(text, t, len + 1);
    HeapFree(GetProcessHeap(), 0, t);
    for (int a = 0; a < n; a++) {
        if (!made[a])
            continue;
        char key[200], low[96];
        lstrcpyA(low, rels[a]);
        CharLowerA(low);
        wsprintfA(key, "path=\"game:\\media\\%s\"", low);
        char *at = NULL;
        for (char *p = text; *p && !at; p++)
            if (!_strnicmp(p, key, strlen(key)))
                at = p;
        char *end = at ? strchr(at, '>') : NULL;
        char *ds = at ? strstr(at, "dirstart=\"") : NULL, *de = at ? strstr(at, "direntries=\"") : NULL;
        if (!end || !ds || ds > end || !de || de > end) {
            HeapFree(GetProcessHeap(), 0, text);
            return 0;
        }
        de = strchr(de + 12, '"');
        char line[600], tail[256];
        int keep = (int)(ds - at);
        lstrcpynA(tail, de + 1, (int)(end - de));
        memcpy(line, at, keep);
        wsprintfA(line + keep, "dirstart=\"%lu\" dirsize=\"%lu\" direntries=\"%lu\"%s", dirs[a][0], dirs[a][1], dirs[a][2], tail);
        size_t ll = strlen(line), old = (size_t)(end - at);
        memmove(at + ll, end, tl - (size_t)(end - text) + 1);
        memcpy(at, line, ll);
        tl = tl - old + ll;
    }
    lstrcpyW(out, g_cache);
    lstrcatW(out, L"zipmanifest.xml");
    int ok = ov_save(out, text, (DWORD)tl);
    HeapFree(GetProcessHeap(), 0, text);
    if (!ok || g_ovN >= 4)
        return 0;
    OvFile *f = &g_ov[g_ovN++];
    lstrcpyW(f->suffix, L"media\\zipmanifest.xml");
    lstrcpyW(f->file, out);
    return 1;
}

/* ---- serving them ---- */
static OvFile *ov_match(const WCHAR *path)
{
    int n;
    if (!path || !g_ovN)
        return NULL;
    n = lstrlenW(path);
    for (int i = 0; i < g_ovN; i++) {
        OvFile *f = &g_ov[i];
        int k = lstrlenW(f->suffix), j;
        if (n <= k || (path[n - k - 1] != '\\' && path[n - k - 1] != '/'))
            continue;
        for (j = 0; j < k; j++) {
            WCHAR a = path[n - k + j], b = f->suffix[j];
            if (a == '/')
                a = '\\';
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b)
                break;
        }
        if (j == k)
            return f;
    }
    return NULL;
}

static HANDLE WINAPI ov_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                    DWORD flags, HANDLE tmpl)
{
    InitOnceExecuteOnce(&g_ovOnce, g_ovBuild, NULL, NULL);
    OvFile *f = disp == OPEN_EXISTING && !(access & (GENERIC_WRITE | GENERIC_ALL)) ? ov_match(name) : NULL;
    return g_CreateFileW(f ? f->file : name, access, f ? share | FILE_SHARE_READ : share, sa, disp, flags, tmpl);
}

static DWORD WINAPI ov_GetFileAttributesW(LPCWSTR name)
{
    InitOnceExecuteOnce(&g_ovOnce, g_ovBuild, NULL, NULL);
    OvFile *f = ov_match(name);
    return g_GetFileAttributesW(f ? f->file : name);
}

static BOOL WINAPI ov_GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info)
{
    InitOnceExecuteOnce(&g_ovOnce, g_ovBuild, NULL, NULL);
    OvFile *f = ov_match(name);
    return g_GetFileAttributesExW(f ? f->file : name, level, info);
}

static HANDLE WINAPI ov_FindFirstFileExW(LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op,
                                         LPVOID filter, DWORD flags)
{
    InitOnceExecuteOnce(&g_ovOnce, g_ovBuild, NULL, NULL);
    OvFile *f = ov_match(name);
    return g_FindFirstFileExW(f ? f->file : name, level, data, op, filter, flags);
}

/* at load: the hooks; build runs at the game's first file open (not under the loader lock) */
static int ov_install(Fh2Engine *e, PINIT_ONCE_FN build)
{
    void *p;
    g_ovBuild = build;
    lstrcpyW(g_cache, g_modDir);
    lstrcatW(g_cache, OV_CACHE);
    g_GetFileAttributesExW = (GetFileAttributesExWFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFileAttributesExW");
    if ((p = fh2_hook_import(e, "KERNEL32.dll", "GetFileAttributesExW", (void *)ov_GetFileAttributesExW)) != NULL)
        g_GetFileAttributesExW = (GetFileAttributesExWFn)p;
    g_GetFileAttributesW = (GetFileAttributesWFn)fh2_hook_import(e, "KERNEL32.dll", "GetFileAttributesW", (void *)ov_GetFileAttributesW);
    g_FindFirstFileExW = (FindFirstFileExWFn)fh2_hook_import(e, "KERNEL32.dll", "FindFirstFileExW", (void *)ov_FindFirstFileExW);
    g_CreateFileW = (CreateFileWFn)fh2_hook_import(e, "KERNEL32.dll", "CreateFileW", (void *)ov_CreateFileW);
    if (!g_CreateFileW) {
        fh2_say("files: the engine's CreateFileW import not found -- the game's own files");
        return 0;
    }
    if (!g_GetFileAttributesW)
        g_GetFileAttributesW = GetFileAttributesW;
    if (!g_FindFirstFileExW)
        g_FindFirstFileExW = FindFirstFileExW;
    return 1;
}
