// BMK4 Oracle 1 trace writer + engine hook implementations — tool-owned.
//
// Reads the REAL db_stream globals to attribute events to (block, offset)
// pairs. Never emits a raw pointer value: traces must be byte-identical
// across runs (ASLR-independent) and across machines.

#include "oracle1_trace.h"
#include "bmk4_oracle1_instr.h"

#include <database/database.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
std::FILE *s_file;
bool s_emitNames;
unsigned int s_assetIndex;

// Resolve a pointer into the zone's blocks. Containment is tested against
// the block START and declared size; a cursor that walked PAST a declared
// size (over-model walk, e.g. fixture 02) still resolves by start pointer
// because the event of interest begins inside the block.
bool ResolveBlock(const void *ptr, unsigned int *blockOut, unsigned int *offsetOut)
{
    if (!g_streamZoneMem)
        return false;
    const unsigned char *p = static_cast<const unsigned char *>(ptr);
    for (unsigned int i = 0; i < 9; ++i)
    {
        const XBlock &b = g_streamZoneMem->blocks[i];
        if (b.data && b.size && p >= b.data && p <= b.data + b.size)
        {
            *blockOut = i;
            *offsetOut = static_cast<unsigned int>(p - b.data);
            return true;
        }
    }
    return false;
}
} // namespace

