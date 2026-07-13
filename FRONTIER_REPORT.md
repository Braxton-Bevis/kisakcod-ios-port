# FRONTIER REPORT — KisakCOD → arm64-apple-ios

**Updated:** 2026-07-13 (M14 hosted-Mac verification) · **Repo:**
`Braxton-Bevis/bmk4` · **Companion docs:** [PORT_JOURNAL.md](PORT_JOURNAL.md)
(M0-M14), [DEPENDENCY_MAP.md](DEPENDENCY_MAP.md), and
[docs/FASTFILE_PLAN.md](docs/FASTFILE_PLAN.md).

Current frontier: **30/30 tracked engine translation units compile for
arm64-apple-ios; DXVK→MoltenVK→Metal is physically device-proven; the real
memory/dvar/command stages boot in the simulator; and real `bg_pmove` now
walks, jumps, lands, and stops on an asset-free synthetic floor.** The M14
unsigned arm64 app compiles and links the complete renderer and movement
stacks. M13/M14 physical-iPad runtime and movement feel remain unverified.

The project is being driven from Windows. Apple builds and simulator runtime
proof use ephemeral GitHub-hosted `macos-15` runners; physical-device claims
come only from marker files pulled from the user's iPad.

---

## Verified milestones

### iOS pipeline and app shell: ✅

- XcodeGen app builds and launches in hosted CI; unsigned arm64 IPA artifacts
  are produced without an Apple account.
- M11 physically proved the app shell on an iPad Pro 13-inch (M5): MetalFX
  spatial upscaling, 120 fps, controller surface, and device sandbox markers.
- M14 run `29267514067` repeated the simulator launch, preserved the M13 boot
  proof, exercised real pmove behavior, and produced an arm64,
  iOS-minimum-16.0 unsigned IPA with renderer and pmove archives linked.

### Build-system port and census: ✅ 30/30

The census covers platform entry/compatibility, game and server logic,
commands, threading, compression/messages, filesystem, shared math/strings,
dvars, hunk memory, `Com_Init`/`Com_Frame`, script VM, animation, physics,
sound/Steam shims, networking, renderer initialization, and the real
pmove/jump/slide/mantle plus angle-vector closure. Run `29267514080` recorded
30 PASS and 0 FAIL.

Windows byte identity remains enforced by the full DirectX build. Run
`29267514051` passed Debug and Release for SP, MP, and dedicated server. The
workflow now follows the repository's real default branch, `main`.

### Filesystem sandbox: ✅

The iOS path layer maps read-only bundle data and writable Documents data;
filesystem enumeration and path operations use POSIX implementations. Game
data is not present yet and is never shipped by this repository.

### Renderer translation path: ✅ physical-device runtime proven

M12 proved a D3D9 Clear/readback/Present through patched DXVK 2.7.1,
MoltenVK 1.4.1, and Metal on the M5 iPad, including bit-exact pixel readback.
The canonical patch and one-command build are
[`dxvk-v2.7.1-ios.patch`](scripts/platform/ios/dxvk-v2.7.1-ios.patch) and
[`build-dxvk-ios.sh`](scripts/platform/ios/build-dxvk-ios.sh).

M13 made device CI reproducible: the job builds DXVK, validates the pinned
MoltenVK archive hash, stages every required static component/header, links
the app, verifies arm64/iOS load commands, and uploads the unsigned IPA.

### Engine execution: ✅ staged subsystem boot in simulator

- Earlier smoke: real math, bit-packing, and shared-string code produced exact
  expected results in simulator and on device.
- M13: real `Com_InitHunkMemory`, 4 KiB temporary hunk read/write/free,
  `Dvar_Init` plus `bmk4_boot=ipad` register/readback, and `Cbuf_Init`/`Cmd_Init`
  plus a behavioral `cmdlist` lookup.
- Exact run `29264276000` marker:
  `boot=hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=ipad), cmd OK — 3 stages up`.
- Closure accounting is explicit: 16 existing scaffold symbols, 57 new
  real-minimal/abort-loud definitions, and Swift-owned `main` = 74/74.

### Player movement: ✅ real synthetic-world runtime in simulator

- `bg_pmove`, `bg_jump`, `bg_slidemove`, `bg_mantle`, and the real
  `AngleVectors` leaf form a hard-required five-object `libkisakpmove.a` for
  both simulator and device builds.
- A capsule trace adapter supplies only an infinite solid `z <= 0` halfspace;
  no map, fastfile, fabricated asset, or proprietary game file is involved.
- A deterministic 240-frame, fixed-60-Hz proof checks walking distance/speed,
  jump apex and air time, landing near z=0, final grounded state, and friction
  decay to rest. Run `29267514067` produced the exact line:
  `pmove=real bg_pmove OK: walk+jump+land+friction on synthetic z=0`.
- The same live state is driven by left-stick forward/strafe, one-frame A
  jump edges, and held B sprint. Its origin/velocity/ground state appear in
  the HUD and marker; the hosted artifact ended grounded at rest at the
  origin.
- This is simulator runtime plus an unsigned arm64 device build. It is not a
  signed install, physical-iPad marker, or human feel test.

---

## Remaining walls

| # | Wall | Status after M14 |
|---|---|---|
| 1 | 32-bit fastfile layouts | Still the dominant wall. The loader expects original PC version-5 zones; `docs/FASTFILE_PLAN.md` stages synthetic FF0-FF3 before user data at FF4. |
| 2 | Full engine closure | Memory/dvar/cmd are live, but headless `Com_Init` still has a much larger transitive closure. |
| 3 | Renderer content readiness | Clear/Present is proven; real content still needs Apple-safe dummy descriptors/resources and engine renderer initialization through the host window seam. |
| 4 | Input/audio/game loop | The shell directly drives pmove from `GCController`, but full touch/controller injection through `Sys_QueEvent`, AVAudioEngine, map load, spawn, and match flow remain. |

## Next work, ordered

1. When the user's Mac/iPad is available, pull the retained M13 boot marker
   and new M14 pmove marker, then feel-test left stick, A jump, and B sprint;
   do not infer device runtime from the unsigned build.
2. Close and run the headless `Com_Init` frontier in bounded 5-10-TU census
   waves. Phase 3 has not started in M14.
3. Execute fastfile plan FF0-FF3 synthetically, then FF4 with the user's
   legally owned original **Call of Duty 4: Modern Warfare (2007)** Windows PC
   data. Console, remastered, and signed zones are out of scope.
4. Finish renderer content readiness, full event input, audio, and match flow.

## Honest inventory

- The BMK4 shell launches; the complete COD4 game does not yet launch.
- M14 is simulator-runtime verified and device-build verified, not
  physical-device-runtime or physical-feel verified.
- The movement world is deliberately flat and synthetic; this proves the
  real movement math and integration seam, not collision against a COD4 map.
- The staged boot intentionally uses documented scaffolding for dependencies
  outside the live path; unexpected use aborts loudly.
- No proprietary game assets are present or permitted in Git history.
- Miles/Bink remain non-native; sound/video replacement work remains.
- The fastfile layout wall, not compilation or the renderer API, is still the
  largest obstacle between the current shell and a playable match.
