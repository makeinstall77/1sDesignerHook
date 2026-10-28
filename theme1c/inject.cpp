#include "inject.h"

#include <cstring>
#include <cwchar>

#include <tlhelp32.h>

#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

typedef struct _UNICODE_STRING_REMOTE {
  USHORT Length;
  USHORT MaximumLength;
  ULONG pad;
  ULONGLONG Buffer; /* PWSTR in remote process */
} UNICODE_STRING_REMOTE;

typedef struct _PROCESS_BASIC_INFORMATION_N {
  PVOID Reserved1;
  PVOID PebBaseAddress;
  PVOID Reserved2[2];
  ULONG_PTR UniqueProcessId;
  PVOID Reserved3;
} PROCESS_BASIC_INFORMATION_N;

typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength);

/* x64 PEB / RTL_USER_PROCESS_PARAMETERS offsets (well-known):
 *   PEB+0x20  → ProcessParameters (PRTL_USER_PROCESS_PARAMETERS)
 *   RTL_USER_PROCESS_PARAMETERS+0x70 → CommandLine (UNICODE_STRING)
 * UNICODE_STRING (x64): Length@0, MaximumLength@2, Buffer@8
 */

static bool ContainsI(const wchar_t* hay, const wchar_t* needle) {
  if (!hay || !needle || !*needle) return false;
  size_t nlen = 0;
  while (needle[nlen]) nlen++;
  for (size_t i = 0; hay[i]; i++) {
    size_t j = 0;
    for (; j < nlen; j++) {
      wchar_t a = hay[i + j];
      wchar_t b = needle[j];
      if (a >= L'a' && a <= L'z') a = (wchar_t)(a - L'a' + L'A');
      if (b >= L'a' && b <= L'z') b = (wchar_t)(b - L'a' + L'A');
      if (!a || a != b) break;
    }
    if (j == nlen) return true;
  }
  return false;
}

