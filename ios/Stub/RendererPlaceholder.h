#pragma once

#include <TargetConditionals.h>

struct IDirect3D9;
struct IDirect3DDevice9;

#if !TARGET_OS_SIMULATOR
// Device-only implementation behind the public Swift bridge (the simulator
// links no DXVK and refuses the stage honestly in D3D9Smoke.mm). The caller
// retains both COM objects for the app lifetime; this proof is intentionally
// one-shot so the engine backend globals never outlive those objects.
const char *kisak_renderer_placeholder_run(IDirect3D9 *d3d9,
                                           IDirect3DDevice9 *device);
const char *kisak_renderer_placeholder_detail_run(void);
#endif
