// bmk4-oracle1 — Oracle 1 driver: loads a zone through the REAL engine
// loader TUs (db_file_load / db_load / db_stream / db_stream_load /
// db_stringtable_load / db_memory / db_auth / db_assetnames / db_registry)
// and emits the bmk4.oracle1.v1 event trace.
//
// The driver owns ONLY the setup half of DB_TryLoadXFileInternal
// (db_registry.cpp:2650-2721); every replicated statement cites its source
// line. The load itself is the real DB_LoadXFile + DB_LoadXFileInternal.
//
// Sanitization: CI invocations pass --fixture-allowlist-root and are
// confined to the repo checkout exactly like Oracle 0 (exit 3 refusal for
// input OR output outside the root). Asset-derived strings enter the trace
// only under --emit-names, which is reserved for synthetic fixtures.

#include <database/database.h>

#include "bmk4_oracle1_instr.h"
#include "oracle1_trace.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

// Engine globals defined in db_registry.cpp (types must match exactly).
extern XZone g_zones[ASSET_TYPE_COUNT];
extern uint8_t g_zoneHandles[32];
extern int32_t g_zoneCount;
extern uint32_t g_zoneIndex;
extern volatile bool g_loadingZone;
extern bool g_mayRecoverLostAssets;
extern uint32_t g_zoneAllocType;

// Scaffold utility (matches engine I_strncpyz declaration in q_shared.h).
void I_strncpyz(char *dest, const char *src, int destsize);

namespace
{
struct Arguments
{
    fs::path input;
    fs::path output;
    std::optional<fs::path> fixtureAllowlistRoot;
    std::string zoneName;
    bool emitNames = false;
};

void PrintUsage()
{
    std::fprintf(stderr,
                 "usage: bmk4-oracle1 --input <zone.ff> --output <trace.txt>\n"
                 "                    [--fixture-allowlist-root <repo-root>] [--emit-names]\n"
                 "                    [--zone-name <name>]\n");
}

// --- allowlist enforcement: copied from Oracle 0 (tools/ff_oracle/ff_oracle.cpp) ---

bool IsWithinRoot(const fs::path &candidate, const fs::path &root)
{
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
        return candidate == root;
    for (const auto &component : relative)
    {
        if (component == "..")
            return false;
    }
    return true;
}

bool CanonicalOutputPath(const fs::path &output, fs::path *result)
{
    const fs::path parent = output.has_parent_path() ? output.parent_path() : fs::current_path();
    std::error_code ec;
    if (!fs::exists(parent, ec) || !fs::is_directory(parent, ec))
        return false;
    if (fs::exists(output, ec))
    {
        *result = fs::canonical(output, ec);
        return !ec;
    }
    *result = fs::canonical(parent, ec) / output.filename();
    return !ec;
}

// Returns 0 = allowed, 3 = refused, 2 = tool error.
int EnforceFixtureAllowlist(const Arguments &args)
{
    if (!args.fixtureAllowlistRoot.has_value())
        return 0;

    std::error_code ec;
    const fs::path root = fs::canonical(*args.fixtureAllowlistRoot, ec);
    if (ec || !fs::is_directory(root))
    {
        std::fprintf(stderr, "bmk4-oracle1: fixture allowlist root is not a directory\n");
        return 2;
    }
    const fs::path input = fs::canonical(args.input, ec);
    if (ec)
    {
        std::fprintf(stderr, "bmk4-oracle1: cannot canonicalize input\n");
        return 2;
    }
    fs::path output;
    if (!CanonicalOutputPath(args.output, &output))
    {
        std::fprintf(stderr, "bmk4-oracle1: output parent directory does not exist\n");
        return 2;
    }
    if (!IsWithinRoot(input, root))
    {
        std::fprintf(stderr, "bmk4-oracle1: fixture allowlist refused input outside repo root: %s\n",
                     input.string().c_str());
        return 3;
    }
    if (!IsWithinRoot(output, root))
    {
        std::fprintf(stderr, "bmk4-oracle1: fixture allowlist refused output outside repo root: %s\n",
                     output.string().c_str());
        return 3;
    }
    return 0;
}

bool ParseArguments(const int argc, char **argv, Arguments *args)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string option(argv[index]);
        if (option == "--emit-names")
        {
            args->emitNames = true;
            continue;
        }
        if (index + 1 >= argc)
        {
            std::fprintf(stderr, "bmk4-oracle1: missing value for option %s\n", option.c_str());
            return false;
        }
        const std::string value(argv[++index]);
        if (option == "--input")
            args->input = value;
        else if (option == "--output")
            args->output = value;
        else if (option == "--fixture-allowlist-root")
            args->fixtureAllowlistRoot = fs::path(value);
        else if (option == "--zone-name")
            args->zoneName = value;
        else
        {
            std::fprintf(stderr, "bmk4-oracle1: unknown option %s\n", option.c_str());
            return false;
        }
    }
    if (args->input.empty() || args->output.empty())
    {
        std::fprintf(stderr, "bmk4-oracle1: --input and --output are required\n");
        return false;
    }
    if (args->zoneName.empty())
        args->zoneName = args->input.stem().string();
    return true;
}
} // namespace

