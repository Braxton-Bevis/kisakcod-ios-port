// Slice 7 stages K0+K1+K2: prove the fastfile translation kernel's
// container spine, RawFile wire walk, and StringTable + script-string
// mechanism against ORACLE-QUALIFIED fixtures 01 and 02.
//
// Fixture-01 constants were produced by the Windows Oracle 0 container
// inspector (bmk4-ff-oracle.exe, CI run 29354619087 build) on 2026-07-16,
// matching tools/zone_fixtures/01_rawfile_inline/MANIFEST.json exactly.
// Fixture-02 constants come from tools/zone_fixtures/
// 02_stringtable_script_remap/MANIFEST.json, ENGINE-QUALIFIED on 2026-07-16
// (static trace with file:line citations in tools/zone_fixtures/
// ENGINE_TRACE_02.md; the fixture was regenerated from that trace after the
// original block attribution was adjudicated a corpus defect). Targeted
// hashes are the manifests' targeted_fields (encodings re-verified: strings
// include their NUL; len is 4 LE bytes; the remap index is 2 LE bytes).
//
// The marker is formatted ONLY after every leg passes: (a) K0 accepts both
// valid fixtures with every field equal to the manifests', (b) SIX
// container-level corruptions derived in memory from fixture 01 are REFUSED
// with the expected codes (magic, version, cut byte, trailing byte, and the
// exact-size reader's declared-size mismatch in BOTH directions), (c) K1
// round-trips the RawFile fields, targeted hashes, AND the public blockUse
// table, (d) fixture 01's malformed twin — whose xfile.size truthfully
// declares its truncated stream, so the container must ACCEPT it — is
// REFUSED by the wire walk with stream_truncation, (e) K2 walks fixture 02
// through the typed dispatch (script-string interning, StringTable cells,
// XAnimParts u16 remap, high-water block accounting), (f) the K1 entry
// point REFUSES fixture 02 (two assets) AND a synthetic one-asset
// StringTable zone (exercising the RAWFILE-only dispatch mask, not just the
// count fence), (g) a synthetic StringTable declaring 2^31 x 2^31 cells is
// REFUSED without crashing (the u64 multiply-wrap regression), (h) fixture
// 02's malformed twin is REFUSED with bad_count, and (i) the header-first
// exact-size reader (frontier ruling P0) ACCEPTS a synthetic zeros-only
// zone declaring mp_killhouse's exact decompressed size — the class the
// old 64 MiB cap refused (docs/REAL_ZONE_EVIDENCE.md; no game data is
// involved). Gates must be able to fail.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <zlib/zlib.h>

#include <ios/ff_kernel.h>

