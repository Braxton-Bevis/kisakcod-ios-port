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
