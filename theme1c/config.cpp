#include "config.h"
#include "appdata.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

void Theme1C_Config_Defaults(Theme1C_Config* cfg) {
  if (!cfg) return;
  cfg->hooksEnabled = true;
  cfg->activeThemeId = L"dark-plus";
  cfg->oneCStartPath.clear();
  cfg->autoInject = true;
  cfg->launchStartOnBoot = true;
}

static std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  if (n <= 0) return std::string();
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, nullptr, nullptr);
  return out;
}

static std::wstring Utf8ToWide(const std::string& u) {
  if (u.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), &out[0], n);
  return out;
}

static std::string EscapeJson(const std::string& s) {
  std::string o;
  for (unsigned char c : s) {
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", c);
      o += buf;
    } else o.push_back((char)c);
  }
  return o;
}

static bool ReadAll(const wchar_t* path, std::string* out) {
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER sz;
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 1024 * 1024) {
    CloseHandle(h);
    return false;
  }
  std::string buf((size_t)sz.QuadPart, '\0');
  DWORD read = 0;
  BOOL ok = ReadFile(h, buf.empty() ? nullptr : &buf[0], (DWORD)buf.size(), &read, nullptr);
  CloseHandle(h);
  if (!ok) return false;
  buf.resize(read);
  if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
    buf.erase(0, 3);
  *out = std::move(buf);
  return true;
}

static bool WriteAll(const wchar_t* path, const std::string& data) {
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
  CloseHandle(h);
  return ok && written == data.size();
}

/* Tiny key finder for flat config object — enough for our schema. */
static bool FindStringKey(const std::string& json, const char* key, std::string* value) {
  std::string needle = std::string("\"") + key + "\"";
  size_t pos = 0;
  for (;;) {
    pos = json.find(needle, pos);
    if (pos == std::string::npos) return false;
    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;
    size_t i = colon + 1;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n')) i++;
    if (i >= json.size() || json[i] != '"') {
      pos++;
      continue;
    }
    i++;
    std::string out;
    while (i < json.size()) {
      char c = json[i++];
      if (c == '"') {
        *value = out;
        return true;
      }
      if (c == '\\' && i < json.size()) {
        char e = json[i++];
        if (e == 'n') out.push_back('\n');
        else if (e == 'r') out.push_back('\r');
        else if (e == 't') out.push_back('\t');
        else out.push_back(e);
      } else {
        out.push_back(c);
      }
    }
    return false;
  }
}

static bool FindBoolKey(const std::string& json, const char* key, bool* value) {
  std::string needle = std::string("\"") + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return false;
  size_t colon = json.find(':', pos + needle.size());
  if (colon == std::string::npos) return false;
  size_t i = colon + 1;
  while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n')) i++;
  if (i + 4 <= json.size() && json.compare(i, 4, "true") == 0) {
    *value = true;
    return true;
  }
  if (i + 5 <= json.size() && json.compare(i, 5, "false") == 0) {
    *value = false;
    return true;
  }
  return false;
}

bool Theme1C_Config_LoadFrom(const wchar_t* path, Theme1C_Config* cfg, std::wstring* error) {
  if (!cfg) {
    if (error) *error = L"Invalid config pointer";
    return false;
  }
  Theme1C_Config_Defaults(cfg);
  std::string text;
  if (!ReadAll(path, &text)) {
    /* Missing file → defaults OK */
    return true;
  }
  bool b;
  std::string s;
  if (FindBoolKey(text, "hooksEnabled", &b)) cfg->hooksEnabled = b;
  if (FindStringKey(text, "activeThemeId", &s)) cfg->activeThemeId = Utf8ToWide(s);
  if (FindStringKey(text, "oneCStartPath", &s)) cfg->oneCStartPath = Utf8ToWide(s);
  if (FindBoolKey(text, "autoInject", &b)) cfg->autoInject = b;
  if (FindBoolKey(text, "launchStartOnBoot", &b)) cfg->launchStartOnBoot = b;
  return true;
}

bool Theme1C_Config_SaveTo(const wchar_t* path, const Theme1C_Config* cfg, std::wstring* error) {
  if (!cfg || !path) {
    if (error) *error = L"Invalid arguments";
    return false;
  }
  std::string json;
  json += "{\n";
  json += "  \"hooksEnabled\": ";
  json += cfg->hooksEnabled ? "true" : "false";
  json += ",\n";
  json += "  \"activeThemeId\": \"";
  json += EscapeJson(WideToUtf8(cfg->activeThemeId));
  json += "\",\n";
  json += "  \"oneCStartPath\": \"";
  json += EscapeJson(WideToUtf8(cfg->oneCStartPath));
  json += "\",\n";
  json += "  \"autoInject\": ";
  json += cfg->autoInject ? "true" : "false";
  json += ",\n";
  json += "  \"launchStartOnBoot\": ";
  json += cfg->launchStartOnBoot ? "true" : "false";
  json += "\n}\n";
  if (!WriteAll(path, json)) {
    if (error) *error = L"Failed to write config.json";
    return false;
  }
  return true;
}

bool Theme1C_Config_Load(Theme1C_Config* cfg, std::wstring* error) {
  wchar_t path[MAX_PATH * 2];
  if (!Theme1C_GetConfigPath(path, sizeof(path) / sizeof(path[0]))) {
    if (error) *error = L"Cannot resolve config path";
    return false;
  }
  return Theme1C_Config_LoadFrom(path, cfg, error);
}

bool Theme1C_Config_Save(const Theme1C_Config* cfg, std::wstring* error) {
  wchar_t root[MAX_PATH * 2];
  wchar_t path[MAX_PATH * 2];
  if (!Theme1C_GetRootDir(root, sizeof(root) / sizeof(root[0])) || !Theme1C_EnsureDir(root)) {
    if (error) *error = L"Cannot create Theme1C AppData directory";
    return false;
  }
  if (!Theme1C_GetConfigPath(path, sizeof(path) / sizeof(path[0]))) {
    if (error) *error = L"Cannot resolve config path";
    return false;
  }
  return Theme1C_Config_SaveTo(path, cfg, error);
}
