# ENGINE TRACE — fixture 02 (StringTable + script strings + XAnimParts u16 remap)

Status: **ENGINE-QUALIFIED 2026-07-16 (K2 wave, Lane A).** This is the static
trace of the REAL loader path for a zone shaped like fixture 02, with
file:line citations into this tree. It adjudicates the corpus defect Sol
flagged in the K0/K1 review (challenge 5, `docs/reviews/
ff-kernel-k01-claude-response.md`): the builder's original mechanism-02
block assignments were traced against the engine and found WRONG for every
StringTable allocation. Fixture 02 was regenerated from this trace.

## Verdict up front

- **CONFIRMED CORPUS DEFECT.** `Load_StringTablePtr`
  (`src/database/db_load.cpp:5711-5728`) pushes NO stream block. At
  asset-dispatch time the active block is 4 — pushed once for the whole
  asset walk at `src/database/db_file_load.cpp:281` (`DB_PushStreamPos(4)`)
  — so the StringTable struct, its name, its values pointer array, and every
  cell string all land in **block 4**, not block 0 as the builder claimed.
- **XAnimParts placement was already correct**: `Load_XAnimPartsPtr`
  (`src/database/db_load.cpp:984-1013`) explicitly does
  `DB_PushStreamPos(0)` at line 990, so the 88-byte struct is a block-0
  allocation; `Load_XAnimParts` (`db_load.cpp:919-982`) then pushes block 4
  at line 922 for the name and names-index internals.
- Corrected block table for the valid fixture:
  `[88, 0, 0, 0, 126, 0, 0, 0, 0]` (was `[144, 0, 0, 0, 72, 0, 0, 0, 0]`).
  Every byte AFTER the 44-byte XFile header is unchanged — in particular the
  212-byte physical stream after the XAssetList — because alignment never
  emits file bytes; only the header's block-0 and block-4 size words
  (decompressed offsets 8 and 24) changed, so the decompressed SHA-256
  changed with them. The logical pad count did change: the old attribution
  had four pad bytes (31→32 block 4; 54→56 block 0; 69→70 block 4), the
  corrected one has two (31→32 and 123→124, both block 4). [Sol round 1,
  challenges 1 and 4.]

## Alignment and cursor model (engine ground truth)

- `DB_AllocStreamPos(alignment)` (`src/database/db_stream.cpp:81-86`):
  `g_streamPos = (uint8_t *)(~alignment & (uint32_t)&g_streamPos[alignment])`
  — align-up of the ACTIVE block's cursor to `alignment+1` bytes
  (`3` → 4-byte, `1` → 2-byte, `0` → no-op, `127` → 128-byte). This moves
  the LOGICAL cursor only; **no file bytes are consumed by alignment.**
  The engine aligns the zone-memory ADDRESS; block bases are allocated with
  at least that alignment, so aligning the block-relative OFFSET (what the
  builder and kernel do) is equivalent.
- `DB_IncStreamPos(size)` (`db_stream.cpp:88-94`): advances the active
  block's logical cursor; asserts it stays inside the block's XFile-declared
  size.
- `DB_PushStreamPos(index)` / `DB_PopStreamPos` (`db_stream.cpp:27-37,
  67-74`): a stack of active-block indices; a stack entry saves the
  PREVIOUS block index plus the NEW block's cursor at push time. Cursors
  persist across switches (`DB_SetStreamIndex`, `db_stream.cpp:48-65`) with
  ONE exception: `DB_PopStreamPos` (`db_stream.cpp:71-72`) REWINDS the
  cursor to its push-time value when the block being popped FROM is block 0
  (`if (!g_streamPosIndex) g_streamPos = ...pos`). Block 0 is the TEMP
  block: each temp allocation is discarded when its owning push/pop scope
  closes, its space is reused by the next temp asset, and
  `XFile.blockSize[0]` is therefore a HIGH-WATER mark, not an end-of-walk
  cursor sum. For fixture 02 (a single temp allocation) high-water and sum
  coincide at 88; a kernel cursor model must implement the rewind and
  compare high-water marks, or it will mis-account the first multi-temp
  zone. [Sol round 1, challenge 2.]
