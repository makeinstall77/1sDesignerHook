# 1sDesignerHook / Theme1C

Брошенный прототип тёмной темы для **Конфигуратора 1С** (только DESIGNER). Платформенные файлы не меняются: рядом с запуском кладётся DLL, лаунчер поднимает `1cv8.exe DESIGNER` и внедряет её в новый процесс.

## Что уже сделано

- **`Theme1C.exe`** — запуск конфигуратора в suspended-состоянии, инъекция `cairo1c8.dll` (APC → fallback `CreateRemoteThread`). Режим ENTERPRISE намеренно запрещён.
- **`cairo1c8.dll`** — runtime-хук без правки файлов на диске:
  - IAT `grphcs.dll` → cairo (`set_source_rgb/rgba`, gradient stop, mesh corner);
  - GDI/syscolor (`SetTextColor`, `SetBkColor`, `GetSysColor*`) по загруженным модулям;
  - точечная подмена палитры в `core83.dll` по фиксированным RVA (привязка к конкретной сборке платформы).
- Маппинг цветов близок к **VS Code Dark+** (фон модуля, chrome, подсветка синтаксиса по известным RGB из этой сборки).
- Freestanding-сборка без CRT: `build.bat` + `minwin.h` + `crtfix.cpp` (MSVC x64).
- По логу `cairo1c.log` патч cairo в `grphcs` отрабатывал на машине автора прототипа.

## Чего нет / ограничения

- Нет стабилизации под разные релизы платформы (RVA в `core83` хрупкие).
- Разделение «отрисовка форм / редактор» через стек (`IsFormPaint`) выключено.
- Нет упаковки, автотестов, поддержки уже запущенного `1cv8`, темы для предприятия.
- Репозиторий заведён постфактум по переданному прототипу; история итераций `cairo1c`…`cairo1c7` в git не тащится.

## Сборка

Нужен MSVC Build Tools (путь в `theme1c\build.bat` при необходимости поправить):

```bat
cd theme1c
build.bat
```

Артефакты: `theme1c\cairo1c8.dll`, `theme1c\Theme1C.exe`.

## Запуск

Положить `Theme1C.exe` и `cairo1c8.dll` в каталог бинарников платформы так, чтобы лаунчер нашёл `..\..\1cv8.exe` относительно своей папки (ожидаемая раскладка как у подпапки в `bin`), либо поправить пути в `launcher.cpp`. Запускать через `Theme1C.exe` вместо прямого DESIGNER.

## Структура

```
theme1c/
  cairo1c.cpp   # хук цветов
  launcher.cpp  # инжектор DESIGNER
  build.bat
  minwin.h
  crtfix.cpp
  *.def         # минимальные импорты для линковки без SDK
```
