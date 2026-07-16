# K2 Lane A — Sol adversarial rounds (ChatGPT 5.6 Sol, ULTRA)

Lane A (Claude Fable 5) is the implementer; Sol is the adversarial critic.
Protocol: Sol attacks (a) the engine trace + fixture regeneration BEFORE
kernel code, and (b) the implementation BEFORE CI dispatch. Every sustained
challenge is answered here before proceeding.

## Round 1 (a) — trace + regenerated fixture 02 (2026-07-16)

Sol STANDING: **NEEDS-FIX** — "the regenerated fixture bytes, block
attribution, manifests, and checksums are accepted"; 8 of 13 challenges
REFUTED (claims stand, including the core corpus-defect adjudication:
StringTable → block 4, XAnimParts placement, block table
`[88,0,0,0,126,0,0,0,0]`, twin single-field delta, SHA256SUMS consistency).
Five challenges SUSTAINED; rulings and actions:

1. **Padding-identical claim (SUSTAINED) — FIXED.** The trace claimed the
   alignment pads "happen to be identical" between old and new
   attributions. False: old had four logical pad bytes (31→32, 54→56,
   69→70), new has two (31→32, 123→124). The correct reason the physical
   bytes survive is that alignment NEVER emits file bytes. Trace rewritten
   to state the pad counts and the real invariant.
2. **Block-0 rewind (SUSTAINED — the important one) — FIXED in trace AND
   adopted in the kernel model.** `DB_PopStreamPos`
   (`src/database/db_stream.cpp:67-74`) rewinds the cursor to its push-time
   value when popping FROM block 0: block 0 is the TEMP block, per-asset
   temp allocations are discarded and their space reused, and
   `XFile.blockSize[0]` is a HIGH-WATER mark, not a cursor sum. Fixture 02
   (single temp allocation) is unaffected — sum == max == 88 — but the K2
   `FFKStream` context implements the rewind exactly (push saves
   {previous index, new block's cursor}; pop from block 0 restores the
   saved cursor) and block accounting compares HIGH-WATER marks. Without
   this, the first multi-temp-asset zone (fixture 04 has two RawFiles)
   would be mis-accounted. Desk-check mirrors the same model.
3. **Malformed-twin narrative (SUSTAINED) — FIXED.** "wraps to 0 then walks
   2^30 slots" overstated determinism: `4 * count` is SIGNED overflow
   (count is `int`, `src/xanim/xanim.h:1107-1111`; multiply at
   `db_load.cpp:580`) — UB at source level, 0 only as expected Win32
   machine behavior; and if 0, the first `Load_TempString(0)` violates the
   `Load_Stream` atStreamStart invariant assertion
   (`db_stream_load.cpp:6`) — assert builds trap, non-assert builds
   ATTEMPT the 2^30 garbage-slot walk. Trace rewritten; the kernel
   `bad_count`-before-allocation ruling is unchanged (Sol concurs).
4. **"Payload sequence unchanged" wording (SUSTAINED) — FIXED.** Literally
   false: decompressed offsets 8 and 24 (block-0/block-4 size words in the
   XFile header) changed, and the decompressed SHA-256 with them. Precise
   claim now in the trace: all bytes AFTER the 44-byte XFile header — in
   particular the 212-byte physical stream after the XAssetList — are
   unchanged.
5. **Filename documentation (SUSTAINED) — FIXED.** Builder comment
   narrowed from "fixture 02 onward" to exactly fixture 02 (03–07 keep
   `valid.ff` until their own bundling waves); README no longer claims
   every directory contains `valid.ff` and now points at each manifest's
   `container.file`.

Kernel-code consequences carried forward from this round:

- `FFKStream` = one physical file cursor + nine aligned logical cursors +
  push/pop stack **with the block-0 pop rewind** + per-block high-water
  accounting compared against the XFile table.
- `bad_count` refusal fires on 64-bit `count * 4 > remaining payload`,
  before any allocation or pointer-array walk.
- Alignment is a logical-only advance (never consumes payload bytes) —
  reservation semantics per `DB_AllocStreamPos`.

## Round 2 (b) — implementation review before CI dispatch

(To be appended after the implementation round runs.)
