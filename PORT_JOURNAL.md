# PORT_JOURNAL — KisakCOD → arm64-apple-ios capability probe

Experiment log. Format per entry: **Attempted / Concrete change / Compiled-Ran? / Exact error / Next hypothesis.**
Goal hierarchy: (1) iOS pipeline, (2) build retarget + dependency map, (3) FS sandboxing, (4) renderer via TRANSLATION (D3D9→DXVK→Vulkan→MoltenVK→Metal), (5) touch input.
This is a capability experiment; partial, well-documented progress is the success criterion.

---

## M0 — Environment reality check (2026-07-11)

**Attempted:** Establish what this development seat (Windows 11 laptop) can and cannot do for an iOS target.

**Findings (verified by direct probes):**

| Capability | Status | Evidence |
|---|---|---|
| Xcode / xcodebuild / xcrun | ❌ absent | `Get-Command` — not on PATH; Xcode does not exist for Windows |
| Visual Studio / MSVC | ❌ absent | `vswhere.exe` missing — no VS install on this machine |
| clang / cmake / ninja | ❌ absent | not on PATH |
| git | ✅ | `C:\Program Files\Git\cmd\git.exe` |
| GitHub CLI, authenticated | ✅ | `gh auth status` → account **Braxton-Bevis**, scopes incl. `repo` |
| Apple Mobile Device Service | ✅ running (iTunes drivers) | `Get-Service` |
| iPhone attached right now | ❌ | PnP entries present but status `Unknown` (historical) |
| Local iOS SDK | ❌ | ships only with Xcode |

**Consequence — pipeline architecture:** every compile, link, sign, and launch step for iOS
must run on a **GitHub Actions macOS runner** (free Xcode + iOS SDK + Simulator). This
Windows seat authors code + project spec + workflows, pushes, and verifies runner output
(logs, screenshots, artifacts). The user has confirmed they will move to a real Mac for
finalization; everything here must therefore be reproducible from a clean `git clone`.

**On-device launch scoping:** a *physical* device launch requires an Apple ID signing
identity (interactive) + attached iPhone. Neither is available to the agent. Achievable
proxy, in decreasing strength: (a) CI builds the app and **launches it in an iOS
Simulator, with screenshot + in-app sandbox marker file as proof**; (b) CI emits an
**unsigned device .ipa** artifact + a documented Sideloadly (Windows) / Xcode (Mac)
signing runbook for the human last-mile. Both are in scope for M1. Physical launch is
logged as a HUMAN-STEP, not attempted-and-faked.

**Repo baseline (KisakCOD-master snapshot, 29.5 MB / 1034 files):**
- CMake ≥3.16, three targets: `KisakCOD-sp`, `KisakCOD-mp`, `KisakCOD-dedi` (scripts/{sp,mp,dedi}).
- Win32/x86 only: `cmake -G "Visual Studio 17 2022" -A Win32`, MSVC flags (`/MT /O2 ...`), DirectX SDK 2010 (D3D9).
- Platform override mechanism already exists: `scripts/platform_override.cmake` swaps
  `src/<path>` for `src/_platform/<platform>/<path>` per file. **`scripts/platform/linux/platform.cmake`
  is an empty placeholder** — no Linux/macOS port exists; mechanism is usable for an `ios` platform.
- Proprietary Win32-x86-binary deps linked into mp: **binklib (Bink video), msslib (Miles Sound System), steamsdk**.
  No ARM64/iOS versions exist → must be stubbed on iOS.
- Existing CI: `.github/workflows/build-kisarcod-win.yaml` (Windows, triggers on `master` only).
  New repo uses default branch `main` so it stays dormant until deliberately invoked.

**Next hypothesis:** a minimal XcodeGen-defined Swift+CAMetalLayer app, built and
simulator-launched entirely in CI, is achievable in one milestone with zero engine code.

---

## M1 — iOS pipeline: stub app builds, launches, renders (2026-07-11) ✅

