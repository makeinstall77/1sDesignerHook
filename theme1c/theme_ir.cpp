#include "theme_ir.h"
#include "theme_state_util.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <windows.h>

/* ---- small helpers ---- */

ThemeIrRgb ThemeIr_Rgb(unsigned char r, unsigned char g, unsigned char b) {
  ThemeIrRgb c;
  c.r = r;
  c.g = g;
  c.b = b;
  return c;
}

static Theme1C_Rgb ToStateRgb(ThemeIrRgb c) {
  return Theme1C_RgbMake(c.r, c.g, c.b);
}

static ThemeIrRgb FromStateRgb(Theme1C_Rgb c) {
  return ThemeIr_Rgb(c.r, c.g, c.b);
}

const wchar_t* ThemeIr_SourceKindName(ThemeIrSourceKind kind) {
  switch (kind) {
    case ThemeIrSource_Builtin: return L"builtin";
    case ThemeIrSource_Custom: return L"custom";
    case ThemeIrSource_Imported: return L"imported";
    default: return L"custom";
  }
}

ThemeIrSourceKind ThemeIr_ParseSourceKind(const wchar_t* s) {
  if (!s) return ThemeIrSource_Custom;
  if (_wcsicmp(s, L"builtin") == 0) return ThemeIrSource_Builtin;
  if (_wcsicmp(s, L"imported") == 0) return ThemeIrSource_Imported;
  return ThemeIrSource_Custom;
}

static void SetRole(ThemeIrSyntax* sx, const char* role, ThemeIrRgb match, ThemeIrRgb out) {
  std::snprintf(sx->role, sizeof(sx->role), "%s", role);
  sx->matchRgb = match;
  sx->outRgb = out;
}

void ThemeIr_LoadBuiltinDarkPlus(ThemeIr* ir) {
  if (!ir) return;
  ir->schemaVersion = 1;
  ir->id = L"dark-plus";
  ir->displayName = L"Dark+";
  ir->source = L"builtin";
  ir->sourceFormat = L"theme1c";
  ir->sourcePath.clear();
  ir->syntaxCount = 5;
  SetRole(&ir->syntax[0], "keyword", ThemeIr_Rgb(210, 76, 21), ThemeIr_Rgb(197, 134, 192));
  SetRole(&ir->syntax[1], "string", ThemeIr_Rgb(212, 47, 51), ThemeIr_Rgb(206, 145, 120));
  SetRole(&ir->syntax[2], "comment", ThemeIr_Rgb(137, 154, 51), ThemeIr_Rgb(106, 153, 85));
  SetRole(&ir->syntax[3], "identifier", ThemeIr_Rgb(37, 139, 211), ThemeIr_Rgb(156, 220, 254));
  SetRole(&ir->syntax[4], "directive", ThemeIr_Rgb(213, 53, 132), ThemeIr_Rgb(86, 156, 214));
  ir->editorBg = ThemeIr_Rgb(30, 30, 30);
  ir->sidebar = ThemeIr_Rgb(37, 37, 38);
  ir->menu = ThemeIr_Rgb(51, 51, 51);
  ir->text = ThemeIr_Rgb(212, 212, 212);
  ir->captionGray = ThemeIr_Rgb(37, 37, 38);
}

static const ThemeIrSyntax* FindRole(const ThemeIr* ir, const char* role) {
  for (int i = 0; i < ir->syntaxCount; i++) {
    if (_stricmp(ir->syntax[i].role, role) == 0) return &ir->syntax[i];
  }
  return nullptr;
}

