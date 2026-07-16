# bmk4-oracle1 — engine-instrumented zone loader (Oracle 1)

Status: DESIGN (Lane C). Reviewed adversarially by the Sol pair before code;
review record in `docs/reviews/oracle1-lane-c-notes.md`.

## 1. Position in the oracle taxonomy

Per `docs/reviews/orchestrator-doctrine-claude.md`, Oracle 0
(`tools/ff_oracle`) is a container inspector: it parses bytes but executes no
loader semantics. Oracle 1 is the *engine-instrumented* loader: the zone is
loaded by the REAL Windows engine loader code and the tool records what that
code actually does. Runtime asset-graph semantics must come from Oracle 1,
not Oracle 0 (doctrine ruling 3), and every kernel wave must be
engine-qualified by Oracle-1-style evidence before it ships
(`ff-kernel-k01-claude-response.md`, challenge 10).

Evidence tier of every claim this tool produces: **RUNTIME (Windows x86,
real loader TUs)** — stronger than the static-read tier the fixture builder
and the layout manifests occupy.

## 2. Closure analysis — what links, what is scaffolded

### 2.1 Real engine TUs (linked whole, unmodified semantics)

The tool links the complete `DATABASE` module exactly as the shipping
targets do (`scripts/common_files.cmake`):

| TU | Role in the load | Link-time externals (beyond the database module / CRT / Win32) |
|---|---|---|
| `db_load.cpp` | All `Load_*` walkers, dispatch (`Load_XAsset(Header)`), all `AllocLoad_*` | `MyAssertHandler`, `SND_SetData`, `Com_GetServerDObj`, `Com_GetClientDObj`, `DObjArchive`, `DObjUnarchive`, gfx loader tails (`Load_Texture`, `Load_VertexBuffer`, `Load_BuildVertexDecl`, `Load_CreateMaterialPixel/VertexShader`, `Load_PicmipWater`) |
| `db_stream.cpp` | Block cursors: `DB_InitStreams/Push/Pop/SetStreamIndex/AllocStreamPos/IncStreamPos/InsertPointer` | `MyAssertHandler` (via iassert/vassert expansion) |
| `db_stream_load.cpp` | `Load_Stream`, `Load_DelayStream`, `DB_ConvertOffsetToPointer/Alias`, `Load_XStringCustom`, `Load_TempStringCustom` | `MyAssertHandler`, `SL_GetString` |
| `db_stringtable_load.cpp` | `Load_ScriptStringCustom` (index remap), `Mark_ScriptStringCustom` | `SL_AddUser` |
| `db_file_load.cpp` | `DB_LoadXFile(Internal)`, staged overlapped reads, inflate pump, `Load_XAssetListCustom`, `Load_XAssetArrayCustom` | `Com_Error`, `Com_Printf`, `va`, `I_stricmp`, `Sys_WaitDatabaseThread`, `R_DelayLoadImage`, `R_FinishStaticVertexBuffer/IndexBuffer`, `KISAK_NULLSUB`, Win32 (`ReadFileEx`, `SleepEx`, …) |
| `db_memory.cpp` | `DB_AllocXZoneMemory`, `DB_MemAlloc` | `Com_Error`, `PMem_Alloc`, `PMem_GetOverAllocatedSize`, `R_*StaticVertexBuffer/IndexBuffer` family |
| `db_auth.cpp` | `DB_AuthLoad_Inflate*` (zlib lane) | zlib (`inflateInit_`, `inflate`, `inflateEnd`) + `MyAssertHandler` (iassert) |
| `db_assetnames.cpp` | `DB_GetXAssetName`, per-type name get/set, type sizes | `MyAssertHandler`, `va` |
| `db_registry.cpp` | `DB_AddXAsset`, `DB_LinkXAssetEntry`, `DB_AllocXAssetEntry`, pools, hash table, `Load_*Asset` for every type, `DB_AllocMaterial`/`DB_FreeMaterial` (defined HERE, not external) | large: `Sys_*` thread/database family, `SL_ConvertToString/GetString/ShutdownSystem/TransferSystem`, `PMem_*`, `FS_*`, `Cmd_*`, `Dvar_RegisterString`, `Com_*` print/error/sync, `NET_Sleep`, `R_*`/`RB_*`/`Material_*` render family, `CG_VisionSetMyChanges`, `BG_FillInAllWeaponItems`, `CM_Unload`, `Image_IsProg`, `Win_GetLanguage`, `Z_Free`, `Hunk_AddAsset`, `track_static_alloc_internal`, `ProfLoad_*`/`Profile_*`, string utils (`I_*`), `DB_LoadSounds`/`DB_SaveSounds`; data globals `cm`, `comWorld`, `gameWorldMp`, `fs_gameDirVar`, `loc_warnings*`, `useFastFile` (via inline `IsFastFileLoad`, REACHED on every successful load), `com_missingAssetOpenFailed`, `com_sv_running`, `fs_numServerReferencedFFs`, `fs_serverReferencedFFNames`. `KISAK_NULLSUB` is inline in qcommon.h — NOT scaffolded. The census is grep-derived and CI-proven: the Debug `/OPT:NOREF` link fails loudly on any miss. |

