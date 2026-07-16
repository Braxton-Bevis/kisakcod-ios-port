// BMK4 slice 7 — fastfile translation kernel, stages K0 + K1 + K2. See
// ff_kernel.h.
//
// K0 mirrors tools/ff_oracle/ff_oracle.cpp acceptance rules and hash domains
// exactly (same magic/version checks, same chunked inflate with truncation
// and trailing-byte detection, same FNV-1a64 offsets). Divergence between
// this file and the oracle IS the bug the round-trip gate exists to catch.
//
// K1/K2 are the deliberate 32-bit wire walks. All wire fields are read as
// explicit little-endian u16/u32 from byte offsets — never by casting wire
// bytes to native structs (LP64 rule: pointers here are 8 bytes; on the wire
// they are 4). The FFKStream context reproduces the engine's stream model
// (trace: tools/zone_fixtures/ENGINE_TRACE_02.md): one PHYSICAL cursor into
// the decompressed image plus NINE ALIGNED LOGICAL block cursors.
// DB_AllocStreamPos (src/database/db_stream.cpp:81-86) is a logical-only
// aligned advance — never a file byte; DB_PushStreamPos/DB_PopStreamPos
// (db_stream.cpp:27-37, 67-74) stack the active block, and popping FROM
// block 0 REWINDS its cursor to the push-time value (block 0 is the TEMP
// block; its XFile size is a HIGH-WATER mark). Final accounting compares
// per-block high-water marks against the XFile block-size table.

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
// EXPLICIT KERNEL POLICY (documented divergence from the oracle's 2 GiB
// bound): the iOS boot lane caps decompressed payloads at 64 MiB, enforced
// exactly at the boundary. Real zones re-derive their budget from the XFile
// header in a later wave; until then anything larger fails closed here.
constexpr size_t kMaximumPayloadSize = 64ull * 1024 * 1024;
constexpr uint32_t kExpectedVersion = 5;
constexpr uint32_t kInlineToken = 0xffffffffu; // -1: payload follows in stream
constexpr uint32_t kAssetTypeXAnimParts = 2;   // IW3 ASSET_TYPE_XANIMPARTS
constexpr uint32_t kAssetTypeRawFile = 31;     // IW3 ASSET_TYPE_RAWFILE
constexpr uint32_t kAssetTypeStringTable = 32; // IW3 ASSET_TYPE_STRINGTABLE
constexpr uint32_t kAssetTypeTableSize = 43;   // IW3 XAssetType count (xanim.h:905-947)

// Single-threaded boot-stage state: the decompressed image K1 walks, plus a
// generation stamp binding each successful FFKContainer to the payload it
// describes (a stale container may not walk a newer payload).
uint8_t* s_payload;
size_t s_payloadSize;
uint32_t s_generation;

void ReleasePayload()
{
    free(s_payload);
    s_payload = nullptr;
    s_payloadSize = 0;
}

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

        // Exact boundary: the cap is a policy, not a fuzzy chunk multiple.
        if (produced > kMaximumPayloadSize)
        {
            refusal = FFK_REFUSE_PAYLOAD_LIMIT;
            break;
        }
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

uint16_t ReadU16(const uint8_t* bytes, const size_t offset)
{
    return static_cast<uint16_t>(
        static_cast<uint32_t>(bytes[offset])
        | (static_cast<uint32_t>(bytes[offset + 1]) << 8));
}

// Engine-faithful stream context (ENGINE_TRACE_02.md): the PHYSICAL cursor
// (sequential position in the decompressed image, the only source of file
// bytes) is separate from the NINE LOGICAL block cursors, which alignment
// and zero-fill/delay reservations advance WITHOUT consuming file bytes.
// Push/pop mirrors DB_PushStreamPos/DB_PopStreamPos exactly: an entry saves
// {previous active index, the NEW block's cursor at push time}; popping
// FROM block 0 rewinds its cursor to the saved value (temp allocations are
// discarded and their space reused — db_stream.cpp:67-74). Accounting is
// per-block HIGH-WATER, which for never-rewinding blocks equals the final
// cursor. The walk's fixed call structure bounds the stack depth at 3
// (asset walk -> asset ptr block -> struct-internal block), so kStackDepth
// can never overflow by construction.
struct FFKStream
{
    const uint8_t* payload;
    size_t size;
    size_t file;             // physical cursor (starts after XAssetList)
    uint32_t block[9];       // logical, aligned cursors
    uint32_t high[9];        // per-block high-water marks
    static constexpr uint32_t kStackDepth = 8;
    struct { uint32_t prevIndex; uint32_t pushedPos; } stack[kStackDepth];
    uint32_t depth;
    uint32_t active;

