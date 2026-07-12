// iOS platform layer — renderer windowing hooks.
// There is exactly one screen on iOS, and the "window" (the CAMetalLayer-backed
// UIView) is created by the app shell BEFORE the renderer initializes. The
// renderer adopts it (R_CreateWindow) instead of CreateWindowExA'ing its own.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// KISAKTODO(ios-render-bringup): implemented by the app shell at render
// bring-up; returns the externally-created host window (UIView*/SDL_Window*)
// the DXVK d3d9 swapchain targets. Stored as GfxWindowParms::hwnd and
// dx.windows[].hwnd exactly where the Win32 path stores its HWND.
void *Sys_iOS_GetHostWindow(void);

#ifdef __cplusplus
}
#endif