Nothing inside these TUs is reimplemented, wrapped-out, or forked. The
functions the K-series kernel needs qualified — `Load_Stream`,
`DB_AllocStreamPos`, push/pop, `Load_RawFile` (db_load.cpp:5643),
`Load_StringTablePtr` (db_load.cpp:5711), `Load_ScriptStringList`,
`Load_TempString*`, `Load_XString*`, `Load_XAnimParts*`,
`DB_ConvertOffsetToPointer/Alias`, `DB_InsertPointer`,
`DB_LinkXAssetEntry` — all execute as compiled from the real sources.

`ASSET_TYPE_RAWFILE = 31`, `ASSET_TYPE_STRINGTABLE = 32`,
`ASSET_TYPE_XANIMPARTS = 2` (xanim.h, MP numbering) — matching the fixture
manifests' `RAWFILE(31)` / `STRINGTABLE(32)` / `XANIMPARTS(2)` labels; the
tool compiles with `KISAK_MP` like the mp/dedi shipping targets.

### 2.2 Tool-owned code (four files, `tools/oracle1/`)

1. **`oracle1_main.cpp`** — CLI, allowlist enforcement (copied from Oracle
   0's refusal pattern: canonicalized containment check, exit 3, output
   path checked too), trace file management, and the zone-load driver.
   The driver replicates the *setup* half of `DB_TryLoadXFileInternal`
   (db_registry.cpp:2650–2716) rather than calling it, because the real
   function front-loads mod-dir probing, the `zone_reorder` dvar, and
   `$init` waits that belong to the full client. The driver performs, in
   order, exactly what the engine performs before `DB_LoadXFile`:
   `DB_Init()` (real), zone-slot claim in `g_zones[1]` + `g_zoneHandles` +
   `g_zoneCount`/`g_loadingZone`/`g_zoneIndex`, `CreateFileA(...,
   0x60000000, ...)` with the engine's exact flags
   (`FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING`), `g_loadingAssets = 1`,
   `DB_ResetZoneSize(0)`, then hands off to the REAL `DB_LoadXFile` +
   `DB_LoadXFileInternal` with the real `g_fileBuf`. Every replicated line
   cites its db_registry.cpp source line in a comment.
2. **`oracle1_trace.cpp/.h`** — the event emitter (schema §4). Owns the
   output stream; every write is flushed line-wise so refusal paths keep
   their prefix.
3. **`oracle1_scaffold.cpp`** — the abort-loud link scaffold in the
   `ios/Stub/BootScaffold.cpp` discipline: every out-of-scope engine symbol
   referenced by the nine TUs gets a definition that prints its own name
   and exits with the scaffold code; a small documented subset is
   *functional* (§2.3). Declarations come from the same engine headers the
   database TUs use, so a signature drift is a compile/link error, not a
   silent ABI break.
4. **`check_trace.py`** — the qualification gates (§6).

### 2.3 Functional scaffold subset (documented allowances)

These are *platform/service boundaries*, not loader semantics. Each is
listed with its contract and why the trace stays honest:

| Symbol(s) | Tool-owned behavior | Honesty argument |
|---|---|---|
| `MyAssertHandler` | emit `ev=error kind=assert` (file:line + unformatted fmt by default; formatted only under `--emit-names`) + flush + `exit 4` | This is the engine's own refusal firing; the tool only reports it. Never returns, matching engine expectation. Default output carries only engine-source literals — no `%s`-interpolated asset names. |
| `Com_Error`, `Com_ErrorAbort`, `Sys_Error` | emit `ev=error kind=com_error` (code + unformatted fmt by default) + flush + `exit 5` | Same; ERR_DROP never returns into the loader. |
| `Com_Printf/PrintWarning/PrintError`, `va`, `Com_sprintf` | suppressed unless `--emit-names`; then stdout/stderr passthrough | Engine print text interpolates asset/zone names — an unsanitized CI-log channel if left open (Sol round-1 finding 9). Never enters the trace stream. |
| driver read ring | VirtualAlloc'd 0x80000 buffer passed to `DB_LoadXFile` instead of `g_fileBuf` | Sector-aligned for the engine's own `FILE_FLAG_NO_BUFFERING` open flags (a static byte array carries no such guarantee in this exe) and OS-zeroed for determinism. Buffer identity has no loader semantics — only the 0x80000 ring size and 4-byte alignment are contracted. |
| `PMem_Alloc`, `PMem_GetOverAllocatedSize`, `PMem_Begin/EndAlloc`, `PMem_Free`, `Z_Free` | 4096-aligned VirtualAlloc arena, **plus 64 KiB guard slack per block** | The engine's own `DB_IncStreamPos` fence (`db_stream.cpp:91`) stays the *observable* failure on an over-model walk instead of a heap AV that would truncate the trace. Block *sizes* given to `DB_AllocXZoneMemory` are the zone's own declared sizes — the fence itself is untouched engine code. |
| `Sys_IsMainThread/IsDatabaseThread/IsRenderThread` | `true/false/false` | Tool is single-threaded; the database thread is never spawned, so main-thread identity is the truthful answer. |
| `Sys_LockWrite/UnlockWrite`, `Sys_Enter/LeaveCriticalSection`, `Sys_WaitDatabaseThread`, `NET_Sleep`, remaining `Sys_*Database*` | uncontended single-thread implementations over the real `FastCriticalSection` fields / no-ops | Single-threaded execution is a *documented divergence* from the engine's worker/database threading. Consequence for evidence: event *order* is the deterministic single-thread walk order (which is also the order the engine's loader logic imposes on the byte stream — the walk itself is sequential even in the engine; only I/O staging overlaps). |
| `SL_GetString`, `SL_ConvertToString`, `SL_AddUser` | deterministic interning table: first-use order, handles 1,2,3…; emits `ev=sl_intern` | The SL subsystem is beyond scope. HANDLE VALUES are tool-defined and the trace must never be read as qualifying them; what IS engine-real is the remap *structure* (`Load_ScriptStringCustom` executes real code storing `strings[index]` into the slot). The trace records index→handle→string-hash triples so gates check structure, not values. Uses of the handle inside `Load_TempStringCustom` (db_stream_load.cpp:80, `SL_GetString(*str, 4u)`) run unmodified. |
| `I_stricmp/I_strncmp/I_strncat/I_strncpyz/I_strnicmp/I_stristr` | plain ASCII implementations | Case-insensitive compare/copy utilities; behavior-identical for the ASCII fixture names. |
| `Dvar_RegisterString` | returns a static zeroed dvar | Only reachable from `DB_TryLoadXFileInternal`/`DB_LoadZone_f` paths the driver does not call; exists to satisfy the Debug (`/OPT:NOREF`) link. |
| `track_static_alloc_internal`, `ProfLoad_Begin/End`, `Profile_*`, `KISAK_NULLSUB` | no-ops | Telemetry only. |
| Data globals `cm`, `comWorld`, `gameWorldMp`, `fs_gameDirVar`, `loc_warnings`, `loc_warningsAsErrors` | zero-initialized storage | Referenced by `DB_XAssetPool` (clipmap/comworld/gameworld singletons) and unreachable print paths. `cm` as the CLIPMAP pool target is real engine topology (`node1_` returns the pool pointer); the storage must merely exist. |
| Everything else (`R_*`, `RB_*`, `Material_*`, `FS_*`, `Cmd_*`, `CG_*`, `BG_*`, `CM_Unload`, `SND_SetData`, `DObj*`, `Com_GetServerDObj`, …) | **abort-loud**: print symbol, `exit 6` | Reached only if a fixture exercises a subsystem outside this wave's scope; a loud abort is the required behavior (never fake a tail). |

The complete scaffold is enumerated by a grep census of the nine TUs
(recorded in `oracle1_scaffold.cpp` comments); the Debug configuration
links with `/OPT:NOREF`, so CI proves the census complete — a missed
symbol is a red link step, not a silent gap.

### 2.4 Explicitly NOT in scope

