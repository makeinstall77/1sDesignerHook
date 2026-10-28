#include "theme_convert.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

/* ---- shared tiny JSONC (subset) for VS Code themes ---- */

enum VKind { VNull, VBool, VNum, VStr, VArr, VObj };

struct VVal {
  VKind kind = VNull;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<VVal> arr;
  std::vector<std::pair<std::string, VVal>> obj;
};

struct VParse {
  const char* p;
  const char* end;
  std::string err;

  explicit VParse(const std::string& s) : p(s.data()), end(s.data() + s.size()) {}

  void Skip() {
    for (;;) {
      while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
      if (p + 1 < end && p[0] == '/' && p[1] == '/') {
        while (p < end && *p != '\n') p++;
        continue;
      }
      if (p + 1 < end && p[0] == '/' && p[1] == '*') {
        p += 2;
        while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) p++;
        if (p + 1 < end) p += 2;
        continue;
      }
      break;
    }
  }

  bool Fail(const char* m) { err = m; return false; }

  bool ParseString(std::string* out) {
    if (p >= end || *p != '"') return Fail("expected string");
    p++;
    out->clear();
    while (p < end) {
      char c = *p++;
      if (c == '"') return true;
      if (c == '\\' && p < end) {
        char e = *p++;
        if (e == 'u') {
          unsigned code = 0;
          for (int i = 0; i < 4; i++) {
            if (p >= end) return Fail("bad \\u");
            char h = *p++;
            code <<= 4;
            if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
            else return Fail("bad \\u");
          }
          if (code < 0x80) out->push_back((char)code);
          else if (code < 0x800) {
            out->push_back((char)(0xC0 | (code >> 6)));
            out->push_back((char)(0x80 | (code & 0x3F)));
          } else {
            out->push_back((char)(0xE0 | (code >> 12)));
            out->push_back((char)(0x80 | ((code >> 6) & 0x3F)));
            out->push_back((char)(0x80 | (code & 0x3F)));
          }
        } else if (e == 'n') out->push_back('\n');
        else if (e == 'r') out->push_back('\r');
        else if (e == 't') out->push_back('\t');
        else out->push_back(e);
      } else {
        out->push_back(c);
      }
    }
    return Fail("unterminated string");
  }

  bool ParseValue(VVal* v);

  bool ParseObject(VVal* v) {
    v->kind = VObj;
    if (p >= end || *p != '{') return Fail("expected '{'");
    p++;
    Skip();
    if (p < end && *p == '}') { p++; return true; }
    for (;;) {
      Skip();
      /* trailing comma */
      if (p < end && *p == '}') { p++; return true; }
      std::string key;
      if (!ParseString(&key)) return false;
      Skip();
      if (p >= end || *p != ':') return Fail("expected ':'");
      p++;
      VVal child;
      if (!ParseValue(&child)) return false;
      v->obj.emplace_back(std::move(key), std::move(child));
      Skip();
      if (p < end && *p == ',') { p++; Skip(); continue; }
      if (p < end && *p == '}') { p++; return true; }
      return Fail("expected ',' or '}'");
    }
  }

  bool ParseArray(VVal* v) {
    v->kind = VArr;
    if (p >= end || *p != '[') return Fail("expected '['");
    p++;
    Skip();
    if (p < end && *p == ']') { p++; return true; }
    for (;;) {
      Skip();
      if (p < end && *p == ']') { p++; return true; }
      VVal child;
      if (!ParseValue(&child)) return false;
      v->arr.push_back(std::move(child));
      Skip();
      if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == ']') { p++; return true; }
      return Fail("expected ',' or ']'");
    }
  }

  bool ParseNumber(VVal* v) {
    const char* start = p;
    if (p < end && (*p == '-' || *p == '+')) p++;
    while (p < end && *p >= '0' && *p <= '9') p++;
    if (p < end && *p == '.') {
      p++;
      while (p < end && *p >= '0' && *p <= '9') p++;
    }
    v->kind = VNum;
    v->num = atof(std::string(start, p).c_str());
    return true;
  }
};

bool VParse::ParseValue(VVal* v) {
  Skip();
  if (p >= end) return Fail("EOF");
  if (*p == '{') return ParseObject(v);
  if (*p == '[') return ParseArray(v);
  if (*p == '"') { v->kind = VStr; return ParseString(&v->str); }
  if (*p == 't' && p + 4 <= end && strncmp(p, "true", 4) == 0) { p += 4; v->kind = VBool; v->b = true; return true; }
  if (*p == 'f' && p + 5 <= end && strncmp(p, "false", 5) == 0) { p += 5; v->kind = VBool; v->b = false; return true; }
  if (*p == 'n' && p + 4 <= end && strncmp(p, "null", 4) == 0) { p += 4; v->kind = VNull; return true; }
  if (*p == '-' || (*p >= '0' && *p <= '9')) return ParseNumber(v);
  return Fail("token");
}

