#pragma once

#include <cstddef>

/* %AppData%\\Theme1C layout helpers. CRT OK — host only. */

bool Theme1C_GetRootDir(wchar_t* out, size_t capChars);
bool Theme1C_GetConfigPath(wchar_t* out, size_t capChars);
bool Theme1C_GetThemesDir(wchar_t* out, size_t capChars);
bool Theme1C_GetThemePath(const wchar_t* themeId, wchar_t* out, size_t capChars);
bool Theme1C_GetBuiltinDarkPlusRepoPath(wchar_t* out, size_t capChars);

bool Theme1C_EnsureDir(const wchar_t* path);
bool Theme1C_EnsureAppDataLayout();