- The database worker thread (`DB_Thread`, `Sys_SpawnDatabaseThread`) — the
  load runs synchronously on the main thread. Staged overlapped I/O still
  runs through the real `DB_ReadData`/`ReadFileEx`/`SleepEx` alertable-APC
  machinery, single-threaded.
- Geometry blocks 7/8 (`R_AllocStatic*Buffer`) — abort-loud; no fixture
  declares vertex/index block bytes.
- `IWff0100` (signed/secure) files — real `DB_AuthLoad_InflateInit` refuses
  via the real `iassert(!isSecure)` + `failureReason` path.
- Real retail zones in CI — allowlist-refused (exit 3) exactly like Oracle
  0; real-zone runs happen only on the owner's machine, locally.

## 3. Instrumentation — mechanism and byte-identity

### 3.1 Why some hooks must live inside engine sources

The observation points (`DB_PushStreamPos`, `DB_IncStreamPos`,
`Load_Stream`, `DB_ConvertOffsetToPointer`, …) are *leaf functions called
from inside other engine TUs*. MSVC's linker has no `--wrap`, so a
tool-owned wrapper can never interpose on engine-internal call edges.
Where a boundary IS tool-owned (the scaffold: `SL_GetString`,
`MyAssertHandler`, `Com_Error`, `PMem_Alloc`) the hook lives in the
scaffold, per the "prefer tool-owned wrappers" rule. The remaining hooks
are `#ifdef BMK4_ORACLE1` blocks inside six engine files.

### 3.2 The `#line` byte-identity discipline — precise claim

`BMK4_ORACLE1` is defined ONLY for the `bmk4-oracle1` target — never for
KisakCOD-sp/mp/dedi. Preprocessor-inert guards keep shipping *code*
identical, but inserting source lines would still shift `__LINE__` inside
downstream `iassert`/`vassert` expansions and change Debug-build string
literals. Every guarded insertion is therefore followed by a `#line N`
directive restoring the original numbering of the next source line:

```cpp
#ifdef BMK4_ORACLE1
    Bmk4Or1_StreamPush(index);          // tool hook, compiled out for shipping
#endif
#line 29                                 // next line is original line 29
```

`#line` participates in both branches, so `__LINE__` (and therefore every
assert string) is identical whether or not the guard is active. `__FILE__`
is untouched; the six TUs use no `__COUNTER__` and no PCH.

**Scope of the claim (Sol round-1 findings 6/7).** What is preserved is
the shipping preprocessor's TOKEN STREAM and logical source locations —
i.e. the generated code and every embedded string literal. It is NOT a
whole-PE/PDB byte-identity claim: `/Zi` debug info embeds source-file
checksums and PDB identity, so PDB bytes (and the PE debug directory)
legitimately change; shipping binaries were never bit-reproducible anyway
(`update_build_number` rewrites buildnumber.h each build and
buildnumber.cpp embeds `__DATE__`/`__TIME__`). The invariant the project
rule protects — shipping code and behavior unchanged — is exactly the
token-stream invariant.

**Mechanized, not manual (finding 7).** The discipline is enforced by
`tools/oracle1/check_line_discipline.py`, which reconstructs the shipping
view (guards + `#line` stripped) and verifies every `#line N` equals the
next shipping-view line number — no pre-edit baseline needed, so it runs
inside the CI gate on every build; a stale restoration after a future edit
is a red step, not a silent drift. At desk it additionally byte-compares
the shipping view against the pre-edit git baseline (`--baseline-rev`).

### 3.3 Hook inventory (all guarded, all one-liners into `bmk4_oracle1_instr.h`)

