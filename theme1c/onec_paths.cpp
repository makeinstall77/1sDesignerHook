#include "onec_paths.h"
#include "config.h"

#include <cstdio>
#include <cstring>

#include <windows.h>
#include <shellapi.h>

static bool CopyW(wchar_t* out, size_t cap, const wchar_t* src) {
  if (!out || cap == 0) return false;
  if (!src) {
    out[0] = 0;
    return false;
  }
  size_t i = 0;
  while (src[i] && i + 1 < cap) {
    out[i] = src[i];
    i++;
  }
  out[i] = 0;
  return src[i] == 0;
}

static bool FileExists(const wchar_t* path) {
  if (!path || !*path) return false;
  DWORD attr = GetFileAttributesW(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool TryJoin(const wchar_t* dir, const wchar_t* relative, wchar_t* out, size_t cap) {
  if (!dir || !*dir || !relative) return false;
  wchar_t cand[MAX_PATH * 2];
  size_t len = 0;
  while (dir[len] && len + 1 < sizeof(cand) / sizeof(cand[0])) {
    cand[len] = dir[len];
    len++;
  }
  cand[len] = 0;
  if (len && cand[len - 1] != L'\\' && len + 1 < sizeof(cand) / sizeof(cand[0])) {
    cand[len++] = L'\\';
    cand[len] = 0;
  }
  for (size_t i = 0; relative[i] && len + 1 < sizeof(cand) / sizeof(cand[0]); i++)
    cand[len++] = relative[i];
  cand[len] = 0;
  if (!FileExists(cand)) return false;
  return CopyW(out, cap, cand);
}

static bool TryEnvPath(const wchar_t* envName, const wchar_t* relative, wchar_t* out, size_t cap) {
  wchar_t base[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(envName, base, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  return TryJoin(base, relative, out, cap);
}

static bool ContainsI(const wchar_t* hay, const wchar_t* needle) {
  if (!hay || !needle || !*needle) return false;
  size_t nlen = wcslen(needle);
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

static bool TryPathAsStartRoot(const wchar_t* pathOrDir, wchar_t* out, size_t cap) {
  if (!pathOrDir || !*pathOrDir) return false;
  if (FileExists(pathOrDir) && ContainsI(pathOrDir, L"1cestart.exe"))
    return CopyW(out, cap, pathOrDir);
  if (TryJoin(pathOrDir, L"1cestart.exe", out, cap)) return true;
  if (TryJoin(pathOrDir, L"common\\1cestart.exe", out, cap)) return true;
  if (TryJoin(pathOrDir, L"..\\common\\1cestart.exe", out, cap)) return true;
  return false;
}

static bool ReadRegSz(HKEY key, const wchar_t* valueName, wchar_t* out, size_t cap) {
  DWORD type = 0;
  DWORD cb = (DWORD)(cap * sizeof(wchar_t));
  LONG rc = RegQueryValueExW(key, valueName, nullptr, &type, (LPBYTE)out, &cb);
  if (rc != ERROR_SUCCESS) return false;
  if (type != REG_SZ && type != REG_EXPAND_SZ) return false;
  size_t chars = cb / sizeof(wchar_t);
  if (chars == 0) {
    out[0] = 0;
    return false;
  }
  if (out[chars - 1] != 0) {
    if (chars >= cap) return false;
    out[chars] = 0;
  }
  return out[0] != 0;
}

static bool ProbeKeyValues(HKEY key, wchar_t* out, size_t cap) {
  static const wchar_t* kNames[] = {
      L"InstallLocation", L"InstallPath", L"Location", L"Common", L"Path", L"", nullptr};
  for (int i = 0; kNames[i]; i++) {
    wchar_t val[MAX_PATH * 2];
    const wchar_t* vn = kNames[i][0] ? kNames[i] : nullptr; /* nullptr = default */
    if (!ReadRegSz(key, vn, val, sizeof(val) / sizeof(val[0]))) continue;
    if (TryPathAsStartRoot(val, out, cap)) return true;
  }
  return false;
}

static bool ScanRegKeyRecursive(HKEY key, int depth, wchar_t* out, size_t cap) {
  if (ProbeKeyValues(key, out, cap)) return true;
  if (depth <= 0) return false;

  wchar_t name[256];
  DWORD index = 0;
  for (;;) {
    DWORD nameLen = (DWORD)(sizeof(name) / sizeof(name[0]));
    LONG rc = RegEnumKeyExW(key, index++, name, &nameLen, nullptr, nullptr, nullptr, nullptr);
    if (rc == ERROR_NO_MORE_ITEMS) break;
    if (rc != ERROR_SUCCESS) continue;
    HKEY child = nullptr;
    if (RegOpenKeyExW(key, name, 0, KEY_READ, &child) != ERROR_SUCCESS) continue;
    bool ok = ScanRegKeyRecursive(child, depth - 1, out, cap);
    RegCloseKey(child);
    if (ok) return true;
  }
  return false;
}

static bool ScanRegTree(HKEY root, const wchar_t* baseKey, wchar_t* out, size_t cap) {
  HKEY key = nullptr;
  REGSAM sams[] = {
      KEY_READ | KEY_WOW64_64KEY,
      KEY_READ | KEY_WOW64_32KEY,
      KEY_READ,
      0};
  for (int i = 0; sams[i]; i++) {
    if (RegOpenKeyExW(root, baseKey, 0, sams[i], &key) == ERROR_SUCCESS) {
      bool ok = ScanRegKeyRecursive(key, 3, out, cap);
      RegCloseKey(key);
      if (ok) return true;
    }
  }
  return false;
}

bool FindOneCStart(wchar_t* out, size_t capChars) {
  if (!out || capChars < 8) return false;
  out[0] = 0;

  Theme1C_Config cfg;
  Theme1C_Config_Defaults(&cfg);
  Theme1C_Config_Load(&cfg, nullptr);
  if (!cfg.oneCStartPath.empty() && FileExists(cfg.oneCStartPath.c_str())) {
    return CopyW(out, capChars, cfg.oneCStartPath.c_str());
  }

  if (TryEnvPath(L"ProgramFiles", L"1cv8\\common\\1cestart.exe", out, capChars)) return true;
  if (TryEnvPath(L"ProgramFiles(x86)", L"1cv8\\common\\1cestart.exe", out, capChars)) return true;
  if (TryEnvPath(L"ProgramW6432", L"1cv8\\common\\1cestart.exe", out, capChars)) return true;

  static const wchar_t* kFixed[] = {
      L"SOFTWARE\\1C\\1Cv8",
      L"SOFTWARE\\1C\\1Cv8.1CEnterprise",
      L"SOFTWARE\\WOW6432Node\\1C\\1Cv8",
      L"SOFTWARE\\WOW6432Node\\1C\\1Cv8.1CEnterprise",
      nullptr};
  for (int i = 0; kFixed[i]; i++) {
    if (ScanRegTree(HKEY_LOCAL_MACHINE, kFixed[i], out, capChars)) return true;
    if (ScanRegTree(HKEY_CURRENT_USER, kFixed[i], out, capChars)) return true;
  }

  HKEY soft = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\1C", 0, KEY_READ | KEY_WOW64_64KEY, &soft) ==
          ERROR_SUCCESS ||
      RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\1C", 0, KEY_READ, &soft) == ERROR_SUCCESS) {
    wchar_t name[256];
    DWORD idx = 0;
    for (;;) {
      DWORD nlen = (DWORD)(sizeof(name) / sizeof(name[0]));
      LONG rc = RegEnumKeyExW(soft, idx++, name, &nlen, nullptr, nullptr, nullptr, nullptr);
      if (rc == ERROR_NO_MORE_ITEMS) break;
      if (rc != ERROR_SUCCESS) continue;
      if (_wcsnicmp(name, L"1Cv8", 4) != 0) continue;
      wchar_t path[512];
      _snwprintf_s(path, _TRUNCATE, L"SOFTWARE\\1C\\%s", name);
      if (ScanRegTree(HKEY_LOCAL_MACHINE, path, out, capChars)) {
        RegCloseKey(soft);
        return true;
      }
    }
    RegCloseKey(soft);
  }

  soft = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\1C", 0, KEY_READ, &soft) == ERROR_SUCCESS) {
    wchar_t name[256];
    DWORD idx = 0;
    for (;;) {
      DWORD nlen = (DWORD)(sizeof(name) / sizeof(name[0]));
      LONG rc = RegEnumKeyExW(soft, idx++, name, &nlen, nullptr, nullptr, nullptr, nullptr);
      if (rc == ERROR_NO_MORE_ITEMS) break;
      if (rc != ERROR_SUCCESS) continue;
      if (_wcsnicmp(name, L"1Cv8", 4) != 0) continue;
      wchar_t path[512];
      _snwprintf_s(path, _TRUNCATE, L"SOFTWARE\\1C\\%s", name);
      if (ScanRegTree(HKEY_CURRENT_USER, path, out, capChars)) {
        RegCloseKey(soft);
        return true;
      }
    }
    RegCloseKey(soft);
  }

  return false;
}

bool LaunchOneCStart(const wchar_t* path) {
  wchar_t resolved[MAX_PATH * 2];
  const wchar_t* use = path;
  if (!use || !*use || !FileExists(use)) {
    if (!FindOneCStart(resolved, sizeof(resolved) / sizeof(resolved[0]))) return false;
    use = resolved;
  }

  SHELLEXECUTEINFOW sei;
  memset(&sei, 0, sizeof(sei));
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"open";
  sei.lpFile = use;
  sei.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&sei)) return false;
  if (sei.hProcess) CloseHandle(sei.hProcess);
  return true;
}
