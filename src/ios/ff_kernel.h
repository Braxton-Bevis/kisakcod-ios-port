// BMK4 slice 7 — fastfile translation kernel, stages K0 + K1 + K2.
//
// K0 (container spine): IWffu100 v5 outer header parse + zlib inflate +
// oracle-domain FNV-1a64 hashing of the decompressed image. Same magic/
// version rules and hash domains as tools/ff_oracle (the Windows Oracle 0
// container inspector), so a K0 result on iOS is directly comparable to an
// oracle dump of the same zone. The inflate lane is the HEADER-FIRST
// EXACT-SIZE reader (frontier ruling P0, docs/reviews/
// frontier-plan-claude-ruling.md): inflate the 44-byte XFile header, read
// xfile.size, allocate declared+44 exactly once (256 MiB policy bound —
// the old 64 MiB cap refused the goal artifact's own zones, see
// docs/REAL_ZONE_EVIDENCE.md), continue the same zlib stream, and require
// exact final length + Z_STREAM_END + no trailing input. This is a RULED
// divergence from the oracle's doubling reader: acceptance narrows to
// truthfully-declared zones (all six real zones declare truthfully).
//
// K1 (RawFile vertical slice): the first 32-bit wire walk. Interprets the
// decompressed image of a single-RawFile zone (fixture mechanism 01):
// XAssetList -> inline asset array -> RawFile struct (32-bit layout:
// name ptr / len / buffer ptr) -> -1 inline string + buffer payloads, with
// per-block cursor accounting checked against the XFile block-size table.
//
// K2 (StringTable + script strings + XAnimParts u16 remap, fixture
// mechanism 02): the walker becomes FFK_WalkZone, a type-indexed dispatch
// table over an engine-faithful stream context — one PHYSICAL file cursor
// separated from NINE ALIGNED LOGICAL block cursors with push/pop and
// reservation support (DB_AllocStreamPos advances a logical cursor with
// alignment and NO file bytes; DB_PopStreamPos REWINDS the temp block 0 to
// its push-time cursor, so XFile.blockSize[0] is a HIGH-WATER mark). The
// engine trace with file:line citations is
// tools/zone_fixtures/ENGINE_TRACE_02.md.
//
// Anything outside the qualified contracts REFUSES with a stable reason
// code — unknown semantics fail closed, never guess (see docs/reviews/
// orchestrator-doctrine-claude.md, gates-must-fail).
//
// iOS-lane only: this TU is in the census and libkisakff.a; it is NOT part
// of the Windows build, whose loader remains the original engine DB_* path.

#pragma once

#include <cstddef>
#include <cstdint>

// Stable refusal codes. String forms are FFK_RefusalName(code).
enum FFKRefusal : uint32_t
{
    FFK_OK = 0,
    // K0 container layer
    FFK_REFUSE_TOO_SHORT,        // shorter than outer header + 1
    FFK_REFUSE_BAD_MAGIC,        // not IWffu100
    FFK_REFUSE_BAD_VERSION,      // version != 5
    FFK_REFUSE_ZLIB_INIT,        // inflateInit failed
    FFK_REFUSE_ZLIB_DATA,        // inflate failed (corrupt stream)
    FFK_REFUSE_ZLIB_TRUNCATED,   // stream ended before Z_STREAM_END
    FFK_REFUSE_TRAILING_BYTES,   // bytes after the zlib stream
    FFK_REFUSE_PAYLOAD_SHORT,    // decompressed < XFile + XAssetList
    FFK_REFUSE_PAYLOAD_LIMIT,    // declared size > 256 MiB policy bound
                                 // (or the single exact allocation failed)
    // K1 stream layer
    FFK_REFUSE_UNSUPPORTED_SCRIPT_STRINGS, // K1 scope: none allowed
    FFK_REFUSE_ASSET_ARRAY_NOT_INLINE,     // assets ptr != 0xffffffff
    FFK_REFUSE_UNSUPPORTED_ASSET_TYPE,     // K1 scope: RAWFILE(31) only
    FFK_REFUSE_ASSET_HEADER_NOT_INLINE,    // header ptr != 0xffffffff
    FFK_REFUSE_NAME_NOT_INLINE,            // name ptr != 0xffffffff
    FFK_REFUSE_BUFFER_NOT_INLINE,          // buffer ptr != 0xffffffff
    FFK_REFUSE_STREAM_TRUNCATION,          // len+1 (or field) exceeds payload
    FFK_REFUSE_NAME_UNTERMINATED,          // no NUL before payload end
    FFK_REFUSE_BUFFER_UNTERMINATED,        // buffer[len] != 0
    FFK_REFUSE_STREAM_NOT_CONSUMED,        // leftover payload after last asset
    FFK_REFUSE_BLOCK_ACCOUNTING,           // per-block use != XFile block table
    // appended (stable codes never renumber)
    FFK_REFUSE_INPUT_TOO_LARGE,            // input exceeds the 32-bit zlib lane
    FFK_REFUSE_UNSUPPORTED_ASSET_COUNT,    // K1: exactly one asset; K2: at most one per type
    FFK_REFUSE_STALE_CONTAINER,            // container is not for the current payload
    // appended at K2 (stable codes never renumber)
    FFK_REFUSE_BAD_COUNT,                  // script-string count cannot fit the payload
                                           // (KERNEL-ADDED: engine is undefined here;
                                           // checked BEFORE any allocation)
    FFK_REFUSE_STRING_NOT_INLINE,          // script-string/cell token != -1 (offset and
                                           // null forms are unimplemented mechanisms)
    FFK_REFUSE_SCRIPT_STRING_INDEX_RANGE,  // u16 remap index >= script-string count
                                           // (KERNEL-ADDED: engine does not bounds-check,
                                           // db_stringtable_load.cpp:3-6)
    FFK_REFUSE_UNSUPPORTED_ASSET_FIELD,    // K2 scope: an XAnimParts field outside the
                                           // qualified EXACT-ZERO metadata shape (see
                                           // WalkXAnimParts; deliberately stricter than
                                           // the engine's pointer-truthiness gating)
    // appended for the header-first exact-size reader (frontier ruling P0,
    // docs/reviews/frontier-plan-claude-ruling.md)
    FFK_REFUSE_PAYLOAD_SIZE_MISMATCH,      // zlib stream produced != declared
                                           // xfile.size + 44 bytes
};

