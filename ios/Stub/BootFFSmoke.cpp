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
// valid fixtures with every field equal to the manifests', (b) four
// container-level corruptions derived in memory from fixture 01 are REFUSED
// with the expected codes, (c) K1 round-trips the RawFile fields and block
// accounting, (d) fixture 01's malformed twin — which the container layer
// must ACCEPT, exactly as the oracle does — is REFUSED by the wire walk
// with stream_truncation, (e) K2 walks fixture 02 through the typed
// dispatch (script-string interning, StringTable cells, XAnimParts u16
// remap, high-water block accounting), (f) the K1 entry point REFUSES
// fixture 02 (scope fence: gates must be able to fail), and (g) fixture
// 02's malformed twin is REFUSED with bad_count BEFORE allocation, the code
// its MALFORMED_MANIFEST.json demands.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    // --- K1 positive: RawFile round trip + block accounting. ---
    FFKRawFileK1 rawfile;
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
    }

    // --- K0 negatives: in-memory container corruptions must be refused. ---
    int containerRefusals = 0;
    if (!failure)
    {
        FFKContainer probe;
        uint8_t* mutant = static_cast<uint8_t*>(malloc(validSize + 1));
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
        if (containerRefusals != 4)
            failure = Fail("K0 negatives", "a container corruption was not refused");
    }

    // --- K1 negative: the malformed twin. Container must ACCEPT (the
    // oracle does); the wire walk must refuse with stream_truncation. ---
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
    // must refuse bad_count BEFORE any allocation, per its manifest.
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

    free(valid);
    free(malformed);
    free(valid02);
    free(malformed02);
    if (failure)
        return failure;

    snprintf(s_status, sizeof(s_status),
             "FF kernel K0+K1+K2 OK — fixture01+02 hashes match oracle, RawFile round trip, "
             "StringTable + script-string remap, "
             "refused %d container + %d stream + %d stale + %d scope + %d bad_count",
             containerRefusals, streamRefusals, staleRefusals, scopeRefusals,
             badCountRefusals);
    return s_status;
}
