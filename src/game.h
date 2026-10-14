/* game.h -- the game's image (mapped by the engine at its preferred base) and detours into its code. */
#pragma once

typedef struct {
    BYTE *base, *text, *textEnd, *rdata, *rdataEnd;
    RUNTIME_FUNCTION *pdata;
    DWORD npdata;
} GameImage;

static int game_image(GameImage *g)
{
    BYTE *b = (BYTE *)0x140000000ull;
    MEMORY_BASIC_INFORMATION m;
    ZeroMemory(g, sizeof *g);
    if (!VirtualQuery(b, &m, sizeof m) || m.State != MEM_COMMIT || ((IMAGE_DOS_HEADER *)b)->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    g->base = b;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        if (!memcmp(s->Name, ".text", 6))
            g->text = b + s->VirtualAddress, g->textEnd = g->text + s->Misc.VirtualSize;
        else if (!memcmp(s->Name, ".rdata", 7))
            g->rdata = b + s->VirtualAddress, g->rdataEnd = g->rdata + s->Misc.VirtualSize;
    }
    IMAGE_DATA_DIRECTORY d = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    g->pdata = (RUNTIME_FUNCTION *)(b + d.VirtualAddress);
    g->npdata = d.Size / sizeof(RUNTIME_FUNCTION);
    return g->text && g->rdata && g->npdata;
}

/* the function p is in (its .pdata start) */
static BYTE *game_func(GameImage *g, BYTE *p)
{
    DWORD rva = (DWORD)(p - g->base), lo = 0, hi = g->npdata;
    while (lo < hi) {
        DWORD mid = (lo + hi) / 2;
        if (g->pdata[mid].EndAddress <= rva)
            lo = mid + 1;
        else if (g->pdata[mid].BeginAddress > rva)
            hi = mid;
        else
            return g->base + g->pdata[mid].BeginAddress;
    }
    return NULL;
}

static BYTE *rel32(BYTE *insn, int dispAt, int len) { return insn + len + *(INT32 *)(insn + dispAt); }

/* a string in .rdata (the whole of it, starting after a 0) */
static BYTE *game_string(GameImage *g, const char *s)
{
    size_t n = strlen(s) + 1;
    for (BYTE *p = g->rdata + 1; p + n <= g->rdataEnd; p++)
        if (*p == (BYTE)s[0] && p[-1] == 0 && !memcmp(p, s, n))
            return p;
    return NULL;
}

/* the first instruction "lea reg, [rip + target]" (48/4C 8D modrm with rip, disp32) */
static BYTE *game_lea(GameImage *g, BYTE *target, BYTE modrm)
{
    for (BYTE *p = g->text; target && p + 7 <= g->textEnd; p++)
        if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8D && p[2] == modrm && rel32(p, 3, 7) == target)
            return p;
    return NULL;
}

/* fn's first n bytes (whole instructions, nothing rip-relative, n >= 14) moved to a trampoline that goes on at fn+n,
   and a jump to hook in their place, when they are what is expected. Returns the trampoline (the function as it
   was), or NULL. */
static void *game_detour(BYTE *fn, int n, const BYTE *expect, void *hook)
{
    DWORD old;
    if (!fn || n < 14 || memcmp(fn, expect, n))
        return NULL;
    BYTE *t = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return NULL;
    memcpy(t, fn, n);
    t[n] = 0xFF, t[n + 1] = 0x25, *(DWORD *)(t + n + 2) = 0;
    *(BYTE **)(t + n + 6) = fn + n;
    if (!VirtualProtect(fn, 14, PAGE_EXECUTE_READWRITE, &old))
        return NULL;
    fn[0] = 0xFF, fn[1] = 0x25, *(DWORD *)(fn + 2) = 0;
    *(void **)(fn + 6) = hook;
    VirtualProtect(fn, 14, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 14);
    return t;
}