    void Init(const uint8_t* bytes, const size_t byteCount)
    {
        memset(this, 0, sizeof(*this));
        payload = bytes;
        size = byteCount;
        file = kXFileSize + kXAssetListSize;
    }

    void Push(const uint32_t index) // DB_PushStreamPos (db_stream.cpp:27-37)
    {
        stack[depth].prevIndex = active;
        stack[depth].pushedPos = block[index];
        ++depth;
        active = index;
    }

    void Pop() // DB_PopStreamPos (db_stream.cpp:67-74): block 0 rewinds
    {
        --depth;
        if (active == 0)
            block[0] = stack[depth].pushedPos;
        active = stack[depth].prevIndex;
    }

    void BumpHigh()
    {
        if (block[active] > high[active])
            high[active] = block[active];
    }

    // DB_AllocStreamPos (db_stream.cpp:81-86): aligned LOGICAL advance,
    // never a file byte ("mask" form: 3 -> 4-byte, 1 -> 2-byte, 0 -> none).
    void Align(const uint32_t mask)
    {
        block[active] = (block[active] + mask) & ~mask;
        BumpHigh();
    }

    // Load_Stream on a physical block (db_stream_load.cpp:29-33): file
    // bytes plus a logical advance of the active block.
    bool Read(const size_t bytes, const uint8_t** out)
    {
        if (bytes > size - file)
            return false;
        *out = payload + file;
        file += bytes;
        block[active] += static_cast<uint32_t>(bytes);
        BumpHigh();
        return true;
    }

    // Load_XStringCustom (db_stream_load.cpp:59-72): NUL-terminated inline
    // string; length includes the NUL.
    bool ReadString(const uint8_t** out, size_t* outLen)
    {
        const uint8_t* start = payload + file;
        const size_t remaining = size - file;
        const uint8_t* nul = static_cast<const uint8_t*>(memchr(start, 0, remaining));
        if (!nul)
            return false;
        const size_t length = static_cast<size_t>(nul - start) + 1;
        *outLen = length;
        return Read(length, out);
    }
};

// The interning table: payload offsets of every loaded script string, in
// declaration order. The engine interns via SL_GetString and remaps u16
// wire indices THROUGH this list (Load_ScriptStringCustom,
// db_stringtable_load.cpp:3-6).
struct FFKScriptStrings
{
    uint32_t count;
    uint32_t* offsets; // payload offset of each NUL-terminated string
};

// --- typed asset handlers (one per qualified mechanism) -------------------

uint32_t WalkRawFile(FFKStream* stream, const FFKScriptStrings* scripts,
                     FFKZoneK2* out)
{
    (void)scripts;
    // Load_RawFilePtr (db_load.cpp:5658-5687) pushes the TEMP block 0
    // before the -1 test; AllocLoad_FxElemVisStateSample aligns to 4.
    stream->Push(0);
    stream->Align(3);
    const uint8_t* rawfile = nullptr;
    if (!stream->Read(12, &rawfile))
        return FFK_REFUSE_STREAM_TRUNCATION;
    const uint32_t nameToken = ReadU32(rawfile, 0);
    const uint32_t len = ReadU32(rawfile, 4);
    const uint32_t bufferToken = ReadU32(rawfile, 8);
    // name is pointer-typed (Load_XString): -1 = inline-follows. Offset and
    // alias forms are mechanisms 03/04/05 and refuse here until K3.
    if (nameToken != kInlineToken)
        return FFK_REFUSE_NAME_NOT_INLINE;
    // Load_RawFile (db_load.cpp:5643-5656) pushes block 4 for name+buffer.
    stream->Push(4);
    stream->Align(0); // AllocLoad_raw_byte
    const uint8_t* name = nullptr;
    size_t nameBytes = 0;
    if (!stream->ReadString(&name, &nameBytes))
        return FFK_REFUSE_NAME_UNTERMINATED;
    // buffer is NOT a tokened pointer on this path: the engine tests it for
    // TRUTHINESS only (db_load.cpp:5649); nonzero means len+1 inline bytes.
    const bool hasBuffer = bufferToken != 0;
    const uint8_t* buffer = nullptr;
    if (hasBuffer)
    {
        stream->Align(0); // AllocLoad_raw_byte
        if (!stream->Read(static_cast<size_t>(len) + 1, &buffer))
            return FFK_REFUSE_STREAM_TRUNCATION;
        // KERNEL-ADDED validation, stricter than the engine (which reads
        // len+1 bytes without inspecting the terminator) — kept from K1: a
        // non-NUL final byte means the producer and len field disagree.
        if (buffer[len] != 0)
            return FFK_REFUSE_BUFFER_UNTERMINATED;
    }
    stream->Pop(); // -> block 0 (popping FROM 4: no rewind)
    stream->Pop(); // popping FROM block 0: temp struct rewound + discarded

    out->rawfileLen = len;
    out->rawfileBufferPresent = hasBuffer ? 1u : 0u;
    out->hashRawFileName = Fnv1a64(name, nameBytes);
    const uint8_t lenLE[4] = {
        static_cast<uint8_t>(len),
        static_cast<uint8_t>(len >> 8),
        static_cast<uint8_t>(len >> 16),
        static_cast<uint8_t>(len >> 24),
    };
    out->hashRawFileLenField = Fnv1a64(lenLE, 4);
    out->hashRawFileBuffer = hasBuffer
        ? Fnv1a64(buffer, static_cast<size_t>(len) + 1)
        : 0;
    return FFK_OK;
}

