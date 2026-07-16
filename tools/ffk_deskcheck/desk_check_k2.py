#!/usr/bin/env python3
"""K2 desk check: mirror the ff_kernel.cpp K2 walker logic in Python and run
it against the REAL regenerated fixture bytes (01 + 02, valid + malformed)
BEFORE any CI dispatch. Refusal names mirror FFK_RefusalName exactly.

The stream context mirrors the planned C FFKStream: one PHYSICAL file cursor
into the decompressed payload + NINE ALIGNED LOGICAL block cursors with a
push/pop active-block stack and reservation-style alignment (logical advance
with no file bytes), per src/database/db_stream.cpp DB_AllocStreamPos /
DB_PushStreamPos / DB_PopStreamPos and db_stream_load.cpp Load_Stream.
"""

import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
FIX = ROOT / "tools" / "zone_fixtures"

XFILE_SIZE = 44
XASSETLIST_SIZE = 16
INLINE = 0xFFFFFFFF
ASSET_XANIMPARTS = 2
ASSET_RAWFILE = 31
ASSET_STRINGTABLE = 32

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def fnv1a64(data: bytes) -> int:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


class Refusal(Exception):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


class Stream:
    """Physical cursor + nine aligned logical block cursors (see header).

    Mirrors DB_PushStreamPos/DB_PopStreamPos exactly, including the block-0
    POP REWIND (db_stream.cpp:67-74): a stack entry saves {previous index,
    the NEW block's cursor at push time}; popping FROM block 0 restores the
    saved cursor (temp allocations are discarded and reused). Accounting is
    therefore HIGH-WATER per block, not the end-of-walk cursor.
    """

    def __init__(self, payload: bytes):
        self.payload = payload
        self.file = XFILE_SIZE + XASSETLIST_SIZE  # physical cursor
        self.block = [0] * 9                      # logical cursors
        self.high = [0] * 9                       # high-water marks
        self.stack = []                           # (prev index, pushed pos)
        self.active = 0

    def push(self, index: int) -> None:
        # DB_PushStreamPos: save previous index + NEW block's push-time pos.
        self.stack.append((self.active, self.block[index]))
        self.active = index

    def pop(self) -> None:
        # DB_PopStreamPos: rewind only when popping FROM block 0.
        prev_index, pushed_pos = self.stack.pop()
        if self.active == 0:
            self.block[0] = pushed_pos
        self.active = prev_index

    def _bump_high(self) -> None:
        if self.block[self.active] > self.high[self.active]:
            self.high[self.active] = self.block[self.active]

    def align(self, mask: int) -> None:
        # DB_AllocStreamPos: logical only, never a file byte.
        self.block[self.active] = (self.block[self.active] + mask) & ~mask
        self._bump_high()

    def read(self, size: int) -> bytes:
        # Load_Stream on a physical block: file bytes + logical advance.
        if size > len(self.payload) - self.file:
            raise Refusal("stream_truncation")
        data = self.payload[self.file : self.file + size]
        self.file += size
        self.block[self.active] += size
        self._bump_high()
        return data

    def read_string(self) -> bytes:
        # Load_XStringCustom: bytes through NUL; returned WITH the NUL.
        nul = self.payload.find(b"\0", self.file)
        if nul < 0:
            raise Refusal("name_unterminated")
        length = nul - self.file + 1
        return self.read(length)


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<H", data, offset)[0]


MAX_DECLARED = 256 * 1024 * 1024  # frontier ruling P0 policy bound


def load_container(zone: bytes) -> dict:
    """Mirror the header-first exact-size reader's outcome semantics."""
    assert zone[:8] == b"IWffu100" and u32(zone, 8) == 5
    payload = zlib.decompress(zone[12:])
    if len(payload) < XFILE_SIZE:
        raise Refusal("payload_short")
    declared_total = u32(payload, 0) + XFILE_SIZE  # checked-add, 64-bit
    if declared_total > MAX_DECLARED:
        raise Refusal("payload_limit")
    if declared_total < XFILE_SIZE + XASSETLIST_SIZE:
        raise Refusal("payload_short")
    if len(payload) != declared_total:
        raise Refusal("payload_size_mismatch")
    return {
        "payload": payload,
        "blockSizes": list(struct.unpack_from("<9I", payload, 8)),
        "scriptStringCount": u32(payload, XFILE_SIZE),
        "scriptStringsToken": u32(payload, XFILE_SIZE + 4),
        "assetCount": u32(payload, XFILE_SIZE + 8),
        "assetsToken": u32(payload, XFILE_SIZE + 12),
    }