static bool EqualsI(const wchar_t* a, const wchar_t* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    wchar_t ca = *a++, cb = *b++;
    if (ca >= L'a' && ca <= L'z') ca = (wchar_t)(ca - L'a' + L'A');
    if (cb >= L'a' && cb <= L'z') cb = (wchar_t)(cb - L'a' + L'A');
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

static void SetErr(std::wstring* err, const wchar_t* msg) {
  if (err) *err = msg ? msg : L"";
}

bool Inject_GetCairoDllPath(wchar_t* out, size_t capChars) {
  if (!out || capChars < 16) return false;
  wchar_t mod[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, mod, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  int slash = -1;
  for (DWORD i = 0; i < n; i++) {
    if (mod[i] == L'\\' || mod[i] == L'/') slash = (int)i;
  }
  if (slash < 0) return false;
  mod[slash] = 0;

  size_t len = 0;
  while (mod[len] && len + 1 < capChars) {
    out[len] = mod[len];
    len++;
  }
  const wchar_t* suffix = L"\\cairo1c8.dll";
  for (size_t i = 0; suffix[i]; i++) {
    if (len + 1 >= capChars) {
      out[0] = 0;
      return false;
    }
    out[len++] = suffix[i];
  }
  out[len] = 0;
  return true;
}

bool ModuleLoadedInProcess(DWORD pid, const wchar_t* name) {
  if (!name || !*name) return false;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) return false;
  MODULEENTRY32W me;
  memset(&me, 0, sizeof(me));
  me.dwSize = sizeof(me);
  bool found = false;
  if (Module32FirstW(snap, &me)) {
    do {
      if (ContainsI(me.szModule, name) || EqualsI(me.szModule, name)) {
        found = true;
        break;
      }
      me.dwSize = sizeof(me);
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return found;
}

bool InjectDll(DWORD pid, const wchar_t* dllPath, std::wstring* err) {
  if (!dllPath || !*dllPath) {
    SetErr(err, L"Пустой путь DLL");
    return false;
  }
  if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
    SetErr(err, L"cairo1c8.dll не найдена рядом с Theme1C.exe");
    return false;
  }
  if (ModuleLoadedInProcess(pid, L"cairo1c8.dll")) {
    SetErr(err, L"Уже внедрена");
    return true;
  }

  HANDLE proc = OpenProcess(
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
          PROCESS_VM_WRITE | PROCESS_VM_READ,
      FALSE,
      pid);
  if (!proc) {
    SetErr(err, L"OpenProcess не удалось (нужны права?)");
    return false;
  }

  size_t bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
  LPVOID remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote) {
    CloseHandle(proc);
    SetErr(err, L"VirtualAllocEx не удалось");
    return false;
  }

  SIZE_T wrote = 0;
  if (!WriteProcessMemory(proc, remote, dllPath, bytes, &wrote) || wrote != bytes) {
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    SetErr(err, L"WriteProcessMemory не удалось");
    return false;
  }

  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  FARPROC load = k32 ? GetProcAddress(k32, "LoadLibraryW") : nullptr;
  if (!load) {
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    SetErr(err, L"LoadLibraryW не найден");
    return false;
  }

  HANDLE thread = CreateRemoteThread(
      proc, nullptr, 0, (LPTHREAD_START_ROUTINE)load, remote, 0, nullptr);
  if (!thread) {
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    SetErr(err, L"CreateRemoteThread не удалось");
    return false;
  }

  WaitForSingleObject(thread, 10000);
  DWORD exitCode = 0;
  GetExitCodeThread(thread, &exitCode);
  CloseHandle(thread);

  /* Give loader a moment, then verify module list */
  bool ok = false;
  for (int i = 0; i < 30; i++) {
    if (ModuleLoadedInProcess(pid, L"cairo1c8.dll")) {
      ok = true;
      break;
    }
    Sleep(50);
  }

  VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
  CloseHandle(proc);

  if (!ok) {
    SetErr(err, L"LoadLibraryW выполнен, но cairo1c8.dll в процессе не найдена");
    return false;
  }
  if (err) err->clear();
  return true;
}

bool ReadProcessCommandLine(DWORD pid, std::wstring* out, std::wstring* err) {
  if (!out) return false;
  out->clear();

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    SetErr(err, L"ntdll недоступен");
    return false;
  }
  auto NtQIP = (NtQueryInformationProcess_t)GetProcAddress(ntdll, "NtQueryInformationProcess");
  if (!NtQIP) {
    SetErr(err, L"NtQueryInformationProcess не найден");
    return false;
  }

  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!proc) {
    SetErr(err, L"OpenProcess для чтения командной строки не удался");
    return false;
  }

  PROCESS_BASIC_INFORMATION_N pbi;
  memset(&pbi, 0, sizeof(pbi));
  ULONG retLen = 0;
  NTSTATUS st = NtQIP(proc, 0 /* ProcessBasicInformation */, &pbi, sizeof(pbi), &retLen);
  if (st < 0 || !pbi.PebBaseAddress) {
    CloseHandle(proc);
    SetErr(err, L"NtQueryInformationProcess не удался");
    return false;
  }

  /* PEB+0x20 → ProcessParameters */
  ULONGLONG processParams = 0;
  SIZE_T rd = 0;
  BYTE* peb = (BYTE*)pbi.PebBaseAddress;
  if (!ReadProcessMemory(proc, peb + 0x20, &processParams, sizeof(processParams), &rd) ||
      rd != sizeof(processParams) || processParams == 0) {
    CloseHandle(proc);
    SetErr(err, L"Не удалось прочитать PEB.ProcessParameters");
    return false;
  }

  /* ProcessParameters+0x70 → CommandLine UNICODE_STRING */
  UNICODE_STRING_REMOTE us;
  memset(&us, 0, sizeof(us));
  if (!ReadProcessMemory(proc, (BYTE*)(ULONG_PTR)processParams + 0x70, &us, sizeof(us), &rd) ||
      rd != sizeof(us)) {
    CloseHandle(proc);
    SetErr(err, L"Не удалось прочитать CommandLine UNICODE_STRING");
    return false;
  }

  if (us.Length == 0 || us.Buffer == 0 || us.Length > 64 * 1024) {
    CloseHandle(proc);
    SetErr(err, L"Пустая командная строка");
    return false;
  }

  size_t chars = us.Length / sizeof(wchar_t);
  std::wstring buf(chars, L'\0');
  if (!ReadProcessMemory(proc, (LPCVOID)(ULONG_PTR)us.Buffer, &buf[0], us.Length, &rd) ||
      rd != us.Length) {
    CloseHandle(proc);
    SetErr(err, L"Не удалось прочитать буфер CommandLine");
    return false;
  }
  CloseHandle(proc);
  *out = std::move(buf);
  if (err) err->clear();
  return true;
}