uint32_t WalkStringTable(FFKStream* stream, const FFKScriptStrings* scripts,
                         FFKZoneK2* out)
{
    (void)scripts;
    // Load_StringTablePtr (db_load.cpp:5711-5728) pushes NO block: every
    // allocation stays in the ACTIVE block — block 4, pushed once for the
    // whole asset walk (db_file_load.cpp:281). This is the engine truth the
    // fixture-02 regeneration encodes (ENGINE_TRACE_02.md).
    stream->Align(3); // AllocLoad_FxElemVisStateSample
    const uint8_t* table = nullptr;
    if (!stream->Read(16, &table))
        return FFK_REFUSE_STREAM_TRUNCATION;
    const uint32_t nameToken = ReadU32(table, 0);
    const uint32_t columnCount = ReadU32(table, 4);
    const uint32_t rowCount = ReadU32(table, 8);
    const uint32_t valuesToken = ReadU32(table, 12);
    if (nameToken != kInlineToken)
        return FFK_REFUSE_NAME_NOT_INLINE;
    stream->Align(0); // AllocLoad_raw_byte
    const uint8_t* name = nullptr;
    size_t nameBytes = 0;
    if (!stream->ReadString(&name, &nameBytes))
        return FFK_REFUSE_NAME_UNTERMINATED;

    // values is truthiness-tested by the engine (db_load.cpp:5703; there is
    // no -1/offset arm). Cell count is rowCount*columnCount in 64-bit (the
    // engine's 32-bit product can overflow; a fitting product that lies
    // about the payload fails Read below — fail closed either way).
    uint64_t hashValues = 0;
    if (valuesToken != 0)
    {
        const uint64_t cellCount =
            static_cast<uint64_t>(rowCount) * static_cast<uint64_t>(columnCount);
        if (cellCount * 4 > stream->size - stream->file)
            return FFK_REFUSE_STREAM_TRUNCATION;
        stream->Align(3); // AllocLoad_FxElemVisStateSample
        const uint8_t* cellTokens = nullptr;
        if (!stream->Read(static_cast<size_t>(cellCount) * 4, &cellTokens))
            return FFK_REFUSE_STREAM_TRUNCATION;
        // utf8_nul_sequence hash: FNV-1a64 over every cell incl NUL, in order.
        hashValues = 14695981039346656037ull;
        for (uint64_t index = 0; index < cellCount; ++index)
        {
            if (ReadU32(cellTokens, static_cast<size_t>(index) * 4) != kInlineToken)
                return FFK_REFUSE_STRING_NOT_INLINE;
            stream->Align(0); // AllocLoad_raw_byte
            const uint8_t* cell = nullptr;
            size_t cellBytes = 0;
            if (!stream->ReadString(&cell, &cellBytes))
                return FFK_REFUSE_NAME_UNTERMINATED;
            for (size_t at = 0; at < cellBytes; ++at)
            {
                hashValues ^= cell[at];
                hashValues *= 1099511628211ull;
            }
        }
    }

    out->stringTableColumns = columnCount;
    out->stringTableRows = rowCount;
    out->hashStringTableName = Fnv1a64(name, nameBytes);
    out->hashStringTableValues = hashValues;
    return FFK_OK;
}