void ThemeIr_ToState(const ThemeIr* ir, Theme1C_State* state) {
  if (!state) return;
  Theme1C_State_FillDarkPlus(state);
  if (!ir) return;

  state->enabled = 1;
  const ThemeIrSyntax* s;
  if ((s = FindRole(ir, "keyword"))) {
    state->matchKeyword = ToStateRgb(s->matchRgb);
    state->outKeyword = ToStateRgb(s->outRgb);
  }
  if ((s = FindRole(ir, "string"))) {
    state->matchString = ToStateRgb(s->matchRgb);
    state->outString = ToStateRgb(s->outRgb);
  }
  if ((s = FindRole(ir, "comment"))) {
    state->matchComment = ToStateRgb(s->matchRgb);
    state->outComment = ToStateRgb(s->outRgb);
  }
  if ((s = FindRole(ir, "identifier"))) {
    state->matchIdentifier = ToStateRgb(s->matchRgb);
    state->outIdentifier = ToStateRgb(s->outRgb);
  }
  if ((s = FindRole(ir, "directive"))) {
    state->matchDirective = ToStateRgb(s->matchRgb);
    state->outDirective = ToStateRgb(s->outRgb);
  }
  state->editorBg = ToStateRgb(ir->editorBg);
  state->sidebar = ToStateRgb(ir->sidebar);
  state->menu = ToStateRgb(ir->menu);
  state->text = ToStateRgb(ir->text);
  state->captionGray = ToStateRgb(ir->captionGray);
}

void ThemeIr_FromState(const Theme1C_State* state, ThemeIr* ir) {
  if (!ir) return;
  ThemeIr_LoadBuiltinDarkPlus(ir);
  if (!state) return;
  ir->source = L"custom";
  ir->sourceFormat = L"theme1c";
  ir->syntaxCount = 5;
  SetRole(&ir->syntax[0], "keyword", FromStateRgb(state->matchKeyword), FromStateRgb(state->outKeyword));
  SetRole(&ir->syntax[1], "string", FromStateRgb(state->matchString), FromStateRgb(state->outString));
  SetRole(&ir->syntax[2], "comment", FromStateRgb(state->matchComment), FromStateRgb(state->outComment));
  SetRole(&ir->syntax[3], "identifier", FromStateRgb(state->matchIdentifier), FromStateRgb(state->outIdentifier));
  SetRole(&ir->syntax[4], "directive", FromStateRgb(state->matchDirective), FromStateRgb(state->outDirective));
  ir->editorBg = FromStateRgb(state->editorBg);
  ir->sidebar = FromStateRgb(state->sidebar);
  ir->menu = FromStateRgb(state->menu);
  ir->text = FromStateRgb(state->text);
  ir->captionGray = FromStateRgb(state->captionGray);
}

/* ---- UTF-8 / wchar helpers ---- */

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

static bool ReadAllFileUtf8(const wchar_t* path, std::string* out, std::wstring* error) {
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (error) *error = L"Cannot open theme file";
    return false;
  }
  LARGE_INTEGER sz;
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 8 * 1024 * 1024) {
    CloseHandle(h);
    if (error) *error = L"Theme file too large or unreadable";
    return false;
  }
  std::string buf((size_t)sz.QuadPart, '\0');
  DWORD read = 0;
  BOOL ok = ReadFile(h, buf.empty() ? nullptr : &buf[0], (DWORD)buf.size(), &read, nullptr);
  CloseHandle(h);
  if (!ok) {
    if (error) *error = L"Failed to read theme file";
    return false;
  }
  buf.resize(read);
  if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
    buf.erase(0, 3);
  *out = std::move(buf);
  return true;
}

static bool WriteAllFileUtf8(const wchar_t* path, const std::string& data, std::wstring* error) {
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (error) *error = L"Cannot write theme file";
    return false;
  }
  DWORD written = 0;
  BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
  CloseHandle(h);
  if (!ok || written != data.size()) {
    if (error) *error = L"Failed to write theme file";
    return false;
  }
  return true;
}

/* ---- minimal JSON subset parser for Theme1C IR ---- */

struct JVal;

enum JKind { JNull, JBool, JNum, JStr, JArr, JObj };

struct JVal {
  JKind kind = JNull;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<JVal> arr;
  std::vector<std::pair<std::string, JVal>> obj;
};

struct JParse {
  const char* p;
  const char* end;
  std::string err;

  explicit JParse(const std::string& s) : p(s.data()), end(s.data() + s.size()) {}

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

  bool Fail(const char* m) {
    err = m;
    return false;
  }

