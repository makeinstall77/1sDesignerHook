#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <windows.h>

enum OneCMode {
  OneCMode_Unknown = 0,
  OneCMode_Designer = 1,
  OneCMode_Enterprise = 2
};

struct OneCProcessInfo {
  DWORD pid;
  OneCMode mode;
  bool injected;
  std::wstring commandLine;
};

/* Resolve Theme1C.exe directory + \\cairo1c8.dll */
bool Inject_GetCairoDllPath(wchar_t* out, size_t capChars);

bool ModuleLoadedInProcess(DWORD pid, const wchar_t* name);
bool InjectDll(DWORD pid, const wchar_t* dllPath, std::wstring* err);

bool ReadProcessCommandLine(DWORD pid, std::wstring* out, std::wstring* err);
OneCMode ClassifyOneCCommandLine(const wchar_t* cmd);
const wchar_t* OneCMode_Name(OneCMode mode);

bool EnumOneCProcesses(std::vector<OneCProcessInfo>* out);

/* Inject DESIGNER processes that lack cairo1c8.dll. No ENTERPRISE. */
void AutoInjectTick(bool hooksEnabled, bool autoInject);

/* Manual apply. ENTERPRISE shows warning MessageBox; returns false if cancelled. */
bool ManualInjectPid(DWORD pid, HWND owner, std::wstring* err);
