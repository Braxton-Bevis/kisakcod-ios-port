# NEXT_SESSION - BMK4 context handoff (2026-07-14, Stage B2 link-closure fix)

Read `FRONTIER_REPORT.md`, the latest `PORT_JOURNAL.md` entries,
`docs/M14_PMOVE_SANDBOX_REPORT.md`, and `docs/FASTFILE_PLAN.md` before
continuing. Re-reference this plan before each edit and refresh it whenever
work pauses.

## Active seat state — Stage B2 119-symbol link closure

- Authoritative B2 commit `b6f2861` passed the 35/35 census and Windows
  Debug/Release. Its stub job exposed the expected whole-object closure:
  exactly 119 undefined symbols, preserved at
  `build-ios-lib/b2-undefined-symbols.txt`. B2 runtime remains unverified.
- The local closure fix accounts for all 119: 114 grouped abort-loud function
  boundaries, four exact-size poison data owners, and one reached real-minimal
  `Sys_GetCpuCount` implementation using the same iOS sysconf/clamp semantics
  as `threads.cpp`. The coordinator also removed a missed duplicate
  `Com_Filter` before `b6f2861`; do not restore it.
- Source tracing disproved the blanket assumption that all 119 are post-fence:
  `Sys_GetCpuCount` is reached by `Com_InitDvars`, and the opening print could
  call absent client/system consoles. The iOS headless lane now retains the
  real common stderr print and explicitly guards both console frontends;
  their scaffold entries remain abort-loud.
- `dvar_cmds.cpp` is deliberately deferred to B4, where the frozen queued
  console-event probe must behaviorally execute real `set`. That wave must
  preflight/census it and delete `Dvar_AddCommands`, `Dvar_Set_f`,
  `Dvar_SetA_f`, `Com_DvarDump`, `info1`, and `info2` together. B2 claims only
  dvar registration/policy readback, never dvar command behavior.
- The new exact line is
  `cominit-spine=Com_Init entered — useFastFile=0, dedicated=2, sv/cl tails fenced`.
  It is emitted only after `Com_Init` returns and the policy/fence postcondition
  passes. The frozen B1 line and all earlier markers remain separately armed.
- Game installation metadata is committed in `docs/ASSET_INVENTORY.md`; all
  five MP boot zones are present on the user's Windows machine. Assets remain
  forbidden from Git, CI, artifacts, and logs.
- This Windows seat must not push or invoke `gh`; the coordination seat owns
  push and hosted-CI observation. Physical-iPad M13/M14 proof remains open and
  is not a blocker for hosted Phase 3 waves.

## Verified state

- M14 implementation commit `aec0ab9` runs COD4's real `Pmove` against an
  asset-free synthetic `z=0` world. GitHub Actions run `29267514067` built and
  launched the simulator app and produced these exact lines:

```
boot=hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=ipad), cmd OK — 3 stages up
pmove=real bg_pmove OK: walk+jump+land+friction on synthetic z=0
pmoveLive=org=(0.0,0.0,0.0) vel=(0.0,0.0,0.0) speed=0 ground=1
```

- The same run hard-required five real movement/math objects, linked them in
  simulator and device apps, and uploaded an unsigned arm64 IPA with iOS
  minimum 16.0. This is device compilation/linkage evidence, not device
  runtime evidence.
- iOS census run `29267514080`: **30 PASS, 0 FAIL**, including `bg_pmove`,
  `bg_jump`, `bg_slidemove`, `bg_mantle`, and real `AngleVectors`.
- Windows regression run `29267514051`: Debug and Release green for SP, MP,
  and dedicated-server targets.
- Pushed-main repeats are also green at core handoff commit `8639782`: census
  run `29268715852` is 30/30, iOS run `29268716011` repeats the exact marker
  and unsigned device build, and Windows run `29268715967` passes Debug and
  Release.
- Local static gates are green: `git diff --check`, Git Bash script syntax,
  30-entry unique/existing census paths, five exact required pmove objects,
  archive link order, exact marker occurrences, and retained M13 boot closure.
- Renderer M12 remains the latest physical-device runtime proof. Neither M13
  staged boot nor M14 pmove was run or marker-verified on the user's iPad.

## What M14 landed

- Restored the three parked pmove sandbox/scaffold files from `docs/wip/` to
  `ios/Stub/` and integrated them into the generated Xcode app.
- Added a synthetic capsule trace against only the solid `z <= 0` halfspace;
  no map, fastfile, fabricated asset, or proprietary game data is involved.
- Added a deterministic 240-frame fixed-60-Hz proof of forward movement,
  jump apex/air time, landing near z=0, final ground contact, and friction to
  rest. CI requires the full exact success string.
- Added the hard-required `libkisakpmove.a` subset with real `bg_pmove`, jump,
  slide, mantle, and angle-vector objects; a missing object fails the build.