int main(const int argc, char **argv)
{
    Arguments args;
    if (!ParseArguments(argc, argv, &args))
    {
        PrintUsage();
        return 2;
    }
    std::error_code ec;
    if (!fs::exists(args.input, ec) || !fs::is_regular_file(args.input, ec))
    {
        std::fprintf(stderr, "bmk4-oracle1: input is not a regular file: %s\n", args.input.string().c_str());
        PrintUsage();
        return 2;
    }
    const int allowlistResult = EnforceFixtureAllowlist(args);
    if (allowlistResult != 0)
        return allowlistResult;

    if (!bmk4or1::TraceOpen(args.output.string().c_str(), args.emitNames))
    {
        std::fprintf(stderr, "bmk4-oracle1: cannot open trace output: %s\n", args.output.string().c_str());
        return 2;
    }
    bmk4or1::EmitZoneOpen(args.zoneName.c_str(),
                          static_cast<unsigned long long>(fs::file_size(args.input, ec)));

    // Registry init exactly as the engine performs it (db_registry.cpp:2242).
    DB_Init();

    // Zone-slot setup replicating DB_TryLoadXFileInternal (db_registry.cpp
    // lines cited per statement). Engine open flags 0x60000000 =
    // FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING (db_registry.cpp:2616).
    const std::string inputPath = args.input.string();
    void *zoneFile = CreateFileA(inputPath.c_str(), 0x80000000, 1u, 0, 3u, 0x60000000u, 0);
    if (zoneFile == reinterpret_cast<void *>(-1))
    {
        std::fprintf(stderr, "bmk4-oracle1: CreateFileA failed for %s\n", inputPath.c_str());
        bmk4or1::TraceClose();
        return 2;
    }

    g_zoneIndex = 1;                                        // :2650-2657 (first free slot)
    XZone *zone = &g_zones[g_zoneIndex];
    memset(zone, 0, sizeof(XZone));                         // :2665
    g_zoneHandles[g_zoneCount] = static_cast<uint8_t>(g_zoneIndex); // :2675
    I_strncpyz(zone->name, args.zoneName.c_str(), 64);      // :2678
    zone->flags = 0;                                        // :2679
    zone->fileSize = static_cast<int>(GetFileSize(zoneFile, 0)); // :2680
    zone->modZone = false;                                  // :2681
    ++g_zoneCount;                                          // :2695
    g_loadingZone = 1;                                      // :2696
    g_mayRecoverLostAssets = 0;                             // :2699
    g_zoneAllocType = 0;                                    // :2700 (DB_GetZoneAllocType(0))
    zone->allocType = 0;                                    // :2711
    g_loadingAssets = 1;                                    // DB_LoadXZone counterpart (:2296)
    DB_ResetZoneSize(0);                                    // :2712

    // Compressed-read ring: the engine passes the static g_fileBuf
    // (db_registry.cpp:2715), which is only 4-byte-guaranteed; the file is
    // opened FILE_FLAG_NO_BUFFERING, whose reads want sector alignment.
    // DOCUMENTED DIVERGENCE (Sol round-1 findings 4/16): the driver passes
    // a VirtualAlloc'd 0x80000 ring instead — page-aligned (sector-safe)
    // and OS-zeroed, which also keeps the over-credited zlib tail
    // deterministic on every process-fresh run. Buffer identity carries no
    // loader semantics; DB_LoadXFile only requires the 0x80000 size
    // contract and 4-byte alignment (db_file_load.cpp:349,364).
    uint8_t *fileBuf = static_cast<uint8_t *>(
        VirtualAlloc(nullptr, 0x80000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!fileBuf)
    {
        std::fprintf(stderr, "bmk4-oracle1: VirtualAlloc failed for the read ring\n");
        bmk4or1::TraceClose();
        return 2;
    }

    // The REAL load (db_registry.cpp:2715-2716).
    DB_LoadXFile(inputPath.c_str(), zoneFile, zone->name, &zone->mem, 0, fileBuf, 0);
    DB_LoadXFileInternal();

    g_loadingZone = 0;                                      // :2720
    g_mayRecoverLostAssets = 1;                             // :2721

    bmk4or1::EmitZoneLoaded(args.zoneName.c_str());
    bmk4or1::TraceClose();
    return 0;
}
