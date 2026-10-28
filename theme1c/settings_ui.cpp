#include "settings_ui.h"
#include "appdata.h"
#include "inject.h"
#include "onec_paths.h"
#include "theme_convert.h"
#include "theme_ir.h"
#include "theme_state_host.h"

#include <commctrl.h>
#include <commdlg.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

enum {
  IDC_CHK_HOOKS = 1001,
  IDC_CHK_AUTO = 1002,
  IDC_CHK_BOOT = 1003,
  IDC_EDIT_START = 1004,
  IDC_BTN_BROWSE = 1005,
  IDC_BTN_LAUNCH = 1006,
  IDC_CMB_THEME = 1007,
  IDC_BTN_CUSTOM = 1008,
  IDC_BTN_IMPORT = 1009,
  IDC_LIST_PROC = 1010,
  IDC_BTN_REFRESH = 1011,
  IDC_BTN_APPLY = 1012,
  IDC_BTN_CLOSE = 1013
};

struct SettingsCtx {
  Theme1C_Config* cfg;
  HWND hwnd;
  HWND listProc;
  std::vector<DWORD> listPids;
};

static HFONT g_uiFont = nullptr;

static HFONT UiFont() {
  if (!g_uiFont) {
    g_uiFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  }
  return g_uiFont;
}

bool ThemeHost_PublishFromConfig(const Theme1C_Config* cfg) {
  if (!cfg) return false;
  if (!ThemeState_IsOpen() && !ThemeState_OpenWriter()) return false;

  ThemeIr ir;
  ThemeIr_LoadBuiltinDarkPlus(&ir);

  wchar_t path[MAX_PATH * 2];
  if (!cfg->activeThemeId.empty() &&
      Theme1C_GetThemePath(cfg->activeThemeId.c_str(), path, sizeof(path) / sizeof(path[0]))) {
    std::wstring err;
    ThemeIr loaded;
    if (ThemeIr_LoadFile(path, &loaded, &err)) ir = std::move(loaded);
  }

  Theme1C_State state;
  memset(&state, 0, sizeof(state));
  ThemeIr_ToState(&ir, &state);
  state.enabled = cfg->hooksEnabled ? 1u : 0u;
  ThemeState_Publish(&state);
  return true;
}

static void SaveCfg(Theme1C_Config* cfg) {
  std::wstring err;
  Theme1C_Config_Save(cfg, &err);
}

static COLORREF IrToColor(ThemeIrRgb c) {
  return RGB(c.r, c.g, c.b);
}

static ThemeIrRgb ColorToIr(COLORREF c) {
  return ThemeIr_Rgb(GetRValue(c), GetGValue(c), GetBValue(c));
}

static bool PickColor(HWND owner, ThemeIrRgb* color) {
  static COLORREF custom[16];
  CHOOSECOLORW cc;
  memset(&cc, 0, sizeof(cc));
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = owner;
  cc.lpCustColors = custom;
  cc.rgbResult = IrToColor(*color);
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (!ChooseColorW(&cc)) return false;
  *color = ColorToIr(cc.rgbResult);
  return true;
}

static ThemeIrRgb* RoleOut(ThemeIr* ir, const char* role) {
  for (int i = 0; i < ir->syntaxCount; i++) {
    if (_stricmp(ir->syntax[i].role, role) == 0) return &ir->syntax[i].outRgb;
  }
  if (ir->syntaxCount < 8) {
    ThemeIrSyntax* s = &ir->syntax[ir->syntaxCount++];
    memset(s, 0, sizeof(*s));
    std::snprintf(s->role, sizeof(s->role), "%s", role);
    s->matchRgb = ThemeIr_Rgb(0, 0, 0);
    s->outRgb = ThemeIr_Rgb(200, 200, 200);
    return &s->outRgb;
  }
  return nullptr;
}

