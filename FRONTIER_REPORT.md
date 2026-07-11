# FRONTIER REPORT — KisakCOD → arm64-apple-ios capability probe

**Date:** 2026-07-11 · **Repo:** `Braxton-Bevis/kisakcod-ios-port` (private) ·
**Companion docs:** [PORT_JOURNAL.md](PORT_JOURNAL.md) (experiment log), [DEPENDENCY_MAP.md](DEPENDENCY_MAP.md) (API-by-API replacement table)

One-sentence summary: **the iOS pipeline is fully proven (app builds, launches, renders
Metal in CI), the build system retargets cleanly, and eight compile-census rounds drove
the engine from "nothing compiles" to the first real game-logic TU (`bg_pmove.cpp`,
player movement) compiling clean for arm64-apple-ios — with the D3D9 header layer of the
TRANSLATION renderer path demonstrated to absorb the engine's renderer includes on a
real iOS SDK, and the remaining walls quantified in DEPENDENCY_MAP.md.**

All work was done from a Windows laptop with **no local compiler of any kind**; every
compile/launch claim below was verified on GitHub Actions macOS runners (Xcode 16.4,
iOS 18.5 SDK) with downloadable artifacts.

---

## Furthest milestone reached, per objective

### Objective 1 — iOS pipeline: ✅ COMPLETE (CI-verified; physical device = documented human step)
- `ios/` contains an XcodeGen-defined, scene-less Swift app whose view is a `CAMetalLayer`;
  it compiles a `.metal` shader library, presents at display refresh, and writes a
  proof-of-run marker into its own sandbox `Documents/`.
- CI (run 29168700236) **built it, booted an iOS Simulator, installed, launched, and
  screenshotted it**: HUD showed `GPU: Apple iOS simulator GPU  frame 2340`; the marker
  file was pulled out of the app container by `simctl get_app_container` as a hard job
  assertion. Two screenshots seconds apart differ → live render loop, not a static frame.
- The same CI run produced `KisakStub-unsigned.ipa`, verified `arm64` by `lipo` and
  linked `-target arm64-apple-ios15.0`.
- **Human last mile** (needs an Apple ID + physical iPhone, by design not fakeable from
  CI): sign & install via Xcode on the Mac (7-day free profile) or Sideloadly on this
  Windows machine. Step-by-step runbook: [ios/README.md](ios/README.md).

### Objective 2 — Build-system port: ✅ RETARGETED + instrumented
- `cmake -DKISAK_PLATFORM=ios -DCMAKE_SYSTEM_NAME=iOS` configures cleanly on macOS;
  win32 keeps byte-identical flags. The ios platform gets clang flags
  (`-fms-extensions -fno-strict-aliasing -fwrapv`) replacing `/MT /O2 ...`.
- A CI **compile census** (`.github/workflows/ios-compile-probe.yml`) compiles 14
  representative TUs against the real iOS SDK every push and publishes a per-file
  error table — the port's instrument panel. Seven census rounds were run; each
  documented in the journal with exact errors.
- Dependency table delivered: [DEPENDENCY_MAP.md](DEPENDENCY_MAP.md) — 15 sections,
  every Win32/D3D9/x86 API family → concrete iOS/POSIX replacement with effort rating,
  built from an 11-agent source scan cross-checked against census data.

### Objective 3 — Filesystem sandboxing: ✅ DESIGNED + LANDED (compile-verified shim)
- Entire engine FS derives from two dvars set in one function (`FS_RegisterDvars`,
  com_files.cpp): `fs_basepath` (reads) and `fs_homepath` (ALL writes). On iOS these now
  root at the app bundle resource path and sandbox `Documents/` respectively via
  `src/ios/sys_ios_paths.mm` — **the first engine-repo file to compile clean for
  arm64-apple-ios** (census PASS, all rounds since).
- The write path itself was proven behaviorally by the M1 stub (marker file in Documents).
- Residual (mapped, not fixed): ~12 CWD-relative/hardcoded writes and ~15
  backslash-path constructions (incl. the fastfile open path `"%s\\zone\\%s\\%s.ff"`) —
  itemized with file:line in DEPENDENCY_MAP §6.