uint32_t WalkXAnimParts(FFKStream* stream, const FFKScriptStrings* scripts,
                        FFKZoneK2* out)
{
    // Load_XAnimPartsPtr (db_load.cpp:984-1013) pushes the TEMP block 0 at
    // line 990; the 88-byte struct is a temp allocation.
    stream->Push(0);
    stream->Align(3); // AllocLoad_FxElemVisStateSample
    const uint8_t* parts = nullptr;
    if (!stream->Read(88, &parts))
        return FFK_REFUSE_STREAM_TRUNCATION;
    // 32-bit wire offsets per src/xanim/xanim.h:150-181.
    const uint32_t nameToken = ReadU32(parts, 0);
    const uint16_t numframes = ReadU16(parts, 14);
    const uint8_t namesCount = parts[27];  // boneCount[9]
    const uint8_t notifyCount = parts[28];
    const uint32_t indexCount = ReadU32(parts, 36);
    const uint32_t namesToken = ReadU32(parts, 48);
    // K2 scope fence: refuse any field that would drive reads of an
    // unimplemented mechanism (Load_XAnimParts db_load.cpp:931-980 and the
    // XAnimIndices union arm selector db_load.cpp:683-700 / K4b). Unknown
    // semantics fail closed.
    const uint32_t unsupportedPointers =
        ReadU32(parts, 52)      // dataByte
        | ReadU32(parts, 56)    // dataShort
        | ReadU32(parts, 60)    // dataInt
        | ReadU32(parts, 64)    // randomDataShort
        | ReadU32(parts, 68)    // randomDataByte
        | ReadU32(parts, 72)    // randomDataInt
        | ReadU32(parts, 76)    // indices union arm
        | ReadU32(parts, 80)    // notify
        | ReadU32(parts, 84);   // deltaPart
    if (unsupportedPointers != 0 || indexCount != 0 || notifyCount != 0
        || numframes >= 0x100u)
        return FFK_REFUSE_UNSUPPORTED_ASSET_FIELD;
    if (nameToken != kInlineToken)
        return FFK_REFUSE_NAME_NOT_INLINE;
    // Load_XAnimParts (db_load.cpp:919-982) pushes block 4 at line 922.
    stream->Push(4);
    stream->Align(0); // AllocLoad_raw_byte
    const uint8_t* name = nullptr;
    size_t nameBytes = 0;
    if (!stream->ReadString(&name, &nameBytes))
        return FFK_REFUSE_NAME_UNTERMINATED;

    // names is truthiness-tested (db_load.cpp:925; no -1/offset arm): any
    // nonzero wire value means boneCount[9] u16 entries follow inline,
    // 2-aligned via AllocLoad_XBlendInfo (db_load.cpp:516-519, 927).
    uint32_t firstIndex = 0;
    uint64_t hashFirstIndex = 0;
    uint64_t hashFirstResolved = 0;
    if (namesToken != 0)
    {
        stream->Align(1);
        const uint8_t* indices = nullptr;
        if (!stream->Read(static_cast<size_t>(namesCount) * 2, &indices))
            return FFK_REFUSE_STREAM_TRUNCATION;
        for (uint32_t index = 0; index < namesCount; ++index)
        {
            // The engine's remap (Load_ScriptStringCustom,
            // db_stringtable_load.cpp:3-6) indexes stringList.strings with
            // NO bounds check; the kernel bounds-checks (KERNEL-ADDED).
            const uint16_t wireIndex = ReadU16(indices, static_cast<size_t>(index) * 2);
            if (wireIndex >= scripts->count)
                return FFK_REFUSE_SCRIPT_STRING_INDEX_RANGE;
            if (index == 0)
            {
                firstIndex = wireIndex;
                const uint8_t indexLE[2] = {
                    static_cast<uint8_t>(wireIndex),
                    static_cast<uint8_t>(wireIndex >> 8),
                };
                hashFirstIndex = Fnv1a64(indexLE, 2);
                const uint8_t* resolved = stream->payload + scripts->offsets[wireIndex];
                const size_t resolvedBytes =
                    strlen(reinterpret_cast<const char*>(resolved)) + 1;
                hashFirstResolved = Fnv1a64(resolved, resolvedBytes);
            }
        }
        out->xanimNamesCount = namesCount;
        out->xanimFirstSourceIndex = firstIndex;
        out->hashXAnimFirstSourceIndex = hashFirstIndex;
        out->hashXAnimFirstResolved = hashFirstResolved;
    }
    stream->Pop(); // -> block 0 (popping FROM 4: no rewind)
    stream->Pop(); // popping FROM block 0: temp struct rewound + discarded
    return FFK_OK;
}