static void RefreshThemeCombo(HWND cmb, const Theme1C_Config* cfg) {
  SendMessageW(cmb, CB_RESETCONTENT, 0, 0);
  wchar_t dir[MAX_PATH * 2];
  if (!Theme1C_GetThemesDir(dir, sizeof(dir) / sizeof(dir[0]))) return;

  wchar_t pattern[MAX_PATH * 2];
  _snwprintf_s(pattern, _TRUNCATE, L"%s\\*.json", dir);

  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern, &fd);
  int select = 0;
  int index = 0;
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      wchar_t id[MAX_PATH];
      wcsncpy_s(id, fd.cFileName, _TRUNCATE);
      wchar_t* dot = wcsrchr(id, L'.');
      if (dot) *dot = 0;

      wchar_t path[MAX_PATH * 2];
      Theme1C_GetThemePath(id, path, sizeof(path) / sizeof(path[0]));
      ThemeIr ir;
      std::wstring err;
      std::wstring label = id;
      if (ThemeIr_LoadFile(path, &ir, &err) && !ir.displayName.empty()) {
        label = ir.displayName + L" (" + id + L")";
      }
      LRESULT item = SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)label.c_str());
      /* store id as item data via separate parallel — use CB_SETITEMDATA with heap copy */
      size_t n = wcslen(id) + 1;
      wchar_t* heap = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
      if (heap) {
        memcpy(heap, id, n * sizeof(wchar_t));
        SendMessageW(cmb, CB_SETITEMDATA, (WPARAM)item, (LPARAM)heap);
      }
      if (_wcsicmp(id, cfg->activeThemeId.c_str()) == 0) select = index;
      index++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  if (index > 0) SendMessageW(cmb, CB_SETCURSEL, (WPARAM)select, 0);
}

static void FreeThemeComboData(HWND cmb) {
  LRESULT count = SendMessageW(cmb, CB_GETCOUNT, 0, 0);
  for (LRESULT i = 0; i < count; i++) {
    wchar_t* p = (wchar_t*)SendMessageW(cmb, CB_GETITEMDATA, (WPARAM)i, 0);
    if (p && p != (wchar_t*)CB_ERR) HeapFree(GetProcessHeap(), 0, p);
  }
}

static const wchar_t* SelectedThemeId(HWND cmb) {
  LRESULT sel = SendMessageW(cmb, CB_GETCURSEL, 0, 0);
  if (sel == CB_ERR) return nullptr;
  return (const wchar_t*)SendMessageW(cmb, CB_GETITEMDATA, (WPARAM)sel, 0);
}

static void ActivateThemeId(Theme1C_Config* cfg, const wchar_t* id) {
  if (!id || !*id) return;
  cfg->activeThemeId = id;
  SaveCfg(cfg);
  ThemeHost_PublishFromConfig(cfg);
}

static void RefreshProcessList(SettingsCtx* ctx) {
  if (!ctx || !ctx->listProc) return;
  ListView_DeleteAllItems(ctx->listProc);
  ctx->listPids.clear();

  std::vector<OneCProcessInfo> list;
  EnumOneCProcesses(&list);
  for (size_t i = 0; i < list.size(); i++) {
    const OneCProcessInfo& p = list[i];
    ctx->listPids.push_back(p.pid);

    wchar_t pidBuf[32];
    _snwprintf_s(pidBuf, _TRUNCATE, L"%lu", (unsigned long)p.pid);

    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = (int)i;
    item.pszText = pidBuf;
    ListView_InsertItem(ctx->listProc, &item);
    ListView_SetItemText(ctx->listProc, (int)i, 1, (LPWSTR)OneCMode_Name(p.mode));
    ListView_SetItemText(ctx->listProc, (int)i, 2, p.injected ? (LPWSTR)L"да" : (LPWSTR)L"нет");
  }
}

struct CustomDlgData {
  Theme1C_Config* cfg;
  ThemeIr ir;
  ThemeIrRgb* slots[9];
  bool closed;
  bool saved;
};

static LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  CustomDlgData* data = (CustomDlgData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  switch (msg) {
    case WM_CREATE: {
      auto* cs = (CREATESTRUCTW*)lParam;
      data = (CustomDlgData*)cs->lpCreateParams;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);
      HFONT font = UiFont();
      static const wchar_t* labels[] = {
          L"Ключевое слово", L"Строка",     L"Комментарий", L"Идентификатор", L"Директива",
          L"Фон редактора",  L"Боковая панель", L"Меню",     L"Текст"};
      int y = 16;
      for (int i = 0; i < 9; i++) {
        HWND lab = CreateWindowW(
            L"STATIC", labels[i], WS_CHILD | WS_VISIBLE, 16, y + 4, 160, 22, hwnd, nullptr,
            nullptr, nullptr);
        SendMessageW(lab, WM_SETFONT, (WPARAM)font, TRUE);
        HWND b = CreateWindowW(
            L"BUTTON", L"Цвет…", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 190, y, 120, 28, hwnd,
            (HMENU)(UINT_PTR)(2000 + i), nullptr, nullptr);
        SendMessageW(b, WM_SETFONT, (WPARAM)font, TRUE);
        y += 34;
      }
      HWND save = CreateWindowW(
          L"BUTTON", L"Сохранить как custom", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 16,
          y + 8, 200, 30, hwnd, (HMENU)(UINT_PTR)2100, nullptr, nullptr);
      SendMessageW(save, WM_SETFONT, (WPARAM)font, TRUE);
      HWND cancel = CreateWindowW(
          L"BUTTON", L"Закрыть", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 230, y + 8, 90, 30,
          hwnd, (HMENU)(UINT_PTR)2101, nullptr, nullptr);
      SendMessageW(cancel, WM_SETFONT, (WPARAM)font, TRUE);
      return 0;
    }
    case WM_COMMAND: {
      if (!data) break;
      int id = LOWORD(wParam);
      if (id >= 2000 && id < 2009) {
        int si = id - 2000;
        if (data->slots[si]) PickColor(hwnd, data->slots[si]);
        return 0;
      }
      if (id == 2100) {
        data->ir.id = L"custom";
        data->ir.displayName = L"Custom";
        data->ir.source = L"custom";
        data->ir.sourceFormat = L"theme1c";
        wchar_t dest[MAX_PATH * 2];
        if (Theme1C_GetThemePath(L"custom", dest, sizeof(dest) / sizeof(dest[0]))) {
          std::wstring err;
          if (ThemeIr_SaveFile(dest, &data->ir, &err)) {
            ActivateThemeId(data->cfg, L"custom");
            data->saved = true;
            MessageBoxW(hwnd, L"Тема custom сохранена и активирована.", L"Theme1C", MB_OK);
          } else {
            MessageBoxW(hwnd, err.c_str(), L"Theme1C", MB_OK | MB_ICONERROR);
          }
        }
        return 0;
      }
      if (id == 2101) {
        data->closed = true;
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      if (data) data->closed = true;
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void RunCustomizeDialog(HWND owner, Theme1C_Config* cfg) {
  static bool registered = false;
  if (!registered) {
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = CustomWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Theme1C_CustomDlg";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    registered = true;
  }

  CustomDlgData data;
  data.cfg = cfg;
  data.closed = false;
  data.saved = false;
  for (int i = 0; i < 9; i++) data.slots[i] = nullptr;
  ThemeIr_LoadBuiltinDarkPlus(&data.ir);
  wchar_t path[MAX_PATH * 2];
  if (!cfg->activeThemeId.empty() &&
      Theme1C_GetThemePath(cfg->activeThemeId.c_str(), path, sizeof(path) / sizeof(path[0]))) {
    std::wstring err;
    ThemeIr loaded;
    if (ThemeIr_LoadFile(path, &loaded, &err)) data.ir = std::move(loaded);
  }
  data.slots[0] = RoleOut(&data.ir, "keyword");
  data.slots[1] = RoleOut(&data.ir, "string");
  data.slots[2] = RoleOut(&data.ir, "comment");
  data.slots[3] = RoleOut(&data.ir, "identifier");
  data.slots[4] = RoleOut(&data.ir, "directive");
  data.slots[5] = &data.ir.editorBg;
  data.slots[6] = &data.ir.sidebar;
  data.slots[7] = &data.ir.menu;
  data.slots[8] = &data.ir.text;

  HWND dlg = CreateWindowExW(
      WS_EX_DLGMODALFRAME, L"Theme1C_CustomDlg", L"Кастомизация темы",
      WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 360, 420,
      owner, nullptr, GetModuleHandleW(nullptr), &data);
  if (!dlg) return;

  EnableWindow(owner, FALSE);
  MSG msg;
  while (!data.closed && IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
}

static void DoImport(HWND owner, Theme1C_Config* cfg, HWND cmb) {
  wchar_t file[MAX_PATH];
  file[0] = 0;
  OPENFILENAMEW ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = L"Темы\0*.json;*.vsix\0Все файлы\0*.*\0";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
  if (!GetOpenFileNameW(&ofn)) return;

  if (!ThemeConvert_FindForFile(file)) {
    MessageBoxW(owner, L"Нет конвертера для этого файла.", L"Импорт темы", MB_OK | MB_ICONERROR);
    return;
  }
  ThemeIr ir;
  ThemeConvertResult res = ThemeConvert_File(file, &ir);
  if (!res.ok) {
    MessageBoxW(owner, res.error.c_str(), L"Импорт темы", MB_OK | MB_ICONERROR);
    return;
  }
  if (ir.id.empty()) ir.id = L"imported";
  /* sanitize id for filename */
  for (auto& ch : ir.id) {
    if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' || ch == L'"' ||
        ch == L'<' || ch == L'>' || ch == L'|')
      ch = L'_';
  }
  wchar_t dest[MAX_PATH * 2];
  if (!Theme1C_GetThemePath(ir.id.c_str(), dest, sizeof(dest) / sizeof(dest[0]))) {
    MessageBoxW(owner, L"Не удалось сформировать путь темы.", L"Theme1C", MB_OK | MB_ICONERROR);
    return;
  }
  std::wstring err;
  if (!ThemeIr_SaveFile(dest, &ir, &err)) {
    MessageBoxW(owner, err.c_str(), L"Theme1C", MB_OK | MB_ICONERROR);
    return;
  }
  FreeThemeComboData(cmb);
  RefreshThemeCombo(cmb, cfg);
  ActivateThemeId(cfg, ir.id.c_str());
  MessageBoxW(owner, L"Тема импортирована и активирована.", L"Theme1C", MB_OK);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  SettingsCtx* ctx = (SettingsCtx*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  switch (msg) {
    case WM_CREATE: {
      auto* cs = (CREATESTRUCTW*)lParam;
      ctx = (SettingsCtx*)cs->lpCreateParams;
      ctx->hwnd = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);

      INITCOMMONCONTROLSEX icc;
      icc.dwSize = sizeof(icc);
      icc.dwICC = ICC_LISTVIEW_CLASSES;
      InitCommonControlsEx(&icc);

      HFONT font = UiFont();
      Theme1C_Config* cfg = ctx->cfg;
      int y = 12;

      auto addChk = [&](int id, const wchar_t* text, bool checked) {
        HWND h = CreateWindowW(
            L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, y, 420, 24, hwnd,
            (HMENU)(UINT_PTR)id, nullptr, nullptr);
        SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 28;
        return h;
      };
      addChk(IDC_CHK_HOOKS, L"Включить хуки темы (hooksEnabled)", cfg->hooksEnabled);
      addChk(IDC_CHK_AUTO, L"Авто-внедрение в DESIGNER (autoInject)", cfg->autoInject);
      addChk(IDC_CHK_BOOT, L"Запускать 1cestart при старте Theme1C", cfg->launchStartOnBoot);

      HWND lab = CreateWindowW(
          L"STATIC", L"Путь к 1cestart.exe:", WS_CHILD | WS_VISIBLE, 16, y, 200, 20, hwnd,
          nullptr, nullptr, nullptr);
      SendMessageW(lab, WM_SETFONT, (WPARAM)font, TRUE);
      y += 22;
      HWND edit = CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", cfg->oneCStartPath.c_str(),
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, y, 340, 24, hwnd, (HMENU)(UINT_PTR)IDC_EDIT_START,
          nullptr, nullptr);
      SendMessageW(edit, WM_SETFONT, (WPARAM)font, TRUE);
      HWND br = CreateWindowW(
          L"BUTTON", L"Обзор…", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 364, y, 80, 24, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_BROWSE, nullptr, nullptr);
      SendMessageW(br, WM_SETFONT, (WPARAM)font, TRUE);
      y += 30;
      HWND launch = CreateWindowW(
          L"BUTTON", L"Запустить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, y, 120, 28, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_LAUNCH, nullptr, nullptr);
      SendMessageW(launch, WM_SETFONT, (WPARAM)font, TRUE);
      y += 36;

      lab = CreateWindowW(
          L"STATIC", L"Тема:", WS_CHILD | WS_VISIBLE, 16, y, 80, 20, hwnd, nullptr, nullptr,
          nullptr);
      SendMessageW(lab, WM_SETFONT, (WPARAM)font, TRUE);
      HWND cmb = CreateWindowW(
          L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 100, y, 260,
          200, hwnd, (HMENU)(UINT_PTR)IDC_CMB_THEME, nullptr, nullptr);
      SendMessageW(cmb, WM_SETFONT, (WPARAM)font, TRUE);
      RefreshThemeCombo(cmb, cfg);
      y += 34;

      HWND custom = CreateWindowW(
          L"BUTTON", L"Кастомизация", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, y, 140, 28,
          hwnd, (HMENU)(UINT_PTR)IDC_BTN_CUSTOM, nullptr, nullptr);
      SendMessageW(custom, WM_SETFONT, (WPARAM)font, TRUE);
      HWND imp = CreateWindowW(
          L"BUTTON", L"Импорт…", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 168, y, 120, 28, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_IMPORT, nullptr, nullptr);
      SendMessageW(imp, WM_SETFONT, (WPARAM)font, TRUE);
      y += 40;

      lab = CreateWindowW(
          L"STATIC", L"Процессы 1cv8.exe:", WS_CHILD | WS_VISIBLE, 16, y, 200, 20, hwnd, nullptr,
          nullptr, nullptr);
      SendMessageW(lab, WM_SETFONT, (WPARAM)font, TRUE);
      y += 22;

      HWND lv = CreateWindowExW(
          WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
          WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 16, y, 430, 160,
          hwnd, (HMENU)(UINT_PTR)IDC_LIST_PROC, nullptr, nullptr);
      ctx->listProc = lv;
      ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
      LVCOLUMNW col;
      memset(&col, 0, sizeof(col));
      col.mask = LVCF_TEXT | LVCF_WIDTH;
      col.pszText = (LPWSTR)L"PID";
      col.cx = 80;
      ListView_InsertColumn(lv, 0, &col);
      col.pszText = (LPWSTR)L"Режим";
      col.cx = 120;
      ListView_InsertColumn(lv, 1, &col);
      col.pszText = (LPWSTR)L"Внедрено";
      col.cx = 100;
      ListView_InsertColumn(lv, 2, &col);
      RefreshProcessList(ctx);
      y += 170;

      HWND ref = CreateWindowW(
          L"BUTTON", L"Обновить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, y, 100, 28, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_REFRESH, nullptr, nullptr);
      SendMessageW(ref, WM_SETFONT, (WPARAM)font, TRUE);
      HWND apply = CreateWindowW(
          L"BUTTON", L"Применить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 128, y, 100, 28, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_APPLY, nullptr, nullptr);
      SendMessageW(apply, WM_SETFONT, (WPARAM)font, TRUE);
      HWND close = CreateWindowW(
          L"BUTTON", L"Закрыть", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 346, y, 100, 28, hwnd,
          (HMENU)(UINT_PTR)IDC_BTN_CLOSE, nullptr, nullptr);
      SendMessageW(close, WM_SETFONT, (WPARAM)font, TRUE);
      return 0;
    }
    case WM_COMMAND: {
      if (!ctx || !ctx->cfg) break;
      Theme1C_Config* cfg = ctx->cfg;
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      if (id == IDC_CHK_HOOKS && code == BN_CLICKED) {
        cfg->hooksEnabled = SendMessageW((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SaveCfg(cfg);
        ThemeHost_PublishFromConfig(cfg);
        return 0;
      }
      if (id == IDC_CHK_AUTO && code == BN_CLICKED) {
        cfg->autoInject = SendMessageW((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SaveCfg(cfg);
        return 0;
      }
      if (id == IDC_CHK_BOOT && code == BN_CLICKED) {
        cfg->launchStartOnBoot = SendMessageW((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SaveCfg(cfg);
        return 0;
      }
      if (id == IDC_BTN_BROWSE) {
        wchar_t file[MAX_PATH];
        file[0] = 0;
        OPENFILENAMEW ofn;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"1cestart.exe\0*.exe\0Все файлы\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        if (GetOpenFileNameW(&ofn)) {
          SetDlgItemTextW(hwnd, IDC_EDIT_START, file);
          cfg->oneCStartPath = file;
          SaveCfg(cfg);
        }
        return 0;
      }
      if (id == IDC_BTN_LAUNCH) {
        wchar_t path[MAX_PATH * 2];
        GetDlgItemTextW(hwnd, IDC_EDIT_START, path, (int)(sizeof(path) / sizeof(path[0])));
        cfg->oneCStartPath = path;
        SaveCfg(cfg);
        if (!LaunchOneCStart(path[0] ? path : nullptr)) {
          wchar_t found[MAX_PATH * 2];
          if (!FindOneCStart(found, sizeof(found) / sizeof(found[0]))) {
            MessageBoxW(
                hwnd,
                L"1cestart.exe не найден. Укажите путь вручную.",
                L"Theme1C", MB_OK | MB_ICONWARNING);
          } else {
            MessageBoxW(hwnd, L"Не удалось запустить 1cestart.exe.", L"Theme1C",
                        MB_OK | MB_ICONERROR);
          }
        }
        return 0;
      }
      if (id == IDC_CMB_THEME && code == CBN_SELCHANGE) {
        HWND cmb = GetDlgItem(hwnd, IDC_CMB_THEME);
        const wchar_t* tid = SelectedThemeId(cmb);
        if (tid) ActivateThemeId(cfg, tid);
        return 0;
      }
      if (id == IDC_BTN_CUSTOM) {
        RunCustomizeDialog(hwnd, cfg);
        HWND cmb = GetDlgItem(hwnd, IDC_CMB_THEME);
        FreeThemeComboData(cmb);
        RefreshThemeCombo(cmb, cfg);
        return 0;
      }
      if (id == IDC_BTN_IMPORT) {
        DoImport(hwnd, cfg, GetDlgItem(hwnd, IDC_CMB_THEME));
        return 0;
      }
      if (id == IDC_BTN_REFRESH) {
        RefreshProcessList(ctx);
        return 0;
      }
      if (id == IDC_BTN_APPLY) {
        int sel = ListView_GetNextItem(ctx->listProc, -1, LVNI_SELECTED);
        if (sel < 0 || sel >= (int)ctx->listPids.size()) {
          MessageBoxW(hwnd, L"Выберите процесс в списке.", L"Theme1C", MB_OK | MB_ICONINFORMATION);
          return 0;
        }
        std::wstring err;
        if (!ManualInjectPid(ctx->listPids[sel], hwnd, &err)) {
          if (!err.empty() && err != L"Отменено пользователем")
            MessageBoxW(hwnd, err.c_str(), L"Theme1C", MB_OK | MB_ICONERROR);
        } else {
          RefreshProcessList(ctx);
        }
        return 0;
      }
      if (id == IDC_BTN_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
      }
      if (id == IDC_EDIT_START && code == EN_KILLFOCUS) {
        wchar_t path[MAX_PATH * 2];
        GetDlgItemTextW(hwnd, IDC_EDIT_START, path, (int)(sizeof(path) / sizeof(path[0])));
        if (cfg->oneCStartPath != path) {
          cfg->oneCStartPath = path;
          SaveCfg(cfg);
        }
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY: {
      HWND cmb = GetDlgItem(hwnd, IDC_CMB_THEME);
      if (cmb) FreeThemeComboData(cmb);
      return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SettingsUi_Show(HWND owner, Theme1C_Config* cfg) {
  if (!cfg) return;

  static bool registered = false;
  if (!registered) {
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Theme1C_Settings";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    registered = true;
  }

  SettingsCtx ctx;
  ctx.cfg = cfg;
  ctx.hwnd = nullptr;
  ctx.listProc = nullptr;

  HWND hwnd = CreateWindowExW(
      WS_EX_DLGMODALFRAME, L"Theme1C_Settings", L"Theme1C — Настройки",
      WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 480,
      620, owner, nullptr, GetModuleHandleW(nullptr), &ctx);
  if (!hwnd) return;

  if (owner) EnableWindow(owner, FALSE);

  MSG msg;
  while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  if (owner) {
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
  }
}
