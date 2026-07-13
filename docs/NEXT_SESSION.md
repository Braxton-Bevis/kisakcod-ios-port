# NEXT_SESSION - BMK4 context handoff (2026-07-13, after M14 CI verification)

Read `FRONTIER_REPORT.md`, the latest `PORT_JOURNAL.md` entries, and
`docs/FASTFILE_PLAN.md` before continuing. Re-reference this plan before each
edit and refresh it whenever work pauses.

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

## Explicitly unverified

- Physical-iPad M13/M14 runtime and feel. This needs signing, installation,
  launch, a marker pulled from the device container, and human control input.
  An unsigned IPA or simulator artifact cannot satisfy this gate.
- The sandbox is deliberately flat. It does not prove collision against a
  COD4 map, full `Sys_QueEvent` input injection, or the complete game loop.
- Full headless `Com_Init`, fastfile loading, gameplay, audio, and a match.
  Phase 3 and fastfile work did not begin during M14.
- No COD4 assets are installed in this workspace. Never commit or upload
  proprietary assets; use only a user-owned stock Windows PC installation.

## Next actions, in order

1. When the personal Mac/iPad is available, install a signed M14 build, pull
   `Documents/metal_first_frame.txt`, require both exact boot and pmove lines,
   and feel-test left-stick movement, A jump, and held-B sprint. Append a
   physical-device M14 addendum with the pulled marker and observations. This
   is the only remaining Phase 2 boundary and does not invalidate the hosted
   proof while hardware is unavailable.
2. In a new bounded slice, begin Phase 3 headless `Com_Init` with census waves
   of 5-10 TUs maximum. Each wave gets an LP64 sweep and `jmp_buf` audit and
   must be pushed green before the next; never start a wave that cannot finish
   in the same session.
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