// --- type-indexed dispatch table ------------------------------------------
// Indexed by the IW3 XAssetType wire value (src/xanim/xanim.h:905-947,
// dispatch mirror of Load_XAssetHeader db_load.cpp:6746-6852). A null entry
// is an unimplemented mechanism and refuses. Handlers are added per wave;
// existing entries never change semantics.
typedef uint32_t (*FFKAssetHandler)(FFKStream*, const FFKScriptStrings*, FFKZoneK2*);

struct FFKDispatchTable
{
    FFKAssetHandler entries[kAssetTypeTableSize];
    constexpr FFKDispatchTable() : entries{}
    {
        entries[kAssetTypeXAnimParts] = WalkXAnimParts;   // Load_XAnimPartsPtr
        entries[kAssetTypeRawFile] = WalkRawFile;         // Load_RawFilePtr
        entries[kAssetTypeStringTable] = WalkStringTable; // Load_StringTablePtr
    }
};
constexpr FFKDispatchTable kDispatch;

uint32_t* TallyFor(const uint32_t type, FFKZoneK2* out)
{
    switch (type)
    {
    case kAssetTypeXAnimParts: return &out->xanimPartsCount;
    case kAssetTypeRawFile: return &out->rawFileCount;
    case kAssetTypeStringTable: return &out->stringTableCount;
    default: return nullptr;
    }
}

