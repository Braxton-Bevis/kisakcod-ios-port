# FF kernel desk-check harness (K2)

A 1:1 Python mirror of `src/ios/ff_kernel.cpp`'s walk logic, run against the
REAL fixture bytes in `tools/zone_fixtures/` BEFORE any CI dispatch. This is
the Lane A K2 wave's pre-dispatch gate: if the mirror and the C kernel ever
disagree on a fixture, one of them is wrong about the engine.

```text
python tools/ffk_deskcheck/desk_check_k2.py .
```

(The argument is the repo root; defaults to the current directory.)

What it mirrors, with the same refusal names as `FFK_RefusalName`:

- The header-first exact-size container reader's OUTCOME semantics
  (frontier ruling P0): declared `xfile.size + 44` must equal the
  decompressed length (`payload_size_mismatch`), bounded by the 256 MiB
  policy (`payload_limit`) and the 60-byte minimum (`payload_short`).
- The `FFKStream` context: one physical cursor + nine aligned logical block
  cursors, push/pop with the block-0 POP REWIND (`db_stream.cpp:67-74`),
  and HIGH-WATER accounting against the XFile block table.
- The typed dispatch walk (RAWFILE=31, STRINGTABLE=32, XANIMPARTS=2),
  script-string interning, and the engine's u16 index remap
  (`db_stringtable_load.cpp:3-6`) with the kernel-added bounds check.
- The frozen `FFK_WalkRawFileZone` wrapper (K1 fences + RAWFILE-only mask).

Checks executed (all must print and end with `desk check: ALL GREEN`):

1. fixture 01 valid: full K1 round trip + block accounting.
2. fixture 01 malformed twin: container accepts (truthful declared size),
   walk refuses `stream_truncation`.
3. fixture 02 valid: script strings, StringTable, u16 remap, high-water
   blocks `[88,0,0,0,126,0,0,0,0]` vs the manifest.
4. fixture 02 malformed twin: `bad_count` before any allocation.
5. fixture 02 vs the K1 wrapper: `unsupported_asset_count` (scope).
6. Declared-size mismatch in both directions: `payload_size_mismatch`.
7. A zeros-only zone declaring mp_killhouse's exact decompressed size
   (76,935,387 bytes; docs/REAL_ZONE_EVIDENCE.md): container ACCEPTS,
   empty asset list refused `unsupported_asset_count`.
8. Beyond-policy declaration (>256 MiB): `payload_limit`.
9. One-asset StringTable vs the K1 wrapper: `unsupported_asset_type`
   (exercises the dispatch mask).
10. 2^31 x 2^31 StringTable cells: `stream_truncation` with no crash
    (the u64 multiply-wrap regression, Sol K2 round 2 challenge 1).

Zero game data: every input is a checked-in synthetic fixture or built
in-memory from constants.
