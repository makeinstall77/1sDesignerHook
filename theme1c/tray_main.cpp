#include "appdata.h"
#include "config.h"
#include "inject.h"
#include "onec_paths.h"
#include "settings_ui.h"
#include "theme_state_host.h"

#include <shellapi.h>

#include <cstring>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

static const UINT WM_TRAY = WM_APP + 1;
static const UINT ID_TIMER_INJECT = 1;
static const UINT IDM_SETTINGS = 100;
static const UINT IDM_LAUNCH = 101;
static const UINT IDM_EXIT = 102;

static Theme1C_Config g_cfg;
static HWND g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid;
static bool g_nidAdded = false;
static HANDLE g_mutex = nullptr;

static void ShowBalloonOrMsg(const wchar_t* text) {
  if (g_nidAdded) {
    g_nid.uFlags = NIF_INFO;
    wcsncpy_s(g_nid.szInfoTitle, L"Theme1C", _TRUNCATE);
    wcsncpy_s(g_nid.szInfo, text, _TRUNCATE);
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
  } else {
    MessageBoxW(nullptr, text, L"Theme1C", MB_OK | MB_ICONINFORMATION);
  }
}

static void RemoveTray() {
  if (g_nidAdded) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_nidAdded = false;
  }
}

static void AddTray(HWND hwnd) {
  memset(&g_nid, 0, sizeof(g_nid));
  g_nid.cbSize = sizeof(g_nid);
  g_nid.hWnd = hwnd;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
  g_nid.uCallbackMessage = WM_TRAY;
  g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wcsncpy_s(g_nid.szTip, L"Theme1C", _TRUNCATE);
  if (Shell_NotifyIconW(NIM_ADD, &g_nid)) g_nidAdded = true;
}

static void PopupMenu(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Настройки");
  AppendMenuW(menu, MF_STRING, IDM_LAUNCH, L"Запустить 1С");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Выход");
  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
  DestroyMenu(menu);
  PostMessageW(hwnd, WM_NULL, 0, 0);
}

static void DoLaunch1C(HWND hwnd) {
  const wchar_t* path = g_cfg.oneCStartPath.empty() ? nullptr : g_cfg.oneCStartPath.c_str();
  if (!LaunchOneCStart(path)) {
    wchar_t found[MAX_PATH * 2];
    if (!FindOneCStart(found, sizeof(found) / sizeof(found[0]))) {
      MessageBoxW(
          hwnd,
          L"1cestart.exe не найден. Откройте Настройки и укажите путь.",
          L"Theme1C",
          MB_OK | MB_ICONWARNING);
    } else {
      MessageBoxW(hwnd, L"Не удалось запустить 1cestart.exe.", L"Theme1C", MB_OK | MB_ICONERROR);
    }
  }
}

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      AddTray(hwnd);
      SetTimer(hwnd, ID_TIMER_INJECT, 1500, nullptr);
      return 0;
    case WM_TIMER:
      if (wParam == ID_TIMER_INJECT) {
        AutoInjectTick(g_cfg.hooksEnabled, g_cfg.autoInject);
      }
      return 0;
    case WM_TRAY:
      if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
        PopupMenu(hwnd);
      } else if (lParam == WM_LBUTTONDBLCLK) {
        SettingsUi_Show(hwnd, &g_cfg);
      }
      return 0;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDM_SETTINGS:
          SettingsUi_Show(hwnd, &g_cfg);
          return 0;
        case IDM_LAUNCH:
          DoLaunch1C(hwnd);
          return 0;
        case IDM_EXIT:
          DestroyWindow(hwnd);
          return 0;
      }
      break;
    case WM_DESTROY:
      KillTimer(hwnd, ID_TIMER_INJECT);
      RemoveTray();
      ThemeState_CloseWriter();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
  g_mutex = CreateMutexW(nullptr, TRUE, L"Local\\Theme1C_TrayMutex");
  if (g_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(
        nullptr,
        L"Theme1C уже запущен (иконка в области уведомлений).",
        L"Theme1C",
        MB_OK | MB_ICONINFORMATION);
    if (g_mutex) CloseHandle(g_mutex);
    return 0;
  }

  Theme1C_EnsureAppDataLayout();
  Theme1C_Config_Defaults(&g_cfg);
  std::wstring err;
  Theme1C_Config_Load(&g_cfg, &err);

  if (!ThemeState_OpenWriter()) {
    MessageBoxW(nullptr, L"Не удалось открыть shared memory Theme1C_State.", L"Theme1C",
                MB_OK | MB_ICONERROR);
    if (g_mutex) {
      ReleaseMutex(g_mutex);
      CloseHandle(g_mutex);
    }
    return 1;
  }
  ThemeHost_PublishFromConfig(&g_cfg);

  if (g_cfg.launchStartOnBoot) {
    const wchar_t* path = g_cfg.oneCStartPath.empty() ? nullptr : g_cfg.oneCStartPath.c_str();
    if (!LaunchOneCStart(path)) {
      /* UI will ask later; soft-fail at boot */
    }
  }

  WNDCLASSW wc;
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = TrayWndProc;
  wc.hInstance = hInst;
  wc.lpszClassName = L"Theme1C_TrayHost";
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (!RegisterClassW(&wc)) {
    ThemeState_CloseWriter();
    if (g_mutex) {
      ReleaseMutex(g_mutex);
      CloseHandle(g_mutex);
    }
    return 1;
  }

  g_hwnd = CreateWindowExW(
      0, L"Theme1C_TrayHost", L"Theme1C", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, hInst,
      nullptr);
  if (!g_hwnd) {
    ThemeState_CloseWriter();
    if (g_mutex) {
      ReleaseMutex(g_mutex);
      CloseHandle(g_mutex);
    }
    return 1;
  }

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (g_mutex) {
    ReleaseMutex(g_mutex);
    CloseHandle(g_mutex);
    g_mutex = nullptr;
  }
  return (int)msg.wParam;
}
