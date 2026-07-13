# NEXT_SESSION - BMK4 context handoff (2026-07-13, Wave 1 red-fix loop)

Read `FRONTIER_REPORT.md`, the latest `PORT_JOURNAL.md` entries,
`docs/M14_PMOVE_SANDBOX_REPORT.md`, and `docs/FASTFILE_PLAN.md` before
continuing. Re-reference this plan before each edit and refresh it whenever
work pauses.

## Active seat state — Wave 1 CRT red fix awaiting coordinator CI

- `origin/main` at `004a678` contains Wave 1 candidate `bdd14f0`, three
  evidence-driven closure fixes, the playable roadmap, and staging-branch CI
  triggers. The exact filesystem archive now contains eight real objects and
  the monotonic census contains 35 TUs.
- The latest staging census reached the newly added `com_shared.cpp` owner and
  failed only at `_time64`; simulator and device jobs consequently could not
  build the hard-required archive. The current local fix adds `_time64`,
  `_localtime64`, and the tree-used sibling `_ctime64` to the existing iOS CRT
  shim. Windows source lanes are untouched.
- Hosted verification remains **UNVERIFIED** until the coordinator pushes this
  local commit to staging and reports all three gates: census **35/35**, exact
  simulator FS marker while retaining M13/M14 markers plus a green unsigned-
  device link, and Windows Debug/Release green. Do not begin Wave 2 before
  those verdicts and run IDs are recorded.
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

## Next actions, in order

1. When the personal Mac/iPad is available, install a signed M14 build, pull
   `Documents/metal_first_frame.txt`, require both exact boot and pmove lines,
   and feel-test left-stick movement, A jump, and held-B sprint. Append a
   physical-device M14 addendum with the pulled marker and observations. This
   is the only remaining Phase 2 boundary and does not invalidate the hosted
   proof while hardware is unavailable. It may be deferred while Phase 3
   begins, but it is mandatory before any physical-device M13/M14 claim.
2. Coordinator: push the local Wave 1 CRT fix to staging and report the census,
   simulator/device-build, and Windows verdicts listed in the active state.
   Fix any red result inside Wave 1. Only after all are green, record exact run
   evidence and begin Stage B1: dvar enum/external-string preflight plus the
   fresh cold-start orchestrator skeleton required by the corrected contract.
3. After Phase 3, follow `docs/FASTFILE_PLAN.md` FF0-FF3 with synthetic zones.
   Before hand-writing FF2 maps for pointer-bearing structs, evaluate the
   GPL-3.0 OpenAssetTools IW3 `ZoneCodeGenerator` as the translation core and
   record fit/no-fit reasoning and required credit in the plan.
4. When the user supplies the Steam install path, inventory it read-only into
   `docs/ASSET_INVENTORY.md`: filenames, sizes, and hashes only. Game files
   must never enter Git, CI, artifacts, or logs; FF4 is local-offline only.

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
