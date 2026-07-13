# OpenAssetTools evaluation for BMK4 Phase 4 (fastfiles)

*Claude-lineage work (coordination seat + Sonnet-class research agent,
2026-07-13). Per docs/CROSS_REVIEW.md, Sol must adversarially review this
before FF2 work builds on it. Fulfills NEXT_SESSION "Next actions" item 3
and FASTFILE_PLAN's OAT-evaluation gate. OAT clone examined at commit
a87850d (2026-07-12), GPL-3.0 — license-compatible, credit required.*

## Recommendation: adapt-OAT-maps

Use OAT's ZoneCodeGenerator (ZCG) + IW3 schema to GENERATE BMK4's
per-struct 32→64 field maps, keeping BMK4's FF0–FF5 on-device architecture
and verification gates intact. Estimated ~10–15 agent-sessions to
FF4-equivalent vs ~20–30 hand-written, with the silent-offset-error class
largely eliminated.

## Headline: the plan's premise was outdated

"OAT's Linker is a 32-bit x86 tool" is no longer true. OAT builds x86 AND
x64 (premake5.lua platforms; ARCH_x86/ARCH_x64) and its CI builds/tests
both arches. On x64, OAT already loads 32-bit IW3 fastfiles into native
64-bit structs — the FF2 translation core exists and is CI-tested.
docs/SupportedAssetTypes.md: "All asset types are supported to be loaded
from other fastfiles in memory" (its ✅/❌ table concerns Unlinker
disk-dump/Linker source-build only, not zone loading).

## Key evidence (paths in the OAT tree)

- ZCG schema: src/ZoneCode/Game/IW3/IW3_Commands.txt (game IW3;
  wordsize 32; 25 assets, 9 XFile blocks) + src/ZoneCode/Game/IW3/
  XAssets/*.txt (25 files, 756 lines of per-struct set string/scriptstring/
  count/condition/reusable/block/reorder commands — including XModel's
  trans (numBones-numRootBones)*4, techset shader-argument union
  conditions, menuDef expression trees) + src/Common/Game/IW3/IW3_Assets.h
  (3,573-line cleaned struct header: parsed for 32-bit layout AND compiled
  natively for 64-bit).
- Layout engine: ZoneCodeGeneratorLib/Parsing/PostProcessing/
  CalculateSizeAndAlignPostProcessor.cpp (offsets/sizes at game wordsize,
  bitfields included); CrossPlatformStructurePostProcessor.cpp
  auto-computes the pure-data vs pointer-bearing split (BMK4's catalog
  distinction, automated).
- Generated field maps: ZoneCodeGeneratorLib/Generating/Templates/
  ZoneLoadTemplate.cpp — on word-size mismatch emits per-struct
  FillStruct_X bodies filling each member at its 32-bit offset into the
  native 64-bit struct (Fill/FillPtr), generated at build into
  <asset>_iw3_load_db.cpp. Hand-written IW3 residue is small:
  ContentLoaderIW3.cpp (explicit ARCH_x86/#else fill paths),
  ZoneLoaderFactoryIW3.cpp, custom actions only for gfximage/loadedsound.
- Pointer mechanics: ZoneLoading/Zone/Stream/ZoneInputStream.h/.cpp —
  pointer-bearing structs alloc out-of-block native memory;
  AddPointerLookup maps (block<<28|offset) → native address; same -1 bias
  and 4 block bits as KisakCOD (ZoneConstantsIW3.h: OFFSET_BLOCK_BIT_COUNT
  =4, version 5, IWffu100). Sentinels -1/-2 in Loading/
  ContentLoaderBase.h; MaybePointerFromLookup::OrNulled handles the
  linker's cross-type memory-reuse edge (BMK4's "interior offsets" hazard,
  already solved there).
- Proven: test/SystemTests/Game/IW3/SimpleZoneIW3.cpp, XAnimIW3.cpp —
  Linker-build → LoadZone round trips in CI on x64. IW3 zone WRITING from
  64-bit also works (ZoneWriteTemplate.cpp branches on word-size mismatch)
  — OAT Linker is usable as a second synthetic-zone generator (FF0 oracle).

## Coverage vs BMK4's catalog

No zone-loading gaps. All 25 live PC top-level types loaded; SndDriverGlobals
content skipped (matches "dead on PC"); dead types excluded same as BMK4's
plan; shaders loaded transitively via MaterialTechniqueSet; full transitive
closure (FxElemDef tree, menu statement graph, clipMap/GfxWorld internals,
XAnimDeltaPart family, SoundFile/SpeakerMap, PhysGeomList/BrushWrapper)
covered. The ❌ entries in SupportedAssetTypes.md affect only disk-format
dump/build paths BMK4 should avoid.

## Paths compared

| Path | Reused / written new | Sessions |
|---|---|---|
| (a2) ZCG-generated field maps (RECOMMENDED) | Reuse ZoneCodeGeneratorLib (~15.3k LOC) + IW3 schema; write one new output template dumping per-struct 32-bit offset/size/pointer-flag tables for BMK4's FF2 core; CI cross-check vs the 55 KISAK_LAYOUT_ASSERTs | ~10–15 |
| (a1) Embed OAT ZoneLoading+ZoneCommon on-device; bridge pools→DB_LinkXAssetEntry/SL_GetString; congruence asserts; arm64 premake platform | ~10–15, higher variance |
| (b) Offline repack to "IW3-64" zones via wordsize-64 writer fork; device loader near-original | ~9–13 but longest dark period, weakest incremental verification |
| Hand-write per current FF2 plan (~110–120 pointer-bearing layouts, not 45) | ~20–30, highest silent-corruption risk |

Even if hand-writing, OAT's XAssets/*.txt is the authoritative cross-check
for counts/unions/conditions/load order.

## Top 3 risks

1. Schema divergence: OAT's IW3_Assets.h vs KisakCOD's decompiled headers
   may differ in nesting/order/unions → subtly wrong offsets for BMK4's
   structs. Mitigation: CI cross-check of all ZCG-computed 32-bit sizes
   against the KISAK_LAYOUT_ASSERT census (two independent derivations
   must agree); longer-term, feed ZCG a cleaned mirror of KisakCOD's own
   headers.
2. Runtime-dependent unions/conditions (XAnimIndices, menu Operand,
   pathnode trees, MaterialShaderArgument): offsets generatable, arm
   selection is runtime logic — ~30 `set condition` sites need explicit
   wiring into BMK4's Load_* code plus synthetic-zone regression cases.
3. Engine-lifecycle integration is uncovered by OAT under EVERY path:
   script-string interning, DB_LinkXAssetEntry replace semantics, zone
   unload, blocks 2/3 delay-stream draining, Material_UploadShaders/D3D
   coupling. OAT loads zones as data; the engine loads them as live state.
   This remains BMK4's own work and dominates residual schedule risk.
