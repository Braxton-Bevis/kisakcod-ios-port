# NEXT_SESSION — context handoff (written 2026-07-11, end of the Mac marathon session)

*For the next agent/session picking up BMK4. Read FRONTIER_REPORT.md and
PORT_JOURNAL.md M7–M12 first; this file covers only the live working state
that isn't obvious from them. Delete or refresh this file when consumed.*

## Where things stand (all machine-verified)

- **Census 26/26** (`scripts/platform/ios/run-census-local.sh`, list in
  `scripts/ios/CMakeLists.txt`) — includes `common.cpp` (Com_Init/Com_Frame),
  `dvar.cpp`, `com_memory.cpp` (mmap port). CI green across the board,
  including the Windows byte-identity regression.
- **Renderer LIVE (M12)**: D3D9 Clear/readback/Present pixel-exact on the
  iPad Pro M5 through DXVK→Vulkan→MoltenVK→Metal. The complete DXVK patch
  (incl. the CAMetalLayer WSI backend, driver name "iOS") is
  `scripts/platform/ios/dxvk-v2.7.1-ios.patch`; reproducer
  `build-dxvk-ios.sh`. App must force_load MoltenVK + DEAD_CODE_STRIPPING=NO.
- **App** (`ios/`, XcodeGen): HUD shows engine/d3d9/boot smoke lines; proof
  marker `Documents/metal_first_frame.txt`; DXVK stderr teed to
  `Documents/dxvk_stderr.log`; crash-guard sentinels (3-attempt counters).

## The machine + device (local facts)

- Toolchain (no Homebrew): `~/.local/bin` (gh, cmake, xcodegen, glslang),
  `~/Library/Python/3.9/bin` (meson, ninja) — export BOTH onto PATH.
- DXVK checkout (patched, build-ios/ configured release): `~/dxvk`.
  MoltenVK static xcframework: `~/MoltenVK`. SDL2 (no longer needed): `~/sdl2-ios`.
- iPad Pro 13" M5, device ID `21B43CC9-D658-522F-A4A7-72EF38CE29B8`, Developer
  Mode on, personal team `NCYQDHQ93F` (cert: Apple Development 37brax@gmail.com).
  **Uninstalling the app drops developer trust** → user must re-trust in
  Settings; prefer reinstall-over-top; crash sentinels reset via the counter,
  not via uninstall. Wireless works via the CoreDevice tunnel once USB-paired.
- Full device iteration loop: `scripts/platform/ios/iterate-device.sh`
  (fix paths at top — it was written against a session scratchpad).

## In-flight work at session end (four streams; agents died with the session)

1. **LP64 hunk-allocator fixes** (`src/universal/com_memory.{cpp,h}`): agent
   was mid-edit at cutoff. If the tree contains a `docs/wip/lp64-hunk-partial.patch`,
   it IS the partial diff (reverted from main to keep census green) — resume
   from it. The spec: fix seven `(uint32_t)&s_hunkData[...] & 0xFFFFF000`
   pointer-truncation sites (use uintptr_t + real page size — iOS is 16KB) and
   give `HunkUser` (com_memory.h:59) pointer-sized end/pos fields under
   KISAK_IOS. Gate: census stays 26/26.
2. **Boot scaffold** (`docs/wip/BootSmoke-wip.cpp` — restore to ios/Stub/ alongside the
   app/HUD/marker; `ios/Stub/BootScaffold.cpp` — NOT yet written): provide the
   74 symbols in `ios/Stub/boot_closure.txt` (real-minimal vs abort-loud stub
   split is documented in the file history / M13 plan), link BootSmoke against
   the leaf objects (com_memory, dvar, cmd, q_shared, com_math, huffman,
   msvc_crt_compat extracted from `ios/libs/iphonesimulator/libkisakcod.a` —
   members are named `src_..._cpp.o`), run in the simulator until it prints
   "hunk OK … dvar OK … cmd OK", then device. LP64 fixes (stream 1) should land
   first or Com_InitHunkMemory may corrupt.
3. **Pmove movement sandbox** (`docs/wip/PmoveSandbox-wip.cpp` is the WIP (moved out of ios/Stub so xcodegen does not compile it)):
   real bg_pmove physics on a synthetic flat world (trace = z=0 plane), C ABI
   `kisak_pmove_init` / `kisak_pmove_frame(fwd,right,jump,sprint,dtMs)` →
   status string; standalone sim test first, then wire to the on-screen
   thumbstick + HUD (app side NOT yet wired). Goal: "COD4 player physics
   walking on the iPad."
4. **Fastfile attack plan** (`docs/FASTFILE_PLAN.md` — not yet written): a
   3-reader workflow was mapping (a) the zone-loader mechanics/seam in
   src/database/, (b) the struct catalog from the 249 KISAK_LAYOUT_ASSERT
   sites (they encode expected x86-32 sizeofs — a machine-checkable spec),
   (c) precedent (iw4x/CoD4x/zonetool; also consider OFFLINE repack of the
   user's fastfiles into a 64-bit-native format on the Mac so the device
   loader stays simple). Re-run that scoping and write the plan.

## Path to playable (order matters — also in README checklist)

boot scaffold → pmove sandbox → headless Com_Init (~700-symbol closure over
remaining TUs; graduate in census waves like M8) → **fastfile wall** (the
dominant cost; needs the user's COD4 files for final verification) → renderer
content-readiness (DXVK dummy-resources for null-descriptor, journal M12
addendum; engine dx.d3d9 init via Sys_iOS_GetHostWindow) → input/audio → match.

## House rules (enforced by CI + the user's expectations)

- Every engine edit `#ifdef KISAK_IOS` with the original in `#else` — the
  Windows regression build must stay byte-identical.
- Every claim machine-verified (census, device marker files, CI). Journal
  entry per milestone in PORT_JOURNAL.md.
- Branding is **BMK4** (repo `Braxton-Bevis/bmk4`); docs keep the
  KisakCOD/LWSS GPL-3.0 credit. Bundle ID stays `dev.braxton.kisakstub`.
