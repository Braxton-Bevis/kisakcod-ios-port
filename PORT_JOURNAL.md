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

**Runtime plan for the translation stack (documented, NOT yet attempted — resolved in M8, see below):**
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

## M7 — The Mac arrives: entire experiment reproduced locally (2026-07-11)

**Attempted:** the FRONTIER_REPORT "what a human does next" list, on a real Mac
(macOS 26.5, Xcode 26.6, iOS 26.5 SDK, 8-core/8GB) with no Homebrew — toolchain
bootstrapped from release binaries alone (cmake 4.3.3, xcodegen 2.45.4,
meson 1.11.2 + ninja via pip, glslang from Khronos releases).

**Compiled/Ran?** ✅ everything:
- Stub app built locally first try (after `xcodebuild -downloadComponent
  MetalToolchain` — Xcode 26 ships the Metal compiler as a separate download).
- Booted an iPhone 17 Pro simulator; screenshots + sandbox marker verified
  (live render loop, virtual controller connected, settings persisted).
- The compile census reproduced EXACTLY against the iOS 26.5 SDK: same 2/14
  PASS, same failure strata as runs 29169xxxxxx on the 18.5 SDK. The
  experiment's CI-only measurements hold on real local hardware.

## M8 — Census sweep: 2/14 → 23/23, all subsystems ported (2026-07-11)

**Concrete changes (mine + six parallel subsystem ports):**
- Shared compat surface: `src/ios/msvc_crt_compat.h/.cpp` (MSVC CRT spellings →
  libc: _stricmp/_vsnprintf/fopen_s/sprintf_s/_TRUNCATE/…, Interlocked* →
  __atomic with Win32 return semantics, __rdtsc → cntvct_el0),
  `src/ios/win32_tags.h` (HWND__/HINSTANCE__/… incomplete tags, tagPOINT,
  ARRAYSIZE, Sleep → nanosleep), zlib `Byte` typedef (TARGET_OS_MAC guard bug),
  win_local.h made an iOS-safe gateway (dinput/winsock gated, DXVK typedefs).
- `qcommon/threads.cpp`: real pthreads port — events as mutex+cond,
  CREATE_SUSPENDED via start-gate, mach thread_suspend/resume, QoS classes.
- `universal/com_files.cpp`+`win_common.cpp`: POSIX port (stat/opendir/mkdir),
  plus LP64 landmines fixed on the FS boot path (DvarValue .integer-as-pointer
  reads → .string, Sys_ListFiles 4-byte-pointer sizings).
- `win32/win_net.cpp`: winsock → BSD sockets. `win32/win_steam.cpp`: explicit
  no-op backend. `sound/`: typed Miles stub (`mss_ios_stub.h` + no-op AIL_*).
- NEW `src/ios/sys_ios_main.mm`: the platform entry layer replacing
  win_main.cpp (Sys_* contract: event queue, POSIX fs, UIPasteboard clipboard,
  sysctl SysInfo; UIKit input wiring left as KISAKTODO).
- `gfx_d3d/r_init.cpp`: windowing seam — R_CreateWindow adopts the app shell's
  CAMetalLayer view via `Sys_iOS_GetHostWindow()` (KISAKTODO bring-up),
  single-screen R_ChooseMonitor, GetAdapterDisplayMode for metrics.
- cmd.cpp dumpraw dev tool gated (std::format needs iOS 16.3; LP64-unsafe).

**Census: all 23 tracked TUs compile clean for arm64-apple-ios** (list grew
from 14: + sys_ios_main, msvc_crt_compat, snd_ios_stub, g_utils_mp, huffman,
msg_mp, q_shared, com_math, win_net, win_steam; win_main.cpp retired in favor
of its replacement). Zero platform headers OR platform symbols left anywhere.

## M9 — FRONTIER RESOLVED: DXVK d3d9 builds as an arm64-apple-ios library (2026-07-11)