// Shared K2 walk. `allowedTypes` is a bitmask over XAssetType wire values —
// FFK_WalkRawFileZone passes RAWFILE-only so its frozen K1 refusal surface
// (unsupported_asset_type for anything else) is preserved exactly.
uint32_t WalkZoneInternal(const FFKContainer* container, FFKZoneK2* out,
                          const uint64_t allowedTypes)
{
    FFKStream stream;
    stream.Init(s_payload, s_payloadSize);

    // --- script-string list (Load_XAssetListCustom db_file_load.cpp:305-314
    // -> Load_ScriptStringList db_load.cpp:641-652, active block 4).
    FFKScriptStrings scripts = { 0, nullptr };
    const uint32_t scriptCount = container->scriptStringCount;
    const uint32_t scriptToken = container->scriptStringsToken;
    uint64_t hashScripts = 14695981039346656037ull; // canonical empty hash
    if (scriptCount == 0)
    {
        // K2 scope: a zero count with a live pointer token is an untested
        // combination (the engine would align-allocate and read nothing).
        if (scriptToken != 0)
            return FFK_REFUSE_STRING_NOT_INLINE;
    }
    else
    {
        if (scriptToken != kInlineToken)
            return FFK_REFUSE_STRING_NOT_INLINE;
        // KERNEL-ADDED, BEFORE ANY ALLOCATION (the manifest's bad_count
        // contract): the pointer array must fit the remaining payload.
        // 64-bit math — the engine's signed 4*count is undefined here
        // (ENGINE_TRACE_02.md, malformed twin section).
        if (static_cast<uint64_t>(scriptCount) * 4 > stream.size - stream.file)
            return FFK_REFUSE_BAD_COUNT;
        stream.Push(4);
        stream.Align(3); // AllocLoad_FxElemVisStateSample
        const uint8_t* pointerArray = nullptr;
        if (!stream.Read(static_cast<size_t>(scriptCount) * 4, &pointerArray))
            return FFK_REFUSE_STREAM_TRUNCATION;
        scripts.offsets = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * scriptCount));
        if (!scripts.offsets)
            return FFK_REFUSE_PAYLOAD_LIMIT;
        for (uint32_t index = 0; index < scriptCount; ++index)
        {
            // Each entry must be -1 (inline-follows). Null and offset forms
            // are engine-reachable but untested mechanisms: fail closed.
            if (ReadU32(pointerArray, static_cast<size_t>(index) * 4) != kInlineToken)
            {
                free(scripts.offsets);
                return FFK_REFUSE_STRING_NOT_INLINE;
            }
            stream.Align(0); // AllocLoad_raw_byte
            const uint8_t* text = nullptr;
            size_t textBytes = 0;
            const size_t offset = stream.file;
            if (!stream.ReadString(&text, &textBytes))
            {
                free(scripts.offsets);
                return FFK_REFUSE_NAME_UNTERMINATED;
            }
            scripts.offsets[index] = static_cast<uint32_t>(offset);
            for (size_t at = 0; at < textBytes; ++at)
            {
                hashScripts ^= text[at];
                hashScripts *= 1099511628211ull;
            }
        }
        stream.Pop();
        scripts.count = scriptCount;
    }
    out->scriptStringCount = scripts.count;
    out->hashScriptStrings = hashScripts;

    // --- asset array (db_file_load.cpp:281-288: one block-4 push for the
    // whole walk; the array is 4-aligned then read whole, then each entry
    // is dispatched with no further stream effect from the entry itself).
    uint32_t refusal = FFK_OK;
    if (container->assetCount < 1)
        refusal = FFK_REFUSE_UNSUPPORTED_ASSET_COUNT;
    else if (container->assetsToken != kInlineToken)
        refusal = FFK_REFUSE_ASSET_ARRAY_NOT_INLINE;
    if (refusal != FFK_OK)
    {
        free(scripts.offsets);
        return refusal;
    }
    stream.Push(4);
    stream.Align(3); // AllocLoad_FxElemVisStateSample
    const uint8_t* assetArray = nullptr;
    if (!stream.Read(static_cast<size_t>(container->assetCount) * 8, &assetArray))
    {
        free(scripts.offsets);
        return FFK_REFUSE_STREAM_TRUNCATION;
    }
    for (uint32_t index = 0; index < container->assetCount && refusal == FFK_OK; ++index)
    {
        const uint32_t type = ReadU32(assetArray, static_cast<size_t>(index) * 8);
        const uint32_t headerToken = ReadU32(assetArray, static_cast<size_t>(index) * 8 + 4);
        const FFKAssetHandler handler =
            (type < kAssetTypeTableSize && ((allowedTypes >> type) & 1))
                ? kDispatch.entries[type]
                : nullptr;
        if (!handler)
        {
            refusal = FFK_REFUSE_UNSUPPORTED_ASSET_TYPE;
            break;
        }
        if (headerToken != kInlineToken)
        {
            // -2 insertion slots (mechanism 03) and offset tokens (04/05)
            // are K3 scope and refuse here.
            refusal = FFK_REFUSE_ASSET_HEADER_NOT_INLINE;
            break;
        }
        uint32_t* tally = TallyFor(type, out);
        if (!tally) // unreachable while table and tallies cover the same types
        {
            refusal = FFK_REFUSE_UNSUPPORTED_ASSET_TYPE;
            break;
        }
        if (++*tally > 1)
        {
            // K2 scope: the scalar per-type result fields can only honestly
            // represent one asset of each type.
            refusal = FFK_REFUSE_UNSUPPORTED_ASSET_COUNT;
            break;
        }
        refusal = handler(&stream, &scripts, out);
    }
    free(scripts.offsets);
    if (refusal != FFK_OK)
        return refusal;
    stream.Pop();

    if (stream.file != stream.size)
        return FFK_REFUSE_STREAM_NOT_CONSUMED;
    for (size_t index = 0; index < 9; ++index)
    {
        out->blockUse[index] = stream.high[index];
        if (stream.high[index] != container->blockSizes[index])
            return FFK_REFUSE_BLOCK_ACCOUNTING;
    }
    return FFK_OK;
}

bool ContainerIsStale(const FFKContainer* container)
{
    return !s_payload || container->refusal != FFK_OK
        || container->generation != s_generation;
}
} // namespace