- `Load_Stream(atStreamStart, ptr, size)`
  (`src/database/db_stream_load.cpp:4-35`): when `atStreamStart && size`,
  blocks 1/2/3 are special (block 1 = memset zero-fill, blocks 2/3 = delayed
  queue — K4a scope); ALL OTHER blocks do `DB_LoadXFileData(ptr, size)` — a
  PHYSICAL sequential read from the zlib stream — then `DB_IncStreamPos`.
  When `atStreamStart == 0` it is a NO-OP (the bytes were already read as
  part of an enclosing struct). This is why the physical file cursor and the
  nine logical block cursors are separate state: alignment and zero-fill
  advance a logical cursor without consuming file bytes.
- Allocator alignments used below: `AllocLoad_FxElemVisStateSample()` =
  `DB_AllocStreamPos(3)` (`src/database/db_load.cpp:2616-2619`);
  `AllocLoad_XBlendInfo()` = `DB_AllocStreamPos(1)` (`db_load.cpp:516-519`);
  `AllocLoad_raw_byte()` = `DB_AllocStreamPos(0)` (`db_load.cpp:547-550`).

## Step-by-step walk (valid fixture 02)

Zone contents: 2 script strings (`script_zero`, `script_one`), asset array
`[STRINGTABLE(32), XANIMPARTS(2)]`, StringTable `synthetic/remap.csv` 2x1
with cells `key`,`value`, XAnimParts `synthetic/remap_anim` with
`boneCount[9]=1`, one u16 names index = 1, numframes=1, all other pointers 0.

Header phase — physical reads into locals/globals, NO block attribution:

1. `DB_LoadXFileInternal` (`src/database/db_file_load.cpp:266`):
   `DB_LoadXFileData(&file, sizeof(XFile))` — 44 bytes (`XFile` =
   `{size, externalSize, blockSize[9]}`, `src/xanim/xanim.h:1122-1128`).
2. `DB_AllocXZoneMemory` + `DB_InitStreams` (`db_file_load.cpp:278-279`;
   `db_stream.cpp:14-25`): all nine cursors at their block bases, active
   block 0.
3. `Load_XAssetListCustom` (`db_file_load.cpp:305-314`):
   `DB_LoadXFileData(&g_varXAssetList, sizeof(XAssetList))` — 16 bytes into
   a GLOBAL (`XAssetList` = `{count, strings, assetCount, assets}`,
   `xanim.h:1107-1120`). Not a `Load_Stream` call: no cursor movement.

Script-string phase — active block 4 (pushed at `db_file_load.cpp:310`,
pushed again inside `Load_ScriptStringList`, `db_load.cpp:644`):

4. `Load_ScriptStringList` (`db_load.cpp:641-652`): `Load_Stream(0,...,8)`
   is a no-op (metadata already in the global). `strings` token is tested
   for TRUTHINESS ONLY (line 645). Token -1 → `strings =
   AllocLoad_FxElemVisStateSample()` → align block 4 to 4 (cursor 0 → 0).
5. `Load_TempStringArray(1, count)` (`db_load.cpp:575-588`):
   `Load_Stream(1, ptr, 4*2)` → PHYSICAL read 8 bytes (the pointer array,
   two -1 tokens) → **block 4: 0 → 8**.
6. Per entry `Load_TempString(0)` (`db_load.cpp:557-573`): token -1 →
   `AllocLoad_raw_byte()` (align 0, no-op) → `Load_TempStringCustom`
   (`db_stream_load.cpp:74-84`) → `Load_XStringCustom`
   (`db_stream_load.cpp:59-72`) reads bytes PHYSICALLY one at a time
   through the NUL, then `DB_IncStreamPos(len+1)`:
   `script_zero\0` = 12 → **block 4: 8 → 20**;
   `script_one\0` = 11 → **block 4: 20 → 31**.
   `SL_GetString(*str, 4)` interns each string and replaces the payload
   pointer with the interned handle — this table is what u16 remaps index.
   NOTE: the engine never validates `count` — a hostile count is undefined
   behavior (signed `4 * count` overflow at `db_load.cpp:580`; see the
   malformed-twin section below for the exact failure modes). The kernel's
   `bad_count` refusal (count*4 in 64-bit must fit the remaining payload,
   checked BEFORE allocation) is KERNEL-ADDED validation, fail-closed where
   the engine is undefined.

Asset-array phase — `DB_PushStreamPos(4)` at `db_file_load.cpp:281`:

7. `assets` token truthiness-tested (`db_file_load.cpp:282`); -1 →
   `AllocLoad_FxElemVisStateSample()` → align block 4 to 4:
   **cursor 31 → 32 (1 logical pad byte, NO file byte)**.
