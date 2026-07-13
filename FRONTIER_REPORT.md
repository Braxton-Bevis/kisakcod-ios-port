# FRONTIER REPORT — KisakCOD → arm64-apple-ios

**Updated:** 2026-07-13 (M13 hosted-Mac verification) · **Repo:**
`Braxton-Bevis/bmk4` · **Companion docs:** [PORT_JOURNAL.md](PORT_JOURNAL.md)
(M0-M13), [DEPENDENCY_MAP.md](DEPENDENCY_MAP.md), and
[docs/FASTFILE_PLAN.md](docs/FASTFILE_PLAN.md).

Current frontier: **26/26 tracked engine translation units compile for
arm64-apple-ios; DXVK→MoltenVK→Metal is physically device-proven; and the real
memory, dvar, and command subsystems now boot behaviorally in the iOS
simulator.** The M13 unsigned arm64 device app also compiles and links the
complete renderer stack. M13 physical-iPad runtime is still unverified.

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
- M13 run `29264276000` repeated the simulator launch and produced an arm64,
  iOS-minimum-16.0 unsigned IPA with the real renderer archives linked.

### Build-system port and census: ✅ 26/26

The census covers platform entry/compatibility, game and server logic,
commands, threading, compression/messages, filesystem, shared math/strings,
dvars, hunk memory, `Com_Init`/`Com_Frame`, script VM, animation, physics,
sound/Steam shims, networking, and renderer initialization. Run
`29263799350` recorded 26 PASS and 0 FAIL.

Windows byte identity remains enforced by the full DirectX build. Run
`29264559274` passed Debug and Release for SP, MP, and dedicated server. The
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

---

## Remaining walls

| # | Wall | Status after M13 |
|---|---|---|
| 1 | 32-bit fastfile layouts | Still the dominant wall. The loader expects original PC version-5 zones; `docs/FASTFILE_PLAN.md` stages synthetic FF0-FF3 before user data at FF4. |
| 2 | Full engine closure | Memory/dvar/cmd are live, but headless `Com_Init` still has a much larger transitive closure. |
| 3 | Renderer content readiness | Clear/Present is proven; real content still needs Apple-safe dummy descriptors/resources and engine renderer initialization through the host window seam. |
| 4 | Input/audio/game loop | Touch/controller event injection, AVAudioEngine, map load, spawn, and match flow remain. |

## Next work, ordered

1. Pull the M13 marker from the physical iPad when the user's Mac/iPad is
   available; do not infer device runtime from the unsigned build.
2. Finish the real `bg_pmove` synthetic-world sandbox and thumbstick/HUD seam.
3. Close and run the headless `Com_Init` frontier.
4. Execute fastfile plan FF0-FF3 synthetically, then FF4 with the user's
   legally owned original **Call of Duty 4: Modern Warfare (2007)** Windows PC
   data. Console, remastered, and signed zones are out of scope.
5. Finish renderer content readiness, input, audio, and match flow.

## Honest inventory

- The BMK4 shell launches; the complete COD4 game does not yet launch.
- M13 is simulator-runtime verified and device-build verified, not
  physical-device-runtime verified.
- The staged boot intentionally uses documented scaffolding for dependencies
  outside the live path; unexpected use aborts loudly.
- No proprietary game assets are present or permitted in Git history.
- Miles/Bink remain non-native; sound/video replacement work remains.
- The fastfile layout wall, not compilation or the renderer API, is still the
  largest obstacle between the current shell and a playable match.