def walk_zone(c: dict, allowed: set[int]) -> dict:
    payload = c["payload"]
    s = Stream(payload)
    out = {
        "scriptStrings": [],
        "typeCounts": {},
        "rawfile": None,
        "stringtable": None,
        "xanim": None,
    }

    # --- script-string list (Load_XAssetListCustom -> Load_ScriptStringList,
    # active block 4). Metadata consistency is a K2 scope fence.
    count = c["scriptStringCount"]
    token = c["scriptStringsToken"]
    if count == 0:
        if token != 0:
            raise Refusal("string_not_inline")
    else:
        if token != INLINE:
            raise Refusal("string_not_inline")
        # KERNEL-ADDED (engine is undefined here): the pointer array must
        # fit the remaining payload BEFORE any allocation. 64-bit math.
        if count * 4 > len(payload) - s.file:
            raise Refusal("bad_count")
        s.push(4)
        s.align(3)  # AllocLoad_FxElemVisStateSample
        ptrs = s.read(4 * count)
        for i in range(count):
            if u32(ptrs, 4 * i) != INLINE:
                raise Refusal("string_not_inline")
            s.align(0)  # AllocLoad_raw_byte
            out["scriptStrings"].append(s.read_string())
        s.pop()

    # --- asset array (db_file_load.cpp:281-288, block 4 pushed once).
    if c["assetCount"] < 1:
        raise Refusal("unsupported_asset_count")
    if c["assetsToken"] != INLINE:
        raise Refusal("asset_array_not_inline")
    s.push(4)
    s.align(3)  # AllocLoad_FxElemVisStateSample
    assets = s.read(8 * c["assetCount"])
    for i in range(c["assetCount"]):
        atype = u32(assets, 8 * i)
        header = u32(assets, 8 * i + 4)
        if atype not in allowed:
            raise Refusal("unsupported_asset_type")
        if header != INLINE:
            raise Refusal("asset_header_not_inline")
        out["typeCounts"][atype] = out["typeCounts"].get(atype, 0) + 1
        if out["typeCounts"][atype] > 1:
            raise Refusal("unsupported_asset_count")
        if atype == ASSET_RAWFILE:
            walk_rawfile(s, out)
        elif atype == ASSET_STRINGTABLE:
            walk_stringtable(s, out)
        elif atype == ASSET_XANIMPARTS:
            walk_xanimparts(s, out)
    s.pop()

    if s.file != len(payload):
        raise Refusal("stream_not_consumed")
    for i in range(9):
        if s.high[i] != c["blockSizes"][i]:
            raise Refusal("block_accounting")
    out["blockUse"] = s.high
    return out


def walk_rawfile(s: Stream, out: dict) -> None:
    # Load_RawFilePtr db_load.cpp:5658-5687 pushes block 0; Load_RawFile
    # 5643-5656 pushes 4 for name+buffer.
    s.push(0)
    s.align(3)  # AllocLoad_FxElemVisStateSample
    struct_bytes = s.read(12)
    name_token = u32(struct_bytes, 0)
    length = u32(struct_bytes, 4)
    buffer_token = u32(struct_bytes, 8)
    if name_token != INLINE:
        raise Refusal("name_not_inline")
    s.push(4)
    s.align(0)
    name = s.read_string()
    buf = None
    if buffer_token != 0:  # engine truthiness (db_load.cpp:5649)
        s.align(0)  # AllocLoad_raw_byte
        buf = s.read(length + 1)
        if buf[-1] != 0:  # KERNEL-ADDED (K1-frozen)
            raise Refusal("buffer_unterminated")
    s.pop()
    s.pop()
    out["rawfile"] = {
        "len": length,
        "bufferPresent": 1 if buffer_token else 0,
        "hashName": fnv1a64(name),
        "hashLenField": fnv1a64(struct.pack("<I", length)),
        "hashBuffer": fnv1a64(buf) if buf is not None else 0,
    }


