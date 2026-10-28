#pragma once

#include "theme_state.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline Theme1C_Rgb Theme1C_RgbMake(unsigned char r, unsigned char g, unsigned char b) {
  Theme1C_Rgb c;
  c.r = r;
  c.g = g;
  c.b = b;
  c.a = 0;
  return c;
}

/* Dark+ defaults matching current cairo1c.cpp MapSyntax / chrome. */
static inline void Theme1C_State_FillDarkPlus(Theme1C_State* s) {
  if (!s) return;
  s->magic = THEME1C_STATE_MAGIC;
  s->version = THEME1C_STATE_VERSION;
  s->generation = 0;
  s->enabled = 1;

  s->matchKeyword = Theme1C_RgbMake(210, 76, 21);
  s->matchString = Theme1C_RgbMake(212, 47, 51);
  s->matchComment = Theme1C_RgbMake(137, 154, 51);
  s->matchIdentifier = Theme1C_RgbMake(37, 139, 211);
  s->matchDirective = Theme1C_RgbMake(213, 53, 132);

  s->outKeyword = Theme1C_RgbMake(197, 134, 192);
  s->outString = Theme1C_RgbMake(206, 145, 120);
  s->outComment = Theme1C_RgbMake(106, 153, 85);
  s->outIdentifier = Theme1C_RgbMake(156, 220, 254);
  s->outDirective = Theme1C_RgbMake(86, 156, 214);

  s->editorBg = Theme1C_RgbMake(30, 30, 30);
  s->sidebar = Theme1C_RgbMake(37, 37, 38);
  s->menu = Theme1C_RgbMake(51, 51, 51);
  s->text = Theme1C_RgbMake(212, 212, 212);
  s->captionGray = Theme1C_RgbMake(37, 37, 38);
  s->legacyEditorBg = Theme1C_RgbMake(0, 43, 54);
}

#ifdef __cplusplus
}
#endif
