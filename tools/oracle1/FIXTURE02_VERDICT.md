# Fixture 02 (`stringtable_script_remap`) — StringTable block verdict

**Dispute.** The fixture builder
(`tools/zone_fixtures/build_zone_fixtures.py`, reflected in
`02_stringtable_script_remap/MANIFEST.json` `stream_events`) placed the
StringTable body — `stringtable[0]` struct (16 B), `.name` (20 B),
`.values` (8 B), `.value[0..1]` (4+6 B) — in **block 0**. The K-series
kernel work flagged this (ff-kernel K0/K1 review, challenge 5): static
reading of the engine says otherwise, and K2 is blocked until the fixture
is engine-qualified.

**Engine truth (static, db_load.cpp / db_stream.cpp / db_file_load.cpp).**
`Load_StringTablePtr` (db_load.cpp:5711–5728) performs **no**
`DB_PushStreamPos`. At STRINGTABLE dispatch time the active block is 4
(virtual), pushed by `DB_LoadXFileInternal` (db_file_load.cpp:281) before
the asset-array walk. Therefore the `-1` header token allocates the
16-byte StringTable struct via `AllocLoad_FxElemVisStateSample()`
(`DB_AllocStreamPos(3)`) **in block 4**, and `Load_StringTable`
(db_load.cpp:5698–5709) reads name, values array, and cell strings —
still with no push — **in block 4**. Contrast `Load_RawFilePtr`
(db_load.cpp:5664: pushes block 0) and `Load_XAnimPartsPtr`
(db_load.cpp:990: pushes block 0), which is why fixture 01 is correct and
fixture 02's XAnimParts struct placement (block 0) is also correct.

**Desk-computed runtime walk (block 4 cursor).**
script-string pointer array 0–7, "script_zero\0" 8–19, "script_one\0"
20–30, asset array (align 3) 32–47, StringTable struct (align 3) 48–63,
name alloc at 64. The name's 20-byte `DB_IncStreamPos` requires
64+20 = 84 > 72 (the fixture's declared block-4 size), so the engine's own
fence `iassert(g_streamPos + size <= ... data + size)` (db_stream.cpp:91)
refuses the zone. Fixture 02 **as built cannot complete a real-engine
load**: its block-4 declaration undersizes the engine's actual block-4
demand, and its 144-byte block 0 receives only XAnimParts' 88-byte struct
(which the walk never reaches past the fence).

**RUNTIME CONFIRMATION (Oracle 1 trace, CI).**

> Status: **CONFIRMED** — GitHub Actions run **29522138080** on
> `oracle1-instrument` head `f7a105e`, step `Oracle 1 engine loader gate`
> GREEN in BOTH configs. The fixture-02 refusal trace is byte-identical
> across the double runs AND across Debug/Release (sha256 prefix
> `35673d636f17` for all four traces). Exit code 4 (engine assert), as
> pinned by the workflow. Gate c (`tools/oracle1/check_trace.py --gate c`,
> adjacency-hardened per Sol round-2) is GREEN and remains the mechanized
> guard: it goes red if any future trace contradicts this section.

Observed trace excerpt (`02_stringtable_script_remap-a.txt`, verbatim
except eliding per-byte inflate runs):

```
ev=assetlist strings=2 strings_token=0xffffffff assets=2 assets_token=0xffffffff
  ...script-string bytes fill block 4 [0,31); handles 1,2 interned...
ev=alloc block=4 align=3 offset=32          <- asset array (cursor 31 aligned to 32)
ev=fill block=4 offset=32 size=16 src=file
ev=asset_dispatch index=0 type=32 name=stringtable
ev=alloc block=4 align=3 offset=48          <- StringTable struct, ACTIVE block 4
ev=fill block=4 offset=48 size=16 src=file
ev=inc block=4 offset=48 size=16
ev=alloc block=4 align=0 offset=64          <- name bytes, still block 4
ev=inflate size=1 dest=block4+64 ... dest=block4+71    (8 in-budget bytes)
ev=inflate size=1 dest=external ... (x12)              (bytes 72..83, beyond the
                                                        declared 72-byte block 4)
ev=inc block=4 offset=64 size=20            <- fence ATTEMPT (cursor never commits)
ev=error kind=assert detail=...db_stream.cpp:91 g_streamPos + size <=
    g_streamZoneMem->blocks[g_streamPosIndex].data + ...size        (exit 4)
```

Gate c verdict line (identical both configs):
`FIXTURE02_VERDICT: stringtable_struct_block=4 stringtable_struct_offset=48
name_block=4 name_offset=64 builder_claim_block=0
engine_contradicts_builder=true load_completed=false
engine_fence_tripped=true`

Block 0 (declared 144 bytes) received **zero** bytes before the refusal;
the XAnimParts asset (index 1) was never dispatched. The desk-computed
walk above and the Sol pair's independent round-1 recomputation are
confirmed byte-for-byte, under the documented evidence label "real loader
walk under Oracle assert/scaffold policy" (RELEASE_ASSERTS keeps the
db_stream.cpp:91 fence observable that shipping Release compiles out; the
walk itself is unmodified engine code).

**Verdict (RUNTIME tier).** The engine puts the StringTable body in the
**active block 4**, not block 0. The builder's block-0 claim is **refuted
at RUNTIME tier**. Lane A's regeneration must (i) move the StringTable
struct, name, values array, and value strings into block-4 accounting,
(ii) resize the declared block sizes accordingly (block 0 carries only
the XAnimParts struct), and (iii) regenerate `stream_events` /
`oracle_v1.blocks.bytes` / container hashes. This section was written
from the CI artifacts alone — Lane A's regeneration was NOT consulted
(independence preserved for the coordinator's reconciliation). On any
disagreement, this runtime trace is the final word (RUNTIME > static
reading).

## RECONCILED (Lane A K2 integration, 2026-07-16)

The coordinator's cross-check of this verdict against Lane A's INDEPENDENT
static trace (`tools/zone_fixtures/ENGINE_TRACE_02.md`, written before this
document was consulted) found the two methods in full agreement: StringTable
body in ACTIVE block 4, struct at block4+48, name at block4+64, XAnimParts
struct in temp block 0, corrected block table `[88,0,0,0,126,0,0,0,0]`.

The K2 wave regenerated fixture 02 exactly as items (i)-(iii) demand
(commit 4bfd3b3; zone basenames namespaced to `fixture02_valid.ff` /
`fixture02_malformed_bad_script_count.ff` for flat app bundling). This
integration therefore FLIPS the CI pin that guarded the OLD fixture:

- fixture 02 exit is now pinned to **0** (clean engine load) instead of 4;
- `check_trace.py --gate c` now pins the full predicted event spine of the
  REGENERATED fixture (struct/name/values placement, XAnimParts push/pop
  discipline, the u16 `scriptstring_remap index=1` structure, both
  `asset_link ... redirected=1` events, `zone_loaded`, and NO error);
- the malformed twin table uses the namespaced twin filename (its
  engine-native refusal pin, exit 4/5, is unchanged).

The refusal behavior of the ORIGINAL fixture documented above remains the
historical runtime confirmation of the corpus defect; the trace excerpt and
run IDs stand as recorded. The runtime trace remains the arbiter for the
regenerated fixture: if it ever contradicts the gate-c spine, that is a
corpus/kernel reconciliation, never a silent expectation edit.