| File | Function | Event(s) |
|---|---|---|
| db_stream.cpp | `DB_PushStreamPos` | `stream_push` (requested index, new stack depth) |
| db_stream.cpp | `DB_PopStreamPos` | `stream_pop` (restored index) |
| db_stream.cpp | `DB_AllocStreamPos` | `alloc` (block, align, offset after alignment) |
| db_stream.cpp | `DB_IncStreamPos` | `inc` (block, offset before, size) |
| db_stream.cpp | `DB_InsertPointer` | `alias_insert` (block-4 slot offset) |
| db_stream_load.cpp | `Load_Stream` (atStreamStart && size) | `fill` (block, offset, size, source = file / zerofill / delay_queue) |
| db_stream_load.cpp | `DB_ConvertOffsetToPointer` | `ptr_offset` (raw token, decoded block, decoded offset) |
| db_stream_load.cpp | `DB_ConvertOffsetToAlias` | `ptr_alias` (raw token, decoded block, decoded offset) |
| db_stream_load.cpp | `Load_DelayStream` | no dedicated hook: drains surface as `inflate dest=block2/3+O` via the `DB_LoadXFileData` hook |
| db_file_load.cpp | `DB_LoadXFileData` | `inflate` (request size; dest resolved to block+offset or `external`) |
| db_file_load.cpp | `DB_LoadXFileInternal` (post-XFile read) | `xfile` (size, externalSize, 9 block sizes) |
| db_file_load.cpp | `Load_XAssetListCustom` (post-header read) | `assetlist` (string count + token, asset count + token) |
| db_load.cpp | `Load_XAsset` (post 8-byte read) | `asset_dispatch` (running index, type id, type name) |
| db_stringtable_load.cpp | `Load_ScriptStringCustom` | `scriptstring_remap` (local index before, handle after) |
| db_registry.cpp | `DB_AddXAsset` (post-link) | `asset_link` (type, FNV-1a64 of name incl. NUL, redirected flag) |

`-1` inline pointer tokens are not separately hooked: an inline token is
precisely "a pointer field followed by `alloc`+`fill` with no
`ptr_offset`/`ptr_alias`/`alias_insert` event", and the schema documents
that reading. `-2` is covered by `alias_insert`; every offset token is
covered by the two convert hooks.

All hook implementations live in `oracle1_trace.cpp`; hooks report
positions ONLY as (block index, offset from `blocks[i].data`) — never raw
pointers — which is what makes the trace ASLR-independent (§5).

## 4. Event schema `bmk4.oracle1.v1`

Line-oriented text, first line `schema=bmk4.oracle1.v1`, then one event per
line: `ev=<name> key=value key=value …` with a fixed key order per event
type, decimal offsets/sizes, `0x`-prefixed lowercase hex tokens, bare
16-digit lowercase hex FNV-1a64 values. Events:

```
zone_open        name=<zonename> bytes=<n>
container        magic=IWffu100 version=5
xfile            size=N external=N b0=N .. b8=N
assetlist        strings=N strings_token=0x… assets=N assets_token=0x…
inflate          size=N dest=blockB+O | dest=external
stream_push      index=B depth=D          (depth before push)
stream_pop       index=B depth=D          (restored index, depth after pop)
alloc            block=B align=A offset=O (offset after alignment)
inc              block=B offset=O size=N  (an ATTEMPT: recorded at entry,
                                           before the engine's block fence;
                                           committed unless followed by
                                           ev=error)
fill             block=B offset=O size=N src=file|zerofill|delay_queue
                                          (a REQUEST like inc: recorded at
                                           classification time; committed
                                           unless followed by ev=error)
ptr_offset       token=0x… block=B offset=O
ptr_alias        token=0x… block=B offset=O
alias_insert     block=4 offset=O
asset_dispatch   index=I type=T name=<typename>
sl_intern        handle=H hash=<fnv64 of bytes incl NUL> [text=<str>]
scriptstring_remap index=I handle=H
asset_link       type=T typename=<name> namehash=<fnv64 incl NUL> redirected=0|1 [name=<str>]
zone_loaded      name=<zonename>
error            kind=assert|com_error|scaffold detail=<escaped text>
```

Notes. (1) `-1` inline tokens have no dedicated event: an inline pointer
is an `alloc`(+`fill`/`inc`) with no `ptr_offset`/`ptr_alias`/
`alias_insert`. (2) Delayed drains appear as `inflate dest=block2/3+O`
events after the walk (issued by `Load_DelayStream` through the real
`DB_LoadXFileData`); queueing appears as `fill … src=delay_queue`.
(3) `asset_link` (named for what it IS — a post-`DB_LinkXAssetEntry`
observation at `DB_AddXAsset`, not proof of a fresh insertion; Sol round-2
finding 10) records an engine truth: a fresh insert CLONES the struct into
the per-type pool (db_registry.cpp DB_AllocXAssetEntry +
DB_CloneXAssetInternal) and `Load_*Asset` writes the POOL pointer back
over the asset-array cell — the linked header normally does not equal the
zone-block pointer, reported as `redirected=1`. The stub-existing /
override / delayed-clone branches also pass through this event, and the
`allowOverride=1` relink in `DB_PostLoadXZone` is outside the load walk
and unhooked; consumers must never read this event as an insertion
outcome. (4) `sl_intern` handles are tool-defined (first-use order,
1-based); the engine-real fact is the remap structure, never the handle
values. (5) `inflate`/`fill` destinations are resolved HALF-OPEN and
span-aware against declared block sizes: a request whose [start,
start+size) range does not fit inside [data, data+size) reports
`external` — the first out-of-budget byte of an over-model walk is
already external (Sol round-2 finding 4).

