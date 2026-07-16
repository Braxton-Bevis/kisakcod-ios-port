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

// Free-text field escaping contract (bmk4.oracle1.v1): bytes outside
// [0-9A-Za-z_./:-] are percent-encoded as %XX (uppercase hex), including
// '%', space, '=', CR/LF and any non-ASCII byte. Free-text values can be
// arbitrary NUL-terminated engine bytes; unescaped they could split
// records or inject fake events (Sol round-1 finding 10).
const char *EscapeField(const char *text, char *out, std::size_t outSize)
{
    static const char hex[] = "0123456789ABCDEF";
    std::size_t o = 0;
    std::size_t i = 0;
    for (; text[i] && o + 7 < outSize; ++i) // reserve room for one escape + %TR marker
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const bool plain = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || c == '_' || c == '.' || c == '/' || c == ':' || c == '-';
        if (plain)
        {
            out[o++] = static_cast<char>(c);
        }
        else
        {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xF];
        }
    }
    if (text[i])
    {
        // Truncation marker: "%TR" is not valid percent-encoding ("%" must
        // be followed by two hex digits), so it is unambiguous (Sol round-2
        // finding 13: silent truncation could collapse distinct values).
        out[o++] = '%';
        out[o++] = 'T';
        out[o++] = 'R';
    }
    out[o] = '\0';
    return out;
}

// Resolve a byte span into the zone's blocks. Containment is HALF-OPEN
// against the declared size and span-aware: the whole [p, p+span) range
// must sit inside [data, data+size) (Sol round-2 finding 4 — end-inclusive
// containment misattributed the first out-of-budget byte of an over-model
// walk to the block instead of external).
bool ResolveBlock(const void *ptr, unsigned int span, unsigned int *blockOut, unsigned int *offsetOut)
{
    if (!g_streamZoneMem)
        return false;
    const unsigned char *p = static_cast<const unsigned char *>(ptr);
    for (unsigned int i = 0; i < 9; ++i)
    {
        const XBlock &b = g_streamZoneMem->blocks[i];
        if (b.data && b.size && p >= b.data && p + (span ? span : 1) <= b.data + b.size)
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
    char escaped[512];
    TraceLine("ev=zone_open name=%s bytes=%llu", EscapeField(basename, escaped, sizeof(escaped)), fileBytes);
}

void EmitZoneLoaded(const char *basename)
{
    char escaped[512];
    TraceLine("ev=zone_loaded name=%s", EscapeField(basename, escaped, sizeof(escaped)));
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
    if (ResolveBlock(slot, 4, &block, &offset))
        TraceLine("ev=alias_insert block=%u offset=%u", block, offset);
    else
        TraceLine("ev=alias_insert block=? offset=?");
}

// NOTE (Sol round-2 finding 3): like `inc`, `fill` is a REQUEST recorded at
// classification time, before the zero-fill/queue/inflate action runs — it
// is committed unless followed by ev=error. See DESIGN.md schema notes.
void Bmk4Or1_Fill(const unsigned char *ptr, int size)
{
    const char *src = "file";
    if (g_streamPosIndex == 1)
        src = "zerofill";
    else if (g_streamPosIndex == 2 || g_streamPosIndex == 3)
        src = "delay_queue";
    unsigned int block = 0;
    unsigned int offset = 0;
    if (ResolveBlock(ptr, static_cast<unsigned int>(size > 0 ? size : 1), &block, &offset))
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
    if (ResolveBlock(pos, size, &block, &offset))
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
    // Event name is asset_link (Sol round-2 finding 10): this is a POST-LINK
    // OBSERVATION at DB_AddXAsset, not proof of a fresh insertion —
    // DB_LinkXAssetEntry has stub-existing / override / delayed-clone
    // branches that all pass through here, and the allowOverride=1 relink
    // (DB_PostLoadXZone) is outside the load walk and unhooked.
    // redirected=1 is the accurate pointer-inequality observation: a fresh
    // insert is pool-cloned (DB_AllocXAssetEntry + DB_CloneXAssetInternal)
    // and Load_*Asset writes the POOL pointer back over the asset-array
    // cell, so linked->header normally differs from the zone-block pointer.
    const int redirected = (linked->header.data != loadedData) ? 1 : 0;
    const char *typeName = (type >= 0 && type < 33) ? g_assetNames[type] : "?";
    if (bmk4or1::TraceEmitNames())
    {
        char escaped[512];
        TraceLine("ev=asset_link type=%d typename=%s namehash=%016llx redirected=%d name=%s",
                  type, typeName, static_cast<unsigned long long>(hash), redirected,
                  EscapeField(name, escaped, sizeof(escaped)));
    }
    else
        TraceLine("ev=asset_link type=%d typename=%s namehash=%016llx redirected=%d",
                  type, typeName, static_cast<unsigned long long>(hash), redirected);
}

void Bmk4Or1_SlIntern(unsigned int handle, const char *text)
{
    const std::uint64_t hash = bmk4or1::Fnv1a64(text, std::strlen(text) + 1);
    if (bmk4or1::TraceEmitNames())
    {
        char escaped[512];
        TraceLine("ev=sl_intern handle=%u hash=%016llx text=%s",
                  handle, static_cast<unsigned long long>(hash),
                  EscapeField(text, escaped, sizeof(escaped)));
    }
    else
        TraceLine("ev=sl_intern handle=%u hash=%016llx",
                  handle, static_cast<unsigned long long>(hash));
}

void Bmk4Or1_Error(const char *kind, const char *detail)
{
    // Escaping keeps one event per line even for multiline engine text.
    char escaped[1536];
    TraceLine("ev=error kind=%s detail=%s", kind, EscapeField(detail, escaped, sizeof(escaped)));
}
