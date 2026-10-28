#pragma once

#include <cstddef>

/* Discover and launch 1cestart.exe */

bool FindOneCStart(wchar_t* out, size_t capChars);
bool LaunchOneCStart(const wchar_t* path);