Escaping contract: `name=`, `text=`, `detail=` and the `zone_open`/
`zone_loaded` names are percent-encoded — every byte outside
`[0-9A-Za-z_./:-]` becomes `%XX` — so arbitrary engine bytes can neither
split records nor inject events (Sol round-1 finding 10). A value that
exceeds the writer's field buffer ends with the marker `%TR` (not valid
percent-encoding, hence unambiguous) instead of truncating silently (Sol
round-2 finding 13).

String hashing convention: FNV-1a64 over the UTF-8 bytes INCLUDING the
terminating NUL — the same `utf8_nul` convention as the fixture manifests
(`tools/zone_fixtures/README.md` §Manifest hashing), so gate checks are
direct equality against manifest fields.

Sanitization (Sol round-1 findings 8/9/12). The promise is "no plaintext
payload fields by default" — hashes are pseudonyms (correlatable against
known-name dictionaries), not confidentiality. Without `--emit-names`:
`text=`/`name=` fields are omitted; engine error text is reduced to its
engine-source LITERAL parts (assert file:line + unformatted fmt string /
Com_Error code + unformatted fmt) so `%s`-interpolated asset names never
reach the trace or stderr; and the `Com_Printf/PrintWarning/PrintError`
scaffold channel is fully suppressed. CI fixture runs pass `--emit-names`
(synthetic labels only, per the fixture charter). The `zone_open` name is
the caller-chosen `--zone-name` (CI: fixture ids).

## 5. Determinism argument (gate a)

Sources of nondeterminism examined:

- **Pointers/ASLR** — no raw pointer value is ever emitted; positions are
  block-relative offsets; `dest=external` replaces out-of-block pointers.
- **Threading** — single thread; APC completion via alertable `SleepEx`
  serializes I/O completion into program order.
- **Allocation** — `PMem_Alloc` scaffold offsets don't enter the trace;
  block offsets derive from `DB_InitStreams` + walk order only.
- **SL handles** — first-use sequential; walk order is deterministic.
- **Iteration order** — no hash-map iteration is traced; `db_hashTable`
  chains affect only lookup, and insertion outcomes are order-determined.
- **Time/size** — `g_trackLoadProgress=0` path skips `GetFileSize`
  accounting; no timestamps in the trace.
- **Buffers** — the driver's read ring and the zone blocks are
  VirtualAlloc'd, so process-fresh and OS-zeroed on every run; the
  `avail_in` over-credit at EOF (`DB_WaitXFileStage` credits a fixed
  0x40000 regardless of bytes transferred) therefore reads zeroes
  identically each run. CAVEAT (Sol round-1 finding 16): this argument is
  per-process-fresh runs of zones smaller than the 512 KiB ring; after a
  ring wrap the stale tail is earlier compressed data, not zeroes — still
  identical across two fresh processes reading the same file, but the
  zero-tail wording applies only to sub-ring zones like the fixtures.
  `SleepEx`'s return value is ignored by the engine; the tool queues no
  user APCs, so no premature wake source exists in-process.

- **Build-path dependence of refusal traces** (Sol round-2 finding 12) —
  `iassert` embeds `__FILE__`, and the CMake target registers the database
  sources by absolute path, so an assert's `detail=` bytes include the
  checkout path. Within one build (the CI double-run) they are identical;
  traces produced by DIFFERENT checkouts differ in exactly those bytes.
  Determinism claims are therefore scoped to same-binary runs; cross-build
  comparison would need path normalization (not a current gate).

CI enforces the gate mechanically: every fixture — including malformed
refusal twins — is loaded twice in separate processes; SHA-256 of the two
traces AND the two exit codes must match. stdout/stderr are diagnostics
and not part of the determinism comparison.

## 6. Qualification gates (gate b, gate c)

`tools/oracle1/check_trace.py <trace> --manifest <MANIFEST.json> --gate <n>`

**Gate b — fixture 01 vs the shipped iOS kernel model.** Asserts the trace
contains, in order (kernel claims ↔ engine events):

1. `alloc block=4 align=3 offset=0` + `fill block=4 offset=0 size=8
   src=file` — asset array in block 4 (K1 model: virtual block).
