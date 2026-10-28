#include "minwin.h"

extern "C" int _fltused = 0;

extern "C" void* memset(void* dst, int value, SIZE_T count) {
    BYTE* p = (BYTE*)dst;
    for (SIZE_T i = 0; i < count; i++) p[i] = (BYTE)value;
    return dst;
}

extern "C" void* memcpy(void* dst, const void* src, SIZE_T count) {
    BYTE* d = (BYTE*)dst;
    const BYTE* s = (const BYTE*)src;
    for (SIZE_T i = 0; i < count; i++) d[i] = s[i];
    return dst;
}
