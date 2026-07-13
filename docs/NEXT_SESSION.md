# NEXT_SESSION - BMK4 context handoff (2026-07-13, Windows Phase 1 checkpoint)

Read `FRONTIER_REPORT.md`, the latest `PORT_JOURNAL.md` entries, and
`docs/FASTFILE_PLAN.md` before continuing. This file records the current live
state and must be refreshed whenever work pauses.

## Verified baseline

- Stream 1 LP64 hunk work is complete and already verified by macOS census run
  `29176526294` and Windows regression run `29176449196`.
- The iOS census remains 26 translation units. Renderer milestone M12 remains
  the last device-verified milestone.
- `docs/FASTFILE_PLAN.md` now exists. Do not start FF4 or asset-dependent work
  during this Phase 1 boot-scaffold slice.
- The current seat is Windows and has no Apple compiler, Xcode, simulator, or
  COD4 game assets. Apple compile/runtime claims require macOS CI or the Mac.

## Current Phase 1 worktree (implemented, CI pending)

- Restored `ios/Stub/BootSmoke.cpp` and added
  `ios/Stub/BootScaffold.cpp`. The scaffold contains the documented
  real-minimal and abort-loud sections for the closure.
- Normalized `ios/Stub/boot_closure.txt` to 74 source identifiers. All 74 are
  unique and statically accounted for: Swift owns `main`, 16 entries are
  supplied by `EngineSmoke.cpp`, and 57 are supplied by `BootScaffold.cpp`.
- Boot smoke now initializes thread data before the hunk, performs a 4 KiB
  hunk read/write check, registers and reads `bmk4_boot=ipad`, and proves
  command registration with `Cmd_FindCommand("cmdlist")`.
- The success marker remains:
  `hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=ipad), cmd OK — 3 stages up`.
- `MetalViewController.swift` calls the smoke on the main queue so
  `Com_InitHunkMemory()` satisfies its main-thread contract, and the bridge
  declares `kisak_boot_smoke()`.
- The smoke archive now requires the leaf objects for `com_memory`, `dvar`,
  `cmd`, `com_math`, `q_shared`, `msg_mp`, `huffman`, and
  `msvc_crt_compat`. Missing members fail the archive build, including in
  `both` mode.
- The simulator workflow applies the canonical DXVK patch and requires a
  proof line matching `^boot=hunk OK .*dvar OK .*cmd OK`. The device build job
  also applies the canonical patch.
- Three boot-used dvar string paths preserve pointers on LP64 under
  `KISAK_IOS`; original Windows expressions remain in their `#else` branches.

## Evidence collected on this Windows seat

- `git diff --check`: clean before this handoff refresh.
- Git Bash `bash -n scripts/platform/ios/build-engine-lib.sh`: exit 0.
- Census manifest: 26 entries.
- Boot closure: 74 nonblank entries, 74 unique entries, all statically
  accounted for by the app/scaffolds.
- Active source contains `BootSmoke.cpp` and `BootScaffold.cpp`; the parked
  `docs/wip/BootSmoke-wip.cpp` is gone.
- No census/layout assertion or Windows regression gate was weakened.

These are static checks only. The following are explicitly **UNVERIFIED**:

- iOS compilation and final archive/app linkage.
- Simulator runtime marker and crash-sentinel behavior.
- Post-change macOS 26/26 census and Windows regression CI.
- Physical-device boot marker.

## Exact next actions

1. Review the local diff and create a local checkpoint commit that clearly
   says the CI gate is pending.
2. With the user's approval, push the checkpoint to `origin/main`; do not
   dispatch workflows or change external state without that approval.
3. Monitor the macOS census, iOS stub simulator/device-build, and Windows
   regression workflows. Treat any red job as a blocker and fix it before
   proceeding.
4. Require the simulator marker line containing `hunk OK`, `dvar OK`, and
   `cmd OK`; do not infer runtime success from a successful build.
5. Only after all remote gates are green, append the evidence-backed M13 entry
   to `PORT_JOURNAL.md` and refresh frontier/checklist documentation.
6. Physical-device execution remains a later Mac-side step requiring the
   user's taps/trust. No COD4 game assets are needed for this phase.

## Guardrails

- Re-reference this plan before each edit and keep this file current whenever
  stopping.
- Preserve the iOS-only `#ifdef KISAK_IOS` / original `#else` discipline for
  engine edits.
- Do not claim M13, device proof, or Phase 1 completion without marker/CI
  evidence.
- Do not touch the fastfile FF4 implementation or invent/download proprietary
  game assets in this slice.
- Branding is BMK4; preserve KisakCOD/LWSS GPL-3.0 credit and bundle ID
  `dev.braxton.kisakstub`.