The "riskiest unknown in the whole port" (FRONTIER_REPORT #2) fell in an
afternoon. dxvk v2.7.1 cross-compiles for iOS with a **5-hunk patch**
(`scripts/platform/ios/dxvk-v2.7.1-ios.patch`):
`__unix__`→`+ __APPLE__` in util_win32_compat.h (the entire "no Apple support"
wall was one preprocessor gate), an lzcnt overload ambiguity (Darwin size_t),
Apple's 1-arg pthread_setname_np, a missing <cstddef>, and
static-archive-instead-of-version-script linking on Darwin. WSI = SDL2 built
for iOS (official upstream support). Result, machine-verified:

> `libdxvk_d3d9.a` — arm64, `Direct3DCreate9`/`Direct3DCreate9Ex` exported,
> 46,804 symbols, alongside libdxvk/libdxso/libwsi/libvkcommon static libs.

Reproducer: `scripts/platform/ios/build-dxvk-ios.sh`. MoltenVK v1.4.1 iOS
xcframework downloaded (prebuilt) as the Vulkan implementation for runtime
bring-up — which is the remaining unknown: instance/device creation +
SDL_Window over the CAMetalLayer + engine d3d9 calls end-to-end.

## M10 — ENGINE CODE EXECUTES ON iOS (2026-07-11)

`scripts/platform/ios/build-engine-lib.sh` archives every census-passing TU
into `ios/libs/<sdk>/libkisakcod.a` (23 TUs, device + simulator). The stub
links the leaf subset (`libkisaksmoke.a`: com_math, q_shared, msg_mp, huffman,
msvc_crt_compat — heavier TUs await their dependency closure) with a
scaffolding TU (`ios/Stub/EngineSmoke.cpp`) satisfying 21 externs into
not-yet-graduated TUs (abort-loudly stubs + zeroed globals, documented).

**Ran?** ✅ Simulator AND physical device. HUD + sandbox marker:
`engine=len=5.00 out=(0.60,0.00,0.80) bits255=8 bits1024=11 wild=0` —
Vec3NormalizeTo (player math), GetMinBitCountForNum (network bit-packing,
running through the LP64 _BitScanReverse fix), I_stricmpwild (FS wildcard) all
return exactly the predicted values.

## M11 — Physical device: iPad Pro 13" (M5), signed with the user's Apple ID (2026-07-11)

Stub v4 (graphics settings expanded: 3-tier shader quality with real pipeline
states, capability-gated ray-tracing toggle, 25–100% render scale, live
resolution readout, 30/60/120/Max cap, measured-FPS HUD) built with automatic
signing (personal team NCYQDHQ93F; first build minted the certificate),
installed and launched via devicectl on iPadOS (Developer Mode + one manual
trust tap in Settings). Marker file pulled off the device:

```
gpu=Apple M5 GPU
drawableSize=(3200.0, 2400.0)
resolution=1600×1200 → 3200×2400
metalfx=spatial 1600x1200 → 3200x2400   ← REAL MetalFX spatial upscaling
raytracing=supported, off               ← M5 reports Metal RT capability
fps=120.0                               ← ProMotion, uncapped
engine=len=5.00 out=(0.60,0.00,0.80) bits255=8 bits1024=11 wild=0
```

The HUD MetalFX line on this chip reads **"spatial 1600x1200 → 3200x2400"** —
the first time the MetalFX code path (simulator: "module absent") has executed:
the scene renders at 50% scale and MTLFXSpatialScaler feeds the drawable at
native 3200×2400. Wireless installs: the CoreDevice pairing tunnel
(iPad-of-Braxton-2.coredevice.local) serves network installs automatically
once paired — verified by devicectl after USB unplug (see README dev notes).

## M12 — RENDERER LIVE: first D3D9 frame through DXVK→Vulkan→MoltenVK→Metal on iPadOS (2026-07-11) ✅

**Attempted:** FRONTIER_REPORT next-step #1 — runtime bring-up of the TRANSLATION
renderer stack on the physical iPad Pro (M5).

**Concrete changes:**
- DXVK gains a native **iOS CAMetalLayer WSI backend** (`src/wsi/ios/`, driver
  name "iOS", selected via `DXVK_WSI_DRIVER`): the window handle IS a
  `CAMetalLayer*`; surfaces via `VK_EXT_metal_surface`; one fake monitor
  reporting the layer's size. SDL2 dropped entirely (no longer built or linked).
- `vulkan_loader.cpp`: on Apple, `dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr")`
  first — MoltenVK is statically linked into the app.
- `env::getExePath()`: `_NSGetExecutablePath` branch (the Linux-only
  /proc/self/exe left NO return path on Apple — clang's UB trap fired at
  `getExeName()+0`, the first real crash of the bring-up).
- Feature relaxations, Apple-gated, for what MoltenVK 1.4.1 cannot provide
  (each confirmed by on-device adapter-filter logs, matching the historical
  DXVK-on-macOS community set): `geometryShader`, `shaderCullDistance`,
  `robustBufferAccess2` + `nullDescriptor` (no VK_EXT_robustness2 at all),
  `VK_KHR_pipeline_library` (GPL already optional; monolithic fallbacks used).
- App side: MoltenVK static slice **force_load**ed with `DEAD_CODE_STRIPPING=NO`
  (dlsym-only symbols otherwise stripped — manifested as signal 5 with zero vk*
  symbols in the binary), `ios/Stub/D3D9Smoke.mm` (one-shot Clear → GPU readback
  → Present on a dedicated 320×240 sublayer), DXVK stderr teed into
  `Documents/dxvk_stderr.log`, crash-guard sentinel with a 3-attempt counter.

**Errors hit (all real, all fixed):** linker dropped all of MoltenVK (dlsym-only);
`getExeName` UB trap; four adapter-filter rejections discovered one device-round
at a time; two duplicate scaffolding symbols (va/Q_fabs — engine TUs define them).

**Compiled/Ran?** ✅ **On the iPad Pro (M5), pulled from the device sandbox:**

```
d3d9=adapter=AMD Radeon RX 6700 XT clear=0xFFBA55D3 read=0x00000000 px=0xFFBA55D3 present=0x00000000 (1024ms)
```

- `CreateDevice` → **D3D_OK**; MoltenVK VkInstance/VkDevice on Vulkan 1.3.334.
- `Clear(0xBA55D3)` → `GetRenderTargetData` → **read-back pixel 0xFFBA55D3 —
  bit-exact**. The D3D9 command stream executed on the M5 GPU.
- `Present` → **D3D_OK** through the new WSI + MoltenVK swapchain — the orchid
  sublayer is visible on screen next to the stub's Metal triangle.
- The adapter string is DXVK's stock game-compat spoofing (Apple vendor ID is
  unknown to its lookup table); `SetupFPU: not supported on this arch` is the
  benign x86-FPU-control path declining on arm64.