namespace bmk4or1
{
bool TraceOpen(const char *path, bool emitNames)
{
    s_file = std::fopen(path, "wb");
    if (!s_file)
        return false;
    s_emitNames = emitNames;
    s_assetIndex = 0;
    TraceLine("schema=bmk4.oracle1.v1");
    return true;
}

void TraceClose()
{
    if (s_file)
    {
        std::fflush(s_file);
        std::fclose(s_file);
        s_file = nullptr;
    }
}

bool TraceEmitNames()
{
    return s_emitNames;
}

void TraceLine(const char *fmt, ...)
{
    if (!s_file)
        return;
    char line[2048];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    std::fputs(line, s_file);
    std::fputc('\n', s_file);
    std::fflush(s_file); // refusal exits must keep the prefix
}

std::uint64_t Fnv1a64(const void *bytes, std::size_t size)
{
    const unsigned char *p = static_cast<const unsigned char *>(bytes);
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= p[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

void EmitZoneOpen(const char *basename, unsigned long long fileBytes)
{
    TraceLine("ev=zone_open name=%s bytes=%llu", basename, fileBytes);
}

void EmitZoneLoaded(const char *basename)
{
    TraceLine("ev=zone_loaded name=%s", basename);
}
} // namespace bmk4or1

using bmk4or1::TraceLine;

// ---------------------------------------------------------------------------
// Engine hooks (called from #ifdef BMK4_ORACLE1 blocks in database TUs).
// ---------------------------------------------------------------------------

void Bmk4Or1_StreamPush(unsigned int requestedIndex)
{
    TraceLine("ev=stream_push index=%u depth=%u", requestedIndex, g_streamPosStackIndex);
}

void Bmk4Or1_StreamPop()
{
    TraceLine("ev=stream_pop index=%u depth=%u", g_streamPosIndex, g_streamPosStackIndex);
}

void Bmk4Or1_Alloc(int alignment)
{
    unsigned int block = g_streamPosIndex;
    unsigned int offset = 0;
    if (g_streamZoneMem && g_streamZoneMem->blocks[block].data)
        offset = static_cast<unsigned int>(g_streamPos - g_streamZoneMem->blocks[block].data);
    TraceLine("ev=alloc block=%u align=%d offset=%u", block, alignment, offset);
}

void Bmk4Or1_Inc(int size)
{
    unsigned int block = g_streamPosIndex;
    unsigned int offset = 0;
    if (g_streamZoneMem && g_streamZoneMem->blocks[block].data)
        offset = static_cast<unsigned int>(g_streamPos - g_streamZoneMem->blocks[block].data);
    TraceLine("ev=inc block=%u offset=%u size=%d", block, offset, size);
}

void Bmk4Or1_InsertPointer(const void **slot)
{
    unsigned int block = 0;
    unsigned int offset = 0;
    if (ResolveBlock(slot, &block, &offset))
        TraceLine("ev=alias_insert block=%u offset=%u", block, offset);
    else
        TraceLine("ev=alias_insert block=? offset=?");
}

void Bmk4Or1_Fill(const unsigned char *ptr, int size)
{
    const char *src = "file";
    if (g_streamPosIndex == 1)
        src = "zerofill";
    else if (g_streamPosIndex == 2 || g_streamPosIndex == 3)
        src = "delay_queue";
    unsigned int block = 0;
    unsigned int offset = 0;
    if (ResolveBlock(ptr, &block, &offset))
        TraceLine("ev=fill block=%u offset=%u size=%d src=%s", block, offset, size, src);
    else
        TraceLine("ev=fill block=%u offset=? size=%d src=%s", g_streamPosIndex, size, src);
}

void Bmk4Or1_PtrOffset(unsigned int token)
{
    TraceLine("ev=ptr_offset token=0x%08x block=%u offset=%u",
              token, (token - 1) >> 28, (token - 1) & 0x0FFFFFFF);
}

void Bmk4Or1_PtrAlias(unsigned int token)
{
    TraceLine("ev=ptr_alias token=0x%08x block=%u offset=%u",
              token, (token - 1) >> 28, (token - 1) & 0x0FFFFFFF);
}

void Bmk4Or1_XFileData(const unsigned char *pos, unsigned int size)
{
    unsigned int block = 0;
    unsigned int offset = 0;
    if (ResolveBlock(pos, &block, &offset))
        TraceLine("ev=inflate size=%u dest=block%u+%u", size, block, offset);
    else
        TraceLine("ev=inflate size=%u dest=external", size);
}

void Bmk4Or1_XFileHeader(const void *xfile)
{
    const XFile *f = static_cast<const XFile *>(xfile);
    TraceLine("ev=container magic=IWffu100 version=5");
    TraceLine("ev=xfile size=%u external=%u b0=%u b1=%u b2=%u b3=%u b4=%u b5=%u b6=%u b7=%u b8=%u",
              f->size, f->externalSize,
              f->blockSize[0], f->blockSize[1], f->blockSize[2], f->blockSize[3],
              f->blockSize[4], f->blockSize[5], f->blockSize[6], f->blockSize[7],
              f->blockSize[8]);
}

void Bmk4Or1_AssetList(const void *assetList)
{
    const XAssetList *list = static_cast<const XAssetList *>(assetList);
    TraceLine("ev=assetlist strings=%d strings_token=0x%08x assets=%d assets_token=0x%08x",
              list->stringList.count,
              static_cast<unsigned int>(reinterpret_cast<uintptr_t>(list->stringList.strings)),
              list->assetCount,
              static_cast<unsigned int>(reinterpret_cast<uintptr_t>(list->assets)));
}

void Bmk4Or1_AssetDispatch(int type)
{
    const char *typeName = (type >= 0 && type < 33) ? g_assetNames[type] : "?";
    TraceLine("ev=asset_dispatch index=%u type=%d name=%s", s_assetIndex++, type, typeName);
}

void Bmk4Or1_ScriptStringRemap(unsigned int indexBefore, unsigned int handleAfter)
{
    TraceLine("ev=scriptstring_remap index=%u handle=%u", indexBefore, handleAfter);
}

void Bmk4Or1_AssetInsert(int type, const void *loadedData, const void *linkedAsset)
{
    const XAsset *linked = static_cast<const XAsset *>(linkedAsset);
    const char *name = DB_GetXAssetName(linked);
    const std::uint64_t hash = bmk4or1::Fnv1a64(name, std::strlen(name) + 1); // utf8_nul convention
    const char *outcome = (linked->header.data == loadedData) ? "new" : "existing";
    const char *typeName = (type >= 0 && type < 33) ? g_assetNames[type] : "?";
    if (bmk4or1::TraceEmitNames())
        TraceLine("ev=asset_insert type=%d typename=%s namehash=%016llx outcome=%s name=%s",
                  type, typeName, static_cast<unsigned long long>(hash), outcome, name);
    else
        TraceLine("ev=asset_insert type=%d typename=%s namehash=%016llx outcome=%s",
                  type, typeName, static_cast<unsigned long long>(hash), outcome);
}

void Bmk4Or1_SlIntern(unsigned int handle, const char *text)
{
    const std::uint64_t hash = bmk4or1::Fnv1a64(text, std::strlen(text) + 1);
    if (bmk4or1::TraceEmitNames())
        TraceLine("ev=sl_intern handle=%u hash=%016llx text=%s",
                  handle, static_cast<unsigned long long>(hash), text);
    else
        TraceLine("ev=sl_intern handle=%u hash=%016llx",
                  handle, static_cast<unsigned long long>(hash));
}

void Bmk4Or1_Error(const char *kind, const char *detail)
{
    // One event per line: engine assert/error text may contain newlines.
    char clean[1024];
    std::size_t n = 0;
    for (; detail[n] && n + 1 < sizeof(clean); ++n)
    {
        const char c = detail[n];
        clean[n] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    clean[n] = '\0';
    TraceLine("ev=error kind=%s detail=%s", kind, clean);
}
