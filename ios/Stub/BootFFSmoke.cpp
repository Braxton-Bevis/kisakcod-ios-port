// Slice 7 stage K0+K1: prove the fastfile translation kernel's container
// spine and first 32-bit wire walk against ORACLE-QUALIFIED fixture 01.
//
// Every expected constant below was produced by the Windows Oracle 0
// container inspector (bmk4-ff-oracle.exe, CI run 29354619087 build) run
// against tools/zone_fixtures/01_rawfile_inline on 2026-07-16, matching
// tools/zone_fixtures/01_rawfile_inline/MANIFEST.json exactly. The K1 field
// hashes are the manifest's targeted_fields (encodings re-verified: strings
// include their NUL; len is 4 little-endian bytes).
//
// The marker is formatted ONLY after: (a) K0 accepts the valid fixture with
// every field equal to the oracle's, (b) four container-level corruptions
// derived in memory from the valid bytes are REFUSED with the expected
// codes, (c) K1 round-trips the RawFile fields and block accounting, and
// (d) the malformed twin — which the container layer must ACCEPT, exactly
// as the oracle does — is REFUSED by the K1 walk with stream_truncation,
// the code its MALFORMED_MANIFEST.json demands. Gates must be able to fail.

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

char s_status[256];

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
                                             const char* malformedPath)
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

    free(valid);
    free(malformed);
    if (failure)
        return failure;

    snprintf(s_status, sizeof(s_status),
             "FF kernel K0+K1 OK — fixture01 hashes match oracle, RawFile round trip, "
             "refused %d container + %d stream + %d stale",
             containerRefusals, streamRefusals, staleRefusals);
    return s_status;
}
