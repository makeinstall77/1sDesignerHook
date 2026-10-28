#include "minwin.h"

// Configurator chrome is drawn by grphcs.dll through cairo.dll.
// WebKit (syntax assistant) also imports cairo, so only grphcs.dll's
// import table is patched. Original platform files are not modified.

typedef void(__cdecl* SetRgbFn)(void* cr, double r, double g, double b);
typedef void(__cdecl* SetRgbaFn)(void* cr, double r, double g, double b, double a);
typedef void(__cdecl* StopFn)(void* pattern, double offset, double r, double g, double b, double a);
typedef void(__cdecl* MeshFn)(void* pattern, unsigned int corner, double r, double g, double b, double a);

static SetRgbFn g_rgb = 0;
static SetRgbaFn g_rgba = 0;
static StopFn g_stop = 0;
static MeshFn g_mesh = 0;
static HMODULE g_self = 0;
static int g_patched = 0;
static int g_logged = 0;
static DWORD g_seen[64];
static int g_seenCount = 0;

static int Channel(double v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (int)(v * 255.0 + 0.5);
}

static int AbsInt(int v) {
    return v < 0 ? -v : v;
}

static int NearColor(double r, double g, double b, int R, int G, int B) {
    int d = AbsInt(Channel(r) - R) + AbsInt(Channel(g) - G) + AbsInt(Channel(b) - B);
    return d <= 12;
}

static void SetRgb8(double* r, double* g, double* b, int R, int G, int B) {
    *r = R / 255.0;
    *g = G / 255.0;
    *b = B / 255.0;
}

// Default 1C module colors seen in this build, retargeted to VS Code Dark+.
// Saturated colors that are not in this table (icons, selection) stay as they are.
static int MapSyntax(double* r, double* g, double* b) {
    if (NearColor(*r, *g, *b, 210, 76, 21)) {
        SetRgb8(r, g, b, 197, 134, 192); // keywords #C586C0
        return 1;
    }
    if (NearColor(*r, *g, *b, 212, 47, 51)) {
        SetRgb8(r, g, b, 206, 145, 120); // strings #CE9178
        return 1;
    }
    if (NearColor(*r, *g, *b, 137, 154, 51)) {
        SetRgb8(r, g, b, 106, 153, 85); // comments #6A9955
        return 1;
    }
    if (NearColor(*r, *g, *b, 37, 139, 211)) {
        SetRgb8(r, g, b, 156, 220, 254); // identifiers #9CDCFE
        return 1;
    }
    if (NearColor(*r, *g, *b, 213, 53, 132)) {
        SetRgb8(r, g, b, 86, 156, 214); // directives #569CD6
        return 1;
    }
    return 0;
}

static void LogLine(const char* text);

struct FormRange {
    BYTE* lo;
    BYTE* hi;
};

static FormRange g_form[8];
static int g_formN = 0;
static int g_formTick = 0;
static int g_formLogged = 0;

typedef unsigned short (WINAPI* CaptureFn)(DWORD, DWORD, void**, DWORD*);
static CaptureFn g_capture = 0;

static int EnsureCapture(void) {
    if (g_capture) return 1;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;
    g_capture = (CaptureFn)GetProcAddress(ntdll, "RtlCaptureStackBackTrace");
    return g_capture != 0;
}

static void RememberForm(const wchar_t* name) {
    if (g_formN >= 8) return;
    HMODULE mod = GetModuleHandleW(name);
    if (!mod) return;
    BYTE* base = (BYTE*)mod;
    LONG lfanew = *(LONG*)(base + 0x3C);
    if (lfanew <= 0 || lfanew > 0x1000) return;
    BYTE* opt = base + lfanew + 24;
    DWORD codeRva = *(DWORD*)(opt + 20);
    DWORD codeSize = *(DWORD*)(opt + 4);
    if (codeRva == 0 || codeSize < 0x1000) return;
    BYTE* lo = base + codeRva;
    for (int i = 0; i < g_formN; i++) {
        if (g_form[i].lo == lo) return;
    }
    g_form[g_formN].lo = lo;
    g_form[g_formN].hi = lo + codeSize;
    g_formN++;
}

