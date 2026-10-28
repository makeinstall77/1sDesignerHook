#pragma once

#include "config.h"

#include <windows.h>

/* Load active theme IR → ThemeIr_ToState → enabled from cfg → Publish. */
bool ThemeHost_PublishFromConfig(const Theme1C_Config* cfg);

/* Modal settings window. Mutates *cfg and persists as needed. */
void SettingsUi_Show(HWND owner, Theme1C_Config* cfg);