To our knowledge this is the first D3D9 frame presented through DXVK on iOS.
The engine's renderer calls (`gfx_d3d/`) now have a proven runtime target:
`R_CreateWindow`'s `Sys_iOS_GetHostWindow()` seam hands the engine this same
CAMetalLayer path. Next: point the engine's `dx.d3d9` initialization at it.

## M12 addendum — feature-relaxation audit + the boot path opens (2026-07-11)

**Audit (agent-verified against MoltenVK v1.4.1 source, not guesswork):** DXVK
2.7.1's compatibility gate has 41 required rows + 3 hard gates; on an M5,
MoltenVK satisfies ALL of them except exactly the four relaxed in M12
(geometryShader, shaderCullDistance, robustBufferAccess2, nullDescriptor) plus
VK_KHR_pipeline_library — i.e. the on-device discovery loop found the complete
set. Two follow-ups recorded for the renderer:
- **nullDescriptor is load-bearing beyond Clear/Present**: real SM2/SM3 content
  binds nothing on some stages, and DXVK's legacy descriptor path writes
  VK_NULL_HANDLE views (dxvk_context.cpp:6335ff) which is only legal under
  robustness2. Fix scoped: extend DXVK's existing dummyResources() pattern
  (already used for the maintenance6 index-buffer fallback) to descriptor
  writes + vertex buffers on Apple.
- **textureCompressionBC passes only on M-class GPUs** (MTLDevice.
  supportsBCTextureCompression). A-series iPhones would need a BC→ASTC/RGBA
  transcode path — deferred until iPhone is a target.

**Boot path:** dvar.cpp (no changes needed), com_memory.cpp (real mmap/
mprotect/madvise port of Z_Virtual*, 16KB-page-aware, MEM_RELEASE size
registry), and qcommon/common.cpp — Com_Init/Com_Frame themselves — all
census-PASS. **26/26.** common.cpp needed only: a buildnumber.h __has_include
fallback, CPUSTRING "ios-arm64", PreFetchCacheLine→__builtin_prefetch, and
_time64→time. Known LP64 landmines on the hunk allocator flagged in-place
(KISAKTODO(lp64): 32-bit pointer truncation in seven hunk functions, HunkUser
pointer-in-int fields) — these gate actually CALLING Com_Init, not compiling it.

## FF-prep - Fastfile attack plan written (2026-07-11, Windows seat)

Stream 4 of docs/NEXT_SESSION.md fulfilled: docs/FASTFILE_PLAN.md (strategy: on-device
load-time translation first, staged FF0-FF5 with house-rule gates; offline-repack
promotion gated on FF5 perf data) + docs/fastfile-struct-catalog.md (251 assert sites
cataloged; 45 pointer-bearing fastfile structs identified; full closure estimated
~150-165 layouts). Produced by a 3-reader scoping fan-out (loader mechanics / struct
catalog / community precedent incl. OpenAssetTools GPL-3.0 as adaptable, CoD4x AGPL as
read-only reference). Both docs reviewed before commit.


## Stream 1 VERIFIED - LP64 hunk allocator green on both gates (2026-07-12, Windows seat)