8. `Load_XAssetArrayCustom(2)` (`db_file_load.cpp:316-329`):
   `Load_Stream(1, ptr, 8*2)` → PHYSICAL read 16 → **block 4: 32 → 48**.
   Then per entry `Load_XAsset(0)` (`db_load.cpp:6854-6859`) →
   `Load_XAssetHeader(0)` dispatch (`db_load.cpp:6746-6852`).

Asset 0 — STRINGTABLE (type 32 → `db_load.cpp:6847-6850`):

9. `Load_StringTablePtr(0)` (`db_load.cpp:5711-5728`): **NO
   `DB_PushStreamPos` ANYWHERE in this function** — the active block REMAINS
   4. Compare `Load_RawFilePtr` (`db_load.cpp:5664`) and
   `Load_XAnimPartsPtr` (`db_load.cpp:990`), which both push block 0. This
   is the adjudicated defect: the builder placed all StringTable material in
   block 0.
   Token -1 (exact `== (StringTable *)-1` test, line 5716; 0 skips; any
   other value → `DB_ConvertOffsetToPointer`, K3 scope) →
   `AllocLoad_FxElemVisStateSample()` → align block 4 to 4 (48 → 48).
10. `Load_StringTable(1)` (`db_load.cpp:5698-5709`):
    `Load_Stream(1, ptr, 16)` → PHYSICAL read 16 (`StringTable` =
    `{name, columnCount, rowCount, values}`,
    `src/universal/q_shared.h:862-869`) → **block 4: 48 → 64**.
11. `Load_XString(0)` on name (`db_load.cpp:590-606`): -1 →
    `AllocLoad_raw_byte()` (align 0) → PHYSICAL read
    `synthetic/remap.csv\0` = 20 → **block 4: 64 → 84**.
12. `values` truthiness-tested (line 5703; NOT a -1 test) →
    `AllocLoad_FxElemVisStateSample()` → align block 4 to 4 (84 → 84) →
    `Load_XStringArray(1, rowCount*columnCount = 2)` (`db_load.cpp:608-621`):
    PHYSICAL read 8 (two -1 cell tokens) → **block 4: 84 → 92**; then per
    cell `Load_XString(0)`: `key\0` = 4 → **92 → 96**; `value\0` = 6 →
    **96 → 102**.

Asset 1 — XANIMPARTS (type 2 → `db_load.cpp:6754-6757`):

13. `Load_XAnimPartsPtr(0)` (`db_load.cpp:984-1013`):
    **`DB_PushStreamPos(0)` at line 990** → active block 0. Token -1
    (line 994; -2 is the K3 alias-insert arm, line 998-999) →
    `AllocLoad_FxElemVisStateSample()` → align block 0 to 4 (0 → 0).
14. `Load_XAnimParts(1)` (`db_load.cpp:919-982`): `Load_Stream(1, ptr, 88)`
    → PHYSICAL read 88 (`XAnimParts`, `src/xanim/xanim.h:150-181`:
    name@0, numframes u16@14, boneCount[10]@18 so boneCount[9]@27,
    indexCount@36, names@48, indices@76) → **block 0: 0 → 88**.
15. `DB_PushStreamPos(4)` at line 922 → active block 4.
16. name `Load_XString(0)` (line 923-924): -1 → align 0 → PHYSICAL read
    `synthetic/remap_anim\0` = 21 → **block 4: 102 → 123**.
17. `names` truthiness-tested (line 925; ANY nonzero wire value means
    inline-follows — there is no -1/offset arm for this field) →
    `AllocLoad_XBlendInfo()` = `DB_AllocStreamPos(1)` → align block 4 to 2:
    **cursor 123 → 124 (1 logical pad byte, NO file byte)** →
    `Load_ScriptStringArray(1, boneCount[9] = 1)` (`db_load.cpp:532-545`):
    PHYSICAL read 2 → **block 4: 124 → 126**.
18. Per entry `Load_ScriptString(0)` → `Load_ScriptStringCustom`
    (`src/database/db_stringtable_load.cpp:3-6`):
    `*var = (uint16_t)varXAssetList->stringList.strings[*var]` — the u16
    REMAP: wire index → interned handle of script string [index]. Wire
    index 1 → `script_one`. The engine does NOT bounds-check the index;
    the kernel refuses an out-of-range index (kernel-added, fail closed).