- Expanded the monotonic compile census from 26 to 30 and made total/pass/fail
  closure an explicit workflow assertion.
- Fixed parked-scaffold hazards: duplicate shared bodies are standalone-only,
  `Sys_SnapVector` uses engine nearest-even semantics, integer dvar defaults
  retain their integer lane, and the mantle rotation stub uses its two-float
  ABI.
- Runs the proof only after successful M13 main-thread boot, crash-guards it,
  resets a clean live state on success, and exposes proof plus origin/velocity
  in the HUD and marker.
- Left stick drives forward/strafe, A queues one jump edge, B holds sprint,
  and live calls use measured elapsed time clamped to 1-50 ms.
- iOS CI still asserts the exact M13 boot line and now asserts the exact M14
  pmove line. Device builds still reproduce DXVK/MoltenVK and verify arm64/iOS
  load commands. Windows engine source behavior remains green.
- Added `docs/M14_PMOVE_SANDBOX_REPORT.md`, a comprehensive implementation,
  evidence, hazard, limitation, physical-test, and next-work record. The iOS
  runbook now explicitly stages the untracked engine/DXVK/MoltenVK libraries
  required by a clean source checkout.

## Explicitly unverified

- Physical-iPad M13/M14 runtime and feel. This needs signing, installation,
  launch, a marker pulled from the device container, and human control input.
  An unsigned IPA or simulator artifact cannot satisfy this gate.
- The sandbox is deliberately flat. It does not prove collision against a
  COD4 map, full `Sys_QueEvent` input injection, or the complete game loop.
- Hosted CI leaves the controller neutral. It proves the live post-reset frame
  path, not interactive thumbstick/A/B feel or response.
- Full headless `Com_Init`, fastfile loading, gameplay, audio, and a match.
  Phase 3 and fastfile work did not begin during M14.
- No COD4 assets are installed in this workspace. Never commit or upload
  proprietary assets; use only a user-owned stock Windows PC installation.

## Phase 3 corrected execution contract (cross-review, 2026-07-13)

`docs/reviews/phase3-plan-sol.md` found blocking defects in the original M15
method before feature work began. The expected wave order remains a
hypothesis, but the following contract supersedes any instruction to append
`Com_Init` to the existing M13 staged initializer.

### Entry and marker integrity

- Interim waves may run bounded probes after `kisak_boot_smoke`, but an
  interim marker claims only its named stage. They do **not** claim that
  `Com_Init` ran.
- Final M15 uses one cold WinMain-equivalent orchestrator on a fresh launch:
  initialize the main thread/platform preamble, call `Dvar_Init`, then call
  the engine's `Com_Init` exactly once. It must not call the old M13
  initializer first: both paths call `Com_InitHunkMemory`, whose second call
  asserts.
- At the final cutover, split M13 initialization from its behavioral probes.
  After `Com_Init` returns, re-earn the unchanged M13 line with hunk
  allocate/read/free, dvar register/readback, and command lookup **without**
  re-running any initializer; then run the existing M14 proof. The old exact
  CI assertions stay armed.
- Headless mode is explicit, not inferred from missing files. It must prove a
  real registered `useFastFile` dvar is disabled and prove the selected
  dedicated/headless policy took effect. Any iOS-only tail skipped inside
  `Com_Init` is enumerated and guarded; the original Windows body remains
  byte-identical in `#else`.
- Missing `fileSysCheck.cfg` is tolerated only when an explicit iOS headless
  flag is active **and** real `useFastFile` is false. Normal/future game mode
  retains the fatal validation. Do not create a dummy `fileSysCheck.cfg`.

The immutable final simulator line is:

```text
cominit=Com_Init OK — 4 subsystems up, no assets
```

It is emitted only after `Com_Init` returns and all four behaviors pass:

1. real filesystem roots equal the app bundle and Documents, and an engine
   `FS_WriteFile` → `FS_ReadFile` byte comparison → `FS_Delete` round trip
   succeeds;
2. real dvar count is greater than 24, including a real registered and
   disabled `useFastFile`;
3. real `dvar_cmds.cpp` owns `set`/`dvarlist`, and a `set` command changes a
   probe dvar (a lookup alone is insufficient); and
4. one real `Com_EventLoop` pass consumes a queued console event and changes a
   second probe dvar. A Swift CADisplayLink frame, screenshot, or pmove frame
   cannot satisfy this condition; full `Com_Frame` is not part of M15.

### Linker-driven wave rules, corrected

- Create a hard-required exact `libkisakcominit.a` subset and link it in both
  simulator and device lanes. Merely adding a census TU or building the
  unlinked `libkisakcod.a` is not runtime evidence.
- Preserve full linker diagnostics (`tee`, not only `tail -n 40`). The
  undefined list defines a wave only after `nm` inventories definitions from
  **all** `ios/Stub` objects and the wave records which scaffold owner masks
  each candidate undefined.
