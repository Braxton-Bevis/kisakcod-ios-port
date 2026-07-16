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

## Round 2 (b) — implementation review before CI dispatch (2026-07-16)

Sol STANDING: **NEEDS-FIX** — 14 challenges: 7 REFUTED (stream-model
fidelity, K1 parity's public contract, bad_count implementation, handler
placement, enum stability, constexpr dispatch portability, marker/grep/
bridge byte agreement), 6 SUSTAINED + 1 latent. Rulings and actions:

1. **StringTable cell-count multiply wrap (SUSTAINED — real OOB read in
   commit 350c443) — FIXED + REGRESSION.** `cellCount * 4` wraps u64 at
   rows = cols = 2^31, passing the bound and over-reading the token array.
   Fixed with the division form (`cellCount > remaining / 4`); the smoke
   gains a synthetic 2^31 x 2^31 StringTable probe that must refuse
   `stream_truncation` without crashing, and the desk check mirrors it.
   (Found independently in self-audit minutes after the commit; Sol's
   catch confirms it and demanded the regression leg — added.)
2. **XAnim fence broader than documented (SUSTAINED) — SETTLED AS
   DOCUMENTED FENCE.** The fence now explicitly claims an EXACT-ZERO
   METADATA SHAPE, deliberately stricter than the engine's
   pointer-truthiness gating (a nonzero count with a null pointer is
   engine-ignored, kernel-refused), applied BEFORE the name token as a
   scope decision, not engine order. Header comment updated to match.
3. **RAWFILE-only mask never exercised (SUSTAINED) — FIXED.** The
   two-asset scope leg exits at the count fence before the mask. Added a
   synthetic ONE-asset StringTable zone that `FFK_WalkRawFileZone` must
   refuse with `unsupported_asset_type`; widening the mask now fails the
   marker. Marker counts 2 scope refusals.
4. **K1 public blockUse unbound (SUSTAINED) — FIXED.** The K1 positive leg
   now memcmps `rawfile.blockUse` against the manifest block table.
5. **Marker overstates bad_count ordering proof (SUSTAINED) — CONCEDED in
   wording.** The marker binds the refusal CODE; the before-allocation
   ordering is desk-checked and review-verified, not marker-proven. Smoke
   comment reworded; no behavioral change (Sol confirmed the code order is
   correct).
6. **Unchecked smoke malloc (SUSTAINED) — FIXED.** The mutant allocation is
   now null-checked; new probe allocations check too.
14. **Fixture-only pushes bypass the smoke (SUSTAINED, latent) — FIXED.**
   `tools/zone_fixtures/**` added to ios-stub.yml's push path filter.

## Frontier ruling P0 folded into K2 (coordinator directive, 2026-07-16)

docs/reviews/frontier-plan-claude-ruling.md (external frontier-model
review) confirmed a P0 the two-model loop missed: the kernel's 64 MiB
payload cap refuses the goal artifact's own zones
(docs/REAL_ZONE_EVIDENCE.md: localized_common_mp 70,269,481;
mp_killhouse 76,935,387 decompressed). Adopted in this wave:

- `InflateOuterPayload` is now the HEADER-FIRST EXACT-SIZE reader: inflate
  the 44-byte XFile header only, read declared xfile.size, 64-bit
  checked-add +44, validate against an explicit 256 MiB policy bound
  (> 3x the largest real zone), allocate EXACTLY ONCE, continue the same
  zlib stream, require exact final length + Z_STREAM_END + no trailing
  input. New appended refusal `payload_size_mismatch` for produced !=
  declared in either direction.
- CONSEQUENCE ADJUDICATED: fixture 01's malformed twin used to declare its
  PRE-truncation size; under the exact reader that is a container-layer
  `payload_size_mismatch`, which would have destroyed the twin's frozen
  contract (container-accept + walk stream_truncation). The twin was
  regenerated with a TRUTHFUL declared size (66), preserving the walk-layer
  refusal exactly; the walk gate stays behaviorally tested.
- LATENT for K4a: fixture 06's malformed twin still declares its
  pre-truncation size and would now refuse at the container layer, not the
  delayed-drain layer its manifest claims. Fixture 06 is not bundled or
  gated yet; the K4a wave must engine-qualify mechanism 06 and regenerate
  that twin the same way (recorded here so it is not rediscovered).
- Smoke: killhouse-size acceptance leg (zeros-only synthetic zone declaring
  exactly 76,935,387 bytes — no game data) + declared-size mismatch
  refusals in both directions; marker extended accordingly (`exact-size
  reader loads a killhouse-size zone`, `refused 6 container ... + 1
  overflow ...`), ios-stub.yml grep updated in the same commit.

## Round 3 (belt-and-braces on the P0 reader) — completed on retry

Attempt 1 stalled against a degraded Codex API after ~4h with no verdict
(its transcript shows it mid-verification of the DeflateZone helper
against the vendored zlib source). The protocol retry completed
decisively. Verdict: **NEEDS-FIX with exactly one code finding**, the
rest of the P0 reader upheld:

1. **Probe-path misclassification (my claim REFUTED) — FIXED.** The
   window-full one-byte probe treated every `Z_OK` as over-production. A
   CUT DEFLATE TRAILER at exactly the declared boundary consumes input,
   emits nothing, and returns `Z_OK` — that input class is a TRUNCATION,
   not a size mismatch. The probe is now a loop: only an actually-emitted
   byte proves over-production (`payload_size_mismatch`); trailer bytes
   consumed without output continue; no-progress with exhausted input is
   `zlib_truncated`; anything else is `zlib_data`. Both classifications
   fail closed — the fix corrects the refusal CODE for a hostile edge, no
   gate weakens.
2. Upheld with file:line evidence: the four fixture-01 mutation probes
   keep their old codes under the exact reader; the twin-01 regeneration
   is coherent (container accepts declared=66, walk refuses
   stream_truncation) — noting correctly that the builder +
   MALFORMED_MANIFEST changed alongside the twin bytes and SHA256SUMS;
   DeflateZone is sound (uInt casts safe, chunked zero fill, finish
   sequencing); the marker snprintf with counts 6/1/1/2/1/1 byte-matches
   the ios-stub.yml grep (239-byte UTF-8 line).