OneCMode ClassifyOneCCommandLine(const wchar_t* cmd) {
  if (!cmd) return OneCMode_Unknown;
  bool ent = ContainsI(cmd, L"ENTERPRISE");
  bool des = ContainsI(cmd, L"DESIGNER");
  if (ent && !des) return OneCMode_Enterprise;
  if (des && !ent) return OneCMode_Designer;
  if (ent && des) return OneCMode_Enterprise; /* prefer safer */
  return OneCMode_Unknown;
}

const wchar_t* OneCMode_Name(OneCMode mode) {
  switch (mode) {
    case OneCMode_Designer: return L"DESIGNER";
    case OneCMode_Enterprise: return L"ENTERPRISE";
    default: return L"Unknown";
  }
}

bool EnumOneCProcesses(std::vector<OneCProcessInfo>* out) {
  if (!out) return false;
  out->clear();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;

  PROCESSENTRY32W pe;
  memset(&pe, 0, sizeof(pe));
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"1cv8.exe") != 0) continue;
      OneCProcessInfo info;
      info.pid = pe.th32ProcessID;
      info.mode = OneCMode_Unknown;
      info.injected = ModuleLoadedInProcess(info.pid, L"cairo1c8.dll");
      std::wstring cmd;
      if (ReadProcessCommandLine(info.pid, &cmd, nullptr)) {
        info.commandLine = std::move(cmd);
        info.mode = ClassifyOneCCommandLine(info.commandLine.c_str());
      }
      out->push_back(std::move(info));
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return true;
}

void AutoInjectTick(bool hooksEnabled, bool autoInject) {
  if (!hooksEnabled || !autoInject) return;

  wchar_t dllPath[MAX_PATH * 2];
  if (!Inject_GetCairoDllPath(dllPath, sizeof(dllPath) / sizeof(dllPath[0]))) return;

  std::vector<OneCProcessInfo> list;
  if (!EnumOneCProcesses(&list)) return;

  for (const auto& p : list) {
    if (p.mode != OneCMode_Designer) continue;
    if (p.injected) continue;
    std::wstring err;
    InjectDll(p.pid, dllPath, &err);
  }
}

bool ManualInjectPid(DWORD pid, HWND owner, std::wstring* err) {
  std::wstring cmd;
  OneCMode mode = OneCMode_Unknown;
  if (ReadProcessCommandLine(pid, &cmd, nullptr)) {
    mode = ClassifyOneCCommandLine(cmd.c_str());
  }
  if (mode == OneCMode_Enterprise) {
    int r = MessageBoxW(
        owner,
        L"Процесс похож на ENTERPRISE. Внедрение темы в предприятие не рекомендуется "
        L"и может быть нестабильным. Продолжить?",
        L"Theme1C",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) {
      SetErr(err, L"Отменено пользователем");
      return false;
    }
  }

  wchar_t dllPath[MAX_PATH * 2];
  if (!Inject_GetCairoDllPath(dllPath, sizeof(dllPath) / sizeof(dllPath[0]))) {
    SetErr(err, L"Не удалось определить путь cairo1c8.dll");
    return false;
  }
  return InjectDll(pid, dllPath, err);
}
