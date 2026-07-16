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

    Pinned to fixture 02 AS BUILT (its bytes are SHA-pinned by the CI gate),
    per the Sol round-2 hardening (findings 6/7/8): every required event is
    identified by ADJACENCY in the real emission order, not by first-match
    scanning, so an interleaved decoy cannot satisfy the gate and an
    unrelated event cannot fail it:

        asset_dispatch type=32
        alloc  align=3 (block B, offset O)      <- struct alloc: FIRST alloc
        fill   block=B offset=O size=16 src=file   (immediately next event)
        ... inflate/inc for the struct ...
        alloc  align=0 (block B2, offset O2)    <- name alloc: next alloc
        ... per-byte inflate events ...
        inc    block=B2 offset=O2 size=20       <- fence attempt
        error  kind=assert                         (immediately next event)

    GREEN requires: B == B2 == 4, the fence-adjacency above, and NO
    zone_loaded anywhere (fixture 02 as built cannot complete a real-engine
    load). Anything else is RED and forces reconciliation of
    FIXTURE02_VERDICT.md.
    """
    dispatch_index = None
    for i, event in enumerate(events):
        if event.get("ev") == "asset_dispatch" and event.get("type") == "32":
            dispatch_index = i
            break
    if dispatch_index is None:
        print("RED gate-c: no STRINGTABLE(32) dispatch event in trace")
        return 1

    segment = []
    for event in events[dispatch_index + 1 :]:
        if event.get("ev") == "asset_dispatch":
            break  # next asset began
        segment.append(event)

    # Struct alloc = FIRST alloc after dispatch; its fill must be adjacent.
    struct_i = next((i for i, e in enumerate(segment) if e.get("ev") == "alloc"), None)
    if struct_i is None:
        print("RED gate-c: no allocation after the STRINGTABLE dispatch")
        return 1
    struct_alloc = segment[struct_i]
    if struct_alloc.get("align") != "3":
        print(f"RED gate-c: first post-dispatch alloc has align={struct_alloc.get('align')}, expected 3")
        return 1
    struct_fill = segment[struct_i + 1] if struct_i + 1 < len(segment) else {}
    if not (
        struct_fill.get("ev") == "fill"
        and struct_fill.get("block") == struct_alloc.get("block")
        and struct_fill.get("offset") == struct_alloc.get("offset")
        and struct_fill.get("size") == "16"
        and struct_fill.get("src") == "file"
    ):
        print(f"RED gate-c: struct alloc not adjacently followed by its 16-byte file fill: {struct_fill}")
        return 1

    # Name alloc = next alloc after the struct fill.
    name_i = next(
        (i for i, e in enumerate(segment) if i > struct_i + 1 and e.get("ev") == "alloc"),
        None,
    )
    name_alloc = segment[name_i] if name_i is not None else None
    if name_alloc is None or name_alloc.get("align") != "0":
        print(f"RED gate-c: no align-0 name allocation after the struct: {name_alloc}")
        return 1

    # Fence: the inc at the name position, immediately followed by an assert.
    fence = False
    fence_inc = None
    for i in range(name_i + 1, len(segment) - 1):
        e = segment[i]
        if (
            e.get("ev") == "inc"
            and e.get("block") == name_alloc.get("block")
            and e.get("offset") == name_alloc.get("offset")
            and e.get("size") == "20"
        ):
            fence_inc = e
            fence = segment[i + 1].get("ev") == "error" and segment[i + 1].get("kind") == "assert"
            break

    struct_block = struct_alloc.get("block")
    name_block = name_alloc.get("block")
    completed = any(e.get("ev") == "zone_loaded" for e in events)

    print(
        "FIXTURE02_VERDICT: "
        f"stringtable_struct_block={struct_block} "
        f"stringtable_struct_offset={struct_alloc.get('offset')} "
        f"name_block={name_block} "
        f"name_offset={name_alloc.get('offset')} "
        f"builder_claim_block=0 "
        f"engine_contradicts_builder={'true' if struct_block != '0' else 'false'} "
        f"load_completed={'true' if completed else 'false'} "
        f"engine_fence_tripped={'true' if fence else 'false'}"
    )
    if fence_inc:
        print(f"FIXTURE02_FENCE_INC: {fence_inc}")

    failures = []
    if struct_block != "4":
        failures.append(
            f"struct landed in block {struct_block}, contradicting the static reading of "
            "Load_StringTablePtr (db_load.cpp:5711, no DB_PushStreamPos -> active block 4)"
        )
    if name_block != "4":
        failures.append(f"name landed in block {name_block}, expected active block 4")
    if not fence:
        failures.append(
            "the DB_IncStreamPos fence adjacency (inc size=20 at the name position "
            "immediately followed by kind=assert) is absent"
        )
    if completed:
        failures.append("zone_loaded present: fixture 02 as built must NOT complete a real-engine load")
    for failure in failures:
        print(f"RED gate-c: {failure}. Reconcile FIXTURE02_VERDICT.md before merging.")
    if failures:
        return 1
    print("GREEN gate-c: engine placed the StringTable body in ACTIVE block 4 and its own fence refused the zone; builder block-0 claim refuted")
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