static int IsFormPaint(void) {
    return 0;
    g_formTick++;
    if (g_formN == 0 || (g_formTick & 127) == 1) {
        RememberForm(L"mngui.dll");
        RememberForm(L"mngdsgn.dll");
        RememberForm(L"mngcore.dll");
        RememberForm(L"moxel.dll");
        RememberForm(L"moxelui.dll");
        RememberForm(L"ecsui.dll");
        RememberForm(L"basicui.dll");
    }
    if (g_formN == 0 || !EnsureCapture()) return 0;
    void* frames[48];
    unsigned short n = g_capture(0, 48, frames, 0);
    for (unsigned short i = 0; i < n; i++) {
        BYTE* a = (BYTE*)frames[i];
        for (int m = 0; m < g_formN; m++) {
            if (a >= g_form[m].lo && a < g_form[m].hi) {
                if (!g_formLogged) {
                    g_formLogged = 1;
                    LogLine("form paint left original\n");
                }
                return 1;
            }
        }
    }
    return 0;
}

static void MapColor(double* r, double* g, double* b) {
    double R = *r;
    double G = *g;
    double B = *b;
    if (R != R || G != G || B != B) return;
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
    if (R > 1) R = 1;
    if (G > 1) G = 1;
    if (B > 1) B = 1;

    if (MapSyntax(&R, &G, &B)) {
        *r = R;
        *g = G;
        *b = B;
        return;
    }

    // Module editor background in this build is a dark cyan, not paper white.
    if (NearColor(R, G, B, 0, 43, 54) || NearColor(R, G, B, 0, 42, 54)) {
        SetRgb8(r, g, b, 30, 30, 30);
        return;
    }
    // Form window caption. Keep it chrome, not text.
    if (NearColor(R, G, B, 150, 150, 150)) {
        SetRgb8(r, g, b, 37, 37, 38);
        return;
    }

    double max = R;
    if (G > max) max = G;
    if (B > max) max = B;
    double min = R;
    if (G < min) min = G;
    if (B < min) min = B;
    double lum = R * 0.2126 + G * 0.7152 + B * 0.0722;
    double sat = max - min;
    int warmPaper = (R > 0.72 && G > 0.68 && B > 0.55 && sat < 0.18 && R + 0.03 >= G && G + 0.03 >= B);
    if (sat >= 0.10 && !warmPaper) return;

    // Exact VS Code text grays must survive, otherwise operators typed in the
    // module settings get painted as a background.
    if (NearColor(R, G, B, 212, 212, 212) || NearColor(R, G, B, 204, 204, 204) || NearColor(R, G, B, 187, 187, 187)) return;
    if (NearColor(R, G, B, 30, 30, 30) || NearColor(R, G, B, 37, 37, 38) || NearColor(R, G, B, 51, 51, 51)) return;
    if (NearColor(R, G, B, 136, 136, 136)) return;

    // VS Code Dark+: white panels -> sidebar, gray chrome -> menu bar, the rest is text.
    if (warmPaper || lum >= 0.94) {
        SetRgb8(r, g, b, 37, 37, 38);
        return;
    }
    if (lum >= 0.78) {
        SetRgb8(r, g, b, 51, 51, 51);
        return;
    }
    SetRgb8(r, g, b, 212, 212, 212);
}

static void LogLine(const char* text) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    int slash = -1;
    for (DWORD i = 0; i < n; i++) {
        if (path[i] == L'\\' || path[i] == L'/') slash = (int)i;
    }
    if (slash < 0 || slash + 14 >= MAX_PATH) return;
    const wchar_t* name = L"cairo1c.log";
    int k = 0;
    while (name[k]) {
        path[slash + 1 + k] = name[k];
        k++;
    }
    path[slash + 1 + k] = 0;

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD len = 0;
    while (text[len]) len++;
    DWORD wrote = 0;
    WriteFile(file, text, len, &wrote, 0);
    CloseHandle(file);
}

