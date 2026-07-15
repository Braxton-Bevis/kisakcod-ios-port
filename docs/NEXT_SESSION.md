# NEXT_SESSION - BMK4 live state (2026-07-15, M15 earned)

Read `docs/HANDOFF_TO_CODEX.md`, the ratified roadmap in
`docs/reviews/roadmap-sol.md` plus
`docs/reviews/roadmap-claude-response.md`, and the latest `PORT_JOURNAL.md`
entries before continuing. Re-reference this file before each edit and refresh
it whenever work pauses.

## Active seat state — M15 on main; renderer queued at the sampler boundary

- B4 was permanently gated and promoted through PR #5. Protected `main` merge
  `b113cdd` passed both hard iOS jobs, the 39/39 census, and Windows Debug and
  Release.
- B5 implementation commit `b1945b9` graduates the real script string-list and
  memory-tree owners, fixes the iOS shared dvar registry count without changing
  the Windows branches, and adds a native closeout that behaviorally exercises
  `set`, filtered `dvarlist`, dvar enumeration, script-string allocate/find/
  convert/release, retained filesystem state, and the queued-event witness.
- Census run `29425591429` is **41/41** (SYNTAX). Windows run `29425591288`
  passed Debug and Release plus FF0a and layout conformance. Stub run
  `29425591377`, attempt 2, passed the exact archive/owner checks, simulator
  runtime, unsigned arm64 device link, and IPA packaging
  (OBJECT+ARCHIVE+LINK+SIM_RUN).
- The simulator proof has launch exit `0`, zero `.ips` reports, 72 live dvars,
  and the exact lines
  `KISAK_M15_DETAIL dvars=72 set=closed dvarlist=1 fs=1 event=1 sl=1` and
  `cominit=Com_Init OK — 4 subsystems up, no assets`. The native B5 sequence
  completed in about 31 ms after process start.
- Attempt 1 of the same run was red because CoreSimulator delayed process
  creation until after the workflow's fixed screenshot window. No app code ran
  in that window. The permanent-gate follow-up waits up to six minutes for the
  native-earned M15 line before capturing or terminating, and retains ten
  minutes of diagnostic logs.
- `docs/media/simulator-m15-headless-boot.png` is the high-contrast screenshot
  from the green attempt (SHA-256
  `0884ab8983044b37b3ea5e22237ff49cb277823b44d8f6bbec732226d3fee0cf`).
  It is retained as asset-free simulator evidence, not used as the README hero;
  that position is reserved for the first real engine-rendered frame.
- M15 is SIM_RUN evidence. The unsigned device IPA is compile/link/package
  evidence only; it is not a physical-device M15 runtime claim.
- Permanent-gate commit `f2cbd0f` passed census run `29427943390` (41/41),
  iOS run `29427946590`, and Windows run `29427946410`. The follow-up
  `b8c80bf` removes shell-smoke images from the README hero and passed exact-SHA
  census run `29429035322`, iOS run `29429033587`, and Windows run
  `29429026387`.
- Staging-to-main PR #6 auto-merged as protected `main` merge `0cc70dd` after
  required PR run `29430079226` passed Debug and Release. Exact-main census
  run `29431086988` passed 41/41, iOS run `29431086945` passed simulator and
  unsigned-device jobs, and Windows run `29431087320` passed Debug/Release,
  FF0a, and layout conformance with no substantive red step.
- Staging commits `f780d19` through `90d835d` now build the patched DXVK 2.7.1
  D3D9 frontend and MoltenVK simulator slice, link the app, and advance adapter
  selection through the Apple-only `imageCubeArray`, single-viewport,
  non-precise-occlusion, and `shaderInt64` boundaries. The `shaderInt64`
  change is paired with a bit-exact two-`uint32` rewrite of DXVK's internal
  2x/4x/8x/16x multisample lookup; it does not merely suppress the capability
  check. Commit `90d835d` restores normal unified-diff context after CI caught
  a zero-context hunk that GNU patch accepted but `git apply` correctly
  rejected.
