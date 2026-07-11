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