- Every marker has a real-symbol allowlist and scaffold denylist. Delete a
  scaffold definition in the same wave that graduates its real owner. No M15
  marker can link the M13-only owners for `Dvar_AddCommands`, `SL_*`,
  `useFastFile`, `com_sv_running`, or `info1`/`info2`.
- Audit every newly **reached path**, not only newly listed TUs. This includes
  an arm64 dvar enum/external-string behavioral preflight before filesystem
  startup, even though `dvar.cpp` was already linked and census-green.
- Each wave remains 5–10 real TUs maximum, gets a complete LP64 and `jmp_buf`
  audit, grows the census monotonically, and must pass simulator, unsigned
  device link, and Windows Debug/Release before the next wave.

### Wave 1 — filesystem closure

The first earned interim line is frozen as:

```text
fs=FS_InitFilesystem OK — bundle base, Documents home, write/read/delete OK, no assets
```

Wave 1 adds a separate filesystem smoke entry, initially hard-requires the
real `com_files.cpp` object, removes every duplicate FS definition from
`BootScaffold.cpp`, and lets the preserved linker output determine the rest.
The current five-object closure prediction (`com_files`, `win_common`,
`sys_ios_paths`, `stringed_hooks`, `stringed_ingame`) is a hypothesis, not a
TU contract. Before runtime, fix all pointer-width sites on the newly reached
filesystem path; known mandatory sites include both 28-byte `searchpath_s`
allocations (40 bytes on arm64), four-byte pointer sorting/list allocations,
remaining `fs_homepath->current.integer` pointer uses, and any language-hook
pointer storage. There are no `jmp_buf` sites in the predicted Wave 1 set,
but the audit remains an explicit zero-result gate.

### Stage B1 — cold dvar preflight candidate

The frozen B1 simulator line is:

```text
cominit-preflight=dvar enum/external string OK — cold Dvar_Init path
```

It is emitted only after the app enters through `BootComInit.cpp` on the main
thread, initializes thread-local engine state, calls real `Dvar_Init` once,
proves both enum read/mutate/restore and external string readback, and verifies
that the tested pointers actually use upper arm64 address bits. The old
`kisak_boot_smoke` symbol is removed; `BootSmoke.cpp` now contains behavioral
probes only and repeats no initializer.

This marker deliberately does **not** claim real dvar console commands or the
production script-string subsystem. `Dvar_AddCommands` and `SL_*` remain
functional scaffold owners for B1 and must be replaced by their real TUs before
M15, as the corrected contract already requires. The B2 candidate deletes the
manual hunk/Cbuf/Cmd tail and moves those calls behind real `Com_Init`; hosted
link/runtime proof is still required before that replacement is accepted.

## Next actions, in order

1. When the personal Mac/iPad is available, install a signed M14 build, pull
   `Documents/metal_first_frame.txt`, require both exact boot and pmove lines,
   and feel-test left-stick movement, A jump, and held-B sprint. Append a
   physical-device M14 addendum with the pulled marker and observations. This
   is the only remaining Phase 2 boundary and does not invalidate the hosted
   proof while hardware is unavailable. It may be deferred while Phase 3
   begins, but it is mandatory before any physical-device M13/M14 claim.
2. Coordinator: push the local 119-symbol closure fix to staging. Require
   census 35/35, Windows Debug/Release, zero remaining undefined/duplicate
   symbols in both app lanes, exact nine-member archive/provenance, unsigned
   device IPA, and all five simulator marker assertions. A named scaffold
   abort is a B2 failure, not permission to weaken the fence or marker.
3. Next Sol turn: adversarially review `docs/ROADMAP_TO_PLAYABLE.md`,
   `docs/OAT_EVALUATION.md`, and both knowledge packs, then reorder the plan
   around the physical-iPad in-game screenshot posted to GitHub. Web research
   is permitted; do not start that critique inside this closure-fix slice.
4. After Phase 3, follow `docs/FASTFILE_PLAN.md` FF0-FF3 with synthetic zones.
   Before hand-writing FF2 maps for pointer-bearing structs, evaluate the
   GPL-3.0 OpenAssetTools IW3 `ZoneCodeGenerator` as the translation core and
   record fit/no-fit reasoning and required credit in the plan.

## Guardrails

- Preserve iOS-only `#ifdef KISAK_IOS` / original `#else` discipline for
  engine edits, and keep census plus Windows Debug/Release green.
- Evidence, not job color alone, gates milestone claims. Physical-device
  claims require a marker actually pulled from the device.
- Do not weaken layout assertions, closure checks, runtime marker checks, or
  renderer linkage to make CI green.
- Do not put COD4 assets in Git, CI, artifacts, or logs.
- Branding is BMK4; preserve KisakCOD/LWSS GPL-3.0 credit and bundle ID
  `dev.braxton.kisakstub`.
