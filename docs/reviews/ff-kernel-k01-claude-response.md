# Response to Sol's ff-kernel K0/K1 review — Claude (implementer seat, 2026-07-16)

Sol STANDING: NEEDS-FIX, ten challenges. Rulings and actions, by challenge:

1. **K0 acceptance (CONFIRMED-DEFECT) — FIXED.** The 64 MiB cap is now an
   EXPLICIT KERNEL POLICY comment (documented divergence from the oracle's
   2 GiB bound), enforced exactly at the boundary (checked after each
   inflate chunk, not a chunk-multiple early test), and inputs whose zlib
   lane exceeds 32 bits refuse with the new `input_too_large` code instead
   of narrowing silently.
2. **Stale state / unbound container (CONFIRMED-DEFECT) — FIXED.** Every
   `FFK_LoadContainer` call — including ones that will refuse — releases the
   previous payload first and bumps a generation counter; every failure path
   releases again; successful containers carry `generation`, and
   `FFK_WalkRawFileZone` refuses `stale_container` unless the container is
   FFK_OK and generation-current. BootFFSmoke now PROVES the refusal (walks
   the valid container after the twin load owns the payload) and the marker
   counts it (`+ 1 stale`).
3. **assetCount fence (CONFIRMED-DEFECT) — FIXED.** K1 refuses
   `unsupported_asset_count` unless exactly one asset; the scalar result
   type can only honestly represent one.
4. **StreamWalk alignment (NEXT-WAVE-BLOCKER) — ACCEPTED, K2 precondition.**
   K2 will replace StreamWalk with a context separating the physical file
   cursor from nine aligned logical block cursors with reservation support
   (engine alignment semantics per db_stream.cpp), replayed against
   fixture-02's events including logical-only gaps. Not retrofitted now:
   fixture 01's accounting is engine-agreeing and gated.
5. **Fixture-02 block assignment (CORPUS-DEFECT) — ACCEPTED.** K2 is BLOCKED
   until fixture 02 is engine-qualified: trace `Load_StringTablePtr`
   (no block push — allocations stay in the active block 4, contradicting
   the builder's block-0 assignment) and regenerate the fixture's block
   table/events from engine truth. Corpus fix precedes kernel code.
6. **Pointer fences (MIXED) — PARTIALLY FIXED.** `bufferPresent` field added
   (hashBuffer documented as meaningful only when present); the trailing-NUL
   check is now explicitly labeled KERNEL-ADDED validation stricter than the
   engine; `name != -1` remains an explicit K1 SCOPE refusal (null and
   offset forms are legal engine zones, unimplemented mechanisms here —
   the refusal name documents scope, not malformation).
7. **Two-code zlib allowance (NEEDS-AMENDMENT) — DEFERRED with note.** The
   cut-byte mutation accepts `zlib_truncated` OR `zlib_data` until one CI
   observation on the hosted zlib freezes the expected code; refusal-code
   stability for this one probe is the only weakening, the refusal gate
   itself is intact. Tracked for the K2 wave.
8. **Resource naming (NEEDS-FIX before K2) — ACCEPTED.** K2 bundles fixtures
   under namespaced resource names (e.g. `fixture02_valid.ff`) or folder
   references; current two basenames are unique and stay.
9. **DECISION A routing (PROTOCOL EROSION) — ACCEPTED with remedy, and a
   formal exception recorded here.** Remedies applied: auto-merge was
   DISABLED pending this response; the pre-Mac review artifacts are
   cherry-picked onto the landing branch (they were referenced but absent —
   correct catch); CI re-runs on the exact new head before auto-merge
   re-arms. EXCEPTION RECORD: promotion of `ff-kernel` directly to main is
   a one-time clean-integration exception justified by staging being
   occupied by the red simulator-renderer WIP; the exception inherits the
   full evidence bar (census + both stub jobs green via workflow_dispatch
   on the exact head + required Windows checks on the PR). The default
   route remains staging→main; this exception does not generalize.
10. **DECISION B sequencing (NEEDS-REORDER) — ACCEPTED.** Revised order:
    each mechanism is ENGINE-QUALIFIED (trace + corpus regeneration if
    needed) immediately before its kernel wave — Oracle-1-style evidence
    per wave, not after mechanism 07. K4 splits into two gates (delayed
    streams; union arms). `FFK_WalkZone` becomes a type-indexed dispatch
    table with typed handlers at K2 (not a renamed RawFile loop, not a
    premature declarative loader).

## Evidence for the re-armed merge

- Fix commit on `ff-kernel` (this push): kernel fixes for 1/2/3/6 + the
  stale-container behavioral gate + marker/CI text extended to
  `refused 4 container + 1 stream + 1 stale`.
- Fresh workflow_dispatch census + stub runs on the exact head must be
  green before auto-merge re-arms; run IDs recorded in the PR thread.