**Attempted:** Objective 1 — code-signed-shell pipeline: Xcode project, bundle ID,
build, launch, Metal layer, all reproducible.

**Concrete changes:**
- `ios/project.yml` (XcodeGen spec, bundle ID `dev.braxton.kisakstub`, iOS 15.0 target)
- `ios/Stub/{AppDelegate,MetalViewController}.swift`, `Shaders.metal` — scene-less UIKit
  app, `CAMetalLayer`-backed view, `CADisplayLink` render loop, RGB triangle + animated
  clear color; writes `Documents/metal_first_frame.txt` into its own sandbox after the
  first presented frame.
- `.github/workflows/ios-stub.yml` — two macOS-15 runner jobs (Xcode 16.4 / iOS 18.5 SDK).
- Repo pushed to **private GitHub repo `Braxton-Bevis/kisakcod-ios-port`** (user approved;
  needed a one-time `gh auth refresh -s workflow` device-flow for workflow-file push).

**Compiled/Ran?** ✅ Run 29168700236, both jobs green on first attempt:
- `simulator-launch-proof` (5m40s): built for iphonesimulator, booted an iPhone sim,
  installed, launched. **Evidence pulled and inspected locally:** screenshot shows
  triangle + HUD `GPU: Apple iOS simulator GPU  frame 2340`; two screenshots seconds
  apart differ (live loop); in-sandbox marker file retrieved by CI
  (`gpu=Apple iOS simulator GPU, drawableSize=(1206.0, 2622.0)`).
- `device-ipa-unsigned` (24s): real-device binary, `lipo` → `Non-fat file ... architecture: arm64`,
  linked `-target arm64-apple-ios15.0`. Artifact `KisakStub-unsigned.ipa` (33.6 KB).

**Errors:** one pipeline stumble, not code: first `git push` rejected —
`refusing to allow an OAuth App to create or update workflow ... without workflow scope`.
Fix: `gh auth refresh -h github.com -s workflow` (documented for reproducibility).

**Remaining HUMAN-STEP (not a blocker for the experiment):** physical-device launch =
sign + install. Two documented paths in `ios/README.md`: Xcode automatic signing on the
user's Mac (free Apple ID, 7-day profile) or Sideloadly on this Windows box with the CI
IPA. Everything up to the signature is machine-verified.

**Next hypothesis (M2):** the `-DKISAK_PLATFORM=ios` CMake graph configures cleanly on
macOS, and a per-file clang census against the iOS SDK will show a portability gradient:
game logic (bg_*, g_*) nearly clean; qcommon/universal failing on Win32 headers;
win32/ and gfx_d3d/ failing catastrophically.

---

## M2 — Build retarget + compile census, rounds 1–2 (2026-07-11)

**Attempted:** retarget the build graph to arm64-apple-ios and measure the *real*
compile error surface of 13 representative TUs against the actual iOS SDK
(Xcode 16.4 clang, `-target arm64-apple-ios15.0`), on the macOS runner.

**Concrete changes (round 1):**
- Root `CMakeLists.txt`: `KISAK_PLATFORM` now overridable (`-DKISAK_PLATFORM=ios`);
  ios platform routes to a new probe subproject instead of mp/sp/dedi.
- `scripts/platform/ios/platform.cmake` (clang flags replace `/MT /O2 ...`),
  `scripts/ios/CMakeLists.txt` (OBJECT-library census target),
  `.github/workflows/ios-compile-probe.yml` (configure check + per-file census + artifact).

**Compiled/Ran?** CMake configure with `-DKISAK_PLATFORM=ios -DCMAKE_SYSTEM_NAME=iOS`
**succeeds** on macOS (run 29168930111). Census round 1: **0/13 TUs compile.** Exact
first-errors, by class:

| Class | Example (verbatim) | Depth |
|---|---|---|
| x86-32 layout asserts | `static assertion failed due to requirement 'sizeof(StringTable) == 16'` (ptr,int,int,ptr = 24 on LP64) | FUNDAMENTAL |
| Missing C header | `use of undeclared identifier 'INT_MIN'` (MSVC leaks `<climits>`) | trivial |
| POSIX symbol clash | `float __cdecl random()` vs stdlib `long random(void)` → "functions that differ only in return type cannot be overloaded" | small |
| MSVC keywords | `__declspec(align(128))` rejected; later `__forceinline`, `unsigned __int32` bitfields | one flag |
| Win32 headers | `'Windows.h' file not found` (qcommon/threads.h), `'dinput.h' file not found` (win32/win_local.h) | subsystem port |
| Binary-only dep | `MSS.H did not detect your platform` (Miles Sound has no iOS/ARM64 library) | stub/replace |

**Round 2 fixes:** `<climits>` in q_shared.h; `random()`→`q_engine_random()` token rename
(iOS only, stdlib pre-included so include-guards protect its decl); `-fdeclspec`;
`KISAK_LAYOUT_ASSERT` macro (relaxed on iOS with grep-able paper trail, unchanged on win32)
applied to the two firing asserts; **M3 filesystem patch** (below) included.

**Round 2 result (run 29169065384):** all round-1 fix classes verified gone.
**`src/ios/sys_ios_paths.mm` PASSES** — first engine-repo TU to compile clean for
arm64-apple-ios. Census reached the next stratum: more layout asserts (SpawnVar 2572,
VariableStackBuffer 12, ParseThreadInfo 17932), `__forceinline` (→ `-fms-extensions`),
`__int32` bitfields (same flag).

**Assessment of the deep wall (measured, not guessed):**
- **249 `static_assert(sizeof ...)` sites in 42 files** — the decomp pins x86-32 struct
  layouts, which fastfile/asset deserialization depends on. Round 3 relaxes ALL of them
  mechanically via `KISAK_LAYOUT_ASSERT` to let the census reach what lies beneath;
  actually *running* on arm64 will require real 64-bit layout work for every serialized
  struct (this is the single largest porting cost in the codebase).
- **73 inline x86 `__asm` occurrences in 20 files** (cg_ents.cpp alone: 22) — each needs
  a C or NEON rewrite.

**Next hypothesis (round 3):** with all layout asserts relaxed + `-fms-extensions`, the
portable-logic TUs (bg_pmove, g_main_mp) get much deeper — likely all the way to success
or to Win32-header includes; gfx_d3d TUs reach `d3d9.h` (then the DXVK-native header
stage measures the translation path); threads/win_main stay walled behind Win32 headers.

---

## M3 — Filesystem sandboxing (2026-07-11) — code landed, compile-verified

**Attempted:** Objective 3 — root every engine write in the iOS app sandbox.

**Analysis:** the FS is Quake-lineage. Every path in the engine derives from two dvars
registered in `FS_RegisterDvars` (src/universal/com_files.cpp):
`fs_basepath` ← `Sys_Cwd()` (read-only game data) and `fs_homepath` ← NULL-stub →
falls back to basepath (ALL writes: configs, players/, mods/, screenshots). The
decomp's own homepath source was already stubbed (`RETURN_ZERO32()`), so the seam is
exactly two assignments.

**Concrete change:**
- New `src/ios/sys_ios.h` + `src/ios/sys_ios_paths.mm`: `Sys_iOS_BundlePath()` (app
  bundle resources, read-only game data), `Sys_iOS_DocumentsPath()` (persistent writable),
  `Sys_iOS_CachesPath()` (purgeable) via NSFileManager/NSBundle, cached as C strings the
  dvar system can hold forever.
- `com_files.cpp` `FS_RegisterDvars`: `#ifdef KISAK_IOS` → basepath = bundle path,
  homepath = Documents. Win32 codepath byte-identical.

