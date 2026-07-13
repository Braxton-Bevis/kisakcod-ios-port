# NEXT_SESSION - BMK4 context handoff (2026-07-13, after M13 CI verification)

Read `FRONTIER_REPORT.md`, the latest `PORT_JOURNAL.md` entries, and
`docs/FASTFILE_PLAN.md` before continuing. Re-reference this plan before each
edit and refresh it whenever work pauses.

## Verified state

- M13 staged engine boot is simulator-verified. GitHub Actions run
  `29264276000` built and launched the app and produced this exact marker:
  `boot=hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=ipad), cmd OK — 3 stages up`.
- The same run rebuilt patched DXVK 2.7.1, verified/staged MoltenVK v1.4.1,
  linked the real renderer stack, and produced `KisakStub-unsigned-ipa`.
  Its executable is 8,505,008 bytes, arm64, iOS platform, minimum iOS 16.0.
- iOS census run `29263799350`: 26 PASS, 0 FAIL.
- Windows regression run `29264559274`: Debug and Release both green for the
  SP, MP, and dedicated-server targets. The workflow now tracks `main`.
- Local static gates: `git diff --check` clean, Git Bash syntax check green,
  census manifest 26, boot closure 74 nonblank/74 unique, partitioned as 16
  `EngineSmoke.cpp` + 57 `BootScaffold.cpp` + Swift-owned `main`.
- Renderer M12 remains the latest physical-device runtime proof. M13 did not
  access or sign for the user's iPad.

## What M13 landed

- Active `BootSmoke.cpp` initializes thread data, exercises real hunk memory,
  registers/reads a real string dvar, and behaviorally verifies `cmdlist`.
- `BootScaffold.cpp` documents real-minimal versus abort-loud closure bodies;
  the normalized 74-symbol manifest has no Mach-O spelling artifacts.
- The smoke archive requires the eight real leaf objects: `com_memory`,
  `dvar`, `cmd`, `com_math`, `q_shared`, `msg_mp`, `huffman`, and
  `msvc_crt_compat`; a missing object fails every build mode.
- Three boot-used dvar string paths preserve full pointers on iOS LP64 while
  their original Windows expressions remain in `#else`.
- Swift calls the smoke on the main queue, crash-guards it, writes the result
  to the HUD/proof file, and CI hard-requires the hunk/dvar/cmd marker.
- Device CI now reproduces the real DXVK/MoltenVK stack instead of depending
  on untracked Mac-local archives.

## Explicitly unverified

- M13 boot smoke on the physical iPad. This needs the user's Mac/iPad,
  signing identity, provisioning, trust, and taps; cloud CI cannot prove it.
- Full headless `Com_Init`, the movement sandbox, real fastfile loading,
  gameplay, audio, and a match.
- No COD4 assets are owned or installed in the workspace. Never commit or
  upload proprietary assets; use a user-owned stock Windows PC installation.

## Next actions, in order

1. Start Phase 2 in a fresh session from the bootstrap prompt. Restore
   `docs/wip/PmoveSandbox-wip.cpp`, `docs/wip/PmoveScaffold-wip.cpp`, and
   `docs/wip/PmoveScaffoldShared-wip.cpp`. Prove the standalone simulator
   status string first, make CI assert it, then wire thumbstick/HUD input,
   perform the user's iPad feel-test, and record M14.
2. Phase 3 is headless `Com_Init`: take census waves of 5-10 TUs maximum.
   Each wave gets an LP64 sweep and `jmp_buf` audit and must be pushed green
   before beginning the next; never start a wave that cannot finish in the
   same session.
3. Follow `docs/FASTFILE_PLAN.md` FF0-FF3 with synthetic zones. Before
   hand-writing FF2 field maps for pointer-bearing structs, evaluate the
   GPL-3.0 OpenAssetTools IW3 `ZoneCodeGenerator` as the translation core and
   record the fit/no-fit reasoning and required credit in the plan.
4. When the user supplies the Steam install path, inventory it read-only into
   `docs/ASSET_INVENTORY.md`: filenames, sizes, and hashes only. Game files
   must never enter Git, CI, artifacts, or logs; FF4 is local-offline only.
5. When the personal Mac/iPad is available, install the M13 build over the
   existing app, pull `metal_first_frame.txt`, require the same boot marker,
   and append the physical-device addendum. This does not block Phase 2.

## Guardrails

- Preserve iOS-only `#ifdef KISAK_IOS` / original `#else` discipline for
  engine edits, and keep census plus Windows Debug/Release green.
- Evidence, not job color alone, gates milestone claims. Physical-device
  claims require a pulled device marker.
- Do not weaken layout assertions, closure checks, runtime marker checks, or
  renderer linkage to make CI green.
- Branding is BMK4; preserve KisakCOD/LWSS GPL-3.0 credit and bundle ID
  `dev.braxton.kisakstub`.