2. `asset_dispatch index=0 type=31` — RAWFILE.
3. `stream_push index=0` then `fill block=0 offset=0 size=12 src=file` —
   RawFile struct in block 0 (`Load_RawFilePtr` pushes 0,
   db_load.cpp:5664).
4. `stream_push index=4` then name bytes: `alloc block=4 align=0 offset=8`
   + `inc block=4 offset=8 size=25` — name interned in block 4 under the
   RawFile's own `DB_PushStreamPos(4)` (db_load.cpp:5646).
5. buffer truthiness: `alloc block=4 align=0 offset=33` + `fill block=4
   offset=33 size=6 src=file` — buffer loaded because the stored token was
   nonzero (db_load.cpp:5649–5653), `len+1` bytes.
6. `asset_link type=31 namehash=fc7845dd3a44c753 redirected=1` (manifest
   `rawfile[0].name` `utf8_nul` hash; pool-clone redirect per §4 note 3).
7. exit code 0 and `zone_loaded`.

**Gate c — fixture 02 StringTable block adjudication.** The dispute: the
fixture builder placed `stringtable[0]`, `.name`, `.values`, `.value[k]`
in block 0 (`MANIFEST.json` stream_events); static reading of
`Load_StringTablePtr` (db_load.cpp:5711–5728) shows NO
`DB_PushStreamPos` — allocations stay in the block active at dispatch
time, which is block 4 (pushed at db_file_load.cpp:281 before the asset
walk). The checker extracts the runtime events between
`asset_dispatch type=32` and the next dispatch/error and reports which
block received (i) the 16-byte StringTable struct alloc/fill and (ii) the
name bytes. The verdict is recorded in
`tools/oracle1/FIXTURE02_VERDICT.md`; the CI step fails if the trace lacks
the struct-placement events or contradicts the recorded verdict.

