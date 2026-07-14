# BMK4 — Roadmap to a playable game (detailed, Mac-free)

> **SUPERSEDED ORDERING (2026-07-14):** the execution order below was revised
> by cross-model review. The authoritative order is now the ten-slice plan in
> [`docs/reviews/roadmap-sol.md`](reviews/roadmap-sol.md) as amended and
> ratified in [`docs/reviews/roadmap-claude-response.md`](reviews/roadmap-claude-response.md).
> Headline changes: the acceptance artifact is a **physical-iPad mp_killhouse
> frame** (not the main menu); the Windows oracle (FF0a) runs FIRST, before
> B3; asset waves are oracle-derived, not guessed; R_Init interleaves with the
> first render-asset wave; the OAT adoption is spike-gated. Stage definitions
> and guardrails below remain in force.


*Coordination seat, 2026-07-13 night. Standing constraints: NO MAC until
unavoidable (device installs via Sideloadly on the Windows laptop; batch
device checkpoints); Claude Max = primary dev pool (resets ~07-16); Sol =
critic (returns 07-20); priority = fastest path to loading into the game;
audio/Bink/polish deferred until after first playable. Authoritative
per-wave state lives in docs/NEXT_SESSION.md; this file is the arc.*

## Stage A — Close out this week (coordinator only, low quota)
- A1. Land Wave 1 green: iterate the red-fix loop on head (diagnose CI →
  real-owner TU into census+archive, or justified abort-loud scaffold →
  push). Gates: census 35/35, both stub jobs green incl. exact fs marker,
  Windows green.
- A2. Journal the wave (run IDs + exact marker lines), refresh
  NEXT_SESSION.md.
- A3. AUTOMATIC on Steam completion: read-only inventory of
  D:\SteamLibrary\steamapps\common\Call of Duty 4 → docs/ASSET_INVENTORY.md
  (filenames/sizes/SHA256 only; game files never enter git/CI/artifacts).
  Verify expected shape: main/*.iwd (iw_00..iw_15 + localized_english_iw*),
  zone/english/*.ff + zone/dlc if present, localization.txt, profiles dir.
- A4. USER (optional, 2 min): boot COD4 once on the PC — validates the
  install and creates a profile; this install is the FF oracle's data.
- A5. Nothing else this week; conserve both pools.

## Stage B — Phase 3: headless Com_Init = M15 (Claude, ~07-16 → ~07-19,
## est. 3-5 sessions)
Method per corrected NEXT_SESSION contract (Sol's review): fresh cold-start
orchestrator (never kisak_boot_smoke first), explicit headless policy
(useFastFile=0, dedicated policy explicit), newly-REACHED-path audit each
wave, scaffold-owner accounting, everything linker-driven in 5-10 TU waves.
- B1. Wave 2 — dvar enum/external-string preflight + cold-start
  orchestrator skeleton (BootComInit path). Gate: preflight assertions in
  simulator proof.
- B2. Wave 3 — common.cpp spine: Com_Init sequence entered; sv/cl heavy
  tails abort-loud. Gate: staged cominit marker grows truthfully.
- B3. Wave 4 — msg/net init: POSIX sockets under KISAK_IOS, loopback only.
- B4. Wave 5 — one real Com_EventLoop pass: queued console event observably
  changes a dvar (frozen gate; a Swift frame cannot satisfy it).
- B5. M15 GATE (CI-enforced forever): exact marker
  `cominit=Com_Init OK — 4 subsystems up, no assets`, dvar count > 24,
  behavioral set/dvarlist, FS write/read/delete, event-loop probe. Journal
  M15. Windows green throughout.
- B6. Sol (07-20): retroactive adversarial review of all Stage B pushes +
  the OAT evaluation (C0 blocker).

## Stage C — Phase 4: the fastfile wall, OAT-accelerated (est. 10-15
## sessions, ~1.5-2.5 weeks)
- C0. Sol adversarially reviews docs/OAT_EVALUATION.md (its first task
  back). FF2 work may not build on OAT before this survives.
- C1. FF0a oracle instrument: Windows KisakCOD build (runs on this laptop,
  real Steam data available locally) gains a dump mode: load a zone, emit
  a canonical text dump of every loaded asset's fields. This is the
  correctness oracle for every later comparison.
- C2. FF0b synthetic zones: (i) minimal hand-rolled zone (rawfile +
  stringtable) built by a script; (ii) OAT's x64 Linker as an independent
  second generator. Oracle loads both.
- C3. ZCG adaptation (the OAT shortcut): new ZoneCodeGenerator output
  template emitting per-struct 32-bit offset/size/pointer-flag tables for
  BMK4. HARD GATE against schema divergence: every ZCG-computed 32-bit
  struct size must equal the corresponding KISAK_LAYOUT_ASSERT value (two
  independent derivations agree), enforced in CI.
- C4. FF1 load spine on iOS: .ff header, block allocation, zlib inflate —
  no pointer translation yet. Gate: synthetic zone's decompressed block
  images byte-identical to oracle's.
- C5. FF2 translation core: staging buffer at Load_Stream; generated field
  maps drive per-struct 32→64 fills; offset→pointer lookup at
  DB_AllocStreamPos (block<<28|offset, -1 bias — same constants OAT
  verified); widened -1/-2 sentinel branches. Gate: rawfile + stringtable
  round-trip equals oracle dump, on simulator, CI-asserted.
- C6. FF3 asset-type waves (order: material → techset → image → xmodel →
  xanim → sound aliases (no audio playback) → menus → fx → clipmap →
  gfxworld → remaining). Each wave: generate maps, wire that wave's share
  of the ~30 runtime union/condition sites, synthetic round-trip vs oracle,
  census/link/Windows gates. The engine-lifecycle glue (script-string
  interning, DB_LinkXAssetEntry, blocks 2/3 delay streams) is wired here —
  it is BMK4-only work no tool covers; expect it to dominate debugging.
- C7. FF4a real-file gate, LOCAL WINDOWS ONLY: oracle build loads the real
  code_post_gfx_mp.ff / common_mp.ff from the Steam install; dumps recorded
  for comparison. Real files never leave this laptop.
- C8. FF4b (deferred to Device Day 1): same real files load on the iPad.

## Stage D — Phase 5+6 subset: render something, click something (est. 4-6
## sessions)
- D1. DXVK dummy-resources workaround for the Apple null-descriptor gap
  (M12 addendum documents the symptom).
- D2. Engine dx.d3d9 init through the existing Sys_iOS_GetHostWindow seam;
  R_Init end-to-end using the shader_bin/precompiled-blob path (no runtime
  D3DX — real COD4 ffs carry compiled shaders).
- D3. Minimal input: touch → Sys_QueEvent key/mouse events — exactly enough
  to drive the menu; full binds later.
- D4. Gate: an engine-driven frame in the simulator (console/whitescreen
  acceptable) with markers; screenshot artifact.

## Stage E — Phase 8: full client link (est. 5-8 sessions, the grind)
- E1. Census-wave cgame*/ui*/client* — heaviest LP64 density; the 73 __asm
  sites get NEON/C equivalents only as linked TUs demand them
  (cg_ents.cpp alone has 22).