  bool ParseString(std::string* out) {
    if (p >= end || *p != '"') return Fail("expected string");
    p++;
    out->clear();
    while (p < end) {
      char c = *p++;
      if (c == '"') return true;
      if (c == '\\' && p < end) {
        char e = *p++;
        switch (e) {
          case '"':
          case '\\':
          case '/': out->push_back(e); break;
          case 'b': out->push_back('\b'); break;
          case 'f': out->push_back('\f'); break;
          case 'n': out->push_back('\n'); break;
          case 'r': out->push_back('\r'); break;
          case 't': out->push_back('\t'); break;
          case 'u': {
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
            if (code < 0x80) {
              out->push_back((char)code);
            } else if (code < 0x800) {
              out->push_back((char)(0xC0 | (code >> 6)));
              out->push_back((char)(0x80 | (code & 0x3F)));
            } else {
              out->push_back((char)(0xE0 | (code >> 12)));
              out->push_back((char)(0x80 | ((code >> 6) & 0x3F)));
              out->push_back((char)(0x80 | (code & 0x3F)));
            }
            break;
          }
          default: return Fail("bad escape");
        }
      } else {
        out->push_back(c);
      }
    }
    return Fail("unterminated string");
  }

  bool ParseValue(JVal* v);

  bool ParseObject(JVal* v) {
    v->kind = JObj;
    if (p >= end || *p != '{') return Fail("expected '{'");
    p++;
    Skip();
    if (p < end && *p == '}') { p++; return true; }
    for (;;) {
      Skip();
      std::string key;
      if (!ParseString(&key)) return false;
      Skip();
      if (p >= end || *p != ':') return Fail("expected ':'");
      p++;
      JVal child;
      if (!ParseValue(&child)) return false;
      v->obj.emplace_back(std::move(key), std::move(child));
      Skip();
      if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == '}') { p++; return true; }
      return Fail("expected ',' or '}'");
    }
  }

  bool ParseArray(JVal* v) {
    v->kind = JArr;
    if (p >= end || *p != '[') return Fail("expected '['");
    p++;
    Skip();
    if (p < end && *p == ']') { p++; return true; }
    for (;;) {
      JVal child;
      if (!ParseValue(&child)) return false;
      v->arr.push_back(std::move(child));
      Skip();
      if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == ']') { p++; return true; }
      return Fail("expected ',' or ']'");
    }
  }

  bool ParseNumber(JVal* v) {
    const char* start = p;
    if (p < end && (*p == '-' || *p == '+')) p++;
    while (p < end && *p >= '0' && *p <= '9') p++;
    if (p < end && *p == '.') {
      p++;
      while (p < end && *p >= '0' && *p <= '9') p++;
    }
    v->kind = JNum;
    v->num = atof(std::string(start, p).c_str());
    return true;
  }
};

bool JParse::ParseValue(JVal* v) {
  Skip();
  if (p >= end) return Fail("unexpected EOF");
  if (*p == '{') return ParseObject(v);
  if (*p == '[') return ParseArray(v);
  if (*p == '"') {
    v->kind = JStr;
    return ParseString(&v->str);
  }
  if (*p == 't' && p + 4 <= end && std::strncmp(p, "true", 4) == 0) {
    p += 4; v->kind = JBool; v->b = true; return true;
  }
  if (*p == 'f' && p + 5 <= end && std::strncmp(p, "false", 5) == 0) {
    p += 5; v->kind = JBool; v->b = false; return true;
  }
  if (*p == 'n' && p + 4 <= end && std::strncmp(p, "null", 4) == 0) {
    p += 4; v->kind = JNull; return true;
  }
  if (*p == '-' || (*p >= '0' && *p <= '9')) return ParseNumber(v);
  return Fail("unexpected token");
}

