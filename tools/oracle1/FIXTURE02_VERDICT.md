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

> Status: **PENDING** — this section is filled in from the first green
> `Oracle 1 engine loader gate` CI run on `oracle1-instrument` (run ID and
> the literal trace excerpt go here; the desk prediction above is
> STATIC-tier until then). Gate c (`tools/oracle1/check_trace.py --gate c`)
> is the mechanized form of this check: it goes red if the runtime trace
> contradicts the block-4 placement, forcing reconciliation of this
> document before any merge.

Predicted trace shape to be confirmed
(`02_stringtable_script_remap-a.txt`, both configs, deterministic):

```
ev=asset_dispatch index=0 type=32 name=stringtable
ev=alloc block=4 align=3 offset=48          <- StringTable struct, ACTIVE block 4
ev=fill block=4 offset=48 size=16 src=file
ev=inc block=4 offset=48 size=16
ev=alloc block=4 align=0 offset=64          <- name bytes, still block 4
ev=inc block=4 offset=64 size=20            <- ATTEMPT (hook fires at entry;
                                               the cursor never commits)
ev=error kind=assert detail=...db_stream.cpp:91...    (exit 4)
```

**Verdict (static tier, runtime confirmation pending).** The engine puts
the StringTable body in the **active block 4**, not block 0. The builder's
block-0 claim is refuted at the STATIC tier now, and becomes refuted at
the RUNTIME tier the moment the pending section above is filled from a
green CI trace — the tier claimed by this document is exactly the tier of
the evidence recorded in it (no forward-dated claims). Lane A's
regeneration must (i) move the StringTable struct, name, values array,
and value strings into block-4 accounting, (ii) resize the declared block
sizes accordingly (block 0 carries only the XAnimParts struct), and (iii)
regenerate `stream_events` / `oracle_v1.blocks.bytes` / container hashes.
The coordinator reconciles this document against Lane A's static-derived
regeneration; once the runtime section is filled, the runtime trace is
the final word (RUNTIME > static reading).