def walk_stringtable(s: Stream, out: dict) -> None:
    # Load_StringTablePtr db_load.cpp:5711-5728: NO push — stays in the
    # active block (4). Struct is {name, columnCount, rowCount, values}.
    s.align(3)  # AllocLoad_FxElemVisStateSample
    st = s.read(16)
    name_token, columns, rows, values_token = struct.unpack("<IIII", st)
    if name_token != INLINE:
        raise Refusal("name_not_inline")
    s.align(0)
    name = s.read_string()
    cells = []
    hash_values = 0  # C: only meaningful when the wire values field is truthy
    if values_token != 0:  # engine truthiness (db_load.cpp:5703)
        total = rows * columns  # 64-bit in Python; C uses u64
        # Division form, mirroring the C overflow-safe bound.
        if total > (len(s.payload) - s.file) // 4:
            raise Refusal("stream_truncation")
        s.align(3)  # AllocLoad_FxElemVisStateSample
        tokens = s.read(4 * total)
        for i in range(total):
            if u32(tokens, 4 * i) != INLINE:
                raise Refusal("string_not_inline")
            s.align(0)
            cells.append(s.read_string())
        hash_values = fnv1a64(b"".join(cells))
    out["stringtable"] = {
        "columns": columns,
        "rows": rows,
        "hashName": fnv1a64(name),
        "hashValues": hash_values,
    }


def walk_xanimparts(s: Stream, out: dict) -> None:
    # Load_XAnimPartsPtr db_load.cpp:984-1013 pushes block 0 (line 990);
    # Load_XAnimParts 919-982 pushes 4 (line 922) for internals.
    s.push(0)
    s.align(3)
    xa = s.read(88)
    name_token = u32(xa, 0)
    numframes = u16(xa, 14)
    bone9 = xa[27]
    index_count = u32(xa, 36)
    names_token = u32(xa, 48)
    notify_count = xa[28]
    # K2 scope fences: any unimplemented mechanism refuses (fail closed).
    pointers = {
        "dataByte": u32(xa, 52), "dataShort": u32(xa, 56),
        "dataInt": u32(xa, 60), "randomDataShort": u32(xa, 64),
        "randomDataByte": u32(xa, 68), "randomDataInt": u32(xa, 72),
        "indices": u32(xa, 76), "notify": u32(xa, 80),
        "deltaPart": u32(xa, 84),
    }
    if any(pointers.values()) or index_count != 0 or notify_count != 0 \
            or numframes >= 0x100:
        raise Refusal("unsupported_asset_field")
    if name_token != INLINE:
        raise Refusal("name_not_inline")
    s.push(4)
    s.align(0)
    name = s.read_string()
    entries = []
    if names_token != 0:  # engine truthiness (db_load.cpp:925)
        s.align(1)  # AllocLoad_XBlendInfo — 2-byte alignment
        raw = s.read(2 * bone9)
        for i in range(bone9):
            idx = u16(raw, 2 * i)
            # KERNEL-ADDED bounds check; the engine does not check
            # (db_stringtable_load.cpp:3-6).
            if idx >= len(out["scriptStrings"]):
                raise Refusal("script_string_index_range")
            entries.append((idx, out["scriptStrings"][idx]))
    s.pop()
    s.pop()
    out["xanim"] = {
        "hashName": fnv1a64(name),
        "namesCount": len(entries),
        "firstIndex": entries[0][0] if entries else 0,
        "hashFirstResolved": fnv1a64(entries[0][1]) if entries else 0,
    }


def walk_rawfile_zone_k1(c: dict) -> dict:
    """Mirror FFK_WalkRawFileZone: frozen K1 fences + RAWFILE-only mask."""
    if c["assetCount"] != 1:
        raise Refusal("unsupported_asset_count")
    if c["scriptStringCount"] != 0 or c["scriptStringsToken"] != 0:
        raise Refusal("unsupported_script_strings")
    return walk_zone(c, {ASSET_RAWFILE})


def expect(cond: bool, what: str) -> None:
    if not cond:
        raise SystemExit(f"DESK CHECK FAIL: {what}")


