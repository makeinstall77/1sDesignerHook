#include "minwin.h"

// Starts a new configurator and loads cairo1c8.dll into that process only.
// Does not attach to an already running 1cv8 and does not inject ENTERPRISE.

static wchar_t g_exeDir[MAX_PATH];
static wchar_t g_binDir[MAX_PATH];
static wchar_t g_dllPath[MAX_PATH];
static wchar_t g_cmd[2048];

static int WLen(const wchar_t* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void WCopy(wchar_t* dst, int cap, const wchar_t* src) {
    int i = 0;
    if (cap <= 0) return;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void WCat(wchar_t* dst, int cap, const wchar_t* src) {
    int n = WLen(dst);
    int i = 0;
    while (src[i] && n + 1 < cap) dst[n++] = src[i++];
    dst[n] = 0;
}

static int ContainsI(const wchar_t* hay, const wchar_t* needle) {
    int nlen = WLen(needle);
    for (int i = 0; hay[i]; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            wchar_t a = hay[i + j];
            wchar_t b = needle[j];
            if (a >= L'a' && a <= L'z') a = (wchar_t)(a - L'a' + L'A');
            if (b >= L'a' && b <= L'z') b = (wchar_t)(b - L'a' + L'A');
            if (a != b) {
                ok = 0;
                break;
            }
        }
        if (ok) return 1;
    }
    return 0;
}

static void ParentDir(wchar_t* path) {
    int n = WLen(path);
    while (n > 0 && path[n - 1] != L'\\' && path[n - 1] != L'/') n--;
    if (n > 0) n--;
    path[n] = 0;
}

static void Fail(const wchar_t* text) {
    MessageBoxW(0, text, L"Theme1C", MB_OK | MB_ICONERROR);
}

static int ModuleLoaded(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me;
    memset(&me, 0, sizeof(me));
    me.dwSize = sizeof(me);
    int found = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (ContainsI(me.szModule, name)) {
                found = 1;
                break;
            }
            me.dwSize = sizeof(me);
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

extern "C" int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    DWORD n = GetModuleFileNameW(0, g_exeDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        Fail(L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043E\u043F\u0440\u0435\u0434\u0435\u043B\u0438\u0442\u044C \u043F\u0443\u0442\u044C \u0437\u0430\u043F\u0443\u0441\u043A\u0430.");
        return 1;
    }
    ParentDir(g_exeDir);
    WCopy(g_binDir, MAX_PATH, g_exeDir);
    ParentDir(g_binDir);
    ParentDir(g_binDir);

    WCopy(g_dllPath, MAX_PATH, g_exeDir);
    WCat(g_dllPath, MAX_PATH, L"\\cairo1c8.dll");
    HANDLE dll = CreateFileW(g_dllPath, 0x80000000, FILE_SHARE_READ, 0, 3, FILE_ATTRIBUTE_NORMAL, 0);
    if (dll == INVALID_HANDLE_VALUE) {
        Fail(L"\u0420\u044F\u0434\u043E\u043C \u0441 \u0437\u0430\u043F\u0443\u0441\u043A\u043E\u043C \u043D\u0435\u0442 cairo1c8.dll.");
        return 1;
    }
    CloseHandle(dll);

    wchar_t exe[MAX_PATH];
    WCopy(exe, MAX_PATH, g_binDir);
    WCat(exe, MAX_PATH, L"\\1cv8.exe");

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int enterprise = 0;
    int hasMode = 0;
    g_cmd[0] = 0;
    WCat(g_cmd, 2048, L"\"");
    WCat(g_cmd, 2048, exe);
    WCat(g_cmd, 2048, L"\"");
    if (argv) {
        for (int i = 1; i < argc; i++) {
            if (ContainsI(argv[i], L"ENTERPRISE")) enterprise = 1;
            if (ContainsI(argv[i], L"DESIGNER") || ContainsI(argv[i], L"ENTERPRISE")) hasMode = 1;
            WCat(g_cmd, 2048, L" ");
            int quote = 0;
            for (int c = 0; argv[i][c]; c++) {
                if (argv[i][c] == L' ') quote = 1;
            }
            if (quote) WCat(g_cmd, 2048, L"\"");
            WCat(g_cmd, 2048, argv[i]);
            if (quote) WCat(g_cmd, 2048, L"\"");
        }
        LocalFree(argv);
    }
    if (enterprise) {
        Fail(L"\u0417\u0430\u043F\u0443\u0441\u043A \u043F\u0440\u0435\u0434\u043F\u0440\u0438\u044F\u0442\u0438\u044F \u0447\u0435\u0440\u0435\u0437 \u044D\u0442\u0443 \u043F\u0440\u043E\u0433\u0440\u0430\u043C\u043C\u0443 \u043E\u0442\u043A\u043B\u044E\u0447\u0451\u043D: \u043E\u043D\u0430 \u043A\u0440\u0430\u0441\u0438\u0442 \u0442\u043E\u043B\u044C\u043A\u043E \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0442\u043E\u0440.");
        return 1;
    }
    if (!hasMode) WCat(g_cmd, 2048, L" DESIGNER");

    SetCurrentDirectoryW(g_binDir);
    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(exe, g_cmd, 0, 0, FALSE, CREATE_SUSPENDED, 0, g_binDir, &si, &pi)) {
        Fail(L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u0437\u0430\u043F\u0443\u0441\u0442\u0438\u0442\u044C 1cv8.exe DESIGNER.");
        return 1;
    }

    SIZE_T bytes = (SIZE_T)(WLen(g_dllPath) + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(pi.hProcess, 0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    SIZE_T wrote = 0;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    void* load = k32 ? GetProcAddress(k32, "LoadLibraryW") : 0;
    if (!remote || !WriteProcessMemory(pi.hProcess, remote, g_dllPath, bytes, &wrote) || !load) {
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        Fail(L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043F\u0435\u0440\u0435\u0434\u0430\u0442\u044C \u043F\u0443\u0442\u044C \u0431\u0438\u0431\u043B\u0438\u043E\u0442\u0435\u043A\u0438 \u0432 \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0442\u043E\u0440.");
        return 1;
    }

    QueueUserAPC((void (CALLBACK*)(ULONG_PTR))load, pi.hThread, (ULONG_PTR)remote);
    ResumeThread(pi.hThread);

    int loaded = 0;
    for (int i = 0; i < 50; i++) {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code) && code != STILL_ACTIVE) break;
        if (ModuleLoaded(pi.dwProcessId, L"cairo1c8.dll")) {
            loaded = 1;
            break;
        }
        Sleep(100);
    }
    if (!loaded) {
        HANDLE thread = CreateRemoteThread(pi.hProcess, 0, 0, (DWORD (WINAPI*)(LPVOID))load, remote, 0, 0);
        if (thread) {
            WaitForSingleObject(thread, 5000);
            CloseHandle(thread);
            loaded = ModuleLoaded(pi.dwProcessId, L"cairo1c8.dll");
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (!loaded) {
        Fail(L"\u041A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0442\u043E\u0440 \u0437\u0430\u043F\u0443\u0441\u0442\u0438\u043B\u0441\u044F, \u043D\u043E cairo1c8.dll \u0432 \u043D\u0435\u0433\u043E \u043D\u0435 \u043F\u043E\u043F\u0430\u043B\u0430.");
        return 1;
    }
    return 0;
}
