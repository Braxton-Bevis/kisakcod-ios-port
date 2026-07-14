# KISAK/OAT layout conformance gate — KISAK-side implementation

Status: **hosted Win32 ABI agreement proven; reviewed schema adjudications encoded**.

## Decision and authority

The KISAK side is a small C++ manifest generator compiled as MSVC Win32 from
the repository's real headers. This is preferable to libclang because the
retail ABI and the existing regression gate are both Microsoft x86; there is
no second layout engine whose packing emulation must be trusted.

The field registry is explicit and follows KISAK declaration order. It is a
reviewable reachability inventory, not a source of layout numbers. The compiler
emits `sizeof`, `alignof`, and `offsetof` for every entry. The comparison script
normalizes OAT's richer `zone-layout-manifest-v1` down to the common ABI schema
and reports mismatches with both the OAT manifest line and the KISAK header
declaration line.

## Initial scope and gate

- `RawFile`: 1 structure, 3 members.
- `menuDef_t`: the full sampled graph, 17 structures and 155 members, including
  expression/operand unions, item subtype data, Material, and sound-alias
  terminal assets.
- Both Debug and Release must configure `-A Win32`, build the generator, emit
  each manifest twice byte-identically, and execute the comparator.
- Zero unexplained mismatches is required to adopt OAT-generated field maps.
  A mismatch is a failed conformance gate, not a warning.

The Windows workflow runs the self-contained command documented in
`tools/layout_conformance/README.md` in both Debug and Release.

## Hosted evidence and adjudication record

Windows run `29358731938` built and ran the Win32 probe in both configurations.
`RawFile` passed directly. Across the complete `menuDef_t` sample, MSVC and OAT
agreed numerically for all 17 structures and 155 members, including structure
sizes/alignments and member offsets/sizes/alignments. The initial comparator
reported four schema mismatches:

1. `src/ui/ui_shared.h` declares `entryInternalData` members as `op` followed
   by `operand`; the OAT manifest emits `operand` followed by `op`. Both records
   identify the type as a union, so declaration order has no layout meaning.
2. `src/ui/ui_shared.h` declares the operand union's string member as `string`;
   the OAT manifest emits `stringVal`. The coordinator's final ruling for run
   `29358731938` identifies these as the same union arm. That one name pair is
   recorded explicitly in the comparator's `ADJUDICATIONS` table.

The comparator now parses and compares each structure's `kind`. Non-union member
order remains strict. When both manifests say `union`, members are paired as a
set after applying only the structure-specific alias table. Every paired member
must still have identical offset, size, and alignment; the self-test proves an
aliased member with divergent numerics fails. Successful adjudication is visible
as `PASS-ADJUDICATED`, with alias and union-order-exemption counts plus a detail
line for every applied alias. This is not a blanket mismatch allowlist.

## Explicit boundary

This half proves C++ ABI conformance only. OAT count expressions, union-arm
conditions, block assignments, strings/script-strings, and asset-reference
semantics must be cross-checked against KISAK's real database `Load_*` source
before slice 7 can consume generated maps.
