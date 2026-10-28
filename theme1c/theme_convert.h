#pragma once

#include "theme_ir.h"

#include <string>

struct ThemeConvertResult {
  bool ok;
  std::wstring error;
};

struct IThemeConverter {
  virtual ~IThemeConverter() {}
  virtual const wchar_t* FormatId() const = 0;
  virtual const wchar_t* DisplayName() const = 0;
  virtual bool CanParse(const wchar_t* path) const = 0;
  virtual bool Convert(const wchar_t* path, ThemeIr* out, std::wstring* error) const = 0;
};

const IThemeConverter** ThemeConvert_List(int* count);
const IThemeConverter* ThemeConvert_FindForFile(const wchar_t* path);
ThemeConvertResult ThemeConvert_File(const wchar_t* path, ThemeIr* out);
