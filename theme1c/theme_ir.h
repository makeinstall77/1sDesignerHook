#pragma once

#include "theme_state.h"

#include <string>

/* Theme1C IR — host-side CRT OK. schemaVersion 1. */

enum ThemeIrSourceKind {
  ThemeIrSource_Builtin = 0,
  ThemeIrSource_Custom = 1,
  ThemeIrSource_Imported = 2
};

struct ThemeIrRgb {
  unsigned char r, g, b;
};

struct ThemeIrSyntax {
  char role[32]; /* keyword|string|comment|identifier|directive */
  ThemeIrRgb matchRgb;
  ThemeIrRgb outRgb;
};

struct ThemeIr {
  int schemaVersion;
  std::wstring id;
  std::wstring displayName;
  std::wstring source;       /* builtin|custom|imported */
  std::wstring sourceFormat; /* e.g. vscode, theme1c */
  std::wstring sourcePath;   /* optional */
  ThemeIrSyntax syntax[8];
  int syntaxCount;
  ThemeIrRgb editorBg;
  ThemeIrRgb sidebar;
  ThemeIrRgb menu;
  ThemeIrRgb text;
  ThemeIrRgb captionGray;
};

bool ThemeIr_LoadFile(const wchar_t* path, ThemeIr* out, std::wstring* error);
bool ThemeIr_SaveFile(const wchar_t* path, const ThemeIr* ir, std::wstring* error);
void ThemeIr_ToState(const ThemeIr* ir, Theme1C_State* state);
void ThemeIr_FromState(const Theme1C_State* state, ThemeIr* ir);
void ThemeIr_LoadBuiltinDarkPlus(ThemeIr* ir);

/* Helpers */
ThemeIrRgb ThemeIr_Rgb(unsigned char r, unsigned char g, unsigned char b);
const wchar_t* ThemeIr_SourceKindName(ThemeIrSourceKind kind);
ThemeIrSourceKind ThemeIr_ParseSourceKind(const wchar_t* s);
