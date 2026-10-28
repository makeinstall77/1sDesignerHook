#pragma once

/* Freestanding reader for %AppData%\Theme1C\state.bin (and exe-dir mirror). */

#include "minwin.h"
#include "theme_state.h"
#include "theme_state_util.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int Theme1C_Client_CatW(wchar_t* dst, int cap, const wchar_t* src) {
  int n = 0;
  int i = 0;
  if (!dst || cap <= 0 || !src) return 0;
  while (dst[n]) n++;
  while (src[i] && n + 1 < cap) dst[n++] = src[i++];
  dst[n] = 0;
  return src[i] == 0;
}

/* Build %APPDATA%\Theme1C\state.bin */
static inline int Theme1C_Client_AppDataStatePath(wchar_t* out, int cap) {
  DWORD n;
  if (!out || cap < 32) return 0;
  n = GetEnvironmentVariableW(L"APPDATA", out, (DWORD)cap);
  if (n == 0 || n >= (DWORD)cap) return 0;
  if (!Theme1C_Client_CatW(out, cap, L"\\Theme1C\\")) return 0;
  return Theme1C_Client_CatW(out, cap, THEME1C_STATE_FILE_NAME);
}

/* Build <dll-dir>\state.bin */
static inline int Theme1C_Client_DllDirStatePath(HMODULE self, wchar_t* out, int cap) {
  DWORD n;
  int slash = -1;
  DWORD i;
  if (!out || cap < 16) return 0;
  n = GetModuleFileNameW(self, out, (DWORD)cap);
  if (n == 0 || n >= (DWORD)cap) return 0;
  for (i = 0; i < n; i++) {
    if (out[i] == L'\\' || out[i] == L'/') slash = (int)i;
  }
  if (slash < 0) return 0;
  out[slash] = 0;
  if (!Theme1C_Client_CatW(out, cap, L"\\")) return 0;
  return Theme1C_Client_CatW(out, cap, THEME1C_STATE_FILE_NAME);
}

static inline int Theme1C_Client_ReadPath(const wchar_t* path, Theme1C_State* dst) {
  HANDLE file;
  DWORD read = 0;
  Theme1C_State tmp;
  if (!path || !dst) return 0;
  file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) return 0;
  if (!ReadFile(file, &tmp, (DWORD)sizeof(tmp), &read, 0) || read != sizeof(tmp)) {
    CloseHandle(file);
    return 0;
  }
  CloseHandle(file);
  if (tmp.magic != THEME1C_STATE_MAGIC) return 0;
  if (tmp.version != THEME1C_STATE_VERSION) return 0;
  if (tmp.generation & 1u) return 0; /* mid-write */
  *dst = tmp;
  return 1;
}

/* Prefer AppData, then DLL directory mirror. */
static inline int Theme1C_Client_ReadState(HMODULE self, Theme1C_State* dst) {
  wchar_t path[MAX_PATH];
  if (!dst) return 0;
  if (Theme1C_Client_AppDataStatePath(path, MAX_PATH) && Theme1C_Client_ReadPath(path, dst)) return 1;
  if (self && Theme1C_Client_DllDirStatePath(self, path, MAX_PATH) && Theme1C_Client_ReadPath(path, dst))
    return 1;
  return 0;
}

#ifdef __cplusplus
}
#endif
