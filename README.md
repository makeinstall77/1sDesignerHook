# 1sDesignerHook / Theme1C

Тёмная тема для **Конфигуратора 1С** (DESIGNER): tray-хост `Theme1C.exe` и runtime-хук `cairo1c8.dll`. Платформенные файлы на диске не меняются.

## Архитектура

- **`Theme1C.exe`** — постоянный host в трее (не короткий лаунчер). При старте может открыть `1cestart.exe` (путь настраивается). Следит за процессами **DESIGNER** и при включённом auto-inject внедряет DLL; также есть ручное **Apply**.
- **`cairo1c8.dll`** — freestanding-хук в процессе конфигуратора (IAT cairo через `grphcs.dll`). Старт worker через экспорт `Theme1C_Start` после `LoadLibrary` (не из `DllMain`).
- **Состояние темы** — файл `%AppData%\Theme1C\state.bin` (и зеркало рядом с exe). Host атомарно пишет IR→state; DLL читает `ReadFile` раз в ~500 мс. Named mmap не используется.

Класть exe рядом с `bin` платформы **не нужно**: `Theme1C.exe`, `cairo1c8.dll` и папка `builtin\` в одном каталоге.

## Настройки (UI из трея)

- Включение/выключение хуков
- Выбор темы (builtin **Dark+**, **RubyBlue**), правка цветов, **импорт тем VS Code**
- Путь к `1cestart.exe`, автозапуск стартера, auto-inject / ручной Apply

Данные: `%AppData%\Theme1C\`

- `config.json` — hooks, active theme, пути, флаги
- `themes\` — JSON тем в формате Theme IR
- `state.bin` — runtime-снимок для DLL

## Theme IR (кратко)

`schemaVersion: 1`. Поля: `id`, `displayName`, `source` (`builtin` | `custom` | `imported`), `sourceFormat`, опционально `sourcePath`; палитра chrome (`editorBg`, `sidebar`, `menu`, `text`, `captionGray`); массив `syntax[]` с ролями (`keyword` | `string` | `comment` | `identifier` | `directive`) и парами `matchRgb` / `outRgb`.

Конвертеры импорта реализуют `IThemeConverter` (`FormatId`, `DisplayName`, `CanParse`, `Convert` → `ThemeIr`). Сейчас есть VS Code; чтобы добавить формат — новый класс в `theme_convert.cpp`, регистрация в `g_converters[]`.

## Сборка

Нужен MSVC x64 (Build Tools / VS) с Windows SDK. `build.bat` ищет установку через `vswhere` и вызывает `vcvars64.bat`.

```bat
cd theme1c
build.bat
```

Артефакты: `theme1c\cairo1c8.dll`, `theme1c\Theme1C.exe`. При успехе: `BUILD_OK`.

Два таргета:

1. **DLL** — без CRT (`/NODEFAULTLIB`, stub `kernel32.lib` из `kernel32.def`)
2. **EXE** — с CRT: `tray_main.cpp`, `inject.cpp`, `settings_ui.cpp`, `onec_paths.cpp`, `theme_ir.cpp`, `appdata.cpp`, `config.cpp`, `theme_convert.cpp`, `theme_state_host.cpp` (`launcher.cpp` не собирается)

## Ограничения

- Только **x64**, фокус на **DESIGNER**
- Патч RVA `core83` отключён (ломал 8.3.27); тема идёт через cairo-хуки
- Эвристики lum/warmPaper в UI не выведены
- Антивирус может мешать инжекту — это вне кода проекта

## Структура `theme1c/`

```
tray_main.cpp      # host / tray
inject.cpp         # инъекция в DESIGNER
settings_ui.cpp    # настройки
cairo1c.cpp        # хук цветов (DLL)
theme_ir.*         # Theme IR
theme_convert.*    # IThemeConverter + VS Code
appdata.cpp / config.cpp / theme_state_host.cpp
build.bat
kernel32.def       # stub для freestanding DLL
launcher.cpp       # устаревший stub, не в линковке
```
