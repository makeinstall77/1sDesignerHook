#include "appdata.h"
#include "theme_ir.h"
#include "theme_state.h"

#include <cstring>

#include <windows.h>
#include <shlobj.h>

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

static bool CatW(wchar_t* out, size_t cap, const wchar_t* suffix) {
  if (!out || !suffix || cap == 0) return false;
  size_t n = 0;
  while (out[n]) n++;
  size_t i = 0;
  while (suffix[i] && n + 1 < cap) out[n++] = suffix[i++];
  out[n] = 0;
  return suffix[i] == 0;
}

bool Theme1C_GetRootDir(wchar_t* out, size_t capChars) {
  if (!out || capChars < 16) return false;
  wchar_t appdata[MAX_PATH];
  HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata);
  if (FAILED(hr)) {
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
  }
  if (!CopyW(out, capChars, appdata)) return false;
  return CatW(out, capChars, L"\\Theme1C");
}

bool Theme1C_GetConfigPath(wchar_t* out, size_t capChars) {
  if (!Theme1C_GetRootDir(out, capChars)) return false;
  return CatW(out, capChars, L"\\config.json");
}

bool Theme1C_GetThemesDir(wchar_t* out, size_t capChars) {
  if (!Theme1C_GetRootDir(out, capChars)) return false;
  return CatW(out, capChars, L"\\themes");
}

bool Theme1C_GetThemePath(const wchar_t* themeId, wchar_t* out, size_t capChars) {
  if (!themeId || !*themeId) return false;
  if (!Theme1C_GetThemesDir(out, capChars)) return false;
  if (!CatW(out, capChars, L"\\")) return false;
  if (!CatW(out, capChars, themeId)) return false;
  return CatW(out, capChars, L".json");
}

bool Theme1C_GetStatePath(wchar_t* out, size_t capChars) {
  if (!Theme1C_GetRootDir(out, capChars)) return false;
  return CatW(out, capChars, L"\\") && CatW(out, capChars, THEME1C_STATE_FILE_NAME);
}

bool Theme1C_GetExeDirStatePath(wchar_t* out, size_t capChars) {
  if (!out || capChars < 8) return false;
  wchar_t mod[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, mod, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  int slash = -1;
  for (DWORD i = 0; i < n; i++) {
    if (mod[i] == L'\\' || mod[i] == L'/') slash = (int)i;
  }
  if (slash < 0) return false;
  mod[slash] = 0;
  if (!CopyW(out, capChars, mod)) return false;
  return CatW(out, capChars, L"\\") && CatW(out, capChars, THEME1C_STATE_FILE_NAME);
}

bool Theme1C_EnsureDir(const wchar_t* path) {
  if (!path || !*path) return false;
  DWORD attr = GetFileAttributesW(path);
  if (attr != INVALID_FILE_ATTRIBUTES) {
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }
  /* Create parents recursively */
  wchar_t buf[MAX_PATH * 2];
  if (!CopyW(buf, sizeof(buf) / sizeof(buf[0]), path)) return false;
  size_t len = 0;
  while (buf[len]) len++;
  for (size_t i = 3; i < len; i++) {
    if (buf[i] == L'\\' || buf[i] == L'/') {
      wchar_t save = buf[i];
      buf[i] = 0;
      if (GetFileAttributesW(buf) == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(buf, nullptr)) {
          DWORD err = GetLastError();
          if (err != ERROR_ALREADY_EXISTS) {
            buf[i] = save;
            return false;
          }
        }
      }
      buf[i] = save;
    }
  }
  if (GetFileAttributesW(buf) == INVALID_FILE_ATTRIBUTES) {
    if (!CreateDirectoryW(buf, nullptr)) {
      DWORD err = GetLastError();
      if (err != ERROR_ALREADY_EXISTS) return false;
    }
  }
  return true;
}

bool Theme1C_GetBuiltinDarkPlusRepoPath(wchar_t* out, size_t capChars) {
  if (!out || capChars < 8) return false;
  wchar_t mod[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, mod, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  /* strip filename */
  int slash = -1;
  for (DWORD i = 0; i < n; i++) {
    if (mod[i] == L'\\' || mod[i] == L'/') slash = (int)i;
  }
  if (slash < 0) return false;
  mod[slash] = 0;
  if (!CopyW(out, capChars, mod)) return false;
  /* Prefer theme1c\builtin\dark-plus.json next to exe, then ..\builtin\ */
  wchar_t try1[MAX_PATH * 2];
  if (CopyW(try1, sizeof(try1) / sizeof(try1[0]), out) &&
      CatW(try1, sizeof(try1) / sizeof(try1[0]), L"\\builtin\\dark-plus.json") &&
      GetFileAttributesW(try1) != INVALID_FILE_ATTRIBUTES) {
    return CopyW(out, capChars, try1);
  }
  if (CopyW(try1, sizeof(try1) / sizeof(try1[0]), out) &&
      CatW(try1, sizeof(try1) / sizeof(try1[0]), L"\\..\\theme1c\\builtin\\dark-plus.json") &&
      GetFileAttributesW(try1) != INVALID_FILE_ATTRIBUTES) {
    return CopyW(out, capChars, try1);
  }
  /* Fall back to expected relative path even if missing — caller may generate. */
  return CopyW(out, capChars, mod) && CatW(out, capChars, L"\\builtin\\dark-plus.json");
}

bool Theme1C_EnsureAppDataLayout() {
  wchar_t root[MAX_PATH * 2];
  wchar_t themes[MAX_PATH * 2];
  if (!Theme1C_GetRootDir(root, sizeof(root) / sizeof(root[0]))) return false;
  if (!Theme1C_EnsureDir(root)) return false;
  if (!Theme1C_GetThemesDir(themes, sizeof(themes) / sizeof(themes[0]))) return false;
  if (!Theme1C_EnsureDir(themes)) return false;

  /* Seed builtins when missing (copy from exe\builtin\, else synthesize Dark+). */
  {
    wchar_t dest[MAX_PATH * 2];
    wchar_t mod[MAX_PATH];
    wchar_t src[MAX_PATH * 2];
    const wchar_t* ids[] = {L"dark-plus", L"ruby-blue"};
    const wchar_t* files[] = {L"dark-plus.json", L"ruby-blue.json"};
    DWORD n = GetModuleFileNameW(nullptr, mod, MAX_PATH);
    int slash = -1;
    if (n && n < MAX_PATH) {
      for (DWORD i = 0; i < n; i++) {
        if (mod[i] == L'\\' || mod[i] == L'/') slash = (int)i;
      }
      if (slash >= 0) mod[slash] = 0;
    }
    for (int t = 0; t < 2; t++) {
      if (!Theme1C_GetThemePath(ids[t], dest, sizeof(dest) / sizeof(dest[0]))) continue;
      if (GetFileAttributesW(dest) != INVALID_FILE_ATTRIBUTES) continue;
      bool copied = false;
      if (slash >= 0 &&
          CopyW(src, sizeof(src) / sizeof(src[0]), mod) &&
          CatW(src, sizeof(src) / sizeof(src[0]), L"\\builtin\\") &&
          CatW(src, sizeof(src) / sizeof(src[0]), files[t]) &&
          GetFileAttributesW(src) != INVALID_FILE_ATTRIBUTES) {
        copied = CopyFileW(src, dest, TRUE) != 0;
      }
      if (!copied && t == 0) {
        ThemeIr ir;
        ThemeIr_LoadBuiltinDarkPlus(&ir);
        std::wstring err;
        ThemeIr_SaveFile(dest, &ir, &err);
      }
    }
  }
  return true;
}
