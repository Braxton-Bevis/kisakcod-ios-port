# FRONTIER REPORT — KisakCOD → arm64-apple-ios

**Updated:** 2026-07-11 (Mac bring-up session) · **Repo:** `Braxton-Bevis/bmk4` ·
**Companion docs:** [PORT_JOURNAL.md](PORT_JOURNAL.md) (experiment log, M0–M11), [DEPENDENCY_MAP.md](DEPENDENCY_MAP.md) (API-by-API replacement table)

One-sentence summary: **every objective of the original capability probe is now
green or beyond — all 23 tracked engine translation units compile clean for
arm64-apple-ios, DXVK's d3d9 module (the renderer runtime, previously "the
riskiest unknown in the whole port") builds as an arm64-apple-ios static
library, and real engine code has been linked into the iOS app and executed
with verified-correct results on both the simulator and a physical iPad Pro
(M5) — with MetalFX spatial upscaling live at 120 fps.**

The original probe (M0–M6) ran entirely from a Windows laptop against GitHub
Actions runners. This session (M7–M11) reproduced everything on a real Mac
(Xcode 26.6, iOS 26.5 SDK) and pushed each frontier past where the probe
stopped. Every claim below is machine-verified: compile logs, an in-sandbox
marker file pulled off the physical device, and CI runs.

---

## Milestones, per objective

### Objective 1 — iOS pipeline: ✅ COMPLETE, now including the physical-device last mile
- The stub app builds locally and in CI, launches in the simulator, and — new —
  **installs and runs on a physical iPad Pro 13" (M5)** with automatic signing
  (personal team `NCYQDHQ93F`; certificate minted headlessly on first build)
  and `devicectl` install/launch. Wireless reinstalls work via the CoreDevice
  network tunnel once USB-paired.
- Marker file pulled from the device sandbox (M11): `gpu=Apple M5 GPU`,
  `fps=120.0`, `drawableSize=(3200.0, 2400.0)`.

### Objective 2 — Build-system port + census: ✅ 23/23 TUs COMPILE
- The census grew from 14 tracked files (2 passing) to **23 tracked files, all
  passing**, spanning: game logic (bg_pmove, g_main_mp, g_utils_mp), server
  core (sv_main_mp), GSC script VM (scr_vm), animation (xmodel), physics
  (phys_ode), command system (cmd), networking (msg_mp, huffman, win_net),
  threading (threads), filesystem (com_files, win_common), shared utilities
  (q_shared, com_math), sound coupling (snd_mss), Steam (win_steam), renderer
  init (r_init), and the new iOS platform layer (sys_ios_paths, sys_ios_main,
  msvc_crt_compat, snd_ios_stub).
- Zero platform headers and zero platform symbols remain in any tracked TU's
  error path. `win_main.cpp` was retired from the census in favor of its
  replacement, `src/ios/sys_ios_main.mm`.
