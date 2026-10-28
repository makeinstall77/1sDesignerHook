#include "theme_state_host.h"
#include "theme_state_util.h"
#include "appdata.h"

#include <cstring>

#include <windows.h>

static bool g_ready = false;
static unsigned int g_generation = 0;

static bool WriteStateAtomic(const wchar_t* finalPath, const Theme1C_State* state) {
  if (!finalPath || !state) return false;

  wchar_t tmp[MAX_PATH * 2];
  size_t i = 0;
  while (finalPath[i] && i + 5 < sizeof(tmp) / sizeof(tmp[0])) {
    tmp[i] = finalPath[i];
    i++;
  }
  if (i + 5 >= sizeof(tmp) / sizeof(tmp[0])) return false;
  tmp[i++] = L'.';
  tmp[i++] = L't';
  tmp[i++] = L'm';
  tmp[i++] = L'p';
  tmp[i] = 0;

  HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wrote = 0;
  BOOL ok = WriteFile(h, state, (DWORD)sizeof(Theme1C_State), &wrote, nullptr);
  CloseHandle(h);
  if (!ok || wrote != sizeof(Theme1C_State)) {
    DeleteFileW(tmp);
    return false;
  }
  if (!MoveFileExW(tmp, finalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(tmp);
    return false;
  }
  return true;
}

bool ThemeState_OpenWriter() {
  wchar_t root[MAX_PATH * 2];
  if (!Theme1C_GetRootDir(root, sizeof(root) / sizeof(root[0]))) return false;
  if (!Theme1C_EnsureDir(root)) return false;
  g_ready = true;
  return true;
}

void ThemeState_CloseWriter() {
  g_ready = false;
}

bool ThemeState_IsOpen() {
  return g_ready;
}

void ThemeState_Publish(const Theme1C_State* state) {
  if (!state) return;
  if (!g_ready && !ThemeState_OpenWriter()) return;

  Theme1C_State copy = *state;
  copy.magic = THEME1C_STATE_MAGIC;
  copy.version = THEME1C_STATE_VERSION;
  /* Keep generation even (stable). Odd reserved for in-progress writers. */
  g_generation += 2;
  if (g_generation == 0) g_generation = 2;
  copy.generation = g_generation;

  wchar_t appPath[MAX_PATH * 2];
  if (Theme1C_GetStatePath(appPath, sizeof(appPath) / sizeof(appPath[0]))) {
    WriteStateAtomic(appPath, &copy);
  }

  /* Mirror next to exe/dll so freestanding reader can fall back without APPDATA. */
  wchar_t exePath[MAX_PATH * 2];
  if (Theme1C_GetExeDirStatePath(exePath, sizeof(exePath) / sizeof(exePath[0]))) {
    WriteStateAtomic(exePath, &copy);
  }
}

void ThemeState_PublishDarkPlus(unsigned int enabled) {
  Theme1C_State s;
  Theme1C_State_FillDarkPlus(&s);
  s.enabled = enabled ? 1u : 0u;
  ThemeState_Publish(&s);
}