static const VVal* ObjGet(const VVal& o, const char* key) {
  if (o.kind != VObj) return nullptr;
  for (const auto& kv : o.obj) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

static bool HexNibble(char c, unsigned* o) {
  if (c >= '0' && c <= '9') { *o = (unsigned)(c - '0'); return true; }
  if (c >= 'a' && c <= 'f') { *o = (unsigned)(c - 'a' + 10); return true; }
  if (c >= 'A' && c <= 'F') { *o = (unsigned)(c - 'A' + 10); return true; }
  return false;
}

static bool ParseColorString(const std::string& s, ThemeIrRgb* out) {
  size_t i = 0;
  if (!s.empty() && s[0] == '#') i = 1;
  /* #RRGGBB or #RRGGBBAA */
  size_t n = s.size() - i;
  if (n != 6 && n != 8) return false;
  unsigned v[8];
  for (size_t k = 0; k < n; k++) {
    if (!HexNibble(s[i + k], &v[k])) return false;
  }
  out->r = (unsigned char)((v[0] << 4) | v[1]);
  out->g = (unsigned char)((v[2] << 4) | v[3]);
  out->b = (unsigned char)((v[4] << 4) | v[5]);
  return true;
}

static bool ReadFileUtf8(const wchar_t* path, std::string* out) {
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER sz;
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 16 * 1024 * 1024) {
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

static std::wstring FileStem(const wchar_t* path) {
  const wchar_t* base = path;
  for (const wchar_t* p = path; *p; p++) {
    if (*p == L'\\' || *p == L'/') base = p + 1;
  }
  std::wstring name(base);
  size_t dot = name.find_last_of(L'.');
  if (dot != std::wstring::npos) name.resize(dot);
  return name;
}

static bool ContainsI(const std::string& hay, const char* needle) {
  size_t n = std::strlen(needle);
  if (n == 0 || hay.size() < n) return false;
  for (size_t i = 0; i + n <= hay.size(); i++) {
    bool ok = true;
    for (size_t j = 0; j < n; j++) {
      char a = hay[i + j];
      char b = needle[j];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) { ok = false; break; }
    }
    if (ok) return true;
  }
  return false;
}

static void CollectScopes(const VVal& scopeVal, std::vector<std::string>* scopes) {
  if (scopeVal.kind == VStr) {
    scopes->push_back(scopeVal.str);
  } else if (scopeVal.kind == VArr) {
    for (const VVal& s : scopeVal.arr) {
      if (s.kind == VStr) scopes->push_back(s.str);
    }
  }
}

static int MapScopeToRole(const std::string& scope) {
  /* Returns: 0 keyword, 1 string, 2 comment, 3 identifier, 4 directive, -1 none
   * Spec: keyword / string / comment / variable|entity.name / storage|keyword.control */
  if (ContainsI(scope, "comment")) return 2;
  if (ContainsI(scope, "string")) return 1;
  if (ContainsI(scope, "directive") || ContainsI(scope, "preprocessor")) return 4;
  if (ContainsI(scope, "storage") || ContainsI(scope, "keyword.control")) return 4;
  if (ContainsI(scope, "keyword") && !ContainsI(scope, "keyword.operator")) return 0;
  if (ContainsI(scope, "variable") || ContainsI(scope, "entity.name")) return 3;
  return -1;
}

class VsCodeThemeConverter : public IThemeConverter {
public:
  const wchar_t* FormatId() const override { return L"vscode"; }
  const wchar_t* DisplayName() const override { return L"VS Code Color Theme"; }

  bool CanParse(const wchar_t* path) const override {
    std::string text;
    if (!ReadFileUtf8(path, &text)) return false;
    /* Quick scan before full parse */
    if (text.find("\"colors\"") == std::string::npos &&
        text.find("\"tokenColors\"") == std::string::npos)
      return false;
    VParse jp(text);
    VVal root;
    if (!jp.ParseValue(&root) || root.kind != VObj) return false;
    return ObjGet(root, "colors") != nullptr || ObjGet(root, "tokenColors") != nullptr;
  }

  bool Convert(const wchar_t* path, ThemeIr* out, std::wstring* error) const override {
    if (!path || !out) {
      if (error) *error = L"Invalid arguments";
      return false;
    }
    std::string text;
    if (!ReadFileUtf8(path, &text)) {
      if (error) *error = L"Cannot read theme file";
      return false;
    }
    VParse jp(text);
    VVal root;
    if (!jp.ParseValue(&root) || root.kind != VObj) {
      if (error) *error = L"Invalid VS Code theme JSON";
      return false;
    }
    if (ObjGet(root, "include")) {
      if (error)
        *error = L"Theme has \"include\" — resolve include first (v1 does not merge includes)";
      return false;
    }

    ThemeIr_LoadBuiltinDarkPlus(out);
    out->source = L"imported";
    out->sourceFormat = L"vscode";
    out->sourcePath = path;
    out->id = FileStem(path);
    out->displayName = out->id;
    if (const VVal* name = ObjGet(root, "name")) {
      if (name->kind == VStr && !name->str.empty()) {
        int n = MultiByteToWideChar(CP_UTF8, 0, name->str.c_str(), (int)name->str.size(), nullptr, 0);
        if (n > 0) {
          out->displayName.assign(n, L'\0');
          MultiByteToWideChar(CP_UTF8, 0, name->str.c_str(), (int)name->str.size(), &out->displayName[0], n);
        }
      }
    }

    /* Chrome from colors */
    if (const VVal* colors = ObjGet(root, "colors")) {
      if (colors->kind == VObj) {
        auto take = [&](const char* key, ThemeIrRgb* dst) {
          if (const VVal* v = ObjGet(*colors, key)) {
            if (v->kind == VStr) {
              ThemeIrRgb c;
              if (ParseColorString(v->str, &c)) *dst = c;
            }
          }
        };
        take("editor.background", &out->editorBg);
        take("sideBar.background", &out->sidebar);
        bool haveMenu = false;
        if (const VVal* mv = ObjGet(*colors, "menu.background")) {
          if (mv->kind == VStr) {
            ThemeIrRgb c;
            if (ParseColorString(mv->str, &c)) {
              out->menu = c;
              haveMenu = true;
            }
          }
        }
        if (!haveMenu) take("titleBar.activeBackground", &out->menu);
        take("editor.foreground", &out->text);
        out->captionGray = out->sidebar;
      }
    }

    /* tokenColors → syntax outs; match stays Dark+ defaults */
    bool got[5] = {false, false, false, false, false};
    ThemeIrRgb outs[5] = {
        out->syntax[0].outRgb, out->syntax[1].outRgb, out->syntax[2].outRgb,
        out->syntax[3].outRgb, out->syntax[4].outRgb};

    if (const VVal* tokens = ObjGet(root, "tokenColors")) {
      if (tokens->kind == VArr) {
        for (const VVal& item : tokens->arr) {
          if (item.kind != VObj) continue;
          const VVal* settings = ObjGet(item, "settings");
          if (!settings || settings->kind != VObj) continue;
          const VVal* fg = ObjGet(*settings, "foreground");
          if (!fg || fg->kind != VStr) continue;
          ThemeIrRgb c;
          if (!ParseColorString(fg->str, &c)) continue;

          std::vector<std::string> scopes;
          if (const VVal* sc = ObjGet(item, "scope")) CollectScopes(*sc, &scopes);
          for (const std::string& scope : scopes) {
            int role = MapScopeToRole(scope);
            if (role < 0 || role > 4) continue;
            if (!got[role]) {
              outs[role] = c;
              got[role] = true;
            }
          }
        }
      }
    }

    /* Ensure 5 syntax rows with Dark+ match + mapped/default out */
    static const char* roles[5] = {"keyword", "string", "comment", "identifier", "directive"};
    ThemeIrRgb matches[5] = {
        ThemeIr_Rgb(210, 76, 21), ThemeIr_Rgb(212, 47, 51), ThemeIr_Rgb(137, 154, 51),
        ThemeIr_Rgb(37, 139, 211), ThemeIr_Rgb(213, 53, 132)};
    ThemeIrRgb darkOut[5] = {
        ThemeIr_Rgb(197, 134, 192), ThemeIr_Rgb(206, 145, 120), ThemeIr_Rgb(106, 153, 85),
        ThemeIr_Rgb(156, 220, 254), ThemeIr_Rgb(86, 156, 214)};
    out->syntaxCount = 5;
    for (int i = 0; i < 5; i++) {
      std::snprintf(out->syntax[i].role, sizeof(out->syntax[i].role), "%s", roles[i]);
      out->syntax[i].matchRgb = matches[i];
      out->syntax[i].outRgb = got[i] ? outs[i] : darkOut[i];
    }
    return true;
  }
};

static VsCodeThemeConverter g_vscode;
static const IThemeConverter* g_converters[] = {&g_vscode};

const IThemeConverter** ThemeConvert_List(int* count) {
  if (count) *count = (int)(sizeof(g_converters) / sizeof(g_converters[0]));
  return g_converters;
}

const IThemeConverter* ThemeConvert_FindForFile(const wchar_t* path) {
  if (!path) return nullptr;
  int n = 0;
  const IThemeConverter** list = ThemeConvert_List(&n);
  for (int i = 0; i < n; i++) {
    if (list[i]->CanParse(path)) return list[i];
  }
  return nullptr;
}

ThemeConvertResult ThemeConvert_File(const wchar_t* path, ThemeIr* out) {
  ThemeConvertResult r;
  r.ok = false;
  const IThemeConverter* conv = ThemeConvert_FindForFile(path);
  if (!conv) {
    r.error = L"No converter can parse this file";
    return r;
  }
  std::wstring err;
  if (!conv->Convert(path, out, &err)) {
    r.error = err.empty() ? L"Conversion failed" : err;
    return r;
  }
  r.ok = true;
  return r;
}