NEXT_SESSION stream 1 complete: 33 pointer-truncation sites converted across 14
functions in com_memory.{cpp,h} (+1 caller in cm_load_obj.cpp), HunkUser end/pos
pointer-sized under KISAK_IOS, page math at native 16KB granularity. Gates:
- iOS census 26/26 PASS (run 29176526294) - after fixing a PRE-EXISTING CI-vs-local
  drift: CI compiled against stock dxvk v2.7.1 headers while the Mac's local census
  used the M9-patched ones; CI now applies scripts/platform/ios/dxvk-v2.7.1-ios.patch
  after cloning (attribution verified against the prior run's artifact).
- Windows regression build green (run 29176449196) - all changes #else-preserve the
  original code.
Stream 2 (boot scaffold) is unblocked. FASTFILE_PLAN.md + struct catalog also landed
this session (see FF-prep entry).

## M13 — staged engine boot: hunk + dvar + command simulator-verified (2026-07-13)

**Attempted:** move beyond leaf-function smoke and execute three real engine
subsystems in order without game assets: hunk memory, dvars, and commands.

**Concrete changes:**

- Restored `ios/Stub/BootSmoke.cpp`; it initializes main-thread scratch data,
  performs a 4 KiB hunk allocation/readback/free, registers and reads
  `bmk4_boot=ipad`, and requires `Cmd_FindCommand("cmdlist")` after command init.
- Added `BootScaffold.cpp` with an explicit real-minimal versus abort-loud
  split. Closure accounting is 16 definitions in `EngineSmoke.cpp`, 57 in the
  new scaffold, and Swift-owned `main`: all 74 normalized manifest entries.
- Expanded `libkisaksmoke.a` to require eight real leaves: `com_memory`,
  `dvar`, `cmd`, `com_math`, `q_shared`, `msg_mp`, `huffman`, and
  `msvc_crt_compat`. Missing leaves now fail every build mode.
- Fixed three boot-used iOS LP64 string-pointer lanes in `dvar.cpp`; original
  Windows expressions remain byte-for-byte in `#else`.
- Swift runs the smoke on the main queue with a crash sentinel and writes the
  result to the HUD/proof file. CI hard-requires hunk/dvar/cmd marker text.
- Device CI now reproduces the real renderer stack from pinned open-source
  inputs (patched DXVK 2.7.1 and hash-verified MoltenVK 1.4.1) instead of
  relying on untracked Mac-local archives. Windows CI now follows `main`.

**Errors hit and fixed:** the raw closure contained eight Mach-O spellings;
the parked smoke omitted `Com_InitThreadData`; a background call violated the
hunk main-thread assertion; dvar string values truncated pointers through the
32-bit integer union lane; the first device CI build lacked DXVK headers and
archives. Each was fixed at its source without weakening a gate.

**Compiled/Ran?** ✅ Remote gates are green:

- iOS census run `29263799350`: **26 PASS, 0 FAIL**.
- iOS stub run `29264276000`: simulator build/launch/proof and arm64 unsigned
  device IPA both green. Exact simulator marker:

```
boot=hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=ipad), cmd OK — 3 stages up
```

- The same run's device executable is arm64, iOS platform, minimum iOS 16.0,
  with the real DXVK/MoltenVK archives linked; artifact
  `KisakStub-unsigned-ipa` uploaded.
- Windows run `29264559274`: Debug and Release green for SP, MP, and dedicated
  server.

**Evidence boundary:** M13 physical-iPad runtime is **not yet verified**. The
unsigned device build proves compilation/linkage only; a signed install and a
marker pulled from the user's iPad are the required device addendum. No COD4
assets were used or needed.

## M14 — real `bg_pmove` synthetic-world sandbox (simulator verified, 2026-07-13)

**Attempted:** execute COD4's real player-movement code against an isolated,
asset-free flat world; prove walking, jumping, landing, and friction with a
deterministic simulator contract; then drive the same state from the existing
thumbstick and show origin/velocity on the HUD.

**Concrete changes (plain-language changelog):**

- Moved all three parked pmove sources from `docs/wip/` into `ios/Stub/` so
  XcodeGen compiles the sandbox and both scaffold layers into the app.
- Kept `bg_pmove.cpp`, `bg_jump.cpp`, `bg_slidemove.cpp`, `bg_mantle.cpp`, and
  `com_math_anglevectors.cpp` as real engine objects in a new, hard-required
  `libkisakpmove.a`; a missing member now stops simulator and device builds.
- Expanded the monotonic iOS census from 26 to 30 tracked TUs. The workflow now
  fails on any tracked compile failure instead of merely recording a red row.
- Added synthetic capsule traces against the solid `z <= 0` halfspace. There
  are no COD4 assets or fabricated map data in this test world.
- Added a 240-frame fixed-step proof that calls real `Pmove` and checks forward
  displacement, jump apex and air time, landing near z=0, final grounded state,
  and friction decay. The standalone entry returns nonzero on any failed
  invariant; the app exposes the same proof string to CI.
- Replaced the old aborting `AngleVectors` app stub with the real engine leaf,
  and matched app compilation to the archive's iOS/MP defines, C++ mode,
  compiler flags, source headers, and patched DXVK headers.
- Integrated safely with M13: the real dvar system boots first, then the pmove
  proof registers the real jump/mantle defaults, and only a passing proof
  enables live movement.
- Wired left-stick y/x to forward/right, A to a queued one-frame jump edge,
  and B to held sprint. The frame loop uses measured, clamped elapsed time and
  the HUD/marker show the live origin and velocity returned by the sandbox.
- Added a crash sentinel for the movement proof and an exact fixed-line CI
  assertion while preserving the existing boot marker assertion.
- Fixed hazards in the parked scaffolds before running them: shared duplicate
  symbols are test-gated; `Sys_SnapVector` uses the engine's nearest-even
  `SnapFloat`; integer dvar defaults no longer overwrite their own union lane;
  and the mantle delta stub writes the real two-float rotation ABI.
- Broadened the iOS stub workflow trigger to cover the engine/build inputs it
  actually consumes. Windows engine sources were not changed, and no game
  files entered the tree, CI inputs, artifacts, or logs.
- Refreshed the root README, frontier report, iOS runbook, and authoritative
  handoff with exact M14 evidence and the physical-device boundary; corrected
  the runbook's documented app deployment target from iOS 15.0 to 16.0.
- Added `docs/M14_PMOVE_SANDBOX_REPORT.md`, a comprehensive report covering
  architecture, every changed file, proof invariants, hazards fixed, hosted
  and pushed-main evidence, remaining gaps, legal boundaries, and the exact
  physical-iPad checklist. Corrected the clean-clone runbook to stage its
  intentionally untracked engine/DXVK/MoltenVK prerequisites before Xcode.

**Errors hit and fixed:** static closure review caught the parked abort-loud
`AngleVectors` body on the per-frame path, missing app header/define settings,
duplicate shared symbols, non-engine snap rounding, integer-dvar union
corruption (`player_sprintForwardMinimum` 105 would become 1), and a four-float
write into a two-float mantle rotation buffer. All were corrected at source;
no assertion or proof condition was relaxed.

**Compiled/Ran?** ✅ Hosted verification is green for implementation commit
`aec0ab9`:

- iOS census run `29267514080`: artifact verified **30 PASS, 0 FAIL**,
  including all five real movement/math members.
- iOS stub run `29267514067`: simulator and unsigned arm64 device jobs both
  green. The simulator artifact retained the M13 boot proof and produced:

```
pmove=real bg_pmove OK: walk+jump+land+friction on synthetic z=0
pmoveLive=org=(0.0,0.0,0.0) vel=(0.0,0.0,0.0) speed=0 ground=1
```

- The same run built hard-required five-object pmove archives for simulator
  and device, linked both apps, and uploaded an arm64, iOS-minimum-16.0
  unsigned IPA. This proves device compilation/linkage, not device runtime.
- Windows run `29267514051`: Debug and Release green for SP, MP, and
  dedicated-server targets.
- Pushed-main repeat runs are also green at `8639782`: census `29268715852`
  (30 PASS, 0 FAIL), iOS `29268716011` (same exact marker plus unsigned device
  build), and Windows `29268715967` (Debug and Release SP/MP/dedicated).
- Final source-level audit found no alternate `Pmove`, false-positive proof,
  bridge ABI mismatch, trace-consumption mismatch, or archive/link-order
  escape. Local `git diff --check`, workflow/script syntax, census-path, exact
  marker, required-object, and archive-order checks also stayed green.

**Evidence boundary / next hypothesis:** cloud-verifiable Phase 2 is complete.
The remaining Phase 2 addendum is a signed install and human feel-test on the
physical iPad: pull `metal_first_frame.txt`, require both exact M13/M14 lines,
and exercise left-stick movement, A jump, and B sprint. No such device marker
was pulled in M14, so physical runtime and feel are explicitly unverified.
Phase 3 and fastfile work did not begin, and no proprietary assets were used.

## Phase 3 Wave 1 candidate — real filesystem closure (2026-07-13, Windows seat; CI UNVERIFIED)

**Attempted:** graduate the asset-free filesystem boot slice into the app and
earn the frozen interim line only after the engine's own `FS_InitFilesystem`
returns and real sandbox I/O succeeds.

**Concrete changes (plain-language changelog):**

- Added `BootFSSmoke.cpp`. Before filesystem startup it behavior-checks the
  newly reached arm64 dvar enum and external-string pointer lanes. It then
  explicitly enables the iOS headless/no-fastfile policy, calls real
  `FS_InitFilesystem`, verifies `fs_basepath` is the bundle and `fs_homepath`
  is Documents, proves `fileSysCheck.cfg` is absent, and requires an engine
  write/read/byte-compare/free/delete round trip.
- Added hard-required exact `libkisakcominit.a` archives for simulator and
  device. Static closure tracing expanded the five-object hypothesis to seven
  real objects: `com_files`, `win_common`, `sys_ios_paths`, `com_fileaccess`,
  `unzip`, `stringed_hooks`, and `stringed_ingame`. Archive-member drift or a
  missing object is now fatal, and both app link lanes include the archive.
- Expanded the monotonic census from 30 to 34 with the four newly tracked
  stdio/zip/localization TUs. The census gate now requires at least 34 entries
  and every entry to pass.
- Fixed newly reached LP64 hazards behind `KISAK_IOS` while preserving each
  original expression in `#else`: dvar enum tables and external strings,
  direct `DvarValue` domain callbacks, `searchpath_s` allocations, pointer
  sorting/list storage, `fileHandleData_t` clearing, remaining filesystem dvar
  path reads, language table accounting, and localization line parsing.
- Shrunk the scaffold by deleting ten definitions now owned by real filesystem
  and lock objects. New closure-only owners are explicit: command-line startup
  is a checked no-op because the stub has no argument ingress; map-load
  profiling is observational; checksum/restart/sound/database-thread tails are
  abort-loud. CI now preserves full Xcode logs and inventories archive/app
  symbols so future scaffold masking is reviewable.
- Swift runs Wave 1 only after the retained M13 proof, crash-guards it, and
  runs M14 only after the exact filesystem result. The proof file gains the
  separately frozen `fs=` line; CI hard-asserts it without weakening the M13
  or M14 assertions.

**Errors found and addressed:** the expected five-object closure omitted
`com_files`' direct stdio and minizip owners; `fileSysCheck.cfg` made an
asset-free boot fatal; 32-bit pointer math remained in both filesystem and
newly reached dvar/language paths; and the existing workflow discarded most
linker output with `tail`. The candidate fixes those causes rather than adding
a fake asset or relaxing an assertion. The seven-object set remains a static
prediction until hosted linking confirms it.

**Compiled/Ran?** **UNVERIFIED in hosted CI.** This workstation has no iOS or
Windows compiler. Windows-available checks passed before the local commit:
`git diff --check`, `bash -n` for the engine archive script, census count 34,
exact marker greps, forbidden graduated-scaffold grep, and an explicit zero
result for `jmp_buf`/`setjmp`/`longjmp` across all seven real Wave 1 TUs.

**Required coordinator verdict before Wave 2:** push the local commit and
require (1) census **34 PASS, 0 FAIL**, (2) simulator marker exactly
`fs=FS_InitFilesystem OK — bundle base, Documents home, write/read/delete OK, no assets`
while the exact M13/M14 lines remain present, (3) a green unsigned arm64 device
IPA link containing the exact archive, and (4) Windows Debug/Release green.
Any red result belongs to Wave 1; do not proceed around it. No COD4 asset was
created, read, committed, or uploaded, and no physical-device claim is made.

### Wave 1 hosted red-fix addendum — MSVC time shim (2026-07-13)

**Evidence received:** the coordinator pushed the original Wave 1 candidate as
`bdd14f0`. Its first census stopped at **33/34** because
`src/qcommon/unzip.cpp:270` could not see the Quake-lineage `LittleShort`
no-op. Both stub lanes then stopped at the hard-required archive check because
the failed object was correctly absent; that was gate enforcement, not a gate
defect. Subsequent coordinator commits restored the iOS little-endian helpers,
filtered only the archive's `__.SYMDEF` pseudo-entry from provenance comparison,
and added real `com_shared.cpp` plus the abort-loud
`FS_PureServerSetLoadedIwds` closure owner. No census or archive requirement
was weakened.

The resulting 35-TU hosted attempt exposed one new sole census error:
`src/universal/com_shared.cpp:162: use of undeclared identifier '_time64'`.
The archive and both stub builds correctly remained blocked on that missing
required object. Run IDs and the Windows verdict were not included in the
coordination report, so they remain **UNVERIFIED** here.

**Current fix:** audited all of `com_shared.cpp`; its complete MSVC time family
is `_time64` plus `_localtime64`. A tree-wide sibling audit also found
`_ctime64` in `ui_shared_obj.cpp`. The `KISAK_IOS` CRT shim now implements all
three with MSVC-compatible `long long` storage while converting through native
64-bit Apple `time_t`; this avoids treating distinct pointer types as aliases.
The original engine TUs and every Windows lane remain unchanged.

**Windows-available checks:** `git diff --check` and Git Bash syntax checks
pass; the census manifest is 35 unique existing paths; the Com_Init archive
contract is eight unique required members; all three frozen simulator marker
assertions remain present; the eight-TU Wave 1 `jmp_buf` audit is still zero;
and every MSVC time spelling currently used in the source tree is covered by
the iOS shim. This workstation has no iOS or Windows compiler, so these checks
are not runtime or hosted-build evidence.

**Required coordinator verdict:** census **35 PASS, 0 FAIL**; an exact
eight-member `libkisakcominit.a` in both lanes; simulator launch retaining the
exact M13, M14, and frozen filesystem markers; green unsigned arm64 device IPA;
and Windows Debug/Release green. Until those hosted results and run IDs return,
Wave 1 and its runtime marker remain **UNVERIFIED** and Stage B1 must not start.

## Phase 3 Stage B1 candidate — fresh cold dvar preflight (2026-07-13, Windows seat; CI UNVERIFIED)

**Attempted:** establish the cold WinMain-equivalent entry required by the
corrected M15 contract and move the existing dvar LP64 check out of the
post-M13 filesystem probe. This slice intentionally stops before `Com_Init`.
A later coordination directive explicitly authorized authoring B1 while the
Wave 1 staging run is active; it did not authorize a Wave 1 green claim or B2.

**Changes:** added `BootComInit.cpp` as the only cold entry. It rejects repeat
entry and non-main-thread execution, initializes engine thread data, calls real
`Dvar_Init` exactly once, and behaviorally proves enum registration,
readback, external mutation, restore, and external-string readback. Both source
pointers and the stored external-string pointer must have nonzero upper 32
bits, so a truncated 32-bit lane cannot earn the marker. The old
`kisak_boot_smoke` symbol was removed; `BootSmoke.cpp` now re-earns the exact
M13 line using post-init probes only. The Wave 1 FS probe no longer duplicates
the dvar preflight.

Swift now enters the cold orchestrator first and records the separately frozen
line `cominit-preflight=dvar enum/external string OK — cold Dvar_Init path` in
the HUD/marker before allowing the retained M13, FS, and M14 chain. CI requires
that exact line, requires all six real dvar functions in `libkisaksmoke.a`,
denies app-object definitions of those functions, requires both new app entry
symbols, and forbids the retired entry name. Existing marker assertions were
not changed or removed.

**Ownership boundary:** B1 adds no engine TU and does not pad the 35-entry
census; it newly reaches the already-real `dvar.cpp`. `Dvar_AddCommands` and
`SL_*` remain functional app scaffolds during this slice. The B1 marker claims
only registry and pointer-lane behavior, not dvar console commands or the real
script-string subsystem. Both scaffold groups remain forbidden for M15. The
manual hunk/Cbuf/Cmd tail in the new orchestrator is explicitly temporary and
must be replaced, not combined with, the real `Com_Init` spine in B2.

**Windows-available evidence:** the standing portability scanner reports
`preflight: no known-class findings` for `src/universal/dvar.cpp`; no new
census TU was added. Hosted compilation, simulator execution, unsigned-device
linkage, and Windows regression remain **UNVERIFIED** on this compilerless
seat.

**Required coordinator verdict:** Wave 1 must first receive its pending green
run IDs. For B1 require census **35 PASS, 0 FAIL**; the exact new preflight line
plus unchanged M13/FS/M14 lines from the simulator marker; real-symbol
allowlist and scaffold-denylist checks green; unsigned arm64 IPA green; and
Windows Debug/Release green. Any failure remains inside B1; B2 must not start.

### Tooling addendum — on-demand macOS lab (CI UNVERIFIED)

Added a `workflow_dispatch` macOS-15 lab runner with one script-path input. It
realpath-confines execution beneath `scripts/platform/ios/lab/`, supplies a
dedicated `lab-out/`, and uploads partial output even after probe failure. The
included read-only example records macOS/Xcode/SDK versions and simulator
runtime/device-type inventories. Dispatch proof and artifact contents are
pending the coordinator; no Mac result is claimed locally.

## Phase 3 Stage B2 candidate — real common.cpp spine (2026-07-13, Windows seat; CI UNVERIFIED)

**Evidence received before this slice:** the Wave 1 time-shim build is
compile-proven: census run `29281941827` passed **35/35**, and Windows run
`29281941785` passed Debug and Release. Stub run `29281941846` stopped on the
single remaining `_copyDWord` undefined. The coordinator supplied that
function's complete four-line behavior as a temporary scaffold with an
explicit common-spine deletion boundary. No later stub verdict was supplied,
so this entry does not claim a wholly green Wave 1 or B1 run.

**Attempted:** replace B1's manual hunk/Cbuf/Cmd tail with entry through the
real `Com_Init` owner while stopping truthfully before the ungraduated heavy
tails. `src/qcommon/common.cpp` is now a ninth hard-required member of the
exact `libkisakcominit.a`; it was already in the monotonic 35-TU census, so the
census was not padded. Archive provenance now requires real `Com_Init`, the
iOS fence query, `_copyDWord`, and the four common globals, and denies their
former app-object owners.

**Runtime policy and marker:** `BootComInit.cpp` retains the frozen arm64 dvar
preflight, then explicitly requests headless/no-assets mode and calls real
`Com_Init`. In the iOS lane, `Com_InitDvars` registers real `useFastFile=0`
and dedicated-internet policy (`dedicated=2`). The temporary B2 fence executes
real endian, Cbuf, Cmd, dvar-policy, and hunk initialization, then returns
before SL/filesystem/database/network/SV/CL/renderer/sound. The orchestrator
requires the fence state and both dvar values before exposing the new line
`cominit-spine=Com_Init entered — useFastFile=0, dedicated=2, sv/cl tails fenced`.
The B1, M13, FS, and M14 assertions remain armed independently.

**Scaffold ownership:** fourteen app definitions were deleted as their real
owners joined (`Com_Error`, five Com_Print/Com_Mem helpers, `Com_DPrintf`,
`Com_LogFileOpen`, `Com_StartupVariable`, `_copyDWord`, `com_dedicated`,
`com_sv_running`, `useFastFile`, and `com_errorEntered`). The known SV/CL,
network, renderer, and sound tail entry points are abort-loud with named
deletion owners; the runtime fence must keep all of them unreachable. B2 does
not claim production `Dvar_AddCommands`, `SL_*`, `info1`, or `info2`; their
remaining scaffolds are still forbidden at M15.

**Portability audit:** the scanner's unconditional `EMMS_INSTRUCTION` finding
is resolved by a `KISAK_IOS` no-op, which is the arm64 equivalent because
there is no x87/MMX alias state. The other asm site is inside the existing
`_M_X686` branch and is not compiled for arm64. Every `common.cpp` `jmp_buf`
site points through `Sys_GetValue(2)` to `q_shared.cpp`'s native
`jmp_buf g_com_error[THREAD_CONTEXT_COUNT]`; there is no fixed 64-byte storage
to widen. `com_files.cpp` scanner findings (`io.h`, `_findfirst64i32`,
`_findnext64i32`, `_findclose`) remain exclusively in the byte-identical
Windows `#else`; its iOS lane uses POSIX headers and directory APIs.

**Windows-available evidence:** `git diff --check`, Git Bash syntax checks,
the 35-path unique/existing census assertion, exact nine-member archive list,
marker greps, real-owner allowlists, and app-scaffold denylists pass locally.
This seat has no compiler. Hosted census, simulator link/runtime, unsigned
device link, and Windows regression for B2 are **UNVERIFIED**.

**Required coordinator verdict:** census remains **35 PASS, 0 FAIL**; the
archive is exactly nine members and contains the asserted real owners; the
simulator links and emits both exact Com_Init lines plus unchanged M13/FS/M14;
the unsigned arm64 device IPA links; and Windows Debug/Release stay green. If
the app link is red, preserve the complete undefined-symbol output: it is the
ratified definition of the next bounded B2 closure, not permission to add
benign defaults or weaken any gate.

## Phase 3 Stage B2 link-closure fix — 119 symbols (2026-07-14, Windows seat; CI UNVERIFIED)

**Hosted verdict received:** authoritative staging commit `b6f2861` passed the
iOS compile census at **35/35** and passed Windows Debug/Release. The stub link
failed, as the linker-driven method predicted, with exactly 119 undefined
symbols from the real `common.cpp` whole-object closure. The complete list is
preserved at `build-ios-lib/b2-undefined-symbols.txt`. The coordinator also
removed one scaffold duplicate missed by the B2 sweep, `Com_Filter`, before
pushing; its real `com_shared.cpp` owner remains intact. No hosted run IDs were
provided with this verdict, so none are invented here.

**Attempted:** close only that evidence-defined linker set without graduating
post-fence subsystems early. All 119 names are accounted for in
`BootScaffold.cpp`: 114 functions are grouped abort-loud by their future real
owner (common/config, client/UI, database/script/assets, network/server, and
renderer/sound/platform); the four direct data references (`clientUIActives`,
`cls`, `sv`, `updateScreenCalled`) use exact-size poison storage because data
cannot abort on access; and `Sys_GetCpuCount` is real-minimal rather than a
stub. The poison sizes are asserted against the MP layouts and the B2 marker
can only be earned after the fence, before any data read.

**Reached-path correction:** source tracing found that the 119-symbol report
was not entirely post-fence. `Com_InitDvars` calls `Sys_GetCpuCount`, so its iOS
body now performs the real `sysconf(_SC_NPROCESSORS_ONLN)` query and clamps to
the engine's supported 1-4 range exactly as `threads.cpp` does. Also, the
opening real `Com_Printf` could call `CL_ConsolePrint` and `Sys_Print` before
`com_dedicated` is registered. Under the explicit iOS headless request,
`Com_PrintMessage` now keeps its unconditional stderr output but fences those
absent console frontends. Both frontend definitions remain abort-loud, so any
unintended call fails by name.

**dvar command decision:** `dvar_cmds.cpp` does **not** join B2. The B2 line
claims real dvar registration and explicit policy readback only; it does not
claim `set`, `seta`, `dvarlist`, or `Com_DvarDump`. Pulling that TU now would
expand a bounded link-only slice without earning a new behavior. It joins no
later than B4, because the frozen queued-console-event probe must execute real
`set`; that wave must preflight/census the TU and atomically delete
`Dvar_AddCommands`, `Dvar_Set_f`, `Dvar_SetA_f`, `Com_DvarDump`, `info1`, and
`info2`. This preserves the Challenge-1 prohibition on M15-grade dvar claims
using fake command owners.

**Windows-available evidence:** a mechanical comparison accounts for all 119
linker names exactly (114 abort functions + one reached real function + four
data owners). No real TU was added, so the census remains 35 and no new-TU
preflight was required. The newly reached `common.cpp` path remains covered by
the prior portability audit; local gates require diff cleanliness, Bash
syntax, exact nine-member archive membership, all five simulator marker
assertions, symbol-accounting totals, and byte-identical Windows branches.
This seat has no compiler, so simulator runtime, device linkage, and Windows
regression for this fix remain **UNVERIFIED**.

**Required coordinator verdict:** census 35/35; Windows Debug/Release green;
simulator and unsigned-device links with no remaining undefined or duplicate
symbols; exact nine-member archive/provenance checks; and simulator marker
contains the unchanged B1/M13/FS/M14 lines plus exact B2
`cominit-spine=Com_Init entered — useFastFile=0, dedicated=2, sv/cl tails fenced`.
Any runtime abort must report its named scaffold and stays inside B2.

