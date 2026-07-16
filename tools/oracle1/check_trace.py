#!/usr/bin/env python3
"""bmk4-oracle1 trace qualification gates (see tools/oracle1/DESIGN.md #6).

gate b: fixture 01 trace must agree with the shipped iOS kernel model.
gate c: fixture 02 trace pins the K2-REGENERATED fixture's clean engine
        load against the predicted event spine (the original pin
        runtime-confirmed the corpus defect; see FIXTURE02_VERDICT.md and
        tools/zone_fixtures/ENGINE_TRACE_02.md).

Exit 0 = gate green; exit 1 = gate red (with reasons on stdout).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def parse_trace(path: Path) -> list[dict[str, str]]:
    events = []
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "schema=bmk4.oracle1.v1":
        raise SystemExit(f"RED: {path} does not start with schema=bmk4.oracle1.v1")
    for line in lines[1:]:
        if not line.startswith("ev="):
            continue
        fields: dict[str, str] = {}
        for token in line.split(" "):
            if "=" in token:
                key, _, value = token.partition("=")
                fields.setdefault(key, value)
        events.append(fields)
    return events


def match(event: dict[str, str], want: dict[str, str]) -> bool:
    return all(event.get(k) == v for k, v in want.items())


def find_subsequence(events: list[dict[str, str]], wants: list[dict[str, str]]) -> list[str]:
    """Returns list of failures (empty = all matched in order)."""
    failures = []
    position = 0
    for want in wants:
        for index in range(position, len(events)):
            if match(events[index], want):
                position = index + 1
                break
        else:
            failures.append(f"missing (in order): {want}")
    return failures


def gate_b(events: list[dict[str, str]], manifest: dict) -> int:
    """Fixture 01 vs the shipped iOS kernel model (DESIGN.md gate b)."""
    expected_namehash = None
    for field in manifest.get("oracle_runtime", {}).get("targeted_fields", []):
        if field.get("path") == "rawfile[0].name":
            expected_namehash = field.get("fnv1a64")
    if expected_namehash is None:
        print("RED: manifest lacks rawfile[0].name fnv1a64")
        return 1

    wants = [
        # asset array in block 4 (K1 model: virtual block)
        {"ev": "alloc", "block": "4", "align": "3", "offset": "0"},
        {"ev": "fill", "block": "4", "offset": "0", "size": "8", "src": "file"},
        # RAWFILE dispatch
        {"ev": "asset_dispatch", "index": "0", "type": "31"},
        # Load_RawFilePtr pushes block 0; struct lands there (db_load.cpp:5664)
        {"ev": "stream_push", "index": "0"},
        {"ev": "fill", "block": "0", "offset": "0", "size": "12", "src": "file"},
        # Load_RawFile pushes block 4 for name+buffer (db_load.cpp:5646)
        {"ev": "stream_push", "index": "4"},
        {"ev": "alloc", "block": "4", "align": "0", "offset": "8"},
        {"ev": "inc", "block": "4", "offset": "8", "size": "25"},
        # buffer truthiness: nonzero token -> len+1 inline bytes (db_load.cpp:5649)
        {"ev": "alloc", "block": "4", "align": "0", "offset": "33"},
        {"ev": "fill", "block": "4", "offset": "33", "size": "6", "src": "file"},
        # real DB_LinkXAssetEntry post-link observation, manifest utf8_nul
        # hash; a fresh insert is pool-cloned so the linked header is
        # redirected off-block (db_registry.cpp DB_AllocXAssetEntry +
        # DB_CloneXAssetInternal) — an engine truth the kernel must model
        {"ev": "asset_link", "type": "31", "namehash": expected_namehash, "redirected": "1"},
        {"ev": "zone_loaded"},
    ]
    failures = find_subsequence(events, wants)
    for failure in failures:
        print(f"RED gate-b: {failure}")
    if not failures:
        print("GREEN gate-b: fixture 01 runtime trace agrees with the shipped kernel model")
    return 1 if failures else 0


def gate_c(events: list[dict[str, str]]) -> int:
    """Fixture 02 StringTable block adjudication (DESIGN.md gate c).

    HISTORY. The ORIGINAL fixture 02 declared block-4 size 72 while the
    engine places the whole StringTable body in ACTIVE block 4
    (Load_StringTablePtr, db_load.cpp:5711-5728: no DB_PushStreamPos), so
    the engine's own DB_IncStreamPos fence refused the zone at the name
    fill — runtime-CONFIRMED by this gate's first pinning (exit 4, see
    FIXTURE02_VERDICT.md). That verdict adjudicated the corpus defect the
    K0/K1 review flagged; two independent methods (Lane A static trace,
    tools/zone_fixtures/ENGINE_TRACE_02.md; this tool's runtime trace)
    reached the same block-4 answer.

    CURRENT PIN. The K2 wave REGENERATED fixture 02 from the engine truth:
    block table [88,0,0,0,126,0,0,0,0], StringTable struct at block4+48,
    name at block4+64, values at block4+84, XAnimParts struct in temp
    block 0, its name at block4+102, u16 names array 2-aligned at
    block4+124. This gate now requires the real engine to LOAD THE
    REGENERATED FIXTURE CLEAN and to place every predicted event exactly
    where the static trace said — the runtime trace remains the arbiter:
    any divergence is a red gate and a corpus/kernel reconciliation, never
    a silent expectation edit.

    Hardening carried over from the original pin (Sol round-2 findings
    6/7/8): the struct allocation is identified by ADJACENCY (first alloc
    after the STRINGTABLE dispatch, its 16-byte file fill immediately
    next), the STRINGTABLE segment must contain NO stream_push (the
    engine pushes no block on this path — a decoy push cannot satisfy the
    gate), and the whole trace must contain NO error event.
    """
    failures: list[str] = []

    # The engine must complete the load: no error events, zone_loaded set.
    if any(e.get("ev") == "error" for e in events):
        first = next(e for e in events if e.get("ev") == "error")
        failures.append(f"error event present in a clean-load pin: {first}")
    completed = any(e.get("ev") == "zone_loaded" for e in events)
    if not completed:
        failures.append("zone_loaded absent: the regenerated fixture must complete")

    # Ordered spine of the predicted walk (ENGINE_TRACE_02.md event table).
    wants = [
        {"ev": "xfile", "size": "228", "external": "0", "b0": "88", "b1": "0",
         "b2": "0", "b3": "0", "b4": "126", "b5": "0", "b6": "0", "b7": "0",
         "b8": "0"},
        {"ev": "assetlist", "strings": "2", "strings_token": "0xffffffff",
         "assets": "2", "assets_token": "0xffffffff"},
        # asset array: cursor 31 aligned to 32, 2 entries = 16 file bytes
        {"ev": "alloc", "block": "4", "align": "3", "offset": "32"},
        {"ev": "fill", "block": "4", "offset": "32", "size": "16", "src": "file"},
        {"ev": "asset_dispatch", "index": "0", "type": "32"},
        # StringTable struct + name + values, ALL in active block 4
        {"ev": "alloc", "block": "4", "align": "3", "offset": "48"},
        {"ev": "fill", "block": "4", "offset": "48", "size": "16", "src": "file"},
        {"ev": "alloc", "block": "4", "align": "0", "offset": "64"},
        {"ev": "inc", "block": "4", "offset": "64", "size": "20"},
        {"ev": "alloc", "block": "4", "align": "3", "offset": "84"},
        {"ev": "fill", "block": "4", "offset": "84", "size": "8", "src": "file"},
        {"ev": "asset_link", "type": "32", "redirected": "1"},
        {"ev": "asset_dispatch", "index": "1", "type": "2"},
        # XAnimParts: temp block 0 struct, block 4 internals
        {"ev": "stream_push", "index": "0"},
        {"ev": "alloc", "block": "0", "align": "3", "offset": "0"},
        {"ev": "fill", "block": "0", "offset": "0", "size": "88", "src": "file"},
        {"ev": "stream_push", "index": "4"},
        {"ev": "alloc", "block": "4", "align": "0", "offset": "102"},
        {"ev": "inc", "block": "4", "offset": "102", "size": "21"},
        {"ev": "alloc", "block": "4", "align": "1", "offset": "124"},
        {"ev": "fill", "block": "4", "offset": "124", "size": "2", "src": "file"},
        # engine u16 remap structure: wire index 1 resolves through the
        # interned list (handle VALUES are tool-defined and never pinned)
        {"ev": "scriptstring_remap", "index": "1"},
        {"ev": "asset_link", "type": "2", "redirected": "1"},
        {"ev": "zone_loaded"},
    ]
    failures.extend(find_subsequence(events, wants))

    # Adjacency hardening on the STRINGTABLE segment.
    dispatch_index = None
    for i, event in enumerate(events):
        if event.get("ev") == "asset_dispatch" and event.get("type") == "32":
            dispatch_index = i
            break
    if dispatch_index is None:
        failures.append("no STRINGTABLE(32) dispatch event in trace")
        segment = []
    else:
        segment = []
        for event in events[dispatch_index + 1 :]:
            if event.get("ev") == "asset_dispatch":
                break
            segment.append(event)

    struct_block = name_block = None
    if segment:
        if any(e.get("ev") == "stream_push" for e in segment):
            failures.append(
                "stream_push inside the STRINGTABLE segment: "
                "Load_StringTablePtr must push no block (db_load.cpp:5711)"
            )
        struct_i = next(
            (i for i, e in enumerate(segment) if e.get("ev") == "alloc"), None
        )
        if struct_i is None:
            failures.append("no allocation after the STRINGTABLE dispatch")
        else:
            struct_alloc = segment[struct_i]
            struct_block = struct_alloc.get("block")
            struct_fill = segment[struct_i + 1] if struct_i + 1 < len(segment) else {}
            if not (
                struct_fill.get("ev") == "fill"
                and struct_fill.get("block") == struct_alloc.get("block")
                and struct_fill.get("offset") == struct_alloc.get("offset")
                and struct_fill.get("size") == "16"
                and struct_fill.get("src") == "file"
            ):
                failures.append(
                    "struct alloc not adjacently followed by its 16-byte "
                    f"file fill: {struct_fill}"
                )
            name_i = next(
                (
                    i
                    for i, e in enumerate(segment)
                    if i > struct_i + 1 and e.get("ev") == "alloc"
                ),
                None,
            )
            name_alloc = segment[name_i] if name_i is not None else {}
            name_block = name_alloc.get("block")

    print(
        "FIXTURE02_VERDICT: regenerated=true "
        f"stringtable_struct_block={struct_block} "
        f"name_block={name_block} "
        f"load_completed={'true' if completed else 'false'} "
        f"engine_fence_tripped={'true' if any(e.get('ev') == 'error' for e in events) else 'false'} "
        f"builder_engine_agree={'true' if not failures else 'false'}"
    )

    for failure in failures:
        print(
            f"RED gate-c: {failure}. The runtime trace is the arbiter: "
            "reconcile the corpus/kernel (ENGINE_TRACE_02.md + "
            "FIXTURE02_VERDICT.md) before merging — never edit this "
            "expectation to match a divergent trace silently."
        )
    if failures:
        return 1
    print(
        "GREEN gate-c: the real engine loaded the REGENERATED fixture 02 "
        "clean and placed every StringTable/XAnimParts event exactly where "
        "the K2 static trace predicted (block table [88,0,0,0,126,0,0,0,0])"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--gate", choices=["b", "c"], required=True)
    args = parser.parse_args()

    events = parse_trace(args.trace)
    if args.gate == "b":
        if not args.manifest:
            print("RED: gate b requires --manifest")
            return 1
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        return gate_b(events, manifest)
    return gate_c(events)


if __name__ == "__main__":
    sys.exit(main())
