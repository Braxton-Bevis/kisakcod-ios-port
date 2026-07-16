// BMK4 slice 7 — fastfile translation kernel, stages K0 + K1. See ff_kernel.h.
//
// K0 mirrors tools/ff_oracle/ff_oracle.cpp acceptance rules and hash domains
// exactly (same magic/version checks, same chunked inflate with truncation
// and trailing-byte detection, same FNV-1a64 offsets). Divergence between
// this file and the oracle IS the bug the round-trip gate exists to catch.
//
// K1 is the first deliberate 32-bit wire walk. All wire fields are read as
// explicit little-endian u32 from byte offsets — never by casting wire bytes
// to native structs (LP64 rule: pointers here are 8 bytes; on the wire they
// are 4). Per-block cursors advance exactly as IW3's DB_AllocStreamPos
// would, and the final accounting must equal the XFile block-size table.

#include "ff_kernel.h"

#include <cstring>
#include <cstdlib>

#include <zlib/zlib.h>

namespace
{
constexpr size_t kOuterHeaderSize = 12;
constexpr size_t kXFileSize = 44;
constexpr size_t kXAssetListSize = 16;
constexpr size_t kMinimumPayloadSize = kXFileSize + kXAssetListSize;
constexpr size_t kInflateChunkSize = 64 * 1024;
// Boot-stage fixtures are hundreds of bytes; real zones come much later and
// will re-derive this bound from the XFile header. 64 MiB fails closed long
// before an iOS allocation does.
constexpr size_t kMaximumPayloadSize = 64ull * 1024 * 1024;
constexpr uint32_t kExpectedVersion = 5;
constexpr uint32_t kInlineToken = 0xffffffffu; // -1: payload follows in stream
constexpr uint32_t kAssetTypeRawFile = 31;     // IW3 ASSET_TYPE_RAWFILE

// Single-threaded boot-stage state: the decompressed image K1 walks.
uint8_t* s_payload;
size_t s_payloadSize;

uint64_t Fnv1a64(const uint8_t* bytes, const size_t size)
{
    uint64_t hash = 14695981039346656037ull;
    for (size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint32_t ReadU32(const uint8_t* bytes, const size_t offset)
{
    return static_cast<uint32_t>(bytes[offset])
        | (static_cast<uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint32_t InflateOuterPayload(const uint8_t* bytes, const size_t size)
{
    free(s_payload);
    s_payload = nullptr;
    s_payloadSize = 0;

    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = const_cast<Bytef*>(bytes + kOuterHeaderSize);
    stream.avail_in = static_cast<uInt>(size - kOuterHeaderSize);
    if (inflateInit(&stream) != Z_OK)
        return FFK_REFUSE_ZLIB_INIT;

    size_t capacity = kInflateChunkSize;
    uint8_t* payload = static_cast<uint8_t*>(malloc(capacity));
    size_t produced = 0;
    int result = Z_OK;
    uint32_t refusal = FFK_OK;

    while (payload && result != Z_STREAM_END)
    {
        if (produced > kMaximumPayloadSize)
        {
            refusal = FFK_REFUSE_PAYLOAD_LIMIT;
            break;
        }
        if (capacity - produced < kInflateChunkSize)
        {
            capacity *= 2;
            uint8_t* grown = static_cast<uint8_t*>(realloc(payload, capacity));
            if (!grown)
            {
                refusal = FFK_REFUSE_PAYLOAD_LIMIT;
                break;
            }
            payload = grown;
        }
        stream.next_out = payload + produced;
        stream.avail_out = static_cast<uInt>(kInflateChunkSize);
        result = inflate(&stream, Z_NO_FLUSH);
        const size_t chunk = kInflateChunkSize - stream.avail_out;
        produced += chunk;

        if (result != Z_OK && result != Z_STREAM_END)
        {
            refusal = FFK_REFUSE_ZLIB_DATA;
            break;
        }
        if (result == Z_OK && chunk == 0 && stream.avail_in == 0)
        {
            refusal = FFK_REFUSE_ZLIB_TRUNCATED;
            break;
        }
    }
    if (!payload)
        refusal = FFK_REFUSE_PAYLOAD_LIMIT;
    if (refusal == FFK_OK && stream.avail_in != 0)
        refusal = FFK_REFUSE_TRAILING_BYTES;
    inflateEnd(&stream);

    if (refusal != FFK_OK)
    {
        free(payload);
        return refusal;
    }
    s_payload = payload;
    s_payloadSize = produced;
    return FFK_OK;
}

// Bounded sequential stream reader with per-block cursor accounting.
struct StreamWalk
{
    const uint8_t* payload;
    size_t size;
    size_t cursor;         // absolute payload offset (starts after XAssetList)
    uint32_t blockUse[9];  // bytes attributed to each block so far

    bool Take(const uint32_t block, const size_t bytes, const uint8_t** out)
    {
        if (bytes > size - cursor)
            return false;
        *out = payload + cursor;
        cursor += bytes;
        blockUse[block] += static_cast<uint32_t>(bytes);
        return true;
    }

    // NUL-terminated inline string; length includes the NUL.
    bool TakeString(const uint32_t block, const uint8_t** out, size_t* outLen)
    {
        const uint8_t* start = payload + cursor;
        const size_t remaining = size - cursor;
        const uint8_t* nul = static_cast<const uint8_t*>(memchr(start, 0, remaining));
        if (!nul)
            return false;
        const size_t length = static_cast<size_t>(nul - start) + 1;
        *out = start;
        *outLen = length;
        cursor += length;
        blockUse[block] += static_cast<uint32_t>(length);
        return true;
    }
};
} // namespace

bool FFK_LoadContainer(const uint8_t* bytes, const size_t size, FFKContainer* out)
{
    memset(out, 0, sizeof(*out));
    if (size < kOuterHeaderSize + 1)
    {
        out->refusal = FFK_REFUSE_TOO_SHORT;
        return false;
    }
    if (memcmp(bytes, "IWffu100", 8) != 0)
    {
        out->refusal = FFK_REFUSE_BAD_MAGIC;
        return false;
    }
    out->version = ReadU32(bytes, 8);
    if (out->version != kExpectedVersion)
    {
        out->refusal = FFK_REFUSE_BAD_VERSION;
        return false;
    }

    const uint32_t inflateRefusal = InflateOuterPayload(bytes, size);
    if (inflateRefusal != FFK_OK)
    {
        out->refusal = inflateRefusal;
        return false;
    }
    if (s_payloadSize < kMinimumPayloadSize)
    {
        out->refusal = FFK_REFUSE_PAYLOAD_SHORT;
        return false;
    }

    out->inputBytes = size;
    out->compressedBytes = size - kOuterHeaderSize;
    out->decompressedBytes = s_payloadSize;
    out->xfileSize = ReadU32(s_payload, 0);
    out->xfileExternalSize = ReadU32(s_payload, 4);
    for (size_t index = 0; index < 9; ++index)
        out->blockSizes[index] = ReadU32(s_payload, 8 + index * 4);
    out->scriptStringCount = ReadU32(s_payload, kXFileSize);
    out->scriptStringsToken = ReadU32(s_payload, kXFileSize + 4);
    out->assetCount = ReadU32(s_payload, kXFileSize + 8);
    out->assetsToken = ReadU32(s_payload, kXFileSize + 12);
    out->hashXFile = Fnv1a64(s_payload, kXFileSize);
    out->hashAssetList = Fnv1a64(s_payload + kXFileSize, kXAssetListSize);
    out->hashPayload = Fnv1a64(s_payload, s_payloadSize);
    out->hashScriptStringMeta = Fnv1a64(s_payload + kXFileSize, 8);
    out->refusal = FFK_OK;
    return true;
}

const uint8_t* FFK_PayloadBytes() { return s_payload; }
size_t FFK_PayloadSize() { return s_payloadSize; }

bool FFK_WalkRawFileZone(const FFKContainer* container, FFKRawFileK1* out)
{
    memset(out, 0, sizeof(*out));

    // K1 scope fence: a single inline RawFile, no script strings. Anything
    // else is a later mechanism (02..07) and must fail closed here.
    if (container->scriptStringCount != 0 || container->scriptStringsToken != 0)
    {
        out->refusal = FFK_REFUSE_UNSUPPORTED_SCRIPT_STRINGS;
        return false;
    }
    if (container->assetsToken != kInlineToken)
    {
        out->refusal = FFK_REFUSE_ASSET_ARRAY_NOT_INLINE;
        return false;
    }

    StreamWalk walk;
    memset(&walk, 0, sizeof(walk));
    walk.payload = s_payload;
    walk.size = s_payloadSize;
    walk.cursor = kXFileSize + kXAssetListSize;

    // Inline XAsset array: assetCount entries of {u32 type, u32 header ptr}
    // in the virtual block (4). 32-bit wire layout — 8 bytes per entry.
    const uint8_t* assetArray = nullptr;
    const size_t assetArrayBytes = static_cast<size_t>(container->assetCount) * 8;
    if (!walk.Take(4, assetArrayBytes, &assetArray))
    {
        out->refusal = FFK_REFUSE_STREAM_TRUNCATION;
        return false;
    }
    for (uint32_t index = 0; index < container->assetCount; ++index)
    {
        const uint32_t type = ReadU32(assetArray, index * 8);
        const uint32_t headerToken = ReadU32(assetArray, index * 8 + 4);
        if (type != kAssetTypeRawFile)
        {
            out->refusal = FFK_REFUSE_UNSUPPORTED_ASSET_TYPE;
            return false;
        }
        if (headerToken != kInlineToken)
        {
            out->refusal = FFK_REFUSE_ASSET_HEADER_NOT_INLINE;
            return false;
        }
    }

    for (uint32_t index = 0; index < container->assetCount; ++index)
    {
        // RawFile struct, 32-bit wire layout in the temp block (0):
        // {const char* name; int len; const char* buffer} = 12 bytes.
        const uint8_t* rawfile = nullptr;
        if (!walk.Take(0, 12, &rawfile))
        {
            out->refusal = FFK_REFUSE_STREAM_TRUNCATION;
            return false;
        }
        const uint32_t nameToken = ReadU32(rawfile, 0);
        const uint32_t len = ReadU32(rawfile, 4);
        const uint32_t bufferToken = ReadU32(rawfile, 8);
        // name is a pointer-typed field (engine loads it via Load_XString):
        // -1 = inline-follows. Offset/alias forms are mechanisms 04/05 and
        // refuse here until K3 implements them.
        if (nameToken != kInlineToken)
        {
            out->refusal = FFK_REFUSE_NAME_NOT_INLINE;
            return false;
        }
        // buffer is NOT a tokened pointer on this path. The engine's own
        // loader (src/database/db_load.cpp:5643-5656, Load_RawFile) tests it
        // for TRUTHINESS only: any nonzero wire value means len+1 bytes
        // follow inline in block 4; zero means no buffer bytes at all.
        const bool hasBuffer = bufferToken != 0;

        // Inline name: NUL-terminated in the virtual block (4) — the engine
        // pushes stream block 4 for both name and buffer (DB_PushStreamPos).
        const uint8_t* name = nullptr;
        size_t nameBytes = 0;
        if (!walk.TakeString(4, &name, &nameBytes))
        {
            out->refusal = FFK_REFUSE_NAME_UNTERMINATED;
            return false;
        }

        // Inline buffer: len + 1 bytes in the virtual block (4), and the
        // final byte must be the NUL the engine's loader relies on. The
        // malformed fixture-01 twin removes the last byte; this Take is the
        // stream_truncation refusal its manifest demands.
        const uint8_t* buffer = nullptr;
        if (hasBuffer)
        {
            if (!walk.Take(4, static_cast<size_t>(len) + 1, &buffer))
            {
                out->refusal = FFK_REFUSE_STREAM_TRUNCATION;
                return false;
            }
            if (buffer[len] != 0)
            {
                out->refusal = FFK_REFUSE_BUFFER_UNTERMINATED;
                return false;
            }
        }

        out->rawfileLen = len;
        out->hashName = Fnv1a64(name, nameBytes);
        uint8_t lenLE[4] = {
            static_cast<uint8_t>(len),
            static_cast<uint8_t>(len >> 8),
            static_cast<uint8_t>(len >> 16),
            static_cast<uint8_t>(len >> 24),
        };
        out->hashLenField = Fnv1a64(lenLE, 4);
        out->hashBuffer = hasBuffer
            ? Fnv1a64(buffer, static_cast<size_t>(len) + 1)
            : 0;
    }

    if (walk.cursor != walk.size)
    {
        out->refusal = FFK_REFUSE_STREAM_NOT_CONSUMED;
        return false;
    }
    for (size_t index = 0; index < 9; ++index)
    {
        out->blockUse[index] = walk.blockUse[index];
        if (walk.blockUse[index] != container->blockSizes[index])
        {
            out->refusal = FFK_REFUSE_BLOCK_ACCOUNTING;
            return false;
        }
    }
    out->refusal = FFK_OK;
    return true;
}

const char* FFK_RefusalName(const uint32_t refusal)
{
    switch (refusal)
    {
    case FFK_OK: return "ok";
    case FFK_REFUSE_TOO_SHORT: return "too_short";
    case FFK_REFUSE_BAD_MAGIC: return "bad_magic";
    case FFK_REFUSE_BAD_VERSION: return "bad_version";
    case FFK_REFUSE_ZLIB_INIT: return "zlib_init";
    case FFK_REFUSE_ZLIB_DATA: return "zlib_data";
    case FFK_REFUSE_ZLIB_TRUNCATED: return "zlib_truncated";
    case FFK_REFUSE_TRAILING_BYTES: return "trailing_bytes";
    case FFK_REFUSE_PAYLOAD_SHORT: return "payload_short";
    case FFK_REFUSE_PAYLOAD_LIMIT: return "payload_limit";
    case FFK_REFUSE_UNSUPPORTED_SCRIPT_STRINGS: return "unsupported_script_strings";
    case FFK_REFUSE_ASSET_ARRAY_NOT_INLINE: return "asset_array_not_inline";
    case FFK_REFUSE_UNSUPPORTED_ASSET_TYPE: return "unsupported_asset_type";
    case FFK_REFUSE_ASSET_HEADER_NOT_INLINE: return "asset_header_not_inline";
    case FFK_REFUSE_NAME_NOT_INLINE: return "name_not_inline";
    case FFK_REFUSE_BUFFER_NOT_INLINE: return "buffer_not_inline";
    case FFK_REFUSE_STREAM_TRUNCATION: return "stream_truncation";
    case FFK_REFUSE_NAME_UNTERMINATED: return "name_unterminated";
    case FFK_REFUSE_BUFFER_UNTERMINATED: return "buffer_unterminated";
    case FFK_REFUSE_STREAM_NOT_CONSUMED: return "stream_not_consumed";
    case FFK_REFUSE_BLOCK_ACCOUNTING: return "block_accounting";
    default: return "unknown";
    }
}