19. All other XAnimParts pointer fields (notify@80, deltaPart@84,
    dataByte@52..randomDataInt@72) are 0 → their truthiness guards skip
    (lines 931-978). `indices` (`Load_XAnimIndices`, `db_load.cpp:683-700`)
    selects an arm by `numframes >= 0x100` (K4b scope); union arm token 0 +
    `indexCount` 0 → no reads.
20. `DB_PopStreamPos` (line 981) → block 0 (popping FROM block 4: no
    rewind); `Load_XAnimPartsAsset` (runtime interning — `DB_AddXAsset`
    copies the 88-byte struct out of temp space; no stream effect);
    `DB_PopStreamPos` (line 1012) — popping FROM block 0 → **block-0 cursor
    REWINDS 88 → 0** (`db_stream.cpp:71-72`) and the active block returns
    to 4. Block 0's XFile size (88) is the high-water mark of this
    discarded temp allocation.

End of walk: `DB_PopStreamPos` (`db_file_load.cpp:288`) → block 0 (popping
FROM block 4: no rewind).

## Corrected event table (what the regenerated fixture encodes)

| # | label | block | align | offset | file bytes |
|---|-------|-------|-------|--------|-----------|
| 1 | script_string_pointer_array | 4 | 4 | 0 | 8 |
| 2 | script_string[0] | 4 | 1 | 8 | 12 |
| 3 | script_string[1] | 4 | 1 | 20 | 11 |
| 4 | asset_array | 4 | 4 | 32 | 16 (pad 31→32, logical only) |
| 5 | stringtable[0] | **4** | 4 | 48 | 16 |
| 6 | stringtable[0].name | **4** | 1 | 64 | 20 |
| 7 | stringtable[0].values | **4** | 4 | 84 | 8 |
| 8 | stringtable[0].value[0] | **4** | 1 | 92 | 4 |
| 9 | stringtable[0].value[1] | **4** | 1 | 96 | 6 |
| 10 | xanimparts[0] | 0 | 4 | 0 | 88 |
| 11 | xanimparts[0].name | 4 | 1 | 102 | 21 |
| 12 | xanimparts[0].names | 4 | 2 | 124 | 2 (pad 123→124, logical only) |

Final block table: block 0 = 88, block 4 = 126, all others 0.
`xfile.size` = 16 (XAssetList) + 212 (physical stream bytes) = 228
(unchanged — physical byte sequence is identical to the old fixture; only
the header block table and the manifests' event attribution moved).

## Malformed twin (`fixture02_malformed_bad_script_count.ff`)

Identical bytes except `XAssetList.stringList.count` (payload offset 44)
patched to `0x40000000`. Engine behavior is UNDEFINED at the source level:
`count` is a signed `int` (`src/xanim/xanim.h:1107-1111`) and
`Load_TempStringArray` computes `4 * count` (`db_load.cpp:580`), a signed
overflow. On Win32 the expected machine result is 0 — the initial
`Load_Stream(1, ptr, 0)` then does no data or cursor work, so the first
loop iteration's `Load_TempString(0)` violates the
`atStreamStart == (ptr == DB_GetStreamPos())` invariant assertion
(`db_stream_load.cpp:6`): assert-enabled builds trap in `MyAssertHandler`,
assert-disabled builds attempt a 2^30-entry garbage-slot walk through
never-loaded memory (`db_load.cpp:582-587`). None of these outcomes is a
contract. The kernel must refuse `bad_count` BEFORE any allocation or array
walk: `count * 4` (64-bit) exceeds the remaining payload. Fail closed where
the engine is undefined; the refusal is kernel-added validation, same
doctrine as the K1 trailing-NUL check. [Sol round 1, challenge 3.]

## Builder deltas applied from this trace

- `build_stringtable_script_remap`: all five StringTable events moved from
  block 0 to block 4 (`Load_StringTablePtr` pushes no block; active block at
  dispatch is 4 per `db_file_load.cpp:281`).
- Fixture 02 zone files renamed to namespaced basenames
  (`fixture02_valid.ff`, `fixture02_malformed_bad_script_count.ff`) so both
  fixture pairs can ship flat in one app bundle (Sol K0/K1 challenge 8);
  manifests' `container.file` and `SHA256SUMS` paths updated. Fixture 01
  and all other fixtures remain byte-identical.
