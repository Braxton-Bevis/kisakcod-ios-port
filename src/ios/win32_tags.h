#pragma once
// DXVK's windows_base.h typedefs the Win32 handle types as void* (HANDLE)
// without declaring the underlying `struct X__` tags that real windows.h
// provides. The decomp names those tags directly (always through pointers),
// so incomplete declarations are all iOS needs.
#ifdef KISAK_IOS
#include <windows.h> // DXVK windows_base.h: base typedefs incl. the POINT struct

typedef POINT tagPOINT; // real windows.h tags the struct 'tagPOINT'; DXVK tags it 'POINT'

#ifndef ARRAYSIZE // real windows.h has it; DXVK's windows_base.h does not
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#include <time.h>
static inline void Sleep(unsigned int ms) // kernel32 Sleep; nanosleep resumes across EINTR like Win32 does
{
    struct timespec ts;
    ts.tv_sec = ms / 1000u;
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0) {}
}

struct HWND__;
struct HINSTANCE__;
struct HMONITOR__;
struct HDC__;
struct HMIXER__;
struct HKEY__;
struct HFONT__;
struct HBRUSH__;
#endif
