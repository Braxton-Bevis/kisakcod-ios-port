# KISAK/OAT layout conformance gate — KISAK-side implementation

Status: **implemented locally; hosted Win32 verdict pending**.

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

The workflow hook is deliberately left to the coordination seat because the
Windows workflows were hot during this slice. The self-contained command is
documented in `tools/layout_conformance/README.md`.

## Headline findings before hosted execution

Two schema mismatches are already confirmed directly from the checked-in
sources:

1. `src/ui/ui_shared.h` declares `entryInternalData` members as `op` followed
   by `operand`; the OAT golden emits `operand` followed by `op`.
2. `src/ui/ui_shared.h` declares the operand union's string member as `string`;
   the OAT golden emits `stringVal`.

The first does not change union offsets but disproves byte-identical field
schema/order. The second is a naming divergence that would break unnormalized
generated field-map consumption. Neither is papered over. Hosted MSVC output
must now settle numeric alignment/offset questions, especially OAT's reported
8-byte alignment for `Material`.

## Explicit boundary

This half proves C++ ABI conformance only. OAT count expressions, union-arm
conditions, block assignments, strings/script-strings, and asset-reference
semantics must be cross-checked against KISAK's real database `Load_*` source
before slice 7 can consume generated maps.