- Shared compat surface: [`src/ios/msvc_crt_compat.h`](src/ios/msvc_crt_compat.h)
  (MSVC CRT spellings, Interlocked* atomics with Win32 return semantics,
  `__rdtsc`→`cntvct_el0`, secure-CRT variants) and
  [`src/ios/win32_tags.h`](src/ios/win32_tags.h) (Win32 struct tags absent from
  DXVK's headers, `ARRAYSIZE`, `Sleep`).

### Objective 3 — Filesystem sandboxing: ✅ POSIX PORT LANDED
- Beyond the original dvar-seam patch: `com_files.cpp`/`win_common.cpp` now have
  real POSIX implementations (stat/opendir/readdir/mkdir/getcwd) behind
  `KISAK_IOS` gates, backslash path production fixed at its two sources, and
  the LP64 landmines on the FS boot path fixed (DvarValue `.integer`-as-pointer
  union reads → `.string`, `Sys_ListFiles` 4-byte-pointer sizings).

### Objective 4 — Renderer (TRANSLATION path): ✅ FRONTIER RESOLVED AT THE LIBRARY LEVEL
- **`libdxvk_d3d9.a` exists for arm64-apple-ios.** dxvk v2.7.1 cross-compiles
  with a 5-hunk patch ([`scripts/platform/ios/dxvk-v2.7.1-ios.patch`](scripts/platform/ios/dxvk-v2.7.1-ios.patch)):
  an `__APPLE__` gate in `util_win32_compat.h` (the entire "DXVK has no Apple
  support" wall was one preprocessor conditional), an lzcnt overload ambiguity,
  Apple's one-argument `pthread_setname_np`, a missing `<cstddef>`, and
  static-archive linking on Darwin. WSI backend: SDL2 built for iOS (official
  upstream support). `Direct3DCreate9`/`Direct3DCreate9Ex` verified exported.
- One-command reproducer: [`scripts/platform/ios/build-dxvk-ios.sh`](scripts/platform/ios/build-dxvk-ios.sh).
- MoltenVK v1.4.1 (prebuilt iOS xcframework) staged as the Vulkan
  implementation for runtime bring-up.
- The engine-side seam is in place: `R_CreateWindow` on iOS adopts the app
  shell's CAMetalLayer view via `Sys_iOS_GetHostWindow()`
  ([`src/ios/r_ios_window.h`](src/ios/r_ios_window.h)).

### Objective 5 — Input/graphics surface: ✅ EXPANDED AND DEVICE-VERIFIED
- Stub v4 settings menu: 3-tier shader quality (three real Metal pipeline
  states), capability-gated ray-tracing toggle (`MTLDevice.supportsRaytracing`;
  the M5 reports **supported** — the DXVK/MoltenVK path still carries no RT,
  so the toggle is the surface for a future native-Metal experiment), render
  scale 25–100%, live resolution readout, 30/60/120/Max frame cap, measured
  FPS. On the iPad: **`metalfx=spatial 1600x1200 → 3200x2400`** — the first
  execution of the MetalFX code path (the simulator SDK lacks the module).

### NEW — Objective 6, engine linking: 🟡 FIRST ENGINE CODE EXECUTING ON iOS
- [`scripts/platform/ios/build-engine-lib.sh`](scripts/platform/ios/build-engine-lib.sh)
  archives every census-passing TU into `ios/libs/<sdk>/libkisakcod.a` (device
  + simulator). The stub links the leaf subset (`libkisaksmoke.a`) plus a
  documented scaffolding TU ([`ios/Stub/EngineSmoke.cpp`](ios/Stub/EngineSmoke.cpp))
  and calls `Vec3NormalizeTo`, `GetMinBitCountForNum`, `I_stricmpwild`.
- Verified on simulator and device:
  `engine=len=5.00 out=(0.60,0.00,0.80) bits255=8 bits1024=11 wild=0` —
  every value exactly as predicted, including the network bit-packing path
  that runs through the LP64 `_BitScanReverse` fix.

---

## The four walls, revisited

| # | Wall | Status after M11 |
|---|---|---|
| 1 | **32-bit layout assumption** (249 asserts; fastfile deserialization) | UNCHANGED — still the fundamental wall, gates loading real game data. Compile-relaxed everywhere; runtime translation/redesign still required. LP64 fixes so far are targeted (FS boot path, msg bit-scan). |
| 2 | **Win32 platform layer** | ✅ PORTED at the census surface: pthreads threads, POSIX fs, BSD sockets, iOS entry layer, windowing seam. Volume remains in untracked TUs, but every family now has a landed, compiling exemplar to pattern-match. |
| 3 | **Binary-only x86 DLLs** (Miles/Bink/Steam) | Steam ✅ no-op backend. Miles ✅ typed stub (`mss_ios_stub.h` + no-op `AIL_*` backend) — AVAudioEngine implementation still future work. Bink untouched. |
| 4 | **Inline x86 `__asm`** (73 sites) | UNCHANGED — none blocked the census set; mechanical NEON/C rewrites remain for cg_ents and friends. |

## What a human does next (ordered, updated)

1. **Renderer runtime bring-up** (the new #1): link `libdxvk_d3d9.a` + MoltenVK
   + SDL2-iOS into the stub, create an SDL window over the CAMetalLayer view,
   call `Direct3DCreate9` and drive one clear+present through
   DXVK→Vulkan→MoltenVK→Metal. Success = the stub's triangle replaced by a
   D3D9-cleared frame. All libraries exist; this is integration work with
   real unknowns (MoltenVK feature coverage for DXVK's d3d9 requirements —
   check `MVK_ALLOW_METAL_ARGUMENT_BUFFERS`, tessellation-free SM3 paths).
2. **Grow the engine archive toward `Com_Init`**: graduate TUs in
   census-guided waves (dvar.cpp, com_memory.cpp mmap port, common_mp.cpp,
   database/*) until the smoke test can call `Com_Init` with a stub
   filesystem — the "engine boots headless on iOS" milestone.
3. **Wall 1 for real**: 64-bit fastfile layout translation (or offset-handles
   per DEPENDENCY_MAP §12), gated behind a working headless boot.
4. **Audio**: AVAudioEngine behind the existing `AIL_*` stub surface.
5. **Game data**: user-supplied COD4 assets into `Documents/` via the Files
   app (fs_homepath already points there).

## Honest inventory

- Nothing renders through DXVK yet — the library links, the engine's renderer
  TU compiles, but no Vulkan instance has been created on iOS in this repo.
- The engine smoke test executes 3 leaf functions; 21 externs are satisfied by
  documented scaffolding stubs in `EngineSmoke.cpp`, not real subsystems.
- `-fms-extensions`, relaxed layout asserts, and `-Wno-c++11-narrowing` remain
  probe-wide; each is grep-able and confined to `KISAK_PLATFORM=ios`.
- The win32 build's byte-identity relies on `#else` branches preserving the
  original code verbatim; the Windows CI regression build is the enforcement
  mechanism (dispatched on the M8–M11 tree — see badge/Actions).
- Miles/Bink remain stubs; sound is silent by design until the AVAudioEngine
  backend lands.
