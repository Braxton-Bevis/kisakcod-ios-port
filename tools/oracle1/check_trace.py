#!/usr/bin/env python3
"""bmk4-oracle1 trace qualification gates (see tools/oracle1/DESIGN.md #6).

gate b: fixture 01 trace must agree with the shipped iOS kernel model.
gate c: fixture 02 trace adjudicates the StringTable block dispute.

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
        # real DB_LinkXAssetEntry insertion, manifest utf8_nul hash
        {"ev": "asset_insert", "type": "31", "namehash": expected_namehash, "outcome": "new"},
        {"ev": "zone_loaded"},
    ]
    failures = find_subsequence(events, wants)
    for failure in failures:
        print(f"RED gate-b: {failure}")
    if not failures:
        print("GREEN gate-b: fixture 01 runtime trace agrees with the shipped kernel model")
    return 1 if failures else 0


def gate_c(events: list[dict[str, str]]) -> int:
    """Fixture 02 StringTable block adjudication (DESIGN.md gate c)."""
    dispatch_index = None
    for i, event in enumerate(events):
        if event.get("ev") == "asset_dispatch" and event.get("type") == "32":
            dispatch_index = i
            break
    if dispatch_index is None:
        print("RED gate-c: no STRINGTABLE(32) dispatch event in trace")
        return 1

    struct_alloc = None
    struct_fill = None
    name_alloc = None
    error_event = None
    for event in events[dispatch_index + 1 :]:
        if event.get("ev") == "asset_dispatch":
            break  # next asset began
        if event.get("ev") == "alloc" and struct_alloc is None and event.get("align") == "3":
            struct_alloc = event
            continue
        if event.get("ev") == "fill" and struct_alloc and struct_fill is None and event.get("size") == "16":
            struct_fill = event
            continue
        if event.get("ev") == "alloc" and struct_fill and name_alloc is None:
            name_alloc = event
            continue
        if event.get("ev") == "error":
            error_event = event
            break

    if struct_alloc is None or struct_fill is None:
        print("RED gate-c: trace lacks the 16-byte StringTable struct alloc/fill events")
        return 1

    struct_block = struct_alloc.get("block")
    name_block = name_alloc.get("block") if name_alloc else "?"
    completed = any(e.get("ev") == "zone_loaded" for e in events)

    print(
        "FIXTURE02_VERDICT: "
        f"stringtable_struct_block={struct_block} "
        f"stringtable_struct_offset={struct_alloc.get('offset')} "
        f"name_block={name_block} "
        f"builder_claim_block=0 "
        f"engine_contradicts_builder={'true' if struct_block != '0' else 'false'} "
        f"load_completed={'true' if completed else 'false'} "
        f"engine_fence_tripped={'true' if error_event else 'false'}"
    )
    if error_event:
        print(f"FIXTURE02_ERROR_EVENT: {error_event}")

    if struct_block != "4":
        print(
            "RED gate-c: runtime placed the StringTable struct in block "
            f"{struct_block}, contradicting the static reading of "
            "Load_StringTablePtr (db_load.cpp:5711, no DB_PushStreamPos -> "
            "active block 4). Reconcile FIXTURE02_VERDICT.md before merging."
        )
        return 1
    print("GREEN gate-c: engine placed the StringTable body in ACTIVE block 4; builder block-0 claim refuted")
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
