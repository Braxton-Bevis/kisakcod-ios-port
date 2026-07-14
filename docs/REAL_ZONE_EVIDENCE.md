# Slice 2 — Real-zone structural evidence (sanitized)

*Coordinator run, 2026-07-14, on the inventory-verified local Steam install
(docs/ASSET_INVENTORY.md). Tool: `bmk4-ff-oracle.exe` from Windows CI run
29354619087 (commit `37c2d4c`), executed locally. Raw dumps stay on the
coordination laptop per the standing asset rule; this report carries only
structural counts, block-size tables, and FNV-1a64 hashes.*

## Result: all six zones parse clean, exit 0, first contact

| zone | ff bytes | decompressed | xfile.size | assets | script strings |
|---|---:|---:|---:|---:|---:|
| code_post_gfx_mp | 89,196 | 527,932 | 527,888 | 368 | 165 |
| localized_code_post_gfx_mp | 832,726 | 1,184,281 | 1,184,237 | 3,622 | 0 |
| ui_mp | 420,146 | 14,197,952 | 14,197,908 | 113 | 0 |
| common_mp | 13,517,381 | 41,520,582 | 41,520,538 | 2,595 | 591 |
| localized_common_mp | 57,032,972 | 70,269,481 | 70,269,437 | 3,160 | 0 |
| mp_killhouse | 39,319,415 | 76,935,387 | 76,935,343 | 547 | 202 |

Every zone: `container.magic=IWffu100`, `container.version=5`,
`container.secure=0`, `blocks.count=9`,
`decompressed_bytes == xfile.size + 44` (the XFile header). The
three-source format triangulation is now CONFIRMED against retail data.

## Block-usage profile (from each zone's XFile block-size table)

| zone | temp[0] | runtime[1] | virtual[4] | vertex[7] | index[8] | blocks 2,3,5,6 |
|---|---:|---:|---:|---:|---:|---:|
| code_post_gfx_mp | 1,092 | 0 | 473,094 | 12,224 | 1,264 | all 0 |
| localized_code_post_gfx_mp | 498,816 | 0 | 229,800 | 0 | 0 | all 0 |
| ui_mp | 444 | 0 | 14,102,419 | 0 | 0 | all 0 |
| common_mp | 2,404 | 0 | 30,188,532 | 9,572,992 | 1,128,448 | all 0 |
| localized_common_mp | 1,740,844 | 0 | 966,877 | 0 | 0 | all 0 |
| mp_killhouse | 8,389,408 | 88,800 | 27,782,442 | 8,733,504 | 1,355,832 | all 0 |

### Screenshot-critical adjudications (amendment A1 ledger)

- **SETTLED — container format.** Magic, version, secure flag, header
  arithmetic, and zlib stream integrity verified end-to-end on all six
  zones (largest: 76.9 MB decompressed).
- **SETTLED — block usage.** Only five of nine blocks carry bytes in ANY
  zone the artifact needs: `temp`, `runtime`, `virtual`, `vertex`,
  `index`. `large_runtime[2]`, `physical_runtime[3]`, `large[5]`, and
  `physical[6]` are ZERO in all six. The slice-7 kernel's block-allocation
  scope shrinks accordingly.
- **SETTLED — runtime block is killhouse-only and tiny.** `runtime[1]` is
  zero in all five boot zones and 88,800 bytes in mp_killhouse; it is a
  zero-filled allocation (no stream bytes), matching FF_RUNTIME_NOTES.
- **NARROWED — delay-stream glue (blocks 2/3).** The header block-size
  table declares zero bytes for both delay-streamed blocks in ALL SIX
  zones. If that holds through an engine-instrumented load (the oracle's
  static pass reports `delayed_records.observed=0`,
  `runtime_observation.complete=0` on real zones — not-observed, not
  proven-absent), the feared delay-stream lifecycle glue is entirely
  off the artifact's critical path. Follow-up probe: the FF0a
  engine-instrumented dump mode counts delayed records during a real
  in-engine load of common_mp (slice 7 exit criterion, local Windows).
- **OPEN (by design) — per-asset field layouts.** The standalone tool's
  runtime observation is incomplete on real zones; per-asset field dumps
  require the instrumented engine build. That is exactly the slice 6/7
  work (layout manifests + oracle-vs-iOS round-trip), not a slice 2 gap.

## Integrity anchors (FNV-1a64, for future oracle-vs-iOS comparison)

| zone | xfile | asset_list | decompressed_payload |
|---|---|---|---|
| code_post_gfx_mp | 8260479f2e5bc126 | 37b40ec682f0f663 | f52849861a4ef8e8 |
| localized_code_post_gfx_mp | fbf1798aa477e267 | 22f25a075712e5e9 | fff3419ff6731e24 |
| ui_mp | 11d60bd0165f8a63 | 847fc6874a3dab90 | f97fee375b3f47e7 |
| common_mp | 04aa2646e8b296fb | 59061dee6f24db0d | 00422cc1bfca07d9 |
| localized_common_mp | 6624b48692f055c0 | f85cfcd181fbaa05 | e5d5455c7110d22a |
| mp_killhouse | adf85dad579f3f5d | fd01390aa89b0d1a | 98dec82a73e556f3 |

An iOS FF1 load spine that reproduces these three hashes per zone has
byte-identical decompression and asset-list parsing to the Windows oracle
— that is the slice-7 FF1 gate, now with real-data anchors instead of
synthetic-only ones.
