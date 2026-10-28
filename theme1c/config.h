#pragma once

#include <string>

struct Theme1C_Config {
  bool hooksEnabled;
  std::wstring activeThemeId;
  std::wstring oneCStartPath;
  bool autoInject;
  bool launchStartOnBoot;
};

void Theme1C_Config_Defaults(Theme1C_Config* cfg);
bool Theme1C_Config_Load(Theme1C_Config* cfg, std::wstring* error);
bool Theme1C_Config_Save(const Theme1C_Config* cfg, std::wstring* error);
bool Theme1C_Config_LoadFrom(const wchar_t* path, Theme1C_Config* cfg, std::wstring* error);
bool Theme1C_Config_SaveTo(const wchar_t* path, const Theme1C_Config* cfg, std::wstring* error);