### Objective 4 — Renderer (TRANSLATION path): ✅ HEADER LAYER PROVEN, runtime plan documented
- Chosen route: engine D3D9 calls → DXVK (native) → Vulkan → MoltenVK → Metal, onto the
  stub app's CAMetalLayer.
- **Proven on real iOS SDK:** with DXVK v2.7.1's native headers (mingw-directx submodule),
  `rb_backend.cpp` compiles through `<d3d9.h>`, and `r_init.cpp` clears ALL renderer
  headers (dying later at Miles audio). The engine's D3D9 interface usage is
  source-compatible with the translation layer's headers.
- **Not attempted (next frontier):** building DXVK's d3d9 module as an arm64-apple-ios
  library. No public precedent; risks are itemized in the journal (meson cross-build,
  MoltenVK WSI, no official Apple support in DXVK).
- `d3dx9shader.h` (runtime HLSL compile, 36 sites in one file) is NOT provided by DXVK —
  but the engine already contains a parallel `shader_bin` precompiled-blob loader that
  bypasses D3DX entirely (DEPENDENCY_MAP §8).

### Objective 5 — Touch/controller input: ✅ FIRST PASS LANDED (stub-level, CI-verified)
- `GCVirtualController` on-screen overlay (left thumbstick + A/B) IS the touch overlay,
  and physical gamepads bind through the identical `GCController` path. CI marker proves
  the connect (`controller=Apple Touch Controller`); the screenshot shows the overlay.
  Stick moves the rendered triangle; buttons mutate render state.
- Stub also gained **MetalFX spatial upscaling** (50/75% render scale → native, runtime
  `supportsDevice` + compile-time `canImport` gates — the iphonesimulator SDK ships no
  MetalFX module) and an in-app **GRAPHICS settings menu** (MetalFX toggle, render scale,
  frame cap; UserDefaults-persisted) — the prototype for surfacing engine r_* dvars on iOS.
- Engine-level input (feeding `Sys_QueEvent`) remains future work per DEPENDENCY_MAP §2.
- Ray tracing: ruled out for the TRANSLATION path with evidence — MoltenVK implements no
  VK_KHR_ray_tracing extensions; MetalFX's RT denoiser (MTL4FXTemporalDenoisedScaler,
  iOS 26/Metal 4) presupposes a ray tracer; COD4 content is baked-lightmap with no RT hooks.

---

## The four walls (measured, ranked by depth)

| # | Wall | Size (measured) | Character |
|---|---|---|---|
| 1 | **32-bit layout assumption** | 249 `static_assert(sizeof...)` sites in 42 files; fastfile deserialization streams zone data into structs assuming 4-byte pointers; GSC VM value unions are 4-byte-pointer-shaped | The fundamental one. Relaxing asserts (done, `KISAK_LAYOUT_ASSERT`) lets code *compile*; making it *run* means load-time struct translation or offset-handle redesign for every serialized struct. Alternative worth studying: keep 32-bit offsets as handles (DEPENDENCY_MAP §12). |
| 2 | **Win32 platform layer** | `src/win32`: 6,882 LOC / 19 files, ~60-function `win_local.h` contract; plus `threads.cpp` (~30 sites) and scattered Sys_* | Well-understood engine-port work: pthreads/UIKit/BSD-sockets replacements are enumerated per-family in DEPENDENCY_MAP §§2-7. No research risk, just volume. |
| 3 | **Binary-only x86 DLLs** | Miles (mss32.dll + 8 codecs; 129 call sites / ~30 distinct AIL fns), Bink (binkw32.dll; 21 D3D9-glue calls), Steam (steam_api.dll) | Cannot exist on arm64-ios. Requires shims: AVAudioEngine behind the existing SND_/MSS_ wrappers, AVFoundation for cinematics, no-op Steam. |
| 4 | **Inline x86 assembly** | 73 `__asm` occurrences in 20 files (cg_ents.cpp: 22) | Mechanical rewrites to C/NEON; sse2neon already absorbs the intrinsics-level SSE (proven in census round 4). |