bool FFK_LoadContainer(const uint8_t* bytes, const size_t size, FFKContainer* out)
{
    memset(out, 0, sizeof(*out));
    // Every load attempt — including one that will be refused — invalidates
    // the previous payload, so no failure path can leave stale bytes exposed.
    ReleasePayload();
    ++s_generation;

    if (size < kOuterHeaderSize + 1)
    {
        out->refusal = FFK_REFUSE_TOO_SHORT;
        return false;
    }
    // The IW3 zlib lane is 32-bit; the oracle rejects larger inputs
    // explicitly and so must we (never narrow silently into uInt).
    if (size - kOuterHeaderSize > 0xffffffffull)
    {
        out->refusal = FFK_REFUSE_INPUT_TOO_LARGE;
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
        ReleasePayload();
        out->refusal = inflateRefusal;
        return false;
    }
    if (s_payloadSize < kMinimumPayloadSize)
    {
        ReleasePayload();
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
    out->generation = s_generation;
    out->refusal = FFK_OK;
    return true;
}

const uint8_t* FFK_PayloadBytes() { return s_payload; }
size_t FFK_PayloadSize() { return s_payloadSize; }

bool FFK_WalkRawFileZone(const FFKContainer* container, FFKRawFileK1* out)
{
    memset(out, 0, sizeof(*out));

    // K1 FROZEN behavior: the scope fences below and every downstream
    // refusal code are unchanged from the original K1 walker; only the
    // stream engine underneath was replaced by the K2 context (whose
    // high-water accounting equals the old per-block sums on every
    // K1-admissible zone — a single temp allocation rewound once).
    //
    // The container must describe the CURRENTLY loaded payload — a result
    // from an earlier (or refused) load may not walk newer bytes.
    if (ContainerIsStale(container))
    {
        out->refusal = FFK_REFUSE_STALE_CONTAINER;
        return false;
    }
    // K1 scope fence: exactly ONE inline RawFile, no script strings. The
    // scalar result type can only honestly represent one asset; zero or
    // many are later mechanisms and must fail closed here.
    if (container->assetCount != 1)
    {
        out->refusal = FFK_REFUSE_UNSUPPORTED_ASSET_COUNT;
        return false;
    }
    if (container->scriptStringCount != 0 || container->scriptStringsToken != 0)
    {
        out->refusal = FFK_REFUSE_UNSUPPORTED_SCRIPT_STRINGS;
        return false;
    }

    // RAWFILE-only dispatch mask preserves the K1 refusal surface: any
    // other asset type — including ones K2 can walk — refuses
    // unsupported_asset_type exactly as the K1 walker did.
    FFKZoneK2 zone;
    memset(&zone, 0, sizeof(zone));
    const uint32_t refusal =
        WalkZoneInternal(container, &zone, 1ull << kAssetTypeRawFile);
    // blockUse is diagnostic and copied even on refusal (the K1 walker
    // exposed the partially-verified table on block_accounting).
    memcpy(out->blockUse, zone.blockUse, sizeof(out->blockUse));
    if (refusal != FFK_OK)
    {
        out->refusal = refusal;
        return false;
    }
    out->rawfileLen = zone.rawfileLen;
    out->bufferPresent = zone.rawfileBufferPresent;
    out->hashName = zone.hashRawFileName;
    out->hashLenField = zone.hashRawFileLenField;
    out->hashBuffer = zone.hashRawFileBuffer;
    out->refusal = FFK_OK;
    return true;
}

bool FFK_WalkZone(const FFKContainer* container, FFKZoneK2* out)
{
    memset(out, 0, sizeof(*out));
    if (ContainerIsStale(container))
    {
        out->refusal = FFK_REFUSE_STALE_CONTAINER;
        return false;
    }
    const uint64_t allowedTypes = (1ull << kAssetTypeXAnimParts)
        | (1ull << kAssetTypeRawFile)
        | (1ull << kAssetTypeStringTable);
    const uint32_t refusal = WalkZoneInternal(container, out, allowedTypes);
    out->refusal = refusal;
    return refusal == FFK_OK;
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
    case FFK_REFUSE_INPUT_TOO_LARGE: return "input_too_large";
    case FFK_REFUSE_UNSUPPORTED_ASSET_COUNT: return "unsupported_asset_count";
    case FFK_REFUSE_STALE_CONTAINER: return "stale_container";
    case FFK_REFUSE_BAD_COUNT: return "bad_count";
    case FFK_REFUSE_STRING_NOT_INLINE: return "string_not_inline";
    case FFK_REFUSE_SCRIPT_STRING_INDEX_RANGE: return "script_string_index_range";
    case FFK_REFUSE_UNSUPPORTED_ASSET_FIELD: return "unsupported_asset_field";
    default: return "unknown";
    }
}
