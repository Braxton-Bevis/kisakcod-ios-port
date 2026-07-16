# BMK4 synthetic IW3 zone fixtures

This directory is the generated, zero-game-data fixture family for ratified
Slice 7. Every zone is an unsigned `IWffu100`, version-5 container assembled
from constants in `build_zone_fixtures.py`. The builder emits a deterministic
zlib stream using DEFLATE stored blocks, so fixture bytes do not depend on a
host compression-library version.

Generate everything:

```text
python tools/zone_fixtures/build_zone_fixtures.py
python tools/zone_fixtures/build_zone_fixtures.py --verify
```

Generate one separately invokable fixture into a scratch directory:

```text
python tools/zone_fixtures/build_zone_fixtures.py \
  --fixture cross_block_offset --output-root scratch/zone-fixtures
```

Each numbered directory contains one valid zone, `MANIFEST.json`, one
malformed twin, and `MALFORMED_MANIFEST.json`. Zone basenames are recorded
in each manifest's `container.file`: fixtures bundled flat into the iOS app
use namespaced basenames (fixture 02: `fixture02_valid.ff` /
`fixture02_malformed_bad_script_count.ff`); fixture 01 keeps its frozen
`valid.ff` / `malformed_truncated_buffer.ff`, and 03-07 keep `valid.ff` /
`malformed_*.ff` until the wave that bundles each one. `MANIFEST.json` records the
current `bmk4.ff-oracle.v1` container/hash fields as well as the runtime values
the Windows loader oracle must validate: per-type counts, normalized script
string content hash, targeted field hashes, pointer relations, and stream
events. Malformed manifests require refusal; accepting one is a failed gate.
The checked-in `SHA256SUMS` covers every valid and malformed zone.

The existing v1 standalone oracle only parses container and XAssetList fields.
Its static acceptance of a structurally malformed asset payload is therefore
explicitly recorded as `oracle_v1_static_parser_may_accept`; CI must apply the
runtime fixture validator before treating the malformed gate as green.

## Discriminating cases

| # | Fixture | What a passing oracle proves | Malformed twin |
|---|---|---|---|
| 1 | `rawfile_inline` | A RawFile body is loaded from block 0; its `-1` name and `len+1` buffer are consumed from block 4. | Drops the final buffer byte; refusal must report stream truncation. |
| 2 | `stringtable_script_remap` | Two strings are interned in declaration order, a minimal StringTable survives, and XAnimParts index `1` remaps to `script_one`. | Declares 1,073,741,824 script strings over two slots; refuse before allocation/walk. |
| 3 | `alias_forward` | `-2` reserves block-4 alias slot `0x10`, back-patches it, and the later asset-array token resolves to the first RawFile pointer. | Uses `-3`; refuse it as neither a sentinel nor an in-range alias. |
| 4 | `cross_block_offset` | A block-0 RawFile field resolves token `block 4, offset 0x10, plus-one encoded` to an earlier string. | Targets empty block 5; refuse the out-of-bounds token. |
| 5 | `interior_offset` | A pointer can resolve to offset `+4` inside an earlier block-0 RawFile allocation and read the synthetic `mid` bytes there. | Uses the one-past-block offset; refuse it. |
| 6 | `delayed_stream_order` | A minimal CLIPMAP_PVS genuinely zero-fills one 32-byte DynEntityPose in block 1 without file bytes, then queued block-2/3 reads drain in record order into their logical destinations. | Removes the final delayed byte; the last record cannot drain and must be refused. |
| 7 | `tagged_union_arm` | `XAnimParts.numframes < 256` selects the byte-pointer arm of `XAnimIndices` and consumes exactly four index bytes. | Changes the selector to 256 while retaining four bytes; the eight-byte uint16 arm must refuse truncation. |

Fixture 6 is deliberately not on the screenshot-critical path. All six real
zones report zero bytes for blocks 2 and 3, and the current source tree has no
asset walker that calls `DB_PushStreamPos(2)` or `(3)`. Its block-1 event is a
real CLIPMAP_PVS walker path; its block-2/3 manifest contains an explicit
`delayed_read_plan` for direct kernel/oracle replay. This settles the physical
byte-order inference without falsely claiming a stock asset type can enqueue
those records today.

## Manifest hashing

All `fnv1a64` values use standard FNV-1a 64-bit lowercase hex. String hashes
include their terminating NUL; a string sequence is the concatenation of each
NUL-terminated UTF-8 string in declaration order. Target entries state their
encoding. Container SHA-256 covers the complete `.ff`, while decompressed
SHA-256 covers the 44-byte XFile header plus XAssetList and physical read
stream.

No fixture contains copied names, strings, layouts, or payloads from a retail
zone. Their human-readable values are synthetic test labels defined in the
builder.