static const JVal* ObjGet(const JVal& o, const char* key) {
  if (o.kind != JObj) return nullptr;
  for (const auto& kv : o.obj) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

static bool HexDigit(char c, unsigned* out) {
  if (c >= '0' && c <= '9') { *out = (unsigned)(c - '0'); return true; }
  if (c >= 'a' && c <= 'f') { *out = (unsigned)(c - 'a' + 10); return true; }
  if (c >= 'A' && c <= 'F') { *out = (unsigned)(c - 'A' + 10); return true; }
  return false;
}

static bool ThemeIr_ParseRgb(const JVal& v, ThemeIrRgb* out) {
  if (v.kind == JArr && v.arr.size() >= 3 &&
      v.arr[0].kind == JNum && v.arr[1].kind == JNum && v.arr[2].kind == JNum) {
    auto clamp = [](double x) -> unsigned char {
      if (x < 0) x = 0;
      if (x > 255) x = 255;
      return (unsigned char)(x + 0.5);
    };
    out->r = clamp(v.arr[0].num);
    out->g = clamp(v.arr[1].num);
    out->b = clamp(v.arr[2].num);
    return true;
  }
  if (v.kind == JStr) {
    const std::string& s = v.str;
    size_t i = 0;
    if (!s.empty() && s[0] == '#') i = 1;
    if (s.size() - i == 6) {
      unsigned v0, v1, v2, v3, v4, v5;
      if (HexDigit(s[i], &v0) && HexDigit(s[i + 1], &v1) && HexDigit(s[i + 2], &v2) &&
          HexDigit(s[i + 3], &v3) && HexDigit(s[i + 4], &v4) && HexDigit(s[i + 5], &v5)) {
        out->r = (unsigned char)((v0 << 4) | v1);
        out->g = (unsigned char)((v2 << 4) | v3);
        out->b = (unsigned char)((v4 << 4) | v5);
        return true;
      }
    }
  }
  return false;
}

static std::string EscapeJson(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          o += buf;
        } else {
          o.push_back((char)c);
        }
    }
  }
  return o;
}

static void AppendRgbJson(std::string* out, ThemeIrRgb c) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "[%u,%u,%u]", (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
  *out += buf;
}

bool ThemeIr_LoadFile(const wchar_t* path, ThemeIr* out, std::wstring* error) {
  if (!path || !out) {
    if (error) *error = L"Invalid arguments";
    return false;
  }
  std::string text;
  if (!ReadAllFileUtf8(path, &text, error)) return false;

  JParse jp(text);
  JVal root;
  if (!jp.ParseValue(&root) || root.kind != JObj) {
    if (error) *error = Utf8ToWide(jp.err.empty() ? "Invalid JSON" : jp.err);
    return false;
  }

  ThemeIr_LoadBuiltinDarkPlus(out);

  if (const JVal* v = ObjGet(root, "schemaVersion")) {
    if (v->kind == JNum) out->schemaVersion = (int)v->num;
  }
  if (out->schemaVersion != 1) {
    if (error) *error = L"Unsupported schemaVersion (expected 1)";
    return false;
  }
  if (const JVal* v = ObjGet(root, "id")) {
    if (v->kind == JStr) out->id = Utf8ToWide(v->str);
  }
  if (const JVal* v = ObjGet(root, "displayName")) {
    if (v->kind == JStr) out->displayName = Utf8ToWide(v->str);
  }
  if (const JVal* v = ObjGet(root, "source")) {
    if (v->kind == JStr) out->source = Utf8ToWide(v->str);
  }
  if (const JVal* v = ObjGet(root, "sourceFormat")) {
    if (v->kind == JStr) out->sourceFormat = Utf8ToWide(v->str);
  }
  if (const JVal* v = ObjGet(root, "sourcePath")) {
    if (v->kind == JStr) out->sourcePath = Utf8ToWide(v->str);
  }

  if (const JVal* syn = ObjGet(root, "syntax")) {
    if (syn->kind == JArr && !syn->arr.empty()) {
      int n = 0;
      ThemeIrSyntax parsed[8];
      for (const JVal& item : syn->arr) {
        if (item.kind != JObj || n >= 8) continue;
        ThemeIrSyntax sx{};
        ThemeIrRgb match = ThemeIr_Rgb(0, 0, 0);
        ThemeIrRgb outs = ThemeIr_Rgb(0, 0, 0);
        bool haveMatch = false, haveOut = false;
        if (const JVal* role = ObjGet(item, "role")) {
          if (role->kind == JStr) {
            std::snprintf(sx.role, sizeof(sx.role), "%s", role->str.c_str());
          }
        }
        if (const JVal* m = ObjGet(item, "matchRgb")) haveMatch = ThemeIr_ParseRgb(*m, &match);
        if (const JVal* o = ObjGet(item, "outRgb")) haveOut = ThemeIr_ParseRgb(*o, &outs);
        if (!sx.role[0] || !haveMatch || !haveOut) continue;
        sx.matchRgb = match;
        sx.outRgb = outs;
        parsed[n++] = sx;
      }
      if (n > 0) {
        out->syntaxCount = n;
        for (int i = 0; i < n; i++) out->syntax[i] = parsed[i];
      }
    }
  }

  if (const JVal* chrome = ObjGet(root, "chrome")) {
    if (chrome->kind == JObj) {
      ThemeIrRgb c;
      if (const JVal* v = ObjGet(*chrome, "editorBg")) {
        if (ThemeIr_ParseRgb(*v, &c)) out->editorBg = c;
      }
      if (const JVal* v = ObjGet(*chrome, "sidebar")) {
        if (ThemeIr_ParseRgb(*v, &c)) out->sidebar = c;
      }
      if (const JVal* v = ObjGet(*chrome, "menu")) {
        if (ThemeIr_ParseRgb(*v, &c)) out->menu = c;
      }
      if (const JVal* v = ObjGet(*chrome, "text")) {
        if (ThemeIr_ParseRgb(*v, &c)) out->text = c;
      }
      if (const JVal* v = ObjGet(*chrome, "captionGray")) {
        if (ThemeIr_ParseRgb(*v, &c)) out->captionGray = c;
      }
    }
  }

  if (out->id.empty()) {
    if (error) *error = L"Theme IR missing id";
    return false;
  }
  return true;
}

