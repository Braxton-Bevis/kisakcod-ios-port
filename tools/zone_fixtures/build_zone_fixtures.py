#!/usr/bin/env python3
"""Build BMK4's deterministic, synthetic IW3 zone fixture family.

The fixtures contain only strings and byte patterns defined in this file.  The
zlib wrapper uses DEFLATE stored blocks so output is independent of the host's
compression-library version.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import Any, Callable


MAGIC = b"IWffu100"
VERSION = 5
XFILE_SIZE = 44
XASSETLIST_SIZE = 16
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3

ASSET_XANIMPARTS = 0x02
ASSET_CLIPMAP_PVS = 0x0B
ASSET_RAWFILE = 0x1F
ASSET_STRINGTABLE = 0x20

SENTINEL_INLINE = 0xFFFFFFFF
SENTINEL_INLINE_ALIAS = 0xFFFFFFFE


def u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def fnv1a64(data: bytes) -> str:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def adler32(data: bytes) -> int:
    a = 1
    b = 0
    for start in range(0, len(data), 5552):
        for byte in data[start : start + 5552]:
            a = (a + byte) % 65521
            b = (b + a) % 65521
    return (b << 16) | a


def deterministic_zlib(data: bytes) -> bytes:
    """Return a zlib stream made only from deterministic stored blocks."""

    output = bytearray(b"\x78\x01")
    if not data:
        chunks = [b""]
    else:
        chunks = [data[i : i + 0xFFFF] for i in range(0, len(data), 0xFFFF)]
    for index, chunk in enumerate(chunks):
        output.append(1 if index == len(chunks) - 1 else 0)
        output.extend(struct.pack("<HH", len(chunk), len(chunk) ^ 0xFFFF))
        output.extend(chunk)
    output.extend(struct.pack(">I", adler32(data)))
    return bytes(output)


def package_payload(payload: bytes) -> bytes:
    return MAGIC + u32(VERSION) + deterministic_zlib(payload)


def offset_token(block: int, offset: int) -> int:
    if not 0 <= block < 9:
        raise ValueError(f"block out of range: {block}")
    if not 0 <= offset < (1 << 28):
        raise ValueError(f"offset out of range: {offset}")
    return ((block << 28) | offset) + 1


class StreamImage:
    """Track logical block cursors and the physical sequence of file reads."""

    def __init__(self) -> None:
        self.cursors = [0] * 9
        self.reads = bytearray()
        self.events: list[dict[str, Any]] = []

    def align(self, block: int, mask: int) -> int:
        cursor = (self.cursors[block] + mask) & ~mask
        self.cursors[block] = cursor
        return cursor

    def read(self, block: int, data: bytes, *, align: int = 0, label: str) -> int:
        start = self.align(block, align)
        self.reads.extend(data)
        self.cursors[block] += len(data)
        self.events.append(
            {"block": block, "offset": start, "bytes": len(data), "label": label}
        )
        return start

    def reserve(self, block: int, size: int, *, align: int = 0, label: str) -> int:
        start = self.align(block, align)
        self.cursors[block] += size
        self.events.append(
            {
                "block": block,
                "offset": start,
                "bytes": size,
                "label": label,
                "file_bytes": 0,
            }
        )
        return start


def xasset(asset_type: int, pointer_token: int) -> bytes:
    return struct.pack("<II", asset_type, pointer_token)


def rawfile(name_token: int, length: int, buffer_token: int) -> bytes:
    return struct.pack("<III", name_token, length, buffer_token)


def stringtable(name_token: int, columns: int, rows: int, values_token: int) -> bytes:
    return struct.pack("<IIII", name_token, columns, rows, values_token)


def xanimparts(
    *,
    name_token: int,
    numframes: int,
    bone_count: int = 0,
    names_token: int = 0,
    index_count: int = 0,
    indices_token: int = 0,
) -> bytes:
    data = bytearray(88)
    struct.pack_into("<I", data, 0, name_token)
    struct.pack_into("<H", data, 14, numframes)
    data[27] = bone_count
    struct.pack_into("<I", data, 36, index_count)
    struct.pack_into("<f", data, 40, 1.0)
    struct.pack_into("<f", data, 44, 1.0)
    struct.pack_into("<I", data, 48, names_token)
    struct.pack_into("<I", data, 76, indices_token)
    return bytes(data)


def clipmap_pvs_with_runtime_pose(name_token: int) -> bytes:
    """Minimal 284-byte clipMap_t whose first DynEntityPose is runtime-only."""

    data = bytearray(284)
    struct.pack_into("<I", data, 0, name_token)
    struct.pack_into("<H", data, 244, 1)  # dynEntCount[0]
    struct.pack_into("<I", data, 256, 1)  # dynEntPoseList[0], inline runtime
    return bytes(data)


def canonical_string_hash(strings: list[str]) -> str:
    return fnv1a64(b"".join(item.encode("utf-8") + b"\0" for item in strings))


def target(path: str, encoding: str, data: bytes, value: Any) -> dict[str, Any]:
    return {
        "path": path,
        "encoding": encoding,
        "value": value,
        "fnv1a64": fnv1a64(data),
    }


@dataclass
class BuiltZone:
    payload: bytes
    stream: StreamImage
    script_strings: list[str]
    asset_type_counts: dict[str, int]
    targets: list[dict[str, Any]]
    relations: list[dict[str, Any]]
    notes: list[str]
    declared_xfile_size: int | None = None

    def zone_bytes(self) -> bytes:
        return package_payload(self.payload)


@dataclass(frozen=True)
class FixtureDefinition:
    number: int
    name: str
    purpose: str
    valid_builder: Callable[[], BuiltZone]
    malformed_name: str
    malformed_builder: Callable[[], BuiltZone]
    refusal_code: str
    refusal_note: str

    @property
    def directory(self) -> str:
        return f"{self.number:02d}_{self.name}"


def assemble(
    stream: StreamImage,
    *,
    script_strings: list[str],
    asset_count: int,
    asset_type_counts: dict[str, int],
    targets: list[dict[str, Any]],
    relations: list[dict[str, Any]] | None = None,
    notes: list[str] | None = None,
    declared_xfile_size: int | None = None,
) -> BuiltZone:
    script_token = SENTINEL_INLINE if script_strings else 0
    assets_token = SENTINEL_INLINE if asset_count else 0
    asset_list = struct.pack(
        "<IIII", len(script_strings), script_token, asset_count, assets_token
    )
    actual_size = XASSETLIST_SIZE + len(stream.reads)
    xfile_size = actual_size if declared_xfile_size is None else declared_xfile_size
    xfile = struct.pack("<II9I", xfile_size, 0, *stream.cursors)
    return BuiltZone(
        payload=xfile + asset_list + bytes(stream.reads),
        stream=stream,
        script_strings=script_strings,
        asset_type_counts=asset_type_counts,
        targets=targets,
        relations=relations or [],
        notes=notes or [],
        declared_xfile_size=declared_xfile_size,
    )


def load_script_string_list(stream: StreamImage, strings: list[str]) -> list[int]:
    if not strings:
        return []
    stream.read(
        4,
        b"".join(u32(SENTINEL_INLINE) for _ in strings),
        align=3,
        label="script_string_pointer_array",
    )
    offsets = []
    for index, value in enumerate(strings):
        offsets.append(
            stream.read(
                4,
                value.encode("utf-8") + b"\0",
                label=f"script_string[{index}]",
            )
        )
    return offsets


def build_raw_inline(*, truncate: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    stream.read(
        4,
        xasset(ASSET_RAWFILE, SENTINEL_INLINE),
        align=3,
        label="asset_array",
    )
    stream.read(
        0,
        rawfile(SENTINEL_INLINE, 5, 1),
        align=3,
        label="rawfile[0]",
    )
    name = "synthetic/raw_inline.txt"
    stream.read(4, name.encode() + b"\0", label="rawfile[0].name")
    buffer = b"hello\0"
    stream.read(4, buffer, label="rawfile[0].buffer")
    declared = None
    notes: list[str] = []
    if truncate:
        declared = XASSETLIST_SIZE + len(stream.reads)
        stream.reads.pop()
        notes.append("Final RawFile buffer byte removed; declared len remains 5.")
    return assemble(
        stream,
        script_strings=[],
        asset_count=1,
        asset_type_counts={"RAWFILE(31)": 1},
        targets=[
            target("rawfile[0].name", "utf8_nul", name.encode() + b"\0", name),
            target("rawfile[0].len", "u32_le", u32(5), 5),
            target("rawfile[0].buffer", "raw_bytes_nul", buffer, "hello\\0"),
        ],
        notes=notes,
        declared_xfile_size=declared,
    )


def build_stringtable_script_remap(*, bad_count: bool = False) -> BuiltZone:
    stream = StreamImage()
    scripts = ["script_zero", "script_one"]
    load_script_string_list(stream, scripts)
    stream.read(
        4,
        xasset(ASSET_STRINGTABLE, SENTINEL_INLINE)
        + xasset(ASSET_XANIMPARTS, SENTINEL_INLINE),
        align=3,
        label="asset_array",
    )

    table_name = "synthetic/remap.csv"
    table_values = ["key", "value"]
    stream.read(
        0,
        stringtable(SENTINEL_INLINE, 2, 1, 1),
        align=3,
        label="stringtable[0]",
    )
    stream.read(0, table_name.encode() + b"\0", label="stringtable[0].name")
    stream.read(
        0,
        u32(SENTINEL_INLINE) * 2,
        align=3,
        label="stringtable[0].values",
    )
    for index, value in enumerate(table_values):
        stream.read(0, value.encode() + b"\0", label=f"stringtable[0].value[{index}]")

    stream.read(
        0,
        xanimparts(
            name_token=SENTINEL_INLINE,
            numframes=1,
            bone_count=1,
            names_token=1,
        ),
        align=3,
        label="xanimparts[0]",
    )
    anim_name = "synthetic/remap_anim"
    stream.read(4, anim_name.encode() + b"\0", label="xanimparts[0].name")
    stream.read(4, struct.pack("<H", 1), align=1, label="xanimparts[0].names")

    built = assemble(
        stream,
        script_strings=scripts,
        asset_count=2,
        asset_type_counts={"XANIMPARTS(2)": 1, "STRINGTABLE(32)": 1},
        targets=[
            target("stringtable[0].name", "utf8_nul", table_name.encode() + b"\0", table_name),
            target(
                "stringtable[0].values",
                "utf8_nul_sequence",
                b"key\0value\0",
                table_values,
            ),
            target("xanimparts[0].names[0].source_index", "u16_le", b"\x01\x00", 1),
            target(
                "xanimparts[0].names[0].resolved_text",
                "utf8_nul",
                b"script_one\0",
                "script_one",
            ),
        ],
        relations=[
            {
                "kind": "script_string_index_remap",
                "source": "xanimparts[0].names[0]",
                "source_index": 1,
                "target": "script_strings[1]",
                "expected_text": "script_one",
            }
        ],
    )
    if bad_count:
        payload = bytearray(built.payload)
        struct.pack_into("<I", payload, XFILE_SIZE, 0x40000000)
        built.payload = bytes(payload)
        built.script_strings = []
        built.notes.append(
            "XAssetList declares 1,073,741,824 script strings but contains only two slots."
        )
    return built


def build_alias_forward(*, bad_sentinel: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    alias_slot_offset = 16
    alias_token = offset_token(4, alias_slot_offset)
    first_token = 0xFFFFFFFD if bad_sentinel else SENTINEL_INLINE_ALIAS
    stream.read(
        4,
        xasset(ASSET_RAWFILE, first_token) + xasset(ASSET_RAWFILE, alias_token),
        align=3,
        label="asset_array",
    )
    if bad_sentinel:
        return assemble(
            stream,
            script_strings=[],
            asset_count=2,
            asset_type_counts={"RAWFILE(31)": 2},
            targets=[],
            notes=["-3 is neither an IW3 inline sentinel nor a valid in-range alias token."],
        )

    stream.read(
        0,
        rawfile(SENTINEL_INLINE, 3, 1),
        align=3,
        label="rawfile[0]",
    )
    actual_slot = stream.reserve(4, 4, align=3, label="rawfile[0].inserted_alias_slot")
    if actual_slot != alias_slot_offset:
        raise AssertionError((actual_slot, alias_slot_offset))
    name = "alias_owner"
    buffer = b"abc\0"
    stream.read(4, name.encode() + b"\0", label="rawfile[0].name")
    stream.read(4, buffer, label="rawfile[0].buffer")
    return assemble(
        stream,
        script_strings=[],
        asset_count=2,
        asset_type_counts={"RAWFILE(31)": 2},
        targets=[
            target("rawfile[0].name", "utf8_nul", name.encode() + b"\0", name),
            target("rawfile[0].buffer", "raw_bytes_nul", buffer, "abc\\0"),
            target("alias_slot.token", "u32_le", u32(alias_token), f"0x{alias_token:08x}"),
        ],
        relations=[
            {
                "kind": "alias_pointer_equality",
                "source": "assets[1].header",
                "alias_slot": {"block": 4, "offset": alias_slot_offset},
                "target": "assets[0].header",
                "expected_equal": True,
            }
        ],
    )


def build_cross_block(*, bad_offset: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    stream.read(
        4,
        xasset(ASSET_RAWFILE, SENTINEL_INLINE) * 2,
        align=3,
        label="asset_array",
    )
    first_name_offset = 16
    token = offset_token(5, 0) if bad_offset else offset_token(4, first_name_offset)
    stream.read(
        0,
        rawfile(SENTINEL_INLINE, 0, 0),
        align=3,
        label="rawfile[0]",
    )
    name = "cross_block_name"
    actual_offset = stream.read(4, name.encode() + b"\0", label="rawfile[0].name")
    if actual_offset != first_name_offset:
        raise AssertionError((actual_offset, first_name_offset))
    stream.read(0, rawfile(token, 0, 0), align=3, label="rawfile[1]")
    return assemble(
        stream,
        script_strings=[],
        asset_count=2,
        asset_type_counts={"RAWFILE(31)": 2},
        targets=[
            target("rawfile[0].name", "utf8_nul", name.encode() + b"\0", name),
            target("rawfile[1].name_token", "u32_le", u32(token), f"0x{token:08x}"),
        ],
        relations=[
            {
                "kind": "cross_block_pointer",
                "source": "rawfile[1].name",
                "target": {"block": 4 if not bad_offset else 5, "offset": 0 if bad_offset else 16},
                "expected_text": name if not bad_offset else None,
            }
        ],
        notes=["Malformed token targets empty block 5."] if bad_offset else [],
    )


def build_interior_offset(*, bad_offset: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    stream.read(
        4,
        xasset(ASSET_RAWFILE, SENTINEL_INLINE) * 2,
        align=3,
        label="asset_array",
    )
    # The first struct's len bytes are b"mid\0".  Its buffer is null, so the
    # intentionally large integer length does not cause a buffer read.
    stream.read(
        0,
        rawfile(SENTINEL_INLINE, 0x0064696D, 0),
        align=3,
        label="rawfile[0]",
    )
    owner_name = "interior_owner"
    stream.read(4, owner_name.encode() + b"\0", label="rawfile[0].name")
    target_offset = 24 if bad_offset else 4
    token = offset_token(0, target_offset)
    stream.read(0, rawfile(token, 0, 0), align=3, label="rawfile[1]")
    return assemble(
        stream,
        script_strings=[],
        asset_count=2,
        asset_type_counts={"RAWFILE(31)": 2},
        targets=[
            target("rawfile[0].name", "utf8_nul", owner_name.encode() + b"\0", owner_name),
            target("rawfile[1].name_token", "u32_le", u32(token), f"0x{token:08x}"),
            target("rawfile[1].name", "utf8_nul", b"mid\0", "mid"),
        ],
        relations=[
            {
                "kind": "interior_pointer",
                "source": "rawfile[1].name",
                "owning_allocation": "rawfile[0]",
                "target": {"block": 0, "offset": target_offset},
                "interior_delta": target_offset,
                "expected_text": "mid" if not bad_offset else None,
            }
        ],
        notes=["Token targets one byte past block 0."] if bad_offset else [],
    )


def build_delayed_stream(*, truncate: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    stream.read(
        4,
        xasset(ASSET_CLIPMAP_PVS, SENTINEL_INLINE),
        align=3,
        label="asset_array",
    )
    stream.read(
        0,
        clipmap_pvs_with_runtime_pose(SENTINEL_INLINE),
        align=3,
        label="clipmap_pvs[0]",
    )
    clipmap_name = "synthetic/runtime_delay"
    stream.read(4, clipmap_name.encode() + b"\0", label="clipmap_pvs[0].name")
    stream.reserve(1, 32, align=3, label="clipmap_pvs[0].dynEntPoseList[0].zero_fill")
    records = [
        (2, 0, b"B2A"),
        (3, 0, b"B3!!"),
        (2, 3, b"z"),
    ]
    for index, (block, offset, data) in enumerate(records):
        actual = stream.align(block, 0)
        if actual != offset:
            raise AssertionError((actual, offset))
        stream.cursors[block] += len(data)
        stream.events.append(
            {
                "block": block,
                "offset": offset,
                "bytes": len(data),
                "label": f"delayed_record[{index}]",
                "phase": "enqueue_without_read",
            }
        )
    delayed_bytes = b"".join(record[2] for record in records)
    full_xfile_size = XASSETLIST_SIZE + len(stream.reads) + len(delayed_bytes)
    physical = delayed_bytes[:-1] if truncate else delayed_bytes
    stream.reads.extend(physical)
    declared = full_xfile_size if truncate else None
    notes = [
        "The minimal CLIPMAP_PVS drives a genuine block-1 DynEntityPose zero-fill.",
        "No stock asset walker in this tree pushes blocks 2 or 3; the manifest's delayed_read_plan is the explicit kernel probe.",
    ]
    if truncate:
        notes.append("Final delayed byte removed while the read plan still requires it.")
    zeroes = bytes(32)
    return assemble(
        stream,
        script_strings=[],
        asset_count=1,
        asset_type_counts={"CLIPMAP_PVS(11)": 1},
        targets=[
            target(
                "clipmap_pvs[0].name",
                "utf8_nul",
                clipmap_name.encode() + b"\0",
                clipmap_name,
            ),
            target(
                "clipmap_pvs[0].dynEntPoseList[0]",
                "zero_filled_raw_bytes",
                zeroes,
                zeroes.hex(),
            ),
            target(
                "delayed_records.physical_order",
                "raw_bytes",
                delayed_bytes,
                delayed_bytes.decode("ascii"),
            ),
            target("block[2].final", "raw_bytes", b"B2Az", "B2Az"),
            target("block[3].final", "raw_bytes", b"B3!!", "B3!!"),
        ],
        relations=[
            {
                "kind": "delayed_read_plan",
                "drain_phase": "after_asset_walk",
                "records": [
                    {
                        "order": index,
                        "block": block,
                        "offset": offset,
                        "bytes": len(data),
                        "expected_hex": data.hex(),
                    }
                    for index, (block, offset, data) in enumerate(records)
                ],
                "physical_bytes_hex": delayed_bytes.hex(),
            }
        ],
        notes=notes,
        declared_xfile_size=declared,
    )


def build_tagged_union(*, wrong_arm: bool = False) -> BuiltZone:
    stream = StreamImage()
    load_script_string_list(stream, [])
    stream.read(
        4,
        xasset(ASSET_XANIMPARTS, SENTINEL_INLINE),
        align=3,
        label="asset_array",
    )
    numframes = 0x100 if wrong_arm else 7
    indices = b"\x03\x01\x04\x01"
    stream.read(
        0,
        xanimparts(
            name_token=SENTINEL_INLINE,
            numframes=numframes,
            index_count=4,
            indices_token=1,
        ),
        align=3,
        label="xanimparts[0]",
    )
    name = "synthetic/union_u8"
    stream.read(4, name.encode() + b"\0", label="xanimparts[0].name")
    stream.read(4, indices, label="xanimparts[0].indices")
    return assemble(
        stream,
        script_strings=[],
        asset_count=1,
        asset_type_counts={"XANIMPARTS(2)": 1},
        targets=[
            target("xanimparts[0].numframes", "u16_le", struct.pack("<H", numframes), numframes),
            target("xanimparts[0].indices", "raw_bytes", indices, indices.hex()),
            target(
                "xanimparts[0].indices.arm",
                "utf8_nul",
                ("uint16\0" if wrong_arm else "uint8\0").encode(),
                "uint16" if wrong_arm else "uint8",
            ),
        ],
        relations=[
            {
                "kind": "tagged_union_arm",
                "union": "XAnimIndices",
                "selector": "xanimparts[0].numframes",
                "predicate": "numframes >= 256",
                "expected_arm": "uint16_t*" if wrong_arm else "uint8_t*",
                "expected_elements": 4,
                "expected_file_bytes": 8 if wrong_arm else 4,
            }
        ],
        notes=["Selector requires 8 index bytes, but only 4 are present."] if wrong_arm else [],
    )


FIXTURES = [
    FixtureDefinition(
        1,
        "rawfile_inline",
        "RawFile body with an inline -1 XString name and inline buffer.",
        build_raw_inline,
        "malformed_truncated_buffer.ff",
        lambda: build_raw_inline(truncate=True),
        "stream_truncation",
        "Refuse when RawFile.len + 1 bytes cannot be read from block 4.",
    ),
    FixtureDefinition(
        2,
        "stringtable_script_remap",
        "StringTable plus script-string interning and XAnimParts uint16 index remap.",
        build_stringtable_script_remap,
        "malformed_bad_script_count.ff",
        lambda: build_stringtable_script_remap(bad_count=True),
        "bad_count",
        "Refuse the impossible script-string count before allocation or array walking.",
    ),
    FixtureDefinition(
        3,
        "alias_forward",
        "-2 RawFile insertion slot followed by a forward file-image alias reference.",
        build_alias_forward,
        "malformed_bad_sentinel.ff",
        lambda: build_alias_forward(bad_sentinel=True),
        "bad_sentinel",
        "Refuse -3: it is neither -1/-2 nor a valid in-range alias token.",
    ),
    FixtureDefinition(
        4,
        "cross_block_offset",
        "RawFile.name in block 0 resolves to a previously loaded string in block 4.",
        build_cross_block,
        "malformed_empty_block_offset.ff",
        lambda: build_cross_block(bad_offset=True),
        "offset_out_of_bounds",
        "Refuse an offset token targeting empty block 5.",
    ),
    FixtureDefinition(
        5,
        "interior_offset",
        "RawFile.name points four bytes into a previously loaded RawFile struct.",
        build_interior_offset,
        "malformed_one_past_interior.ff",
        lambda: build_interior_offset(bad_offset=True),
        "offset_out_of_bounds",
        "Refuse the one-past-block interior pointer.",
    ),
    FixtureDefinition(
        6,
        "delayed_stream_order",
        "Block-1 zero fill and explicit block-2/3 delayed-record drain ordering.",
        build_delayed_stream,
        "malformed_truncated_delayed_record.ff",
        lambda: build_delayed_stream(truncate=True),
        "delayed_stream_truncation",
        "Refuse when the final queued delayed record cannot be fully drained.",
    ),
    FixtureDefinition(
        7,
        "tagged_union_arm",
        "XAnimIndices selects its uint8_t pointer arm when numframes is below 256.",
        build_tagged_union,
        "malformed_wrong_arm_size.ff",
        lambda: build_tagged_union(wrong_arm=True),
        "union_arm_truncation",
        "Refuse when the uint16_t arm selected by numframes has only uint8_t-arm bytes.",
    ),
]


def oracle_v1_expectations(zone: bytes, payload: bytes) -> dict[str, Any]:
    xfile = payload[:XFILE_SIZE]
    asset_list = payload[XFILE_SIZE : XFILE_SIZE + XASSETLIST_SIZE]
    block_sizes = list(struct.unpack_from("<9I", xfile, 8))
    script_count, script_token, asset_count, assets_token = struct.unpack(
        "<IIII", asset_list
    )
    return {
        "schema": "bmk4.ff-oracle.v1",
        "container.magic": MAGIC.decode(),
        "container.version": VERSION,
        "container.secure": 0,
        "container.input_bytes": len(zone),
        "container.compressed_bytes": len(zone) - 12,
        "container.decompressed_bytes": len(payload),
        "xfile.size": struct.unpack_from("<I", xfile, 0)[0],
        "xfile.external_size": struct.unpack_from("<I", xfile, 4)[0],
        "blocks.bytes": block_sizes,
        "assets.total": asset_count,
        "assets.pointer_token": f"0x{assets_token:08x}",
        "script_strings.count": script_count,
        "script_strings.pointer_token": f"0x{script_token:08x}",
        "script_strings.metadata_hash.fnv1a64": fnv1a64(asset_list[:8]),
        "targeted_hash.xfile.fnv1a64": fnv1a64(xfile),
        "targeted_hash.asset_list.fnv1a64": fnv1a64(asset_list),
        "targeted_hash.decompressed_payload.fnv1a64": fnv1a64(payload),
    }


def valid_manifest(definition: FixtureDefinition, built: BuiltZone) -> dict[str, Any]:
    zone = built.zone_bytes()
    return {
        "schema": "bmk4.zone-fixture-manifest.v1",
        "fixture": definition.name,
        "mechanism": definition.number,
        "purpose": definition.purpose,
        "expectation": "accept",
        "container": {
            "file": "valid.ff",
            "sha256": sha256(zone),
            "decompressed_sha256": sha256(built.payload),
            "magic": MAGIC.decode(),
            "version": VERSION,
        },
        "oracle_v1": oracle_v1_expectations(zone, built.payload),
        "oracle_runtime": {
            "asset_type_counts": built.asset_type_counts,
            "script_strings": {
                "count": len(built.script_strings),
                "content_hash.fnv1a64": canonical_string_hash(built.script_strings),
            },
            "targeted_fields": built.targets,
            "relations": built.relations,
        },
        "stream_events": built.stream.events,
        "notes": built.notes,
    }


def malformed_manifest(
    definition: FixtureDefinition, built: BuiltZone
) -> dict[str, Any]:
    zone = built.zone_bytes()
    return {
        "schema": "bmk4.zone-fixture-refusal.v1",
        "fixture": definition.name,
        "mechanism": definition.number,
        "expectation": "refuse",
        "container": {
            "file": definition.malformed_name,
            "sha256": sha256(zone),
            "decompressed_sha256": sha256(built.payload),
            "magic": MAGIC.decode(),
            "version": VERSION,
        },
        "expected_refusal": {
            "code": definition.refusal_code,
            "note": definition.refusal_note,
            "must_fail_gate": True,
        },
        "oracle_v1_static_parser_may_accept": True,
        "notes": built.notes,
    }


def json_bytes(value: dict[str, Any]) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def expected_files(selected: list[FixtureDefinition]) -> dict[Path, bytes]:
    files: dict[Path, bytes] = {}
    sums: list[tuple[str, str]] = []
    for definition in selected:
        valid = definition.valid_builder()
        malformed = definition.malformed_builder()
        prefix = Path(definition.directory)
        valid_zone = valid.zone_bytes()
        malformed_zone = malformed.zone_bytes()
        files[prefix / "valid.ff"] = valid_zone
        files[prefix / "MANIFEST.json"] = json_bytes(valid_manifest(definition, valid))
        files[prefix / definition.malformed_name] = malformed_zone
        files[prefix / "MALFORMED_MANIFEST.json"] = json_bytes(
            malformed_manifest(definition, malformed)
        )
        sums.append((sha256(valid_zone), str(prefix / "valid.ff").replace("\\", "/")))
        sums.append(
            (
                sha256(malformed_zone),
                str(prefix / definition.malformed_name).replace("\\", "/"),
            )
        )
    if len(selected) == len(FIXTURES):
        files[Path("SHA256SUMS")] = (
            "".join(f"{digest}  {name}\n" for digest, name in sums).encode("ascii")
        )
    return files


def write_files(root: Path, files: dict[Path, bytes]) -> None:
    root.mkdir(parents=True, exist_ok=True)
    for relative, data in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def verify_files(root: Path, files: dict[Path, bytes]) -> bool:
    failures = []
    for relative, expected in files.items():
        path = root / relative
        if not path.exists():
            failures.append(f"missing: {relative}")
        elif path.read_bytes() != expected:
            failures.append(f"byte mismatch: {relative}")
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return False
    print(f"verified {len(files)} deterministic files under {root}")
    return True


def select_fixtures(name: str | None) -> list[FixtureDefinition]:
    if name is None:
        return FIXTURES
    for fixture in FIXTURES:
        if name in (fixture.name, str(fixture.number), fixture.directory):
            return [fixture]
    raise SystemExit(f"unknown fixture {name!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path(__file__).resolve().parent,
        help="directory receiving fixture subdirectories",
    )
    parser.add_argument("--fixture", help="build one fixture by number or name")
    parser.add_argument("--verify", action="store_true", help="compare without writing")
    parser.add_argument("--list", action="store_true", help="list fixture names")
    args = parser.parse_args()
    if args.list:
        for fixture in FIXTURES:
            print(f"{fixture.number}: {fixture.name}")
        return 0
    selected = select_fixtures(args.fixture)
    files = expected_files(selected)
    if args.verify:
        return 0 if verify_files(args.output_root, files) else 1
    write_files(args.output_root, files)
    print(f"wrote {len(files)} deterministic files under {args.output_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