- Exact staging SHA `90d835d209bef469287cacc6103a1275aa219609` has 41/41
  census run `29438325538`, fully green device archive/unsigned arm64 IPA in
  iOS run `29438330184`, and fully green Windows Debug/Release, FF0a, and
  layout run `29438325418`. Its simulator archive, app link, and ownership
  checks are green. SIM_RUN is red only because MoltenVK's hosted simulator GPU
  lacks required `shaderSampledImageArrayDynamicIndexing`; DXVK then reports
  no adapters and the D3D smoke fails closed in 43 ms. M15 still earns with 72
  dvars and every behavioral bit set, pmove earns, launch exit is `0`, and the
  proof has zero `.ips` reports. Evidence is in run `29438330184`; the local
  downloaded copy is `scratch-run-29438330184` and must not be committed.
- The next feature is load-bearing: D3D9 and DXVK's built-in presentation code
  use the global sampler heap. Do **not** make
  `shaderSampledImageArrayDynamicIndexing` optional by itself. Pair any Apple
  simulator admission with a bounded fixed/constant-index sampler fallback,
  safe descriptor-set creation when descriptor indexing is absent, and an
  abort-loud boundary for textured paths the fallback cannot reproduce.
  Source audit predicts the same Apple simulator will also lack aggregate
  `descriptorIndexing`, its update/partially-bound/runtime-array features, and
  `samplerMirrorClampToEdge`; treat those as one coherent fallback wave rather
  than one dishonest capability-toggle round at a time.
- The audited renderer implementation is remotely preserved on branch
  `renderer-placeholder-queued` at `5121928`. Its first commit bounds
  simulator dead stripping while retaining `_vkGetInstanceProcAddr`; its
  second grows the census to 51 and drives a generated recognizable room
  through real IW3 `R_GetCommandBuffer(RC_DRAW_TRIANGLES)` → `RB_*` → D3D9.
  It has not run in Apple CI and is not on staging or main. It must be rebased
  after the sampler fallback goes green, then landed in the two existing
  commits so link closure is proved separately from the scene. No screenshot
  from this branch has been earned or published.

## Current next actions

1. Implement and source-audit one coherent Apple simulator sampler fallback
   for `shaderSampledImageArrayDynamicIndexing` plus the dependent descriptor
   indexing/runtime-array features. Preserve the full physical-device path
   whenever those features exist, and fail closed on any unsupported textured
   engine shader. Require the unchanged CreateDevice/Clear/readback/Present
   marker in staging; a build or adapter admission alone is not evidence.
2. Once that runtime gate is green, promote the simulator-DXVK plumbing to
   protected `main`; the clear frame must not be published as the requested
   render.
3. Rebase `renderer-placeholder-queued` on the promoted line. Push and prove
   its link-closure commit first, then its generated IW3 R/RB scene commit.
   Inspect the artifact visually and require its native draw-count,
   non-background-readback, and Present marker before publishing it as an
   honest generated-placeholder game-engine screenshot.
4. Promote the green renderer result to protected `main` through the normal
   staging PR. Never merge the current red simulator adapter or the unrun
   renderer checkpoint merely to move branch pointers.
5. In parallel with renderer closure, begin slice 7 with the smallest
   synthetic FF1/FF2 RawFile valid+malformed twin. The 32→64 fastfile
   translation kernel remains mandatory for the final real `mp_killhouse`
   physical-iPad frame.

## Historical checkpoint — Stage B2 119-symbol link closure

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

## Stage B2 simulator runtime hypotheses (run `29352352608`)

The app launched and remained observable through the screenshot window, but
never wrote `Documents/metal_first_frame.txt`. The next simulator proof run
captures `proof/app-console.txt`, a process-scoped unified log, the launch exit
code, and any `KisakStub` `.ips` report. Match that evidence against these
hypotheses before changing engine behavior:

| # | Hypothesis / concrete failure scenario | Cheapest settling evidence |
|---|---|---|
| 1 | **The explicit headless request is not visible when the real common spine tests it.** `BootComInit.cpp:113` sets the flag before `Com_Init`, but if the state is lost or a duplicate owner is linked, `common.cpp:1349` misses the B2 branch and immediately reaches the abort-loud `SL_Init` tail instead of setting `com_iOSBootSpineReached`. | `app-console.txt` contains `boot scaffold reached unexpected dependency: SL_Init`; absence of that line plus a later spine message refutes this ordering/state failure. |
| 2 | **The nominally empty startup line reaches a deferred dvar-command boundary.** `Com_ParseCommandLine` deliberately creates one console line even for `""` (`common.cpp:839-851`), and `Com_StartupVariable(0)` tokenizes it at `common.cpp:1359`. Any unexpected retained `set`/`seta` token would call the B4-deferred abort-loud `Dvar_Set_f`/`Dvar_SetA_f` owners. | `app-console.txt` names `Dvar_Set_f(B4 dvar_cmds owner)` or `Dvar_SetA_f(B4 dvar_cmds owner)`; normal engine prints followed by neither name refute this path. |
| 3 | **A real early failure is being masked by the post-fence recovery scaffolds.** The most direct trigger is the 512 MiB headless hunk reservation (`com_memory.cpp:390-407`), whose failure reaches abort-loud `Sys_OutOfMemErrorInternal`; another `Com_Error` trigger longjmps to `Com_Init`, whose real-minimal `Sys_Error` aborts. If recovery inspects the intentionally poisoned `cls`, it can instead expose `CL_ConsoleFixPosition`/UI aborts and hide the originating error. | First use the `.ips` faulting frame and the last console line: `Sys_OutOfMemErrorInternal`, `Sys_Error: Error during initialization`, or a named client/UI scaffold settles the terminal path; the preceding engine error text identifies the original trigger. |

### ADJUDICATED (run `29354619117`, commit `37c2d4c`)

The diagnostics run settled it: **none of the three hypotheses**. The captured
`app-console.txt` shows the preflight, spine, and 3-stage probe markers, then
`----- FS_Startup -----`, `Current language: english`, and the terminal line
`boot scaffold reached unexpected dependency: getBuildNumber`. The boot
progressed past the B2 boundary into the FS bring-up; the first `Com_Printf`
after the console log opened called `getBuildNumber()` (`common.cpp:522`),
whose owner was still a `BOOT_UNREACHED` scaffold. Fix: `src/buildnumber.cpp`
graduated into the census (TU #36) and the `libkisakcominit.a` member list;
`buildnumber.h` is generated in the iOS lanes via `increment_build.sh` (it is
gitignored and Windows-generated otherwise); the scaffold was deleted. Note
hypothesis 3's 512 MiB reservation was NOT reached-and-refused — the trip
happened before any large hunk commit. Expect further one-symbol-per-run
scaffold trips on this path; each is the wave mechanism working, not a defect.

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

## Historical B2 next actions (superseded)

This list is retained as provenance. The active actions are at the top of this
file and the ratified ten-slice ordering overrides this older checkpoint.

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

## 2026-07-14 ratified ordering override and FF0a checkpoint

`docs/reviews/roadmap-claude-response.md` ratifies the ten-slice order in
`docs/reviews/roadmap-sol.md`, with amendments A1-A4. It supersedes the stale
“After Phase 3” ordering above: FF0a Slice 1, then the private real-zone Slice
2, then B3 → B4 → B5. The OAT conformance spike runs independently on lane 2
and must converge before Slice 7. The frozen acceptance artifact is a physical-
iPad `mp_killhouse` frame, not the main menu.

Lane 1 has implemented FF0a Slice 1 locally: a Windows-only
`bmk4-ff-oracle` target, generated empty IW3 fixture, canonical v1 structural
dump, byte-stability assertions, canonical repo-root input/output allowlisting,
and a fixture-output-only artifact glob. Hosted compilation and runtime remain
**UNVERIFIED**. Require Debug/Release tool builds, exact schema/hash assertions,
identical dump hashes, and both outside-root refusal tests before Slice 1 is
green. The coordinator's separate B2 fix at staging `f6ffe33` was still running
when this checkpoint was written; any red verdict there preempts Slice 2.

After both gates are green, Slice 2 runs the oracle locally against the five
inventory-confirmed MP boot zones plus `mp_killhouse.ff`. Raw reports stay out
of git/CI/artifacts; only a sanitized structural closure report may land.
