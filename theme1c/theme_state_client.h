#pragma once

/* Freestanding reader for Local\Theme1C_State (cairo1c.dll, no CRT). */

#include "minwin.h"
#include "theme_state.h"
#include "theme_state_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma intrinsic(_ReadWriteBarrier)
static inline void Theme1C_Client_Barrier(void) { _ReadWriteBarrier(); }
#else
static inline void Theme1C_Client_Barrier(void) {}
#endif

/* Seqlock-read one snapshot. Returns 1 if magic/version/generation stable. */
static inline int Theme1C_Client_TryCopy(
    const volatile Theme1C_State* src,
    Theme1C_State* dst) {
  unsigned int g1;
  unsigned int g2;
  if (!src || !dst) return 0;

  g1 = src->generation;
  Theme1C_Client_Barrier();
  if (g1 & 1u) return 0;

  memcpy((void*)dst, (const void*)(Theme1C_State*)src, sizeof(Theme1C_State));
  Theme1C_Client_Barrier();
  g2 = src->generation;

  if (g1 != g2 || (g2 & 1u)) return 0;
  if (dst->magic != THEME1C_STATE_MAGIC) return 0;
  if (dst->version != THEME1C_STATE_VERSION) return 0;
  return 1;
}

/* Open mapping read-only and map view. Leaves *outMap/*outView unchanged on failure. */
static inline int Theme1C_Client_Open(
    HANDLE* outMap,
    const volatile Theme1C_State** outView) {
  HANDLE map;
  LPVOID view;
  if (!outMap || !outView) return 0;
  if (*outView) return 1;

  map = OpenFileMappingW(FILE_MAP_READ, FALSE, THEME1C_STATE_MAPPING_NAME);
  if (!map) return 0;

  view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, sizeof(Theme1C_State));
  if (!view) {
    CloseHandle(map);
    return 0;
  }

  *outMap = map;
  *outView = (const volatile Theme1C_State*)view;
  return 1;
}

static inline void Theme1C_Client_Close(
    HANDLE* map,
    const volatile Theme1C_State** view) {
  if (view && *view) {
    UnmapViewOfFile((LPCVOID)*view);
    *view = 0;
  }
  if (map && *map) {
    CloseHandle(*map);
    *map = 0;
  }
}

#ifdef __cplusplus
}
#endif
