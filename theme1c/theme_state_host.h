#pragma once

#include "theme_state.h"

/* Host-side shared-memory publisher (CRT OK). */

bool ThemeState_OpenWriter();
void ThemeState_CloseWriter();
bool ThemeState_IsOpen();
void ThemeState_Publish(const Theme1C_State* state);

/* Fill Dark+ into mapped view and publish (enabled flag from arg). */
void ThemeState_PublishDarkPlus(unsigned int enabled);