namespace
{
// Oracle-qualified fixture 01 constants (see header comment).
constexpr uint64_t kInputBytes = 134;
constexpr uint64_t kCompressedBytes = 122;
constexpr uint64_t kDecompressedBytes = 111;
constexpr uint32_t kXFileSize = 67;
constexpr uint32_t kXFileExternalSize = 0;
constexpr uint32_t kBlockSizes[9] = {12, 0, 0, 0, 39, 0, 0, 0, 0};
constexpr uint32_t kScriptStringCount = 0;
constexpr uint32_t kScriptStringsToken = 0x00000000u;
constexpr uint32_t kAssetCount = 1;
constexpr uint32_t kAssetsToken = 0xffffffffu;
constexpr uint64_t kHashXFile = 0x82139fcf4ee7711dull;
constexpr uint64_t kHashAssetList = 0xacb786bb777c77a0ull;
constexpr uint64_t kHashPayload = 0xb1e9b4a8c78a80b9ull;
constexpr uint64_t kHashScriptStringMeta = 0xa8c7f832281a39c5ull;
constexpr uint32_t kRawFileLen = 5;
constexpr uint64_t kHashName = 0xfc7845dd3a44c753ull;   // "synthetic/raw_inline.txt\0"
constexpr uint64_t kHashLenField = 0x2d401a55eec16520ull; // 05 00 00 00
constexpr uint64_t kHashBuffer = 0xa9bc8acca21f39b1ull;  // "hello\0"

// Engine-qualified fixture 02 constants (see header comment).
constexpr uint64_t kF2InputBytes = 295;
constexpr uint64_t kF2CompressedBytes = 283;
constexpr uint64_t kF2DecompressedBytes = 272;
constexpr uint32_t kF2XFileSize = 228;
constexpr uint32_t kF2XFileExternalSize = 0;
constexpr uint32_t kF2BlockSizes[9] = {88, 0, 0, 0, 126, 0, 0, 0, 0};
constexpr uint32_t kF2ScriptStringCount = 2;
constexpr uint32_t kF2ScriptStringsToken = 0xffffffffu;
constexpr uint32_t kF2AssetCount = 2;
constexpr uint32_t kF2AssetsToken = 0xffffffffu;
constexpr uint64_t kF2HashXFile = 0xb996ccead3f6d137ull;
constexpr uint64_t kF2HashAssetList = 0x0d2dd29bf8f536ddull;
constexpr uint64_t kF2HashPayload = 0x989e9221d7f07b01ull;
constexpr uint64_t kF2HashScriptStringMeta = 0x5b76ef155f79d963ull;
constexpr uint64_t kF2HashScriptStrings = 0xdbdbd34c08d4b111ull; // "script_zero\0script_one\0"
constexpr uint32_t kF2StringTableColumns = 2;
constexpr uint32_t kF2StringTableRows = 1;
constexpr uint64_t kF2HashStringTableName = 0x1143552c0469f770ull; // "synthetic/remap.csv\0"
constexpr uint64_t kF2HashStringTableValues = 0x8338d73bed2f6627ull; // "key\0value\0"
constexpr uint32_t kF2SourceIndex = 1;
constexpr uint64_t kF2HashSourceIndex = 0x082f2207b4e88cc4ull; // 01 00
constexpr uint64_t kF2HashResolved = 0xd5a577873ce075a7ull;    // "script_one\0"

char s_status[320];

uint8_t* ReadWholeFile(const char* path, size_t* outSize)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return nullptr;
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    if (length <= 0)
    {
        fclose(file);
        return nullptr;
    }
    fseek(file, 0, SEEK_SET);
    uint8_t* bytes = static_cast<uint8_t*>(malloc(static_cast<size_t>(length)));
    if (!bytes || fread(bytes, 1, static_cast<size_t>(length), file)
            != static_cast<size_t>(length))
    {
        free(bytes);
        fclose(file);
        return nullptr;
    }
    fclose(file);
    *outSize = static_cast<size_t>(length);
    return bytes;
}

const char* Fail(const char* stage, const char* detail)
{
    snprintf(s_status, sizeof(s_status), "FF kernel FAIL: %s (%s)", stage, detail);
    return s_status;
}

void WriteU32(uint8_t* bytes, const size_t offset, const uint32_t value)
{
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<uint8_t>(value >> 24);
}

// Deflate a synthetic decompressed image (prefix bytes, then zeroFill zero
// bytes) into a fresh IWffu100 v5 container. Streaming: the zero fill is
// fed from a fixed chunk, so building the killhouse-size probe never
// materializes a 76 MB input buffer. Only used to build in-memory probe
// zones — the compressed byte pattern itself is irrelevant to the probes.
uint8_t* DeflateZone(const uint8_t* prefix, const size_t prefixSize,
                     const uint64_t zeroFill, size_t* outSize)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_SPEED) != Z_OK)
        return nullptr;
    size_t capacity = 256 * 1024;
    uint8_t* out = static_cast<uint8_t*>(malloc(capacity));
    if (!out)
    {
        deflateEnd(&zs);
        return nullptr;
    }
    memcpy(out, "IWffu100", 8);
    WriteU32(out, 8, 5);
    size_t written = 12;
    static uint8_t s_zeros[65536];
    const uint64_t totalInput = prefixSize + zeroFill;
    uint64_t fed = 0;
    int result = Z_OK;
    do
    {
        if (zs.avail_in == 0 && fed < totalInput)
        {
            if (fed < prefixSize)
            {
                zs.next_in = const_cast<Bytef*>(prefix) + fed;
                zs.avail_in = static_cast<uInt>(prefixSize - fed);
            }
            else
            {
                const uint64_t left = totalInput - fed;
                zs.next_in = s_zeros;
                zs.avail_in = static_cast<uInt>(
                    left < sizeof(s_zeros) ? left : sizeof(s_zeros));
            }
            fed += zs.avail_in;
        }
        if (capacity - written < 64 * 1024)
        {
            capacity *= 2;
            uint8_t* grown = static_cast<uint8_t*>(realloc(out, capacity));
            if (!grown)
            {
                free(out);
                deflateEnd(&zs);
                return nullptr;
            }
            out = grown;
        }
        zs.next_out = out + written;
        const uInt window = static_cast<uInt>(capacity - written);
        zs.avail_out = window;
        result = deflate(&zs, fed == totalInput ? Z_FINISH : Z_NO_FLUSH);
        written += window - zs.avail_out;
        if (result == Z_STREAM_ERROR)
        {
            free(out);
            deflateEnd(&zs);
            return nullptr;
        }
    } while (result != Z_STREAM_END);
    deflateEnd(&zs);
    *outSize = written;
    return out;
}
} // namespace

