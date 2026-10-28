#pragma once

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long long ULONGLONG;
typedef long LONG;
typedef long long LONGLONG;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef BYTE* LPBYTE;
typedef int BOOL;
typedef unsigned __int64 ULONG_PTR;
typedef ULONG_PTR SIZE_T;
typedef DWORD* LPDWORD;

#define WINAPI __stdcall
#define CALLBACK __stdcall
#define TRUE 1
#define FALSE 0
#define NULL 0
#define INVALID_HANDLE_VALUE ((HANDLE)(LONGLONG)-1)

#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READWRITE 0x40

#define CREATE_SUSPENDED 0x00000004
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define STILL_ACTIVE 259

#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_APPEND_DATA 0x0004

#define SECTION_MAP_READ 0x0004
#define FILE_MAP_READ SECTION_MAP_READ

#define TH32CS_SNAPMODULE 0x00000008
#define MAX_PATH 260
#define MB_OK 0
#define MB_ICONERROR 0x10

struct STARTUPINFOW {
    DWORD cb;
    DWORD pad0;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    DWORD pad1;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};

struct MODULEENTRY32W {
    DWORD dwSize;
    DWORD th32ModuleID;
    DWORD th32ProcessID;
    DWORD GlblcntUsage;
    DWORD ProccntUsage;
    DWORD pad0;
    BYTE* modBaseAddr;
    DWORD modBaseSize;
    DWORD pad1;
    HMODULE hModule;
    wchar_t szModule[256];
    wchar_t szExePath[260];
};

static_assert(sizeof(STARTUPINFOW) == 104, "STARTUPINFOW");
static_assert(sizeof(PROCESS_INFORMATION) == 24, "PROCESS_INFORMATION");
static_assert(sizeof(MODULEENTRY32W) == 1080, "MODULEENTRY32W");

extern "C" {
__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) void* WINAPI GetProcAddress(HMODULE, LPCSTR);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HMODULE, LPWSTR, DWORD);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessId(void);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE, LPVOID, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, LPCVOID, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) DWORD WINAPI GetEnvironmentVariableW(LPCWSTR, LPWSTR, DWORD);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) DWORD WINAPI GetTickCount(void);
__declspec(dllimport) HANDLE WINAPI CreateThread(void*, SIZE_T, DWORD (WINAPI*)(LPVOID), LPVOID, DWORD, DWORD*);
__declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);
__declspec(dllimport) DWORD WINAPI GetLastError(void);
__declspec(dllimport) HANDLE WINAPI OpenFileMappingW(DWORD, BOOL, LPCWSTR);
__declspec(dllimport) LPVOID WINAPI MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
__declspec(dllimport) BOOL WINAPI UnmapViewOfFile(LPCVOID);

__declspec(dllimport) BOOL WINAPI CreateProcessW(LPCWSTR, LPWSTR, void*, void*, BOOL, DWORD, void*, LPCWSTR, STARTUPINFOW*, PROCESS_INFORMATION*);
__declspec(dllimport) BOOL WINAPI WriteProcessMemory(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
__declspec(dllimport) LPVOID WINAPI VirtualAllocEx(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI VirtualFreeEx(HANDLE, LPVOID, SIZE_T, DWORD);
__declspec(dllimport) DWORD WINAPI QueueUserAPC(void (CALLBACK*)(ULONG_PTR), HANDLE, ULONG_PTR);
__declspec(dllimport) DWORD WINAPI ResumeThread(HANDLE);
__declspec(dllimport) HANDLE WINAPI CreateRemoteThread(HANDLE, void*, SIZE_T, DWORD (WINAPI*)(LPVOID), LPVOID, DWORD, DWORD*);
__declspec(dllimport) HANDLE WINAPI CreateToolhelp32Snapshot(DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI Module32FirstW(HANDLE, MODULEENTRY32W*);
__declspec(dllimport) BOOL WINAPI Module32NextW(HANDLE, MODULEENTRY32W*);
__declspec(dllimport) DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
__declspec(dllimport) BOOL WINAPI GetExitCodeProcess(HANDLE, DWORD*);
__declspec(dllimport) void WINAPI ExitProcess(DWORD);
__declspec(dllimport) LPWSTR WINAPI GetCommandLineW(void);
__declspec(dllimport) BOOL WINAPI SetCurrentDirectoryW(LPCWSTR);

__declspec(dllimport) int WINAPI MessageBoxW(HANDLE, LPCWSTR, LPCWSTR, DWORD);
__declspec(dllimport) LPWSTR* WINAPI CommandLineToArgvW(LPCWSTR, int*);
__declspec(dllimport) HANDLE WINAPI LocalFree(HANDLE);
}

/* Provided by crtfix.cpp — no #pragma function (build uses /Oi-). */
extern "C" void* memset(void* dst, int value, SIZE_T count);
extern "C" void* memcpy(void* dst, const void* src, SIZE_T count);
extern "C" int _fltused;