Desk-computed prediction (to be confirmed/refuted by the first CI run,
recorded either way; independently recomputed and confirmed by the Sol
pair, round-1 finding 13): block-4 cursor walk = script-string bytes
[0,31), asset array (aligned) [32,48), StringTable struct [48,64), name
alloc at 64; the name's 20-byte `inc` ATTEMPT (recorded at hook entry —
the cursor never commits) trips the engine's own fence `g_streamPos +
size <= block.data + block.size` (64+20 > 72 declared) → `ev=error
kind=assert` and exit 4. The name path goes through `Load_XStringCustom`,
so there is no 20-byte `fill` — the evidence events are `alloc block=4
offset=64`, per-byte `inflate` events, the attempted `inc … size=20`, and
the assert. That partial trace IS the adjudication: the engine put the
StringTable body in block 4 until its fixture-declared block-4 budget ran
out, and block 0 never received a StringTable byte (values, insertion,
and the XAnimParts dispatch are never reached). Fixture 02 as built
cannot complete a real-engine load; Lane A's regeneration must move the
StringTable body accounting to block 4.

Evidence label (Sol round-1 finding 5): Oracle 1 runtime claims are "real
loader walk under Oracle assert/scaffold policy" — `RELEASE_ASSERTS`
keeps fences observable that shipping Release compiles out, and the
PMem scaffold's guard slack means an over-model walk reports through the
engine fence instead of corrupting a heap that the real engine would have
sized `size+15`. Fixture 02's 12-byte overrun sits inside even the real
`+15`, so its verdict does not depend on the slack.

**Gate can fail (doctrine rule 5):** the CI step also runs
`01_rawfile_inline/malformed_truncated_buffer.ff` and
`02…/malformed_bad_script_count.ff` and asserts a NONZERO, non-3 exit
(engine-native refusal via truncation assert / `Load_Stream` stream-start
assert with `RELEASE_ASSERTS` forced on for this target), plus the exit-3
allowlist refusals with the FF0a `ErrorActionPreference` guard pattern.

## 7. Exit codes

| code | meaning |
|---|---|
| 0 | zone loaded; trace complete (`zone_loaded`) |
| 2 | usage / input-not-a-file / trace-unwritable (tool-level) |
| 3 | fixture allowlist refusal (input or output outside root) — matches Oracle 0 |
| 4 | engine refusal via `MyAssertHandler` (sanitized per §4 by default) |
| 5 | engine refusal via `Com_Error`/`Sys_Error` (sanitized per §4 by default) |
| 6 | abort-loud scaffold reached (out-of-scope subsystem; symbol on stderr) |

CI treats only {0, 4, 5} as engine-native outcomes for fixtures; 2/3/6 in
a fixture run is a red gate (closure gap or harness bug), never a
recordable "refusal" (Sol round-1 finding 19).

## 8. Build wiring

`tools/oracle1/CMakeLists.txt`, added beside `tools/ff_oracle` for
`KISAK_PLATFORM=win32`:

- target `bmk4-oracle1`: the nine database TUs + the three tool TUs.
- defines: `KISAK_MP;CINEMA;USE_SEPARATE_BLIT_TEXTURE;WIN32;_CONSOLE;_MBCS`
  (mp target parity) + `BMK4_ORACLE1` + `RELEASE_ASSERTS` (asserts stay
  observable in Release; tool-only, shipping flags untouched).
- includes: `SRC_DIR`, `DEPS_DIR`, `tools/oracle1`, DXSDK include (headers
  only — d3d9 types appear in engine headers; no d3d import lib is linked).
- links: `bmk4-ff-oracle-zlib` (same deps/zlib the engine embeds; provides
  `inflateInit_` for db_auth.cpp), kernel32/user32 defaults only.
- runtime: `MultiThreaded$<$<CONFIG:Debug>:Debug>` (engine parity).
- 32-bit only: the CI generator is `-A Win32`; the loader's pointer-token
  arithmetic (`(uint32_t)&block.data[…]`) requires ILP32. The CMake file
  hard-fails on a 64-bit platform.
- warning policy: the whole target builds at the default warning level
  without /WX — the tool TUs include engine headers (database.h, qcommon.h,
  gfx headers) that are not /W4-clean, so a split /WX policy would gate the
  oracle on engine-header cosmetics. (/permissive- is kept for engine
  parity.)

## 9. CI wiring (`build-kisarcod-win.yaml`, both configs)

1. `Build` step: add `--target "bmk4-oracle1"`.
2. New step `Oracle 1 engine loader gate` (id `oracle1_gate`, PowerShell,
   FF0a step style). The `ErrorActionPreference` relax wraps the WHOLE
   step (every oracle invocation can legitimately exit nonzero — Sol
   round-1 finding 18); `throw` still enforces every assertion; the step
   ends with the FF0a trailing `exit 0`.
   - `check_line_discipline.py` first (mechanized §3.2 gate);
   - content pinning: every fixture fed to the tool must match its
     SHA-256 in the reviewed `tools/zone_fixtures/SHA256SUMS` — path
     containment alone does not prove synthetic content (Sol round-1
     finding 11);
   - run fixtures 01–07 `valid.ff` twice each with
     `--fixture-allowlist-root $workspace --emit-names`; assert both runs
     byte-identical (SHA-256) with identical exit codes; assert every exit
     is engine-native {0,4,5}; record per-fixture outcomes in the step
     summary;
   - fixture 01 must exit 0; `check_trace.py --gate b` on its trace;
   - fixture 02: `check_trace.py --gate c` on its trace; verdict line into
     `$GITHUB_STEP_SUMMARY`;
   - malformed 01 + malformed 02: double-run determinism too, and must
     refuse with engine-native {4,5} (0 = accepted-malformed red; 3 =
     wrong-channel red);
   - allowlist refusal probes (input outside root; output outside root)
     expect exit 3 and no output file.
3. Upload `oracle1-traces-<config>` artifact (traces + verdict output;
   synthetic fixture data only — containment + content pinning).
4. Register the step in the staging `Verdict for coordinator` list.

## 10. Risks / expected CI surprises

- **Scaffold census misses** — Debug `/OPT:NOREF` guarantees discovery in
  the first link; budgeted for one fix round.
- **Signature drift** — scaffold includes engine headers instead of
  redeclaring, so drift is a compile error in the tool target only.
- **`#line` interactions with /MP or PCH** — the database TUs use no PCH;
  `/MP` is per-file and unaffected.
- **Engine headers under the tool target** — the nine TUs already compile
  under the mp target with the same defines/includes; the tool target
  reproduces those flags. Tracy is NOT defined for the tool
  (`TRACY_ENABLE` off ⇒ `Profile_*` are plain externs ⇒ scaffolded).
- **Fixture 02 prediction wrong** (e.g. the fence does not fire where
  desk-computed) — the gate records what the trace ACTUALLY says; the
  verdict document is written from the CI trace, not from the prediction.