- E2. APFS case-sensitivity: normalize at the FS boundary
  (FS_FilenameCompare folds case; the device filesystem doesn't).
- E3. GATE: full client binary links for device, undefined symbols = 0;
  unsigned IPA artifact.

## Stage F — DEVICE DAY 1: the menu milestone (first iPad touch since M12;
## Mac stays in the drawer)
- F1. Sideloadly on this laptop signs CI's IPA with the user's Apple ID
  (USB; 7-day resign; reinstall over-the-top, never uninstall).
- F2. Apple Devices app (Windows): copy the inventory-verified game files
  into the app's Documents (= fs_homepath).
- F3. Batched physical checkpoints in one sitting: M13 boot marker, M14
  pmove feel-test, FF4b real-ff load, D4 engine frame. Marker evidence is
  surfaced in the HUD + exported via the share sheet (no devicectl needed).
- F4. GATE = THE MILESTONE: COD4 main menu, rendered from the user's own
  game files, on the iPad. Journal with photos.

## Stage G — Phases 9-10: playable (est. 5-10 sessions)
- G1. mp_killhouse zone loads (clipmap/gfxworld waves from C6 carry it).
- G2. Real collision replaces the synthetic z=0 plane under the proven
  pmove; full input wiring (sticks, buttons, touch-look).
- G3. Spawn path: local listen server, walk + shoot. (Stock COD4 MP has no
  bots; first "match" = solo on the map / LAN loopback. GPL-compatible bot
  options can come later.)
- G4. GATE = PLAYABLE: spawn, move, shoot in mp_killhouse on the iPad.
- G5. Only now: the deferred backlog — audio (AVAudioEngine behind AIL_*),
  Bink graceful-skip verification, frame pacing/MetalFX on the engine
  frame, config persistence, the 7-day resign routine doc.

## Standing risks
1. Fastfile engine-lifecycle glue (C6) — biggest unknown, no tool covers it.
2. Schema divergence OAT↔KisakCOD — gated at C3 by the dual-derivation
   size check.
3. Batched device checkpoints — device-only bugs surface late by policy;
   accepted trade (renderer path already device-proven at M12).
4. Quota calendar — Claude ~07-16, Sol 07-20; Stage B start is
   quota-bound, not work-bound.