extern "C" const char* kisak_ff_kernel_smoke(const char* validPath,
                                             const char* malformedPath,
                                             const char* valid02Path,
                                             const char* malformed02Path)
{
    size_t validSize = 0;
    uint8_t* valid = ReadWholeFile(validPath, &validSize);
    if (!valid)
        return Fail("fixture load", "valid.ff unreadable from bundle");
    size_t malformedSize = 0;
    uint8_t* malformed = ReadWholeFile(malformedPath, &malformedSize);
    if (!malformed)
    {
        free(valid);
        return Fail("fixture load", "malformed twin unreadable from bundle");
    }
    size_t valid02Size = 0;
    uint8_t* valid02 = ReadWholeFile(valid02Path, &valid02Size);
    size_t malformed02Size = 0;
    uint8_t* malformed02 = ReadWholeFile(malformed02Path, &malformed02Size);
    if (!valid02 || !malformed02)
    {
        free(valid);
        free(malformed);
        free(valid02);
        free(malformed02);
        return Fail("fixture load", "fixture02 pair unreadable from bundle");
    }

    // --- K0 positive: every container field must equal the oracle's. ---
    FFKContainer container;
    const char* failure = nullptr;
    if (!FFK_LoadContainer(valid, validSize, &container))
        failure = Fail("K0 accept", FFK_RefusalName(container.refusal));
    else if (container.inputBytes != kInputBytes
        || container.compressedBytes != kCompressedBytes
        || container.decompressedBytes != kDecompressedBytes)
        failure = Fail("K0 sizes", "container byte counts diverge from oracle");
    else if (container.xfileSize != kXFileSize
        || container.xfileExternalSize != kXFileExternalSize)
        failure = Fail("K0 xfile", "XFile size fields diverge from oracle");
    else if (memcmp(container.blockSizes, kBlockSizes, sizeof(kBlockSizes)) != 0)
        failure = Fail("K0 blocks", "block-size table diverges from oracle");
    else if (container.scriptStringCount != kScriptStringCount
        || container.scriptStringsToken != kScriptStringsToken
        || container.assetCount != kAssetCount
        || container.assetsToken != kAssetsToken)
        failure = Fail("K0 asset list", "XAssetList fields diverge from oracle");
    else if (container.hashXFile != kHashXFile
        || container.hashAssetList != kHashAssetList
        || container.hashPayload != kHashPayload
        || container.hashScriptStringMeta != kHashScriptStringMeta)
        failure = Fail("K0 hashes", "FNV domains diverge from oracle");

    // --- K1 positive: RawFile round trip + block accounting. The PUBLIC
    // blockUse table is bound to the manifest's block sizes directly, so a
    // wrapper that dropped or corrupted the copy could not earn the marker.
    FFKRawFileK1 rawfile;
    uint8_t payloadSnapshot[kDecompressedBytes];
    if (!failure)
    {
        if (!FFK_WalkRawFileZone(&container, &rawfile))
            failure = Fail("K1 walk", FFK_RefusalName(rawfile.refusal));
        else if (rawfile.rawfileLen != kRawFileLen || rawfile.bufferPresent != 1)
            failure = Fail("K1 len", "declared len/presence diverges from manifest");
        else if (rawfile.hashName != kHashName
            || rawfile.hashLenField != kHashLenField
            || rawfile.hashBuffer != kHashBuffer)
            failure = Fail("K1 hashes", "targeted-field hashes diverge from manifest");
        else if (memcmp(rawfile.blockUse, kBlockSizes, sizeof(kBlockSizes)) != 0)
            failure = Fail("K1 accounting", "public blockUse diverges from manifest");

        // Snapshot the decompressed image while fixture 01 owns the payload:
        // the declared-size mismatch probes below re-deflate mutations of it.
        if (!failure)
        {
            if (FFK_PayloadSize() != sizeof(payloadSnapshot))
                failure = Fail("K1 snapshot", "payload size diverges from manifest");
            else
                memcpy(payloadSnapshot, FFK_PayloadBytes(), sizeof(payloadSnapshot));
        }
    }

    // --- K0 negatives: in-memory container corruptions must be refused. ---
    int containerRefusals = 0;
    if (!failure)
    {
        FFKContainer probe;
        uint8_t* mutant = static_cast<uint8_t*>(malloc(validSize + 1));
        if (!mutant)
            return Fail("K0 negatives", "mutant allocation failed");
        memcpy(mutant, valid, validSize);

        mutant[0] ^= 0xff; // not IWffu100
        if (!FFK_LoadContainer(mutant, validSize, &probe)
            && probe.refusal == FFK_REFUSE_BAD_MAGIC)
            ++containerRefusals;
        mutant[0] ^= 0xff;

        mutant[8] ^= 0x01; // version != 5
        if (!FFK_LoadContainer(mutant, validSize, &probe)
            && probe.refusal == FFK_REFUSE_BAD_VERSION)
            ++containerRefusals;
        mutant[8] ^= 0x01;

        // Cut the final byte: zlib stream cannot finish. Depending on the
        // linked zlib's no-progress behavior this reports truncated or a
        // data error — both are fail-closed refusals of the same corruption.
        if (!FFK_LoadContainer(mutant, validSize - 1, &probe)
            && (probe.refusal == FFK_REFUSE_ZLIB_TRUNCATED
                || probe.refusal == FFK_REFUSE_ZLIB_DATA))
            ++containerRefusals;

        mutant[validSize] = 0x00; // one byte past the zlib stream
        if (!FFK_LoadContainer(mutant, validSize + 1, &probe)
            && probe.refusal == FFK_REFUSE_TRAILING_BYTES)
            ++containerRefusals;

        free(mutant);

        // Exact-size reader negatives (frontier ruling P0): a declared
        // xfile.size that lies in EITHER direction must refuse
        // payload_size_mismatch. Rebuilt from the snapshotted image with
        // only the declared-size word changed, then re-deflated.
        uint8_t lying[sizeof(payloadSnapshot)];
        for (int delta = -1; delta <= 1; delta += 2)
        {
            memcpy(lying, payloadSnapshot, sizeof(payloadSnapshot));
            WriteU32(lying, 0, kXFileSize + static_cast<uint32_t>(delta));
            size_t zoneSize = 0;
            uint8_t* zone = DeflateZone(lying, sizeof(lying), 0, &zoneSize);
            if (!zone)
            {
                failure = Fail("K0 size mismatch", "probe deflate failed");
                break;
            }
            if (!FFK_LoadContainer(zone, zoneSize, &probe)
                && probe.refusal == FFK_REFUSE_PAYLOAD_SIZE_MISMATCH)
                ++containerRefusals;
            free(zone);
        }

        if (!failure && containerRefusals != 6)
            failure = Fail("K0 negatives", "a container corruption was not refused");
    }

    // --- K1 negative: the malformed twin. Its xfile.size truthfully
    // declares the truncated stream, so the container must ACCEPT it; the
    // wire walk must refuse with stream_truncation (RawFile.len still
    // demands the removed byte). ---
    int streamRefusals = 0;
    int staleRefusals = 0;
    if (!failure)
    {
        FFKContainer twinContainer;
        FFKRawFileK1 twinWalk;
        if (!FFK_LoadContainer(malformed, malformedSize, &twinContainer))
            failure = Fail("K1 twin container",
                           "container refused what the oracle accepts");
        else if (FFK_WalkRawFileZone(&twinContainer, &twinWalk))
            failure = Fail("K1 twin walk", "truncated RawFile buffer was ACCEPTED");
        else if (twinWalk.refusal != FFK_REFUSE_STREAM_TRUNCATION)
            failure = Fail("K1 twin code", FFK_RefusalName(twinWalk.refusal));
        else
            streamRefusals = 1;

        // Generation binding: the VALID container is now stale (the twin
        // load owns the payload); walking it must be refused, not honored.
        FFKRawFileK1 staleWalk;
        if (!failure)
        {
            if (FFK_WalkRawFileZone(&container, &staleWalk))
                failure = Fail("K1 stale", "stale container walked a newer payload");
            else if (staleWalk.refusal != FFK_REFUSE_STALE_CONTAINER)
                failure = Fail("K1 stale code", FFK_RefusalName(staleWalk.refusal));
            else
                staleRefusals = 1;
        }
    }

    // --- K2 positive: fixture 02 through the typed dispatch walker. Every
    // container field and every walked value must equal the engine-qualified
    // manifest's.
    int scopeRefusals = 0;
    if (!failure)
    {
        FFKContainer c2;
        FFKZoneK2 zone;
        if (!FFK_LoadContainer(valid02, valid02Size, &c2))
            failure = Fail("K2 accept", FFK_RefusalName(c2.refusal));
        else if (c2.inputBytes != kF2InputBytes
            || c2.compressedBytes != kF2CompressedBytes
            || c2.decompressedBytes != kF2DecompressedBytes)
            failure = Fail("K2 sizes", "container byte counts diverge from manifest");
        else if (c2.xfileSize != kF2XFileSize
            || c2.xfileExternalSize != kF2XFileExternalSize)
            failure = Fail("K2 xfile", "XFile size fields diverge from manifest");
        else if (memcmp(c2.blockSizes, kF2BlockSizes, sizeof(kF2BlockSizes)) != 0)
            failure = Fail("K2 blocks", "block-size table diverges from manifest");
        else if (c2.scriptStringCount != kF2ScriptStringCount
            || c2.scriptStringsToken != kF2ScriptStringsToken
            || c2.assetCount != kF2AssetCount
            || c2.assetsToken != kF2AssetsToken)
            failure = Fail("K2 asset list", "XAssetList fields diverge from manifest");
        else if (c2.hashXFile != kF2HashXFile
            || c2.hashAssetList != kF2HashAssetList
            || c2.hashPayload != kF2HashPayload
            || c2.hashScriptStringMeta != kF2HashScriptStringMeta)
            failure = Fail("K2 hashes", "FNV domains diverge from manifest");
        else if (!FFK_WalkZone(&c2, &zone))
            failure = Fail("K2 walk", FFK_RefusalName(zone.refusal));
        else if (zone.scriptStringCount != kF2ScriptStringCount
            || zone.hashScriptStrings != kF2HashScriptStrings)
            failure = Fail("K2 scripts", "interned script strings diverge from manifest");
        else if (zone.stringTableCount != 1 || zone.xanimPartsCount != 1
            || zone.rawFileCount != 0)
            failure = Fail("K2 tallies", "per-type asset counts diverge from manifest");
        else if (zone.stringTableColumns != kF2StringTableColumns
            || zone.stringTableRows != kF2StringTableRows
            || zone.hashStringTableName != kF2HashStringTableName
            || zone.hashStringTableValues != kF2HashStringTableValues)
            failure = Fail("K2 stringtable", "StringTable fields diverge from manifest");
        else if (zone.xanimNamesCount != 1
            || zone.xanimFirstSourceIndex != kF2SourceIndex
            || zone.hashXAnimFirstSourceIndex != kF2HashSourceIndex
            || zone.hashXAnimFirstResolved != kF2HashResolved)
            failure = Fail("K2 remap", "u16 script-string remap diverges from manifest");
        else if (memcmp(zone.blockUse, kF2BlockSizes, sizeof(kF2BlockSizes)) != 0)
            failure = Fail("K2 accounting", "high-water block use diverges from XFile");

        // K1 scope fence on the SAME container: the frozen single-RawFile
        // entry point must refuse a two-asset zone (gates must fail).
        FFKRawFileK1 scopeWalk;
        if (!failure)
        {
            if (FFK_WalkRawFileZone(&c2, &scopeWalk))
                failure = Fail("K2 scope", "K1 walker accepted a two-asset zone");
            else if (scopeWalk.refusal != FFK_REFUSE_UNSUPPORTED_ASSET_COUNT)
                failure = Fail("K2 scope code", FFK_RefusalName(scopeWalk.refusal));
            else
                scopeRefusals = 1;
        }
    }

    // --- K2 negative: the bad-count twin. Container must ACCEPT (the
    // static parser does; oracle_v1_static_parser_may_accept); the walk
    // must refuse bad_count per its manifest (the check-before-allocation
    // ordering is desk-checked and review-verified; the marker binds the
    // refusal code, not the internal ordering).
    int badCountRefusals = 0;
    if (!failure)
    {
        FFKContainer twin2;
        FFKZoneK2 twinZone;
        if (!FFK_LoadContainer(malformed02, malformed02Size, &twin2))
            failure = Fail("K2 twin container",
                           "container refused what the static parser accepts");
        else if (FFK_WalkZone(&twin2, &twinZone))
            failure = Fail("K2 twin walk", "impossible script-string count ACCEPTED");
        else if (twinZone.refusal != FFK_REFUSE_BAD_COUNT)
            failure = Fail("K2 twin code", FFK_RefusalName(twinZone.refusal));
        else
            badCountRefusals = 1;
    }

    // --- Dispatch-mask scope: a synthetic ONE-asset StringTable zone. The
    // K1 entry point must refuse it with unsupported_asset_type — this is
    // the leg that fails if the RAWFILE-only mask is ever widened (the
    // two-asset leg above exits at the count fence and cannot see it).
    if (!failure)
    {
        uint8_t maskPayload[68];
        memset(maskPayload, 0, sizeof(maskPayload));
        WriteU32(maskPayload, 0, 24);          // xfile.size = XAssetList + array
        WriteU32(maskPayload, 52, 1);          // assetCount = 1
        WriteU32(maskPayload, 56, 0xffffffffu); // assets inline
        WriteU32(maskPayload, 60, 32);         // ASSET_TYPE_STRINGTABLE
        WriteU32(maskPayload, 64, 0xffffffffu); // header inline
        size_t zoneSize = 0;
        uint8_t* zone = DeflateZone(maskPayload, sizeof(maskPayload), 0, &zoneSize);
        if (!zone)
            failure = Fail("K1 mask", "probe deflate failed");
        else
        {
            FFKContainer maskContainer;
            FFKRawFileK1 maskWalk;
            if (!FFK_LoadContainer(zone, zoneSize, &maskContainer))
                failure = Fail("K1 mask container", FFK_RefusalName(maskContainer.refusal));
            else if (FFK_WalkRawFileZone(&maskContainer, &maskWalk))
                failure = Fail("K1 mask", "K1 walker accepted a StringTable zone");
            else if (maskWalk.refusal != FFK_REFUSE_UNSUPPORTED_ASSET_TYPE)
                failure = Fail("K1 mask code", FFK_RefusalName(maskWalk.refusal));
            else
                ++scopeRefusals;
            free(zone);
        }
    }

    // --- Overflow regression (Sol round 2, challenge 1): a StringTable
    // declaring 2^31 x 2^31 cells once wrapped the u64 cell-byte product to
    // zero and over-read the token array. The division-form bound must
    // refuse it — and not crash.
    int overflowRefusals = 0;
    if (!failure)
    {
        uint8_t overflowPayload[86];
        memset(overflowPayload, 0, sizeof(overflowPayload));
        WriteU32(overflowPayload, 0, 42);          // xfile.size
        WriteU32(overflowPayload, 52, 1);          // assetCount = 1
        WriteU32(overflowPayload, 56, 0xffffffffu); // assets inline
        WriteU32(overflowPayload, 60, 32);         // ASSET_TYPE_STRINGTABLE
        WriteU32(overflowPayload, 64, 0xffffffffu); // header inline
        WriteU32(overflowPayload, 68, 0xffffffffu); // name inline
        WriteU32(overflowPayload, 72, 0x80000000u); // columnCount
        WriteU32(overflowPayload, 76, 0x80000000u); // rowCount
        WriteU32(overflowPayload, 80, 1);           // values truthy
        overflowPayload[84] = 'x';                  // name "x\0"
        size_t zoneSize = 0;
        uint8_t* zone = DeflateZone(overflowPayload, sizeof(overflowPayload), 0, &zoneSize);
        if (!zone)
            failure = Fail("K2 overflow", "probe deflate failed");
        else
        {
            FFKContainer overflowContainer;
            FFKZoneK2 overflowZone;
            if (!FFK_LoadContainer(zone, zoneSize, &overflowContainer))
                failure = Fail("K2 overflow container",
                               FFK_RefusalName(overflowContainer.refusal));
            else if (FFK_WalkZone(&overflowContainer, &overflowZone))
                failure = Fail("K2 overflow", "2^62-cell StringTable was ACCEPTED");
            else if (overflowZone.refusal != FFK_REFUSE_STREAM_TRUNCATION)
                failure = Fail("K2 overflow code", FFK_RefusalName(overflowZone.refusal));
            else
                overflowRefusals = 1;
            free(zone);
        }
    }

    // --- P0 acceptance (frontier ruling): the exact-size reader must FIT
    // the goal artifact's own zone class. Build a zeros-only zone declaring
    // mp_killhouse's exact decompressed size (docs/REAL_ZONE_EVIDENCE.md:
    // 76,935,387 bytes; xfile.size 76,935,343) — no game data — and require
    // container acceptance with the exact size. The old 64 MiB cap refused
    // this class. The walk must still fail closed on its empty asset list.
    int largeAccepts = 0;
    if (!failure)
    {
        constexpr uint64_t kKillhouseDecompressed = 76935387ull;
        uint8_t bigHeader[44];
        memset(bigHeader, 0, sizeof(bigHeader));
        WriteU32(bigHeader, 0, static_cast<uint32_t>(kKillhouseDecompressed - 44));
        size_t zoneSize = 0;
        uint8_t* zone = DeflateZone(bigHeader, sizeof(bigHeader),
                                    kKillhouseDecompressed - 44, &zoneSize);
        if (!zone)
            failure = Fail("P0 large zone", "probe deflate failed");
        else
        {
            FFKContainer bigContainer;
            FFKZoneK2 bigZone;
            if (!FFK_LoadContainer(zone, zoneSize, &bigContainer))
                failure = Fail("P0 large accept", FFK_RefusalName(bigContainer.refusal));
            else if (bigContainer.decompressedBytes != kKillhouseDecompressed
                || bigContainer.xfileSize != kKillhouseDecompressed - 44)
                failure = Fail("P0 large size", "exact reader size diverges");
            else if (FFK_WalkZone(&bigContainer, &bigZone))
                failure = Fail("P0 large walk", "empty asset list was ACCEPTED");
            else if (bigZone.refusal != FFK_REFUSE_UNSUPPORTED_ASSET_COUNT)
                failure = Fail("P0 large walk code", FFK_RefusalName(bigZone.refusal));
            else
                largeAccepts = 1;
            free(zone);
        }
    }

    free(valid);
    free(malformed);
    free(valid02);
    free(malformed02);
    if (failure)
        return failure;
    if (largeAccepts != 1)
        return Fail("P0 large zone", "killhouse-size acceptance did not run");

    snprintf(s_status, sizeof(s_status),
             "FF kernel K0+K1+K2 OK — fixture01+02 hashes match oracle, exact-size reader "
             "loads a killhouse-size zone, RawFile round trip, StringTable + script-string "
             "remap, refused %d container + %d stream + %d stale + %d scope + %d overflow "
             "+ %d bad_count",
             containerRefusals, streamRefusals, staleRefusals, scopeRefusals,
             overflowRefusals, badCountRefusals);
    return s_status;
}