static void LogColor(const char* kind, double r, double g, double b) {
    if (g_logged > 80) return;
    int R = (int)(r * 255.0 + 0.5);
    int G = (int)(g * 255.0 + 0.5);
    int B = (int)(b * 255.0 + 0.5);
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
    if (R > 255) R = 255;
    if (G > 255) G = 255;
    if (B > 255) B = 255;
    DWORD key = ((DWORD)R << 16) | ((DWORD)G << 8) | (DWORD)B;
    for (int i = 0; i < g_seenCount; i++) {
        if (g_seen[i] == key) return;
    }
    if (g_seenCount < 64) g_seen[g_seenCount++] = key;
    g_logged++;

    char buf[80];
    int p = 0;
    while (kind[p] && p < 12) {
        buf[p] = kind[p];
        p++;
    }
    buf[p++] = ' ';
    // three decimals, no CRT
    int vals[3] = {R, G, B};
    for (int vi = 0; vi < 3; vi++) {
        int v = vals[vi];
        char tmp[4];
        int tn = 0;
        if (v == 0) tmp[tn++] = '0';
        while (v > 0 && tn < 3) {
            tmp[tn++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (tn > 0) buf[p++] = tmp[--tn];
        if (vi < 2) buf[p++] = ',';
    }
    buf[p++] = '\n';
    buf[p] = 0;
    LogLine(buf);
}

static void __cdecl HookRgb(void* cr, double r, double g, double b) {
    LogColor("rgb", r, g, b);
    MapColor(&r, &g, &b);
    g_rgb(cr, r, g, b);
}

static void __cdecl HookRgba(void* cr, double r, double g, double b, double a) {
    LogColor("rgba", r, g, b);
    MapColor(&r, &g, &b);
    g_rgba(cr, r, g, b, a);
}

static void __cdecl HookStop(void* pattern, double offset, double r, double g, double b, double a) {
    LogColor("stop", r, g, b);
    MapColor(&r, &g, &b);
    g_stop(pattern, offset, r, g, b, a);
}

static void __cdecl HookMesh(void* pattern, unsigned int corner, double r, double g, double b, double a) {
    LogColor("mesh", r, g, b);
    MapColor(&r, &g, &b);
    g_mesh(pattern, corner, r, g, b, a);
}

static int NameEq(const char* a, const char* b) {
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        if (ca == 0) return 1;
    }
}

static int PatchImports(BYTE* base) {
    LONG lfanew = *(LONG*)(base + 0x3C);
    if (lfanew <= 0 || lfanew > 0x1000) return 0;
    BYTE* opt = base + lfanew + 24;
    WORD magic = *(WORD*)opt;
    if (magic != 0x20B) return 0;
    DWORD importRva = *(DWORD*)(opt + 120);
    DWORD sizeOfImage = *(DWORD*)(opt + 56);
    if (importRva == 0 || importRva >= sizeOfImage) return 0;

    int hits = 0;
    for (DWORD descOff = importRva; descOff + 20 < sizeOfImage; descOff += 20) {
        DWORD oft = *(DWORD*)(base + descOff);
        DWORD nameRva = *(DWORD*)(base + descOff + 12);
        DWORD ft = *(DWORD*)(base + descOff + 16);
        if (nameRva == 0) break;
        if (oft == 0 || ft == 0) continue;
        if (nameRva >= sizeOfImage || oft >= sizeOfImage || ft >= sizeOfImage) continue;
        if (!NameEq((const char*)(base + nameRva), "cairo.dll")) continue;

        ULONGLONG* names = (ULONGLONG*)(base + oft);
        ULONGLONG* slots = (ULONGLONG*)(base + ft);
        for (int i = 0; i < 4096; i++) {
            ULONGLONG entry = names[i];
            if (entry == 0) break;
            if (entry & 0x8000000000000000ULL) continue;
            DWORD byName = (DWORD)entry;
            if (byName + 2 >= sizeOfImage) continue;
            const char* fn = (const char*)(base + byName + 2);
            void* hook = 0;
            void** saved = 0;
            if (NameEq(fn, "cairo_set_source_rgb")) {
                hook = (void*)HookRgb;
                saved = (void**)&g_rgb;
            } else if (NameEq(fn, "cairo_set_source_rgba")) {
                hook = (void*)HookRgba;
                saved = (void**)&g_rgba;
            } else if (NameEq(fn, "cairo_pattern_add_color_stop_rgba")) {
                hook = (void*)HookStop;
                saved = (void**)&g_stop;
            } else if (NameEq(fn, "cairo_mesh_pattern_set_corner_color_rgba")) {
                hook = (void*)HookMesh;
                saved = (void**)&g_mesh;
            }
            if (!hook) continue;
            DWORD old = 0;
            if (!VirtualProtect(&slots[i], sizeof(ULONGLONG), PAGE_READWRITE, &old)) continue;
            if (*saved == 0) *saved = (void*)slots[i];
            slots[i] = (ULONGLONG)hook;
            DWORD ignored = 0;
            VirtualProtect(&slots[i], sizeof(ULONGLONG), old, &ignored);
            hits++;
        }
    }
    return hits;
}

typedef DWORD(WINAPI* SetColorFn)(void* hdc, DWORD color);
typedef DWORD(WINAPI* GetSysFn)(int index);
typedef void*(WINAPI* SysBrushFn)(int index);
typedef void*(WINAPI* SolidBrushFn)(DWORD color);

static SetColorFn g_setText = 0;
static SetColorFn g_setBk = 0;
static GetSysFn g_getSys = 0;
static SysBrushFn g_getSysBrush = 0;
static SolidBrushFn g_solid = 0;
static void* g_sysBrush[32];

static DWORD Pack(int r, int g, int b) {
    return (DWORD)(r | (g << 8) | (b << 16));
}

static DWORD MapSys(int index) {
    switch (index) {
    case 0:
    case 1:
    case 4:
    case 5:
    case 12:
        return Pack(30, 30, 30);
    case 15:
        return Pack(45, 45, 45);
    case 6:
    case 10:
    case 11:
    case 16:
        return Pack(62, 62, 66);
    case 20:
        return Pack(60, 60, 60);
    case 7:
    case 8:
    case 9:
    case 18:
    case 19:
        return Pack(212, 212, 212);
    case 17:
        return Pack(160, 160, 160);
    case 13:
        return Pack(9, 71, 113);
    case 14:
        return Pack(255, 255, 255);
    default:
        return 0xFFFFFFFF;
    }
}

static DWORD LightenText(DWORD color) {
    if (color == 0xFFFFFFFF) return color;
    int r = (int)(color & 255);
    int g = (int)((color >> 8) & 255);
    int b = (int)((color >> 16) & 255);
    int max = r;
    if (g > max) max = g;
    if (b > max) max = b;
    int min = r;
    if (g < min) min = g;
    if (b < min) min = b;
    if (max - min > 28) return color;
    if (max >= 230) return color;
    return Pack(212, 212, 212);
}

static DWORD WINAPI HookSetText(void* hdc, DWORD color) {
    return g_setText(hdc, LightenText(color));
}

static DWORD WINAPI HookSetBk(void* hdc, DWORD color) {
    if (color != 0xFFFFFFFF) {
        double r = (color & 255) / 255.0;
        double g = ((color >> 8) & 255) / 255.0;
        double b = ((color >> 16) & 255) / 255.0;
        MapColor(&r, &g, &b);
        color = Pack(Channel(r), Channel(g), Channel(b));
    }
    return g_setBk(hdc, color);
}

static DWORD WINAPI HookGetSys(int index) {
    DWORD mapped = MapSys(index);
    if (mapped != 0xFFFFFFFF) return mapped;
    return g_getSys(index);
}

static void* WINAPI HookGetSysBrush(int index) {
    DWORD mapped = MapSys(index);
    if (mapped == 0xFFFFFFFF || index < 0 || index >= 32 || !g_solid) return g_getSysBrush(index);
    if (!g_sysBrush[index]) g_sysBrush[index] = g_solid(mapped);
    return g_sysBrush[index];
}

static int PatchSlot(BYTE* base, DWORD sizeOfImage, ULONGLONG* names, ULONGLONG* slots, int i, void* hook, void** saved) {
    if ((ULONGLONG)hook == slots[i]) return 0;
    DWORD old = 0;
    if (!VirtualProtect(&slots[i], sizeof(ULONGLONG), PAGE_READWRITE, &old)) return 0;
    if (*saved == 0) *saved = (void*)slots[i];
    slots[i] = (ULONGLONG)hook;
    DWORD ignored = 0;
    VirtualProtect(&slots[i], sizeof(ULONGLONG), old, &ignored);
    (void)sizeOfImage;
    (void)base;
    (void)names;
    return 1;
}

static int PatchGdiImports(BYTE* base) {
    LONG lfanew = *(LONG*)(base + 0x3C);
    if (lfanew <= 0 || lfanew > 0x1000) return 0;
    BYTE* opt = base + lfanew + 24;
    if (*(WORD*)opt != 0x20B) return 0;
    DWORD importRva = *(DWORD*)(opt + 120);
    DWORD sizeOfImage = *(DWORD*)(opt + 56);
    if (importRva == 0 || importRva >= sizeOfImage) return 0;
    int hits = 0;
    for (DWORD descOff = importRva; descOff + 20 < sizeOfImage; descOff += 20) {
        DWORD oft = *(DWORD*)(base + descOff);
        DWORD nameRva = *(DWORD*)(base + descOff + 12);
        DWORD ft = *(DWORD*)(base + descOff + 16);
        if (nameRva == 0) break;
        if (oft == 0 || ft == 0) continue;
        if (nameRva >= sizeOfImage || oft >= sizeOfImage || ft >= sizeOfImage) continue;
        const char* dll = (const char*)(base + nameRva);
        int gdi = NameEq(dll, "gdi32.dll") || NameEq(dll, "gdi32full.dll");
        int user = NameEq(dll, "user32.dll");
        if (!gdi && !user) continue;
        ULONGLONG* names = (ULONGLONG*)(base + oft);
        ULONGLONG* slots = (ULONGLONG*)(base + ft);
        for (int i = 0; i < 4096; i++) {
            ULONGLONG entry = names[i];
            if (entry == 0) break;
            if (entry & 0x8000000000000000ULL) continue;
            DWORD byName = (DWORD)entry;
            if (byName + 2 >= sizeOfImage) continue;
            const char* fn = (const char*)(base + byName + 2);
            if (gdi && NameEq(fn, "SetTextColor") && g_setText != (SetColorFn)HookSetText) {
                hits += PatchSlot(base, sizeOfImage, names, slots, i, (void*)HookSetText, (void**)&g_setText);
            } else if (gdi && NameEq(fn, "SetBkColor") && g_setBk != (SetColorFn)HookSetBk) {
                hits += PatchSlot(base, sizeOfImage, names, slots, i, (void*)HookSetBk, (void**)&g_setBk);
            } else if (user && NameEq(fn, "GetSysColor") && g_getSys != HookGetSys) {
                hits += PatchSlot(base, sizeOfImage, names, slots, i, (void*)HookGetSys, (void**)&g_getSys);
            } else if (user && NameEq(fn, "GetSysColorBrush") && g_getSysBrush != HookGetSysBrush) {
                hits += PatchSlot(base, sizeOfImage, names, slots, i, (void*)HookGetSysBrush, (void**)&g_getSysBrush);
            }
        }
    }
    return hits;
}

static int SkipModule(const wchar_t* name, int chars) {
    const wchar_t* skip[] = {
        L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"gdi32.dll", L"gdi32full.dll",
        L"user32.dll", L"win32u.dll", L"cairo.dll", L"webkit.dll", L"cairo1c.dll", L"cairo1c2.dll",
        L"cairo1c3.dll", L"cairo1c4.dll", L"cairo1c5.dll", L"cairo1c6.dll", L"cairo1c7.dll", L"cairo1c8.dll"
    };
    for (int s = 0; s < 17; s++) {
        const wchar_t* b = skip[s];
        int i = 0;
        int same = 1;
        for (; i < chars; i++) {
            wchar_t a = name[i];
            wchar_t c = b[i];
            if (c == 0) { same = 0; break; }
            if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');
            if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
            if (a != c) { same = 0; break; }
        }
        if (same && b[i] == 0) return 1;
    }
    return 0;
}

extern "C" unsigned __int64 __readgsqword(unsigned long);
#pragma intrinsic(__readgsqword)

static int PatchLoadedGdi(void) {
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    BYTE* ldr = *(BYTE**)(peb + 0x18);
    BYTE* head = ldr + 0x10;
    BYTE* cur = *(BYTE**)head;
    int hits = 0;
    int guard = 0;
    while (cur != head && guard < 256) {
        guard++;
        BYTE* base = *(BYTE**)(cur + 0x30);
        unsigned short chars = *(unsigned short*)(cur + 0x58);
        wchar_t* name = *(wchar_t**)(cur + 0x60);
        chars = (unsigned short)(chars / 2);
        if (base && name && !SkipModule(name, chars)) hits += PatchGdiImports(base);
        cur = *(BYTE**)cur;
    }
    return hits;
}

static void PatchCoreColor(HMODULE mod, const char* name, DWORD color) {
    BYTE* slot = (BYTE*)GetProcAddress(mod, name);
    if (!slot) return;
    DWORD old = 0;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) return;
    *(DWORD*)slot = color;
    DWORD ignored = 0;
    VirtualProtect(slot, 4, old, &ignored);
}

