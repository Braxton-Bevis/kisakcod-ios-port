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

*Verdict pending — codex exec run in flight; findings and rulings will be
appended verbatim-summarized below.*

## Round 2 — implementation (pre-CI)

*Pending round 1 closure.*

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