struct FFKContainer // K0 result
{
    uint32_t refusal;            // FFKRefusal; fields below valid when FFK_OK
    uint32_t generation;         // binds this result to the loaded payload
    uint32_t version;
    uint64_t inputBytes;
    uint64_t compressedBytes;
    uint64_t decompressedBytes;
    uint32_t xfileSize;
    uint32_t xfileExternalSize;
    uint32_t blockSizes[9];
    uint32_t scriptStringCount;
    uint32_t scriptStringsToken;
    uint32_t assetCount;
    uint32_t assetsToken;
    uint64_t hashXFile;          // FNV-1a64 payload[0..44)
    uint64_t hashAssetList;      // FNV-1a64 payload[44..60)
    uint64_t hashPayload;        // FNV-1a64 payload[0..n)
    uint64_t hashScriptStringMeta; // FNV-1a64 payload[44..52)
};

struct FFKRawFileK1 // K1 result (single-RawFile zone; assetCount must be 1)
{
    uint32_t refusal;            // FFKRefusal; fields below valid when FFK_OK
    uint32_t rawfileLen;         // declared len field
    uint32_t bufferPresent;      // engine truthiness of the wire buffer field
    uint64_t hashName;           // FNV-1a64 name bytes incl NUL (utf8_nul)
    uint64_t hashLenField;       // FNV-1a64 of the 4 little-endian len bytes
    uint64_t hashBuffer;         // FNV-1a64 buffer bytes incl NUL; ONLY
                                 // meaningful when bufferPresent != 0
    uint32_t blockUse[9];        // per-block bytes consumed by the walk
};

struct FFKZoneK2 // K2 result (typed dispatch over RAWFILE/STRINGTABLE/XANIMPARTS)
{
    uint32_t refusal;            // FFKRefusal; fields below valid when FFK_OK
    // Script-string mechanism: the interned list loaded by the walk.
    uint32_t scriptStringCount;
    uint64_t hashScriptStrings;  // FNV-1a64 of every string incl NUL, in
                                 // declaration order (manifest canonical form)
    // Per-type asset tallies. K2 scope: at most ONE asset of each supported
    // type per zone — the scalar fields below can only honestly represent one.
    uint32_t rawFileCount;
    uint32_t stringTableCount;
    uint32_t xanimPartsCount;
    // RawFile (valid when rawFileCount == 1; same semantics as FFKRawFileK1).
    uint32_t rawfileLen;
    uint32_t rawfileBufferPresent;
    uint64_t hashRawFileName;
    uint64_t hashRawFileLenField;
    uint64_t hashRawFileBuffer;  // ONLY meaningful when bufferPresent != 0
    // StringTable (valid when stringTableCount == 1).
    uint32_t stringTableColumns;
    uint32_t stringTableRows;
    uint64_t hashStringTableName;    // utf8_nul
    uint64_t hashStringTableValues;  // utf8_nul_sequence of all cells in order;
                                     // ONLY meaningful when the wire values
                                     // field was truthy (engine truthiness)
    // XAnimParts (valid when xanimPartsCount == 1).
    uint32_t xanimNamesCount;            // boneCount[9] entries loaded
    uint32_t xanimFirstSourceIndex;      // wire u16 value before remap
    uint64_t hashXAnimFirstSourceIndex;  // FNV-1a64 of the 2 little-endian bytes
    uint64_t hashXAnimFirstResolved;     // remapped interned text incl NUL
    // Accounting: per-block HIGH-WATER marks (block 0 rewinds on pop; see
    // ENGINE_TRACE_02.md). Equal to the XFile table when FFK_OK.
    uint32_t blockUse[9];
};

// K0: parse + inflate + hash a candidate fastfile held in memory. The
// payload buffer is owned by the kernel; PayloadBytes/PayloadSize expose it
// to the walkers and are valid until the next FFK_LoadContainer call.
// Single-threaded by contract (boot-stage usage only).
bool FFK_LoadContainer(const uint8_t* bytes, size_t size, FFKContainer* out);
const uint8_t* FFK_PayloadBytes();
size_t FFK_PayloadSize();

// K1 (FROZEN behavior): walk the CURRENT loaded payload as a single-RawFile
// zone. Keeps its exact K1 scope fences and refusal codes; internally runs
// the K2 stream context with a RAWFILE-only dispatch mask.
bool FFK_WalkRawFileZone(const FFKContainer* container, FFKRawFileK1* out);

// K2: walk the CURRENT loaded payload through the type-indexed dispatch
// table (RAWFILE=31, STRINGTABLE=32, XANIMPARTS=2). Loads the script-string
// list into the interning table and applies the engine's u16 index remap.
bool FFK_WalkZone(const FFKContainer* container, FFKZoneK2* out);

const char* FFK_RefusalName(uint32_t refusal);