**Compiled/Ran?** sys_ios_paths.mm compiles clean for arm64-apple-ios (census round 2).
The com_files.cpp patch itself can't compile-verify until the file's other blockers fall
(it's in the census list, so this is tracked automatically). Behavioral proof of the
write path: the M1 stub already wrote+read back a file in Documents/ under CI.

**Known residual risk (documented, deferred):** the engine has scattered direct-path
file I/O outside FS_* (dependency-scan agents are mapping every fopen/CreateFile site);
those sites must be audited to route through FS_BuildOSPath once the engine links.

---

## M2 — Census round 3 + fixes for round 4 (2026-07-11)

**Attempted:** relax the entire layout-assert stratum in one mechanical move and see
what the census reaches underneath; add the DXVK-native header stage (Objective 4).

**Concrete changes:** scripted conversion of all remaining 247 `static_assert(sizeof`
sites (41 files) to `KISAK_LAYOUT_ASSERT` (variadic — 3 sites carry message args) with
automatic `#include <universal/kisak_layout.h>` insertion; `-fdeclspec` →
`-fms-extensions` (also unlocks `__forceinline`, `unsigned __int32` bitfields).

**Compiled/Ran?** Run 29169151347: census step ran; results (from step log):
- The assert stratum is GONE. Six TUs (bg_pmove, g_main_mp, sv_main_mp, cmd, com_files,
  r_init) now reach **`xmmintrin.h:14: "This header is only meant to be used on x86 and
  x64 architecture"`** — the SSE-intrinsics stratum. Poison source is a single site:
  `qcommon/qcommon.h:1570-71` includes `<xmmintrin.h>` + `<intrin.h>` unconditionally.
- Three TUs (scr_vm, xmodel, phys_ode) reach **`'d3d9.h' file not found`** via
  `gfx_d3d/r_gfx.h` / `r_material.h` — the D3D9 renderer types leak into script VM,
  animation, and physics headers. Confirms the TRANSLATION-path header shim matters far
  beyond gfx_d3d itself.
- threads.cpp (Windows.h), win_main/win_common (dinput.h), snd_mss (Miles) unchanged.

**Error (own bug):** the new DXVK header stage FAILED — `BLOCKER: no d3d9.h in dxvk
include tree`. Diagnosis via GitHub API: `include/native/directx` in doitsujin/dxvk is a
**git submodule** → `Joshua-Ashton/mingw-directx-headers`; `git clone --depth 1` leaves
it empty. Fix: `--recurse-submodules=include/native/directx --shallow-submodules`.

**Round 4 changes (pushed):** vendored `deps/sse2neon/sse2neon.h` v1.8.0 (MIT, SSE→NEON)
gated by `KISAK_IOS` at the qcommon.h poison site (`<intrin.h>` MSVC intrinsics get no
blanket shim — per-site surfacing is intentional); DXVK submodule clone fix;
`workflow_dispatch` on the upstream Windows CI → **win32 regression build triggered**
(run 29169268868) to prove the ~50-file src/ touch didn't break MSVC.

**Next hypothesis:** round 4 gets bg_pmove/g_main_mp very deep (possibly PASS); the DXVK
stage compiles r_init.cpp substantially further against mingw-directx d3d9.h; the win32
build stays green (all engine-code edits are `#ifdef KISAK_IOS`-guarded or
semantics-preserving macros).

---

## M2/M4 — Census rounds 4–5: SSE cleared, D3D9 header wall PROVEN SOLVABLE (2026-07-11)

**Round 4 (run 29169266083) results:**
- **sse2neon works.** The xmmintrin wall is gone from all six affected TUs.
- **DXVK stage headline:** `rb_backend.cpp` compiled *through* `<d3d9.h>` using DXVK's
  mingw-directx submodule headers on arm64-apple-ios — first hard evidence the
  TRANSLATION path absorbs the engine's D3D9 interface usage at source level. Its next
  error was engine-internal (`unknown type name 'byte'` — include-order, r_gfx.h used
  standalone), not a D3D9 gap.
- New shallow strata: MSVC forward-enum decl (`enum team_t;` — exactly ONE in the tree,
  fixed with `: __int32` matching game/teams.h:7), an assert variant
  `static_assert((sizeof...)` the transform regex missed (2 sites), and the two gateway
  headers: `qcommon/threads.h` (Windows.h for literally two typedefs — replaced with
  `DWORD`/`HANDLE` typedefs on iOS) and `win32/win_local.h` (dinput.h; include gated
  out of com_files.cpp for iOS).

**Round 5 (run 29169370283) results — convergence:**
- 8 of 13 engine TUs now funnel to a single wall: `'d3d9.h' file not found`
  (rb_backend.h:3 and r_gfx.h:4 leak it engine-wide: cmd, threads, com_files, g_main_mp,
  sv_main_mp, scr_vm, xmodel, phys_ode, r_init).
- In the DXVK stage, `r_init.cpp` passed ALL renderer headers and died at Miles
  (`mss.h`, pulled via snd_public.h:5 — which uses zero Miles types; include was
  gratuitous at that layer, now gated). Note mss.h also collides with DXVK's
  windows_base.h (`typedef void* LPSTR` vs `char*`) — Miles' Win32 entanglement is total;
  stub/replace is the only route (already scoped in DEPENDENCY_MAP).
- bg_pmove reached deps: `ode/common.h:29 'malloc.h' file not found` — classic Apple
  gap, fixed with `__APPLE__` gate (stdlib.h+alloca.h).

## M2/M4 — Census rounds 6–8: first engine TU compiles (2026-07-11)

**Round 6** (DXVK headers promoted to census baseline; snd_public.h's gratuitous mss.h
include gated; `byte` typedef for standalone r_gfx.h; ODE malloc.h Apple gate): exposed
`deps/ode/error.h:30 #include <vadefs.h>` — clang's MSVC-compat vadefs.h does
`#include_next` into a Microsoft CRT that doesn't exist on iOS. Fixed with `_MSC_VER`
gate → `<stdarg.h>`.

**Round 7:** 10 of 13 engine TUs converged on a single error —
`ui_shared.h:1401: use of undeclared identifier 'IsValidSeed'`. Inspection: a broken,
never-instantiated template member (`KeywordHash_PickSeed` calls a function that exists
nowhere; sibling `KeywordHash_IsValidSeed` even lacks a return). MSVC never parses
uninstantiated template bodies; ISO clang does. **Significance: those 10 TUs had ZERO
platform/dependency headers left in their error paths** — the frontier shifted from
"missing Windows" to "MSVC-permissive C++ dialect."

**Round 8** (`-fdelayed-template-parsing`, clang's MSVC template-semantics switch):

> **`src/bgame/bg_pmove.cpp` — PASS.** The player-movement code of COD4 compiles clean
> for `-target arm64-apple-ios15.0`. First full engine translation unit through, 8
> rounds after 0/13.

Remaining failure classes at experiment close (all characterized):
- `deps/zlib/zconf.h:223 unknown type name 'Byte'` (3 TUs) — vendored-zlib typedef
  interaction, shallow.
- `r_init.h:76 unknown type name 'HWND__'` (5 TUs) — engine forward-references the
  Win32 `HWND__` struct tag; DXVK's windows_base.h typedefs `HWND` without the tag.
  One-line shim (`struct HWND__ { int unused; };`) or engine-side `#ifdef`, shallow.
- `dinput.h` (3 TUs: win_main, win_common, phys_ode via win_local.h) — the win32
  platform layer itself; terminal by design (gets REPLACED, not compiled, on iOS).
- `mss.h` (snd_mss.cpp) — Miles Sound System; terminal by design (binary-only Win32
  dep; AVAudioEngine shim scoped in DEPENDENCY_MAP §9).

Census loop closed here deliberately: every remaining error is either shallow-mechanical
or a scoped replace-subsystem item. Continuing rounds would be porting, not probing.

## M6 — Stub v2/v3: MetalFX upscaling, controller support, graphics settings menu (2026-07-11)

**Attempted (user request):** MetalFX + controller support + in-app graphics settings.
Ray tracing itself ruled out with documented evidence: MoltenVK implements no
VK_KHR_ray_tracing/ray_query (so the TRANSLATION path structurally cannot carry RT);
MetalFX's RT piece (MTL4FXTemporalDenoisedScaler) is iOS 26/Metal 4 and presupposes a
ray tracer producing noisy RT output; COD4 lighting is baked lightmaps with no RT hooks.
MetalFX **spatial upscaling** (MTLFXSpatialScaler, iOS 16+) is the applicable piece.

**Concrete changes (ios/Stub):**
- Scene renders at a configurable scale (50/75/100%) into an offscreen texture;
  `MTLFXSpatialScaler` upscales into the drawable (`framebufferOnly=false`), gated by
  `supportsDevice` at runtime AND `#if canImport(MetalFX)` at compile time.
- Controller support: `GCVirtualController` (left thumbstick + A/B on-screen overlay —
  doubling as the Objective-5 touch overlay) and physical controllers via
  `GCControllerDidConnect`, one shared bind path. Stick moves the triangle (shader
  offset uniform), A switches palette, B recenters.
- GRAPHICS settings menu (gear button): MetalFX on/off, render scale, frame cap
  (30/60/max via `CADisplayLink.preferredFrameRateRange`), persisted in UserDefaults,
  panel auto-shown on first launch. Stub-scale preview of the engine's future
  r_metalfx/r_renderScale dvar surface — the engine's own graphics menu (decompiled
  ui/ code) cannot run until the engine renders.

**Errors hit (real, fixed):** first v2 push failed on the simulator job only:
`error: no such module 'MetalFX'` — the **iphonesimulator SDK in Xcode 16.4 does not
ship the MetalFX module at all** (device SDK does; device job passed). Fix:
compile-gate with canImport, honest fallback status string.

**Compiled/Ran?** v2-fix run green: simulator marker file shows
`controller=Apple Touch Controller` (virtual controller connected + bound) and
`metalfx=module absent in this SDK (simulator)`; screenshot shows A/B buttons rendered
on screen. Device IPA (with live MetalFX path) builds green. v3 (settings menu) result
recorded below.

## FINAL — win32 regression verdict (2026-07-11) ✅

Run 29169595761 (workflow_dispatch on final HEAD, full engine Debug + Release):
**both green** (Release 24m41s, Debug 17m13s). All ~50 engine-source files touched by
the experiment (layout-assert macro conversion across 43 files, `#ifdef KISAK_IOS`
gates in com_files/com_math/qcommon/threads.h/snd_public.h/bg_public.h, ODE header
gates, r_gfx.h `byte` typedef) are invisible to the MSVC/win32 build. The probe is
non-destructive to the upstream target. Experiment closed — see FRONTIER_REPORT.md.

**Decision (Objective 4):** DXVK native headers are now the *census baseline* — the
TRANSLATION path is formally chosen. `scripts/platform/ios/platform.cmake` accepts
`-DDXVK_NATIVE_INCLUDE=<dxvk>/include/native` for the CMake path; CI pins dxvk v2.7.1
(with the `--recurse-submodules=include/native/directx` lesson baked in).

**Runtime plan for the translation stack (documented, NOT yet attempted):**
engine d3d9 calls → DXVK d3d9 module built *native* for iOS → Vulkan → MoltenVK →
CAMetalLayer (the stub app's layer, already proven on-device-class). Honest risk
register: (1) DXVK has no official Apple support — community D3D9-on-MoltenVK forks
exist for macOS, none for iOS; building libdxvk_d3d9 for arm64-apple-ios is
unexplored territory (meson cross file + MoltenVK static lib + no-JIT constraints).
(2) DXVK's WSI needs an iOS backend (SDL2-iOS or a small custom CAMetalLayer WSI).
(3) `d3dx9shader.h` (r_material_load_obj.cpp) is NOT part of DXVK — D3DX shader
compilation must be stubbed; COD4 fastfiles carry precompiled SM3 bytecode which DXVK
consumes directly, so runtime HLSL compilation is avoidable. Item (1) is the next
frontier past this experiment's horizon.

---