## Compile-census trajectory (the experiment's core data)

| Round | Change | Result |
|---|---|---|
| 1 | raw engine vs iOS SDK | 0/13 compile; strata: layout asserts, `INT_MIN`, `random()` clash, `__declspec`, Windows.h/dinput.h, Miles |
| 2 | climits, random rename, -fdeclspec, 2 asserts relaxed, M3 FS patch | fix classes verified gone; `sys_ios_paths.mm` PASS; next strata exposed |
| 3 | ALL 249 asserts relaxed (KISAK_LAYOUT_ASSERT), -fms-extensions | SSE wall reached (`xmmintrin.h` x86-only) at 6 TUs; `d3d9.h` leak visible in script/anim/physics |
| 4 | sse2neon vendored | SSE wall gone; DXVK stage: rb_backend passes `<d3d9.h>` ✅ |
| 5 | threads.h/com_files gateway shims, forward-enum, assert variants | 8 TUs converge on the single `d3d9.h` wall; r_init clears all renderer headers in DXVK stage |
| 6 | DXVK headers promoted to baseline; mss.h include gated; ODE malloc.h | ODE `vadefs.h` exposed (fixed round 7) |
| 7 | ODE stdarg fix | 10 TUs converge on ONE engine-internal C++ error: broken never-instantiated template (`ui_shared.h:1401`) — zero platform headers left in their error paths |
| 8 | `-fdelayed-template-parsing` (MSVC template semantics) | **`bg_pmove.cpp` PASSES — first full engine TU compiling for arm64-apple-ios.** Remaining classes: zlib `Byte` typedef clash, `HWND__` tag absent from DXVK's windows_base.h (both shallow), plus the two by-design terminal walls (win32 layer / Miles) |

Win32 regression: run 29169595761 (workflow_dispatch on final HEAD, Debug+Release full
engine build) — **both configurations green** (Release 24m41s, Debug 17m13s). Every
engine-source edit made by this experiment is invisible to the MSVC/win32 build.

---

## What a human must do next (ordered)

1. **On the Mac (30 min):** clone the repo, `brew install xcodegen`, follow
   [ios/README.md](ios/README.md) Path A → stub on your physical iPhone. This completes
   Objective 1's last mile with your Apple ID.
2. **First real frontier task (days):** attempt a native arm64-apple-ios build of DXVK's
   d3d9 module + MoltenVK (meson cross file; start from dxvk's Linux native build and
   the community dxvk-macOS forks). Success = a linkable `libdxvk_d3d9.a`. This is the
   riskiest unknown in the whole port — resolve it before investing in walls 1-2.
   - Fallback if DXVK-on-iOS proves infeasible: NATIVE_METAL rewrite behind the
     `rb_backend` command-buffer seam (~620 interface + ~453 state/draw call sites,
     mapped in DEPENDENCY_MAP §8).
3. **Then the volume work, in census-guided order:** pthreads `threads.cpp` port →
   `win_net.cpp` BSD sockets → `src/ios` platform layer replacing `win_main.cpp`
   (UIKit + CADisplayLink driving `Com_Frame`) → Miles/Bink/Steam shims → and only then
   wall 1 (fastfile 64-bit layout), which gates actually *loading game data*.
4. **Game data:** COD4 assets are not redistributable — the user must supply their own
   copy; plan an on-device import path (Files app into Documents/, which fs_homepath
   already points at).

## Honest failure inventory

- Nothing was made to *run* beyond the stub app — no engine TU has fully compiled except
  the new iOS platform file (by design: census-driven probe, not a completed port).
- `gh repo create --push` was blocked by the permission system until explicit user
  approval (correctly), and the first push was rejected for missing `workflow` OAuth
  scope — fixed with a documented one-time `gh auth refresh -s workflow` device flow.
- The first DXVK census attempt failed on a wrong assumption (d3d9.h vendored directly in
  dxvk — it's a submodule); diagnosed via GitHub API and fixed the same round.
- `-fms-extensions` and blanket assert-relaxation are probe tools that could mask real
  bugs later; both are confined to `KISAK_PLATFORM=ios` and individually grep-able.