bool ThemeIr_SaveFile(const wchar_t* path, const ThemeIr* ir, std::wstring* error) {
  if (!path || !ir) {
    if (error) *error = L"Invalid arguments";
    return false;
  }
  std::string json;
  json.reserve(1024);
  json += "{\n";
  json += "  \"schemaVersion\": 1,\n";
  json += "  \"id\": \"" + EscapeJson(WideToUtf8(ir->id)) + "\",\n";
  json += "  \"displayName\": \"" + EscapeJson(WideToUtf8(ir->displayName)) + "\",\n";
  json += "  \"source\": \"" + EscapeJson(WideToUtf8(ir->source)) + "\",\n";
  json += "  \"sourceFormat\": \"" + EscapeJson(WideToUtf8(ir->sourceFormat)) + "\"";
  if (!ir->sourcePath.empty()) {
    json += ",\n  \"sourcePath\": \"" + EscapeJson(WideToUtf8(ir->sourcePath)) + "\"";
  }
  json += ",\n  \"syntax\": [\n";
  for (int i = 0; i < ir->syntaxCount; i++) {
    const ThemeIrSyntax& sx = ir->syntax[i];
    json += "    {\"role\":\"";
    json += EscapeJson(sx.role);
    json += "\",\"matchRgb\":";
    AppendRgbJson(&json, sx.matchRgb);
    json += ",\"outRgb\":";
    AppendRgbJson(&json, sx.outRgb);
    json += "}";
    if (i + 1 < ir->syntaxCount) json += ",";
    json += "\n";
  }
  json += "  ],\n";
  json += "  \"chrome\": {\n";
  json += "    \"editorBg\": "; AppendRgbJson(&json, ir->editorBg); json += ",\n";
  json += "    \"sidebar\": "; AppendRgbJson(&json, ir->sidebar); json += ",\n";
  json += "    \"menu\": "; AppendRgbJson(&json, ir->menu); json += ",\n";
  json += "    \"text\": "; AppendRgbJson(&json, ir->text); json += ",\n";
  json += "    \"captionGray\": "; AppendRgbJson(&json, ir->captionGray); json += "\n";
  json += "  }\n";
  json += "}\n";
  return WriteAllFileUtf8(path, json, error);
}