def main() -> None:
    all_types = {ASSET_RAWFILE, ASSET_STRINGTABLE, ASSET_XANIMPARTS}

    # ---- fixture 01 valid: FROZEN K1 behavior via the new context.
    m1 = json.loads((FIX / "01_rawfile_inline" / "MANIFEST.json").read_text())
    z1 = (FIX / "01_rawfile_inline" / "valid.ff").read_bytes()
    c1 = load_container(z1)
    r1 = walk_rawfile_zone_k1(c1)
    t1 = {t["path"]: t for t in m1["oracle_runtime"]["targeted_fields"]}
    expect(f'{r1["rawfile"]["hashName"]:016x}' == t1["rawfile[0].name"]["fnv1a64"],
           "fixture01 name hash")
    expect(f'{r1["rawfile"]["hashLenField"]:016x}' == t1["rawfile[0].len"]["fnv1a64"],
           "fixture01 len hash")
    expect(f'{r1["rawfile"]["hashBuffer"]:016x}' == t1["rawfile[0].buffer"]["fnv1a64"],
           "fixture01 buffer hash")
    expect(r1["blockUse"] == m1["oracle_v1"]["blocks.bytes"], "fixture01 blocks")
    print("fixture01 valid: OK", r1["blockUse"])

    # ---- fixture 01 malformed twin: stream_truncation (FROZEN).
    zt = (FIX / "01_rawfile_inline" / "malformed_truncated_buffer.ff").read_bytes()
    try:
        walk_rawfile_zone_k1(load_container(zt))
        raise SystemExit("DESK CHECK FAIL: fixture01 twin accepted")
    except Refusal as refusal:
        expect(refusal.code == "stream_truncation", f"twin01 code {refusal.code}")
    print("fixture01 twin: refused stream_truncation")

    # ---- fixture 02 valid.
    d2 = FIX / "02_stringtable_script_remap"
    m2 = json.loads((d2 / "MANIFEST.json").read_text())
    z2 = (d2 / "fixture02_valid.ff").read_bytes()
    c2 = load_container(z2)
    r2 = walk_zone(c2, all_types)
    rt = m2["oracle_runtime"]
    t2 = {t["path"]: t for t in rt["targeted_fields"]}
    expect(len(r2["scriptStrings"]) == rt["script_strings"]["count"],
           "fixture02 script count")
    expect(f'{fnv1a64(b"".join(r2["scriptStrings"])):016x}'
           == rt["script_strings"]["content_hash.fnv1a64"],
           "fixture02 script content hash")
    expect(f'{r2["stringtable"]["hashName"]:016x}'
           == t2["stringtable[0].name"]["fnv1a64"], "fixture02 st name hash")
    expect(f'{r2["stringtable"]["hashValues"]:016x}'
           == t2["stringtable[0].values"]["fnv1a64"], "fixture02 st values hash")
    expect(r2["xanim"]["firstIndex"]
           == t2["xanimparts[0].names[0].source_index"]["value"],
           "fixture02 source index")
    expect(f'{r2["xanim"]["hashFirstResolved"]:016x}'
           == t2["xanimparts[0].names[0].resolved_text"]["fnv1a64"],
           "fixture02 resolved hash")
    relation = rt["relations"][0]
    expect(relation["kind"] == "script_string_index_remap"
           and r2["scriptStrings"][relation["source_index"]]
           == relation["expected_text"].encode() + b"\0",
           "fixture02 remap relation")
    expect(r2["blockUse"] == m2["oracle_v1"]["blocks.bytes"], "fixture02 blocks")
    print("fixture02 valid: OK", r2["blockUse"])

    # ---- fixture 02 malformed twin: bad_count BEFORE allocation.
    zt2 = (d2 / "fixture02_malformed_bad_script_count.ff").read_bytes()
    mm2 = json.loads((d2 / "MALFORMED_MANIFEST.json").read_text())
    try:
        walk_zone(load_container(zt2), all_types)
        raise SystemExit("DESK CHECK FAIL: fixture02 twin accepted")
    except Refusal as refusal:
        expect(refusal.code == mm2["expected_refusal"]["code"],
               f"twin02 code {refusal.code}")
    print("fixture02 twin: refused bad_count")

    # ---- K1 wrapper scope fence on fixture 02 (the smoke's "scope" leg):
    # the frozen single-RawFile entry point must refuse a two-asset zone.
    try:
        walk_rawfile_zone_k1(c2)
        raise SystemExit("DESK CHECK FAIL: K1 wrapper accepted fixture02")
    except Refusal as refusal:
        expect(refusal.code == "unsupported_asset_count",
               f"scope code {refusal.code}")
    print("fixture02 vs K1 wrapper: refused unsupported_asset_count")

    def deflate_zone(payload: bytes) -> bytes:
        return b"IWffu100" + struct.pack("<I", 5) + zlib.compress(payload)

    # ---- Exact-size reader probes (frontier ruling P0).
    base = c1["payload"]
    for delta in (-1, 1):
        lying = bytearray(base)
        struct.pack_into("<I", lying, 0, u32(base, 0) + delta)
        try:
            load_container(deflate_zone(bytes(lying)))
            raise SystemExit("DESK CHECK FAIL: lying declared size accepted")
        except Refusal as refusal:
            expect(refusal.code == "payload_size_mismatch",
                   f"mismatch code {refusal.code}")
    print("declared-size mismatch (both directions): refused payload_size_mismatch")

    killhouse = 76935387  # docs/REAL_ZONE_EVIDENCE.md decompressed bytes
    big = bytearray(killhouse)
    struct.pack_into("<I", big, 0, killhouse - 44)
    cbig = load_container(deflate_zone(bytes(big)))
    expect(len(cbig["payload"]) == killhouse, "killhouse-size zone accepted")
    try:
        walk_zone(cbig, all_types)
        raise SystemExit("DESK CHECK FAIL: empty asset list accepted")
    except Refusal as refusal:
        expect(refusal.code == "unsupported_asset_count",
               f"large walk code {refusal.code}")
    print("killhouse-size zone: container ACCEPTED, empty walk refused")

    beyond = bytearray(60)
    struct.pack_into("<I", beyond, 0, MAX_DECLARED + 1 - 44)
    try:
        load_container(deflate_zone(bytes(beyond)))
        raise SystemExit("DESK CHECK FAIL: beyond-policy declaration accepted")
    except Refusal as refusal:
        expect(refusal.code == "payload_limit", f"policy code {refusal.code}")
    print("beyond-policy declaration: refused payload_limit")

    # ---- Dispatch-mask probe: one-asset StringTable vs the K1 wrapper.
    mask = bytearray(68)
    struct.pack_into("<I", mask, 0, 24)
    struct.pack_into("<I", mask, 52, 1)
    struct.pack_into("<I", mask, 56, 0xFFFFFFFF)
    struct.pack_into("<I", mask, 60, ASSET_STRINGTABLE)
    struct.pack_into("<I", mask, 64, 0xFFFFFFFF)
    cmask = load_container(deflate_zone(bytes(mask)))
    try:
        walk_rawfile_zone_k1(cmask)
        raise SystemExit("DESK CHECK FAIL: K1 wrapper accepted a StringTable")
    except Refusal as refusal:
        expect(refusal.code == "unsupported_asset_type",
               f"mask code {refusal.code}")
    print("one-asset StringTable vs K1 wrapper: refused unsupported_asset_type")

    # ---- Overflow regression: 2^31 x 2^31 StringTable cells.
    overflow = bytearray(86)
    struct.pack_into("<I", overflow, 0, 42)
    struct.pack_into("<I", overflow, 52, 1)
    struct.pack_into("<I", overflow, 56, 0xFFFFFFFF)
    struct.pack_into("<I", overflow, 60, ASSET_STRINGTABLE)
    struct.pack_into("<I", overflow, 64, 0xFFFFFFFF)
    struct.pack_into("<I", overflow, 68, 0xFFFFFFFF)
    struct.pack_into("<I", overflow, 72, 0x80000000)
    struct.pack_into("<I", overflow, 76, 0x80000000)
    struct.pack_into("<I", overflow, 80, 1)
    overflow[84] = ord("x")
    coverflow = load_container(deflate_zone(bytes(overflow)))
    try:
        walk_zone(coverflow, all_types)
        raise SystemExit("DESK CHECK FAIL: 2^62-cell StringTable accepted")
    except Refusal as refusal:
        expect(refusal.code == "stream_truncation",
               f"overflow code {refusal.code}")
    print("2^62-cell StringTable: refused stream_truncation (no crash)")

    print("desk check: ALL GREEN")


if __name__ == "__main__":
    main()
