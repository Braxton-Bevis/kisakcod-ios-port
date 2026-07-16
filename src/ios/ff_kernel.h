// BMK4 slice 7 — fastfile translation kernel, stages K0 + K1.
//
// K0 (container spine): IWffu100 v5 outer header parse + zlib inflate +
// oracle-domain FNV-1a64 hashing of the decompressed image. Byte-for-byte
// the same acceptance rules and hash domains as tools/ff_oracle (the Windows
// Oracle 0 container inspector), so a K0 result on iOS is directly
// comparable to an oracle dump of the same zone.
//
// K1 (RawFile vertical slice): the first 32-bit wire walk. Interprets the
// decompressed image of a single-RawFile zone (fixture mechanism 01):
// XAssetList -> inline asset array -> RawFile struct (32-bit layout:
// name ptr / len / buffer ptr) -> -1 inline string + buffer payloads, with
// per-block cursor accounting checked against the XFile block-size table.
// Anything outside that contract REFUSES with a stable reason code —
// unknown semantics fail closed, never guess (see docs/reviews/
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
    FFK_REFUSE_PAYLOAD_LIMIT,    // decompressed > safety limit
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
};

struct FFKContainer // K0 result
{
    uint32_t refusal;            // FFKRefusal; fields below valid when FFK_OK
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

struct FFKRawFileK1 // K1 result (single-RawFile zone)
{
    uint32_t refusal;            // FFKRefusal; fields below valid when FFK_OK
    uint32_t rawfileLen;         // declared len field
    uint64_t hashName;           // FNV-1a64 name bytes incl NUL (utf8_nul)
    uint64_t hashLenField;       // FNV-1a64 of the 4 little-endian len bytes
    uint64_t hashBuffer;         // FNV-1a64 buffer bytes incl NUL
    uint32_t blockUse[9];        // per-block bytes consumed by the walk
};

// K0: parse + inflate + hash a candidate fastfile held in memory. The
// payload buffer is owned by the kernel; PayloadBytes/PayloadSize expose it
// to the K1 walk and are valid until the next FFK_LoadContainer call.
// Single-threaded by contract (boot-stage usage only).
bool FFK_LoadContainer(const uint8_t* bytes, size_t size, FFKContainer* out);
const uint8_t* FFK_PayloadBytes();
size_t FFK_PayloadSize();

// K1: walk the CURRENT loaded payload as a single-RawFile zone.
bool FFK_WalkRawFileZone(const FFKContainer* container, FFKRawFileK1* out);

const char* FFK_RefusalName(uint32_t refusal);