static void PatchRva(HMODULE mod, DWORD rva, DWORD color) {
    BYTE* slot = (BYTE*)mod + rva;
    DWORD old = 0;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) return;
    *(DWORD*)slot = color;
    DWORD ignored = 0;
    VirtualProtect(slot, 4, old, &ignored);
}

static void PatchConfiguratorPalette(void) {
    HMODULE core = GetModuleHandleW(L"core83.dll");
    if (!core) return;
    DWORD ink = Pack(212, 212, 212);
    DWORD menu = Pack(51, 51, 51);
    DWORD side = Pack(37, 37, 38);
    DWORD textRvas[] = {
        0x3C2A3C, 0x3C2A40, 0x3C2A64, 0x3C2A70, 0x3C2A6C,
        0x3C2A0C, 0x3C2A10, 0x3C29F8, 0x3C29F4, 0x3C2968
    };
    for (int i = 0; i < 10; i++) PatchRva(core, textRvas[i], ink);
    PatchRva(core, 0x3C2AB0, menu);
    PatchRva(core, 0x3C2A38, menu);
    PatchRva(core, 0x3C2A60, side);
    PatchRva(core, 0x3C2A34, side);
    PatchRva(core, 0x3C2AB8, side);
    LogLine("core palette patched\n");
}

static DWORD WINAPI Worker(LPVOID) {
    Sleep(50);
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (gdi) g_solid = (SolidBrushFn)GetProcAddress(gdi, "CreateSolidBrush");
    for (int i = 0; i < 600 && !g_patched; i++) {
        HMODULE mod = GetModuleHandleW(L"grphcs.dll");
        if (mod) {
            int hits = PatchImports((BYTE*)mod);
            if (hits > 0) {
                g_patched = 1;
                LogLine("patched grphcs cairo\n");
                break;
            }
        }
        Sleep(100);
    }
    if (!g_patched) LogLine("grphcs cairo imports not found\n");
    PatchConfiguratorPalette();
    int gdiHits = 0;
    for (int i = 0; i < 80; i++) {
        gdiHits += PatchLoadedGdi();
        Sleep(250);
    }
    if (gdiHits > 0) LogLine("patched gdi text and system colors\n");
    return 0;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == 1) {
        g_self = (HMODULE)instance;
        DisableThreadLibraryCalls((HMODULE)instance);
        CreateThread(0, 0, Worker, 0, 0, 0);
    }
    return TRUE;
}
