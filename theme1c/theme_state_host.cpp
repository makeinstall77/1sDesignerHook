#include "theme_state_host.h"
#include "theme_state_util.h"

#include <cstring>

#include <windows.h>

static HANDLE g_mapping = nullptr;
static Theme1C_State* g_view = nullptr;

bool ThemeState_OpenWriter() {
  if (g_view) return true;

  HANDLE map = CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      nullptr,
      PAGE_READWRITE,
      0,
      (DWORD)sizeof(Theme1C_State),
      THEME1C_STATE_MAPPING_NAME);
  if (!map) return false;

  void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Theme1C_State));
  if (!view) {
    CloseHandle(map);
    return false;
  }

  g_mapping = map;
  g_view = (Theme1C_State*)view;

  /* First open: initialize if magic missing. */
  if (g_view->magic != THEME1C_STATE_MAGIC || g_view->version != THEME1C_STATE_VERSION) {
    Theme1C_State init;
    Theme1C_State_FillDarkPlus(&init);
    init.generation = 0;
    ThemeState_Publish(&init);
  }
  return true;
}

void ThemeState_CloseWriter() {
  if (g_view) {
    UnmapViewOfFile(g_view);
    g_view = nullptr;
  }
  if (g_mapping) {
    CloseHandle(g_mapping);
    g_mapping = nullptr;
  }
}

bool ThemeState_IsOpen() {
  return g_view != nullptr;
}

void ThemeState_Publish(const Theme1C_State* state) {
  if (!state) return;
  if (!g_view && !ThemeState_OpenWriter()) return;

  unsigned int gen = g_view->generation;
  /* seqlock: odd = write in progress */
  unsigned int nextOdd = (gen & ~1u) + 1u;
  MemoryBarrier();
  g_view->generation = nextOdd;
  MemoryBarrier();

  Theme1C_State copy = *state;
  copy.magic = THEME1C_STATE_MAGIC;
  copy.version = THEME1C_STATE_VERSION;
  /* Preserve odd generation during payload write; bump to even after. */
  copy.generation = nextOdd;

  /* Copy fields except generation first via byte copy of body, then finalize. */
  g_view->magic = copy.magic;
  g_view->version = copy.version;
  g_view->enabled = copy.enabled;
  g_view->matchKeyword = copy.matchKeyword;
  g_view->matchString = copy.matchString;
  g_view->matchComment = copy.matchComment;
  g_view->matchIdentifier = copy.matchIdentifier;
  g_view->matchDirective = copy.matchDirective;
  g_view->outKeyword = copy.outKeyword;
  g_view->outString = copy.outString;
  g_view->outComment = copy.outComment;
  g_view->outIdentifier = copy.outIdentifier;
  g_view->outDirective = copy.outDirective;
  g_view->editorBg = copy.editorBg;
  g_view->sidebar = copy.sidebar;
  g_view->menu = copy.menu;
  g_view->text = copy.text;
  g_view->captionGray = copy.captionGray;
  g_view->legacyEditorBg = copy.legacyEditorBg;

  MemoryBarrier();
  g_view->generation = nextOdd + 1u; /* even = stable */
  MemoryBarrier();
}

void ThemeState_PublishDarkPlus(unsigned int enabled) {
  Theme1C_State s;
  Theme1C_State_FillDarkPlus(&s);
  s.enabled = enabled ? 1u : 0u;
  ThemeState_Publish(&s);
}
