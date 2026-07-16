# Oracle 1 (Lane C) — Sol adversarial review record

Protocol: the Sol pair (ChatGPT 5.6, ULTRA reasoning, read-only sandbox)
attacks (a) tools/oracle1/DESIGN.md before code and (b) the implementation
before CI. Challenges and rulings are recorded here; the final report
summarizes them.

## Round 1 — DESIGN.md (pre-code)

Brief: attack closure feasibility (nine-TU link plan + scaffold census),
byte-identity risk (#ifdef BMK4_ORACLE1 + #line restoration), sanitization
holes (CI trace/artifact leakage), the fixture-02 desk prediction (block-4
walk recomputation), and determinism.

**Sol STANDING: NEEDS-FIX — 19 findings.** Rulings and actions (Lane C):

1. `useFastFile` closure (CONFIRMED-DEFECT) — **ACCEPTED; was already
   fixed in the implementation** before the verdict landed (Lane C hit the
   same inline during scaffold construction): scaffold defines a backing
   dvar with `current.enabled = true`.
2. Census incomplete/unclean (CONFIRMED-DEFECT) — **FIXED.** Added
   `com_missingAssetOpenFailed`, `com_sv_running`,
   `fs_numServerReferencedFFs`, `fs_serverReferencedFFNames[32]`; removed
   the `KISAK_NULLSUB` definition (inline in qcommon.h:1557 — defining it
   again is an ODR break); DESIGN table corrected: `DB_AllocMaterial`/
   `DB_FreeMaterial` are db_registry-internal, `MyAssertHandler` rows
   added for db_stream/db_auth. Census remains CI-proven by the Debug
   `/OPT:NOREF` link.
3. Nine-TU plan linkable (OK-AS-DESIGNED) — noted.
4. `g_fileBuf` unbuffered-I/O alignment (RISK) — **FIXED**: driver passes
   a VirtualAlloc'd 0x80000 ring (sector-aligned, OS-zeroed), documented
   divergence; the engine contract (ring size, 4-byte check) is kept.
5. Refusal semantics scaffold-assisted (RISK) — **ACCEPTED**: all runtime
   claims are now labeled "real loader walk under Oracle assert/scaffold
   policy" (DESIGN §6, verdict doc); fixture-02's 12-byte overrun sits
   inside even the engine's own `size+15`, so the verdict is
   slack-independent.
6. `#line` is not whole-PE/PDB identity (CONFIRMED-DEFECT) — **ACCEPTED;
   claim narrowed** to preprocessor token-stream + logical-location
   identity (DESIGN §3.2), explicitly excluding PDB/debug-directory bytes
   and noting the pre-existing buildnumber/`__DATE__` nondeterminism.
7. Manual `#line` fragility (RISK) — **FIXED with a mechanized gate**:
   `tools/oracle1/check_line_discipline.py` (baseline-free structural
   check) runs first inside the CI oracle1 gate; desk mode additionally
   byte-compares the shipping view against the pre-edit git baseline.
8. `error detail=` leaks names (CONFIRMED-DEFECT) — **FIXED**: without
   `--emit-names` the assert/Com_Error channel carries only
   engine-source literals (file:line, unformatted fmt, error code) in
   both trace and stderr.
9. `Com_Print*` unsanitized channel (CONFIRMED-DEFECT) — **FIXED**:
   suppressed entirely unless `--emit-names`.
10. Schema escaping (CONFIRMED-DEFECT) — **FIXED**: normative
    percent-encoding for all free-text fields (bytes outside
    `[0-9A-Za-z_./:-]` → `%XX`).
11. Containment is not a content allowlist (CONFIRMED-DEFECT) —
    **FIXED**: the CI gate pins every fixture it feeds to the tool
    against the reviewed `tools/zone_fixtures/SHA256SUMS` before any run.
12. FNV hashes are pseudonyms (RISK) — **ACCEPTED**: promise re-worded to
    "no plaintext payload fields by default" (DESIGN §4).
13. Fixture-02 walk (OK-AS-DESIGNED) — Sol independently recomputed the
    block-4 cursor and confirmed the desk prediction, refining it: the
    final `inc` is an attempt, the committed cursor stays 64, and values/
    insertion/XAnimParts dispatch are never reached. Folded into DESIGN
    §6 and FIXTURE02_VERDICT.md.
14. `[0,31)` wording NIT — **FIXED**.
15. Gate C attempt-vs-commit (RISK) — **ACCEPTED/ALIGNED**: the
    `DB_IncStreamPos` hook fires at entry, so `ev=inc` is documented as
    an attempt (committed unless followed by `ev=error`); gate c keys on
    the struct alloc/fill, the name alloc, and the assert — not on a
    20-byte fill (none exists on the `Load_XStringCustom` path) nor a
    committed cursor.
16. Determinism overstated (RISK) — **ACCEPTED**: DESIGN §5 now scopes
    the zero-tail argument to sub-ring, process-fresh runs and records
    the ring-wrap caveat and the ignored `SleepEx` return (no in-process
    user APC source).
17. Hook input discipline / outcome tri-state (RISK) — **ACCEPTED**:
    hooks read only explicitly-initialized fields; the unknowable
    `new|existing|override` was replaced by the observable
    `redirected=0|1` (pool-clone redirect — itself an engine truth the
    kernel must model; Lane C had independently hit this while
    desk-walking DB_LinkXAssetEntry).
18. EAP wrapper scope (CONFIRMED-DEFECT vs the DESIGN text) — **FIXED in
    DESIGN**; the implemented step already relaxed EAP for the whole
    step. Every oracle invocation is wrapped; trailing `exit 0` kept.
19. CI closure not proven for 03–07 (CONFIRMED-DEFECT) — **FIXED**: every
    fixture outcome must be engine-native {0,4,5} (scaffold 6 / tool 2/3
    are red); malformed twins are double-run for determinism; stdout/
    stderr are documented as outside the determinism comparison.

## Round 2 — implementation (pre-CI)

Brief: attack (1) trace truthfulness, (2) determinism, (3) whether the
fixture-02 verdict follows from what gate c actually checks, (4) MSVC x86
compile/link closure, (5) CI PowerShell correctness.

**Sol STANDING: NEEDS-FIX — 14 findings (2 CI-killers) + 3 OK.** Rulings
and actions (Lane C):

1. `oracle1_trace.h` missing `<cstddef>` for `std::size_t`
   (CONFIRMED-DEFECT, compile blocker) — **FIXED**.
2. `Material_ReleaseTechniqueSet` + `Image_Free` unresolved
   (CONFIRMED-DEFECT, link blocker) — **FIXED** (abort-loud scaffolds;
   declared xanim.h:1476 / r_image.h:130). These are ADDRESS-TAKEN in the
   `DB_RemoveXAssetHandler` table (db_registry.cpp:2884-2885), the exact
   class a call-site grep census cannot see; Lane C swept all nine TUs for
   `&identifier` references and found no further misses
   (`R_IsInRemoteScreenUpdate` was already scaffolded; the rest are
   TU-internal or data globals already defined).
3. `fill` emitted pre-action without attempt semantics (CONFIRMED-DEFECT)
   — **FIXED by contract**: `fill` is documented as a REQUEST like `inc`
   (committed unless followed by `ev=error`), in DESIGN §4 and at the
   hook. Rationale for not moving the hook: the file branch's
   `DB_LoadXFileData` never returns on refusal, so a post-action hook
   would silently DROP the very request the refusal interrupted.
4. End-inclusive, span-blind block containment (CONFIRMED-DEFECT) —
   **FIXED**: `ResolveBlock` is now half-open and span-aware; the first
   out-of-budget byte of an over-model walk reports `external`.
5. SL interning dangling `c_str()` on vector growth (CONFIRMED-DEFECT) —
   **FIXED**: `std::deque` (stable element addresses for process
   lifetime; DB_CreateDefaultEntry stores the pointer permanently).
6. Gate C first-match latching could pass a block-0 StringTable / fail a
   correct trace (CONFIRMED-DEFECT) — **FIXED**: gate c rewritten to
   ADJACENCY matching in real emission order (struct alloc must be the
   FIRST post-dispatch alloc, align=3, immediately followed by its
   block/offset-matched 16-byte `src=file` fill).
7. Gate C weaker than the accepted round-1 ruling (CONFIRMED-DEFECT) —
   **FIXED**: gate c now REQUIRES the align-0 name alloc, the fence
   adjacency, name_block==struct_block==4, and NO `zone_loaded`; the CI
   step additionally pins fixture 02's exit to exactly 4.
8. `engine_fence_tripped` could lie on any error (CONFIRMED-DEFECT) —
   **FIXED**: fence is true only for `inc size=20` at the name position
   immediately followed by `kind=assert`.
9. Verdict doc claimed RUNTIME tier while marked PENDING
   (CONFIRMED-DEFECT) — **FIXED**: tier language now tracks the evidence
   actually recorded; no forward-dated claims.
10. `asset_insert` name overstates post-link observation (RISK) —
    **ACCEPTED**: event renamed `asset_link`; unhooked
    `allowOverride=1` relink and the pass-through branches documented;
    `redirected` documented as pointer-inequality observation only.
11. Relaxed EAP could null-hash missing traces into a silent pass (RISK)
    — **FIXED**: `Get-TraceHash` helper (Test-Path + `-ErrorAction
    Stop`) for all four hash sites; pinning hash also `-ErrorAction
    Stop`.
12. Refusal traces embed `__FILE__` build paths (RISK) — **ACCEPTED &
    DOCUMENTED** (DESIGN §5): determinism claims are scoped to
    same-binary runs.
13. EscapeField silent truncation (RISK) — **FIXED**: unambiguous `%TR`
    truncation marker (invalid as percent-encoding by construction).
14. /W4 /WX design-vs-CMake mismatch (NIT) — already resolved at
    `dbe08f1` (DESIGN now documents the no-/WX policy); Sol reviewed a
    mid-edit state.
15-17. OK findings (hook placement + pinned-walk determinism; build
    wiring incl. driver externs matching db_registry exactly; PS 5.1
    constructs) — noted; finding 15 explicitly confirms `alloc`/`inc`
    placement and source classification are truthful and that no
    pointer/clock/thread/padding value reaches trace bytes on the pinned
    walks.

## CI evidence (round 1 of 4 — GREEN, no further rounds needed)

Run **29522138080** (`workflow_dispatch`, head `f7a105e`): every step
GREEN in both configs — Configure, Build (nine engine TUs + bmk4-oracle1
compiled and linked first try; the census plus Sol's two round-2 link
blockers held), FF0a gate, **Oracle 1 engine loader gate**, layout gate,
uploads. From the `oracle1-traces-{Debug,Release}` artifacts:

- Determinism: all 9 traces (7 valid + 2 malformed) byte-identical across
  double runs AND across Debug/Release.
- Fixture 01: exit 0, gate b GREEN — runtime events match the shipped
  kernel model exactly, including `asset_link redirected=1` (the
  DB_LinkXAssetEntry pool-clone redirect is now RUNTIME-tier fact).
- Fixture 02: exit 4, gate c GREEN — StringTable body in ACTIVE block 4;
  runtime section of tools/oracle1/FIXTURE02_VERDICT.md filled from these
  artifacts (Lane A's parallel work not consulted — independence kept).
- Fixture 03 (alias_forward): exit 0 — `-2` alias insert, back-patch and
  `ptr_alias` resolution engine-qualified.
- **Fixture 04 (cross_block_offset): exit 4 — NEW runtime corpus
  discovery.** Its second RawFile's name offset-token (`0x40000011` →
  block4+16) resolves to the SAME string as asset[0]'s name
  ("cross_block_name"), and DB_LinkXAssetEntry refuses a same-name
  same-type entry within one zone at the db_registry.cpp:1900 assert
  (`existingEntry->entry.zoneIndex != newEntry->entry.zoneIndex`).
  Manifest says `expectation: accept` — the fixture needs a name token
  aliasing DIFFERENT bytes (fixture 05's interior-offset pattern, which
  loads clean, shows the correct construction). Assert-tier note: retail
  Release (asserts off) would fall into the override path instead — the
  assert documents engine intent; oracle policy refuses loudly.
- Fixture 05 (interior_offset): exit 0 — interior-offset name resolution
  engine-qualified.
- Fixture 06 (delayed_stream_order): exit 0 — CLIPMAP_PVS walk with the
  real block-1 zero-fill (`fill block=1 offset=0 size=32 src=zerofill`).
  No block-2/3 events, consistent with the corpus README: no stock
  walker pushes blocks 2/3; the manifest's delayed_read_plan is for
  kernel replay, not engine observation.
- Fixture 07 (tagged_union_arm): exit 0 — numframes<256 byte-arm
  consumption engine-qualified. Note: no green fixture currently
  exercises `scriptstring_remap` (07's XAnimParts declares no names; 02
  fences before XAnimParts) — the remap hook becomes observable once
  Lane A's regenerated fixture 02 lands.
- Malformed 01 exit 4 (zlib-short `err == Z_OK` assert,
  db_file_load.cpp:402) and malformed 02 exit 4 (Load_Stream
  stream-start assert after the 4*count wrap) — both deterministic,
  engine-native, distinct from allowlist exit 3; allowlist input/output
  probes exited 3 with no output file created. Gates demonstrably fail
  (doctrine rule 5) at tool, checker, and CI tiers.

## Lane C desk evidence gathered independently of Sol

- `#line` discipline mechanically verified: a script strips every
  `#ifdef BMK4_ORACLE1` block + `#line` directive from the six edited
  engine files and byte-compares against `git show HEAD:` — all six equal,
  and every `#line N` value equals the original number of the next line
  (scratchpad `check_line_discipline.py`, run green 2026-07-16).
- FNV-1a64 `utf8_nul` convention validated against the fixture manifests:
  `synthetic/raw_inline.txt\0` = `fc7845dd3a44c753` (manifest
  rawfile[0].name), `script_zero\0script_one\0` = `dbdbd34c08d4b111`
  (fixture-02 script_strings content hash).
- check_trace.py gate b and gate c dry-run GREEN on desk-simulated traces
  derived by hand-walking the engine code, and demonstrably RED on
  mutated traces (struct block flipped to 0; asset_insert removed) —
  doctrine rule 5 (gates must be able to fail) holds at the checker level.
- Malformed-fixture refusal paths desk-traced: malformed 01 refuses via
  the `err == Z_OK` assert in DB_LoadXFileData (db_file_load.cpp:402);
  malformed 02 refuses via the Load_Stream stream-start iassert
  (db_stream_load.cpp:6) after the 4*count size wrap — both exit 4,
  engine-native, distinct from allowlist exit 3.
