// BMK4 Oracle 1 link scaffold — tool-owned.
//
// Discipline copied from ios/Stub/BootScaffold.cpp: the nine REAL database
// TUs retain references to engine subsystems that are out of scope for the
// oracle. Dependencies actually reached by a fixture zone load get small,
// documented, functional implementations; everything else aborts loudly
// with its own symbol name (exit 7 == scaffold reached) so an accidental
// expansion of the load path can never be silent.
//
// Declarations come from the SAME engine headers the database TUs compile
// against, so any signature drift is a compile error here, and the linker
// verifies each definition against the mangled references of the real
// callers. The census below was produced by a called-minus-defined sweep
// over: db_load, db_stream, db_stream_load, db_stringtable_load,
// db_file_load, db_memory, db_auth, db_assetnames, db_registry. The Debug
// configuration links with /OPT:NOREF, so CI itself proves the census
// complete.

#include <database/database.h>

#include <qcommon/qcommon.h>
#include <universal/q_shared.h>
#include <qcommon/threads.h>
#include <win32/win_net.h>
#include <qcommon/com_bsp.h>
#include <game/g_bsp.h>
#include <universal/physicalmemory.h>
#include <universal/com_files.h>
#include <universal/com_memory.h>
#include <qcommon/cmd.h>
#include <qcommon/mem_track.h>
#include <win32/win_localize.h>
#include <script/scr_stringlist.h>
#include <sound/snd_public.h>
#include <xanim/dobj.h>
#include <universal/profile.h>
#include <stringed/stringed_hooks.h>
#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_buffers.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_water.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_rendercmds.h>
#include <gfx_d3d/rb_shade.h>
#include <gfx_d3d/rb_uploadshaders.h>
#include <gfx_d3d/r_staticmodelcache.h>
#include <cgame/cg_local.h>
#include <bgame/bg_local.h>

#include "bmk4_oracle1_instr.h"
#include "oracle1_trace.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Abort-loud default.
// ---------------------------------------------------------------------------

[[noreturn]] static void Bmk4Or1ScaffoldAbort(const char *symbol)
{
    std::fprintf(stderr, "bmk4-oracle1: scaffold reached out-of-scope engine dependency: %s\n", symbol);
    Bmk4Or1_Error("scaffold", symbol);
    bmk4or1::TraceClose();
    std::exit(7);
}

#define OR1_UNREACHED(symbol) Bmk4Or1ScaffoldAbort(symbol)

// ---------------------------------------------------------------------------
// Refusal channels: the engine's own failure paths, reported then exited.
// These are FUNCTIONAL in the sense that they never return, exactly as the
// loader expects (the engine's handlers dialog/longjmp/abort).
// ---------------------------------------------------------------------------

void MyAssertHandler(const char *filename, int line, int type, const char *fmt, ...)
{
    (void)type;
    char message[1024];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char detail[1280];
    std::snprintf(detail, sizeof(detail), "%s:%d %s", filename, line, message);
    std::fprintf(stderr, "bmk4-oracle1: engine assert: %s\n", detail);
    Bmk4Or1_Error("assert", detail);
    bmk4or1::TraceClose();
    std::exit(4);
}

void QDECL Com_Error(errorParm_t code, const char *fmt, ...)
{
    char message[1024];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char detail[1152];
    std::snprintf(detail, sizeof(detail), "code=%d %s", static_cast<int>(code), message);
    std::fprintf(stderr, "bmk4-oracle1: engine Com_Error: %s\n", detail);
    Bmk4Or1_Error("com_error", detail);
    bmk4or1::TraceClose();
    std::exit(5);
}

void __cdecl Com_ErrorAbort()
{
    Bmk4Or1_Error("com_error", "Com_ErrorAbort");
    bmk4or1::TraceClose();
    std::exit(5);
}

void Sys_Error(const char *error, ...)
{
    char message[1024];
    std::va_list args;
    va_start(args, error);
    std::vsnprintf(message, sizeof(message), error, args);
    va_end(args);
    std::fprintf(stderr, "bmk4-oracle1: engine Sys_Error: %s\n", message);
    Bmk4Or1_Error("com_error", message);
    bmk4or1::TraceClose();
    std::exit(5);
}

// ---------------------------------------------------------------------------
// Console output: formatting only, never enters the trace stream.
// ---------------------------------------------------------------------------

void QDECL Com_Printf(int channel, const char *fmt, ...)
{
    (void)channel;
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
}

void Com_PrintWarning(int channel, const char *fmt, ...)
{
    (void)channel;
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
}

void Com_PrintError(int channel, const char *fmt, ...)
{
    (void)channel;
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
}

char *QDECL va(const char *format, ...)
{
    static char buffers[4][1024];
    static int slot;
    char *out = buffers[slot];
    slot = (slot + 1) & 3;
    std::va_list args;
    va_start(args, format);
    std::vsnprintf(out, sizeof(buffers[0]), format, args);
    va_end(args);
    return out;
}

int Com_sprintf(char *dest, uint32_t size, const char *fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    const int written = std::vsnprintf(dest, size, fmt, args);
    va_end(args);
    return written;
}

// ---------------------------------------------------------------------------
// String utilities (ASCII, behavior-identical for fixture names).
// ---------------------------------------------------------------------------

int I_stricmp(const char *s0, const char *s1)
{
    return _stricmp(s0, s1);
}

int I_strnicmp(const char *s0, const char *s1, int n)
{
    return _strnicmp(s0, s1, static_cast<size_t>(n));
}

int __cdecl I_strncmp(const char *s0, const char *s1, int n)
{
    return std::strncmp(s0, s1, static_cast<size_t>(n));
}

void I_strncpyz(char *dest, const char *src, int destsize)
{
    if (destsize <= 0)
        return;
    std::strncpy(dest, src, static_cast<size_t>(destsize) - 1);
    dest[destsize - 1] = '\0';
}

void I_strncat(char *dest, int size, const char *src)
{
    const int len = static_cast<int>(std::strlen(dest));
    if (len < size)
        I_strncpyz(dest + len, src, size - len);
}

const char *__cdecl I_stristr(const char *s0, const char *substr)
{
    const size_t subLen = std::strlen(substr);
    for (; *s0; ++s0)
    {
        if (_strnicmp(s0, substr, subLen) == 0)
            return s0;
    }
    return subLen == 0 ? s0 : nullptr;
}

// ---------------------------------------------------------------------------
// Threading: the tool is single-threaded BY DESIGN (documented divergence
// from the engine's database worker; the loader walk itself is sequential
// either way). Locks are uncontended; database-thread waits are no-ops.
// ---------------------------------------------------------------------------

bool __cdecl Sys_IsMainThread() { return true; }
bool __cdecl Sys_IsDatabaseThread() { return false; }
bool __cdecl Sys_IsRenderThread() { return false; }

void __cdecl Sys_WaitDatabaseThread() {}
void __cdecl Sys_DatabaseCompleted() {}
void __cdecl Sys_DatabaseCompleted2() {}
bool __cdecl Sys_IsDatabaseReady() { return true; }
bool __cdecl Sys_IsDatabaseReady2() { return true; }
void __cdecl Sys_NotifyDatabase() {}
void __cdecl Sys_WakeDatabase() {}
void __cdecl Sys_WakeDatabase2() {}
void __cdecl Sys_SyncDatabase() {}
void __cdecl Sys_WaitStartDatabase() {}
bool __cdecl Sys_HaveSuspendedDatabaseThread(ThreadOwner) { return false; }
void __cdecl Sys_SuspendDatabaseThread(ThreadOwner) {}
void __cdecl Sys_ResumeDatabaseThread(ThreadOwner) {}
char __cdecl Sys_SpawnDatabaseThread(void(__cdecl *)(uint32_t)) { OR1_UNREACHED("Sys_SpawnDatabaseThread"); }

void Sys_LockWrite(FastCriticalSection *critSect)
{
    // threads_interlock.h semantics, uncontended single-thread case.
    InterlockedIncrement(reinterpret_cast<volatile LONG *>(&critSect->writeCount));
    while (critSect->readCount)
        NET_Sleep(0);
}

void Sys_UnlockWrite(FastCriticalSection *critSect)
{
    InterlockedDecrement(reinterpret_cast<volatile LONG *>(&critSect->writeCount));
}

void Sys_EnterCriticalSection(int) {}
void Sys_LeaveCriticalSection(int) {}

void __cdecl Sys_SetValue(int, void *) { OR1_UNREACHED("Sys_SetValue"); }
void *__cdecl Sys_GetValue(int) { OR1_UNREACHED("Sys_GetValue"); }

void NET_Sleep(int) {}

uint32_t __cdecl Sys_Milliseconds() { return 0; } // deterministic: time never enters a trace

// ---------------------------------------------------------------------------
// Memory: zeroed, page-aligned arena with guard slack so an over-model walk
// is caught by the ENGINE's own DB_IncStreamPos fence instead of a heap AV
// that would truncate the trace. Block sizes handed to DB_AllocXZoneMemory
// remain the zone's declared sizes — the fence itself is untouched.
// ---------------------------------------------------------------------------

uint8_t *__cdecl PMem_Alloc(uint32_t size, uint32_t alignment, uint32_t type, uint32_t allocType)
{
    (void)type;
    (void)allocType;
    (void)alignment; // VirtualAlloc returns 64KiB-aligned reservations >= any engine request (0x1000)
    constexpr uint32_t kGuardSlack = 64 * 1024;
    void *mem = VirtualAlloc(nullptr, size + kGuardSlack, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return static_cast<uint8_t *>(mem); // zero-initialized by the OS: deterministic
}

int __cdecl PMem_GetOverAllocatedSize() { return 0; }
void __cdecl PMem_BeginAlloc(const char *, uint32_t) {}
void __cdecl PMem_EndAlloc(const char *, uint32_t) {}
void __cdecl PMem_Free(const char *, uint32_t) {}

void Z_Free(void *, int) { OR1_UNREACHED("Z_Free"); }
void __cdecl Hunk_AddAsset(XAssetHeader, _DWORD *) { OR1_UNREACHED("Hunk_AddAsset"); }

// ---------------------------------------------------------------------------
// Script-string interning (SL_*): tool-owned deterministic table. The SL
// subsystem is beyond scope; handle VALUES are tool-defined (first-use
// order, 1-based) and must never be read as engine-conforming. What IS
// engine-real is the remap structure executed by Load_ScriptStringCustom /
// Load_TempStringCustom over these handles. Every first-use emits
// ev=sl_intern so gates can bind handle -> string content hash.
// ---------------------------------------------------------------------------

namespace
{
std::vector<std::string> &SlStrings()
{
    static std::vector<std::string> strings{std::string()}; // slot 0 reserved: engine treats 0 as none
    return strings;
}

std::map<std::string, uint32_t> &SlLookup()
{
    static std::map<std::string, uint32_t> lookup;
    return lookup;
}
} // namespace

uint32_t SL_GetString(const char *str, uint32_t user)
{
    (void)user;
    auto &lookup = SlLookup();
    const auto it = lookup.find(str);
    if (it != lookup.end())
        return it->second;
    auto &strings = SlStrings();
    const uint32_t handle = static_cast<uint32_t>(strings.size());
    strings.push_back(str);
    lookup.emplace(str, handle);
    Bmk4Or1_SlIntern(handle, str);
    return handle;
}

const char *SL_ConvertToString(uint32_t stringValue)
{
    auto &strings = SlStrings();
    if (stringValue < strings.size())
        return strings[stringValue].c_str();
    return "";
}

void __cdecl SL_AddUser(uint32_t, uint32_t) {}
void SL_ShutdownSystem(uint32_t) {}
void SL_TransferSystem(uint32_t, uint32_t) {}

// ---------------------------------------------------------------------------
// Telemetry / registration no-ops.
// ---------------------------------------------------------------------------

void track_static_alloc_internal(void *, int, const char *, int) {}
void __cdecl KISAK_NULLSUB() {}
void __cdecl ProfLoad_Begin(const char *) {}
void __cdecl ProfLoad_End() {}
void __cdecl Profile_Guard(int) {}
void __cdecl Profile_Recover(int) {}
void __cdecl Cmd_AddCommandInternal(const char *, void(__cdecl *)(), cmd_function_s *) {}
const char *__cdecl Cmd_Argv(int) { return ""; }

const dvar_s *__cdecl Dvar_RegisterString(const char *dvarName, const char *value, uint16_t flags, const char *description)
{
    (void)dvarName;
    (void)flags;
    (void)description;
    static dvar_s dvar{};
    dvar.current.string = value ? value : "";
    return &dvar;
}

char *__cdecl Win_GetLanguage()
{
    static char language[] = "english";
    return language;
}

char *__cdecl Sys_DefaultInstallPath()
{
    static char path[] = ".";
    return path;
}

// ---------------------------------------------------------------------------
// File-system / config paths not taken by the oracle driver.
// ---------------------------------------------------------------------------

int __cdecl FS_HashFileName(const char *, int) { OR1_UNREACHED("FS_HashFileName"); }
void __cdecl FS_FCloseFile(int) { OR1_UNREACHED("FS_FCloseFile"); }
int __cdecl FS_FOpenTextFileWrite(const char *) { OR1_UNREACHED("FS_FOpenTextFileWrite"); }
int __cdecl FS_FOpenFileAppend(const char *) { OR1_UNREACHED("FS_FOpenFileAppend"); }
uint32_t __cdecl FS_Write(const char *, uint32_t, int) { OR1_UNREACHED("FS_Write"); }

const dvar_s *fs_gameDirVar; // read only on mod-dir paths the driver never takes
const dvar_t *loc_warnings;
const dvar_t *loc_warningsAsErrors;

// IsFastFileLoad() is an inline in q_shared.h reading useFastFile; the
// oracle IS the fastfile load path, so the dvar reads enabled=true.
static dvar_s s_useFastFileDvar = []
{
    dvar_s d{};
    d.current.enabled = true;
    return d;
}();
const dvar_t *useFastFile = &s_useFastFileDvar;

const char *__cdecl Com_GetExtensionSubString(const char *) { OR1_UNREACHED("Com_GetExtensionSubString"); }
void __cdecl Com_SyncThreads() { OR1_UNREACHED("Com_SyncThreads"); }
void __cdecl Com_UnloadWorld() { OR1_UNREACHED("Com_UnloadWorld"); }
void __cdecl CM_Unload() { OR1_UNREACHED("CM_Unload"); }

// ---------------------------------------------------------------------------
// Out-of-scope subsystem tails (sound, dobj, renderer, cgame, bgame):
// abort-loud, never faked.
// ---------------------------------------------------------------------------

void __cdecl SND_SetData(MssSoundCOD4 *, void *) { OR1_UNREACHED("SND_SetData"); }
void __cdecl DB_LoadSounds() { OR1_UNREACHED("DB_LoadSounds"); }
void __cdecl DB_SaveSounds() { OR1_UNREACHED("DB_SaveSounds"); }

DObj_s *__cdecl Com_GetClientDObj(uint32_t, int) { OR1_UNREACHED("Com_GetClientDObj"); }
DObj_s *__cdecl Com_GetServerDObj(uint32_t) { OR1_UNREACHED("Com_GetServerDObj"); }
void __cdecl DObjArchive(DObj_s *) { OR1_UNREACHED("DObjArchive"); }
void __cdecl DObjUnarchive(DObj_s *) { OR1_UNREACHED("DObjUnarchive"); }

void __cdecl R_DelayLoadImage(XAssetHeader) { OR1_UNREACHED("R_DelayLoadImage"); }
bool __cdecl Image_IsProg(GfxImage *) { OR1_UNREACHED("Image_IsProg"); }
void __cdecl RB_UnbindAllImages() { OR1_UNREACHED("RB_UnbindAllImages"); }
void __cdecl Load_Texture(GfxTexture *, GfxImage *) { OR1_UNREACHED("Load_Texture"); }

void *__cdecl R_AllocStaticVertexBuffer(IDirect3DVertexBuffer9 **, int) { OR1_UNREACHED("R_AllocStaticVertexBuffer"); }
void *__cdecl R_AllocStaticIndexBuffer(IDirect3DIndexBuffer9 **, int) { OR1_UNREACHED("R_AllocStaticIndexBuffer"); }
void __cdecl R_FinishStaticVertexBuffer(IDirect3DVertexBuffer9 *) { OR1_UNREACHED("R_FinishStaticVertexBuffer"); }
void __cdecl R_FinishStaticIndexBuffer(IDirect3DIndexBuffer9 *) { OR1_UNREACHED("R_FinishStaticIndexBuffer"); }
void __cdecl R_FreeStaticVertexBuffer(IDirect3DVertexBuffer9 *) { OR1_UNREACHED("R_FreeStaticVertexBuffer"); }
void __cdecl R_FreeStaticIndexBuffer(IDirect3DIndexBuffer9 *) { OR1_UNREACHED("R_FreeStaticIndexBuffer"); }
void __cdecl R_UnlockVertexBuffer(IDirect3DVertexBuffer9 *) { OR1_UNREACHED("R_UnlockVertexBuffer"); }
void __cdecl R_UnlockIndexBuffer(IDirect3DIndexBuffer9 *) { OR1_UNREACHED("R_UnlockIndexBuffer"); }
void __cdecl Load_VertexBuffer(IDirect3DVertexBuffer9 **, uint8_t *, int) { OR1_UNREACHED("Load_VertexBuffer"); }

void __cdecl Load_BuildVertexDecl(MaterialVertexDeclaration **) { OR1_UNREACHED("Load_BuildVertexDecl"); }
void __cdecl Load_CreateMaterialPixelShader(GfxPixelShaderLoadDef *, MaterialPixelShader *) { OR1_UNREACHED("Load_CreateMaterialPixelShader"); }
void __cdecl Load_CreateMaterialVertexShader(GfxVertexShaderLoadDef *, MaterialVertexShader *) { OR1_UNREACHED("Load_CreateMaterialVertexShader"); }
void __cdecl Load_PicmipWater(water_t **) { OR1_UNREACHED("Load_PicmipWater"); }

void __cdecl Material_ClearShaderUploadList() { OR1_UNREACHED("Material_ClearShaderUploadList"); }
void __cdecl Material_DirtySort() { OR1_UNREACHED("Material_DirtySort"); }
void __cdecl Material_DirtyTechniqueSetOverrides() { OR1_UNREACHED("Material_DirtyTechniqueSetOverrides"); }
void __cdecl Material_OriginalRemapTechniqueSet(MaterialTechniqueSet *) { OR1_UNREACHED("Material_OriginalRemapTechniqueSet"); }
void __cdecl Material_OverrideTechniqueSets() { OR1_UNREACHED("Material_OverrideTechniqueSets"); }
void __cdecl Material_UploadShaders(MaterialTechniqueSet *) { OR1_UNREACHED("Material_UploadShaders"); }

void __cdecl RB_ClearPixelShader() { OR1_UNREACHED("RB_ClearPixelShader"); }
void __cdecl RB_ClearVertexDecl() { OR1_UNREACHED("RB_ClearVertexDecl"); }
void __cdecl RB_ClearVertexShader() { OR1_UNREACHED("RB_ClearVertexShader"); }

void __cdecl R_SyncRenderThread() { OR1_UNREACHED("R_SyncRenderThread"); }
void __cdecl R_ReleaseThreadOwnership() { OR1_UNREACHED("R_ReleaseThreadOwnership"); }
void __cdecl R_ShutdownStreams() { OR1_UNREACHED("R_ShutdownStreams"); }
void __cdecl R_UnloadWorld() { OR1_UNREACHED("R_UnloadWorld"); }
void __cdecl R_ClearAllStaticModelCacheRefs() { OR1_UNREACHED("R_ClearAllStaticModelCacheRefs"); }
void __cdecl R_BeginRemoteScreenUpdate() { OR1_UNREACHED("R_BeginRemoteScreenUpdate"); }
void __cdecl R_EndRemoteScreenUpdate() { OR1_UNREACHED("R_EndRemoteScreenUpdate"); }
bool __cdecl R_IsInRemoteScreenUpdate() { OR1_UNREACHED("R_IsInRemoteScreenUpdate"); }
int __cdecl R_PopRemoteScreenUpdate() { OR1_UNREACHED("R_PopRemoteScreenUpdate"); }
void __cdecl R_PushRemoteScreenUpdate(int) { OR1_UNREACHED("R_PushRemoteScreenUpdate"); }

void __cdecl CG_VisionSetMyChanges() { OR1_UNREACHED("CG_VisionSetMyChanges"); }
void __cdecl BG_FillInAllWeaponItems() { OR1_UNREACHED("BG_FillInAllWeaponItems"); }

// ---------------------------------------------------------------------------
// Zeroed storage for engine data globals referenced by DB_XAssetPool
// (clipmap/comworld/gameworld singleton pool targets) — storage only, the
// pool topology using them is real db_registry code.
// ---------------------------------------------------------------------------

clipMap_t cm;
ComWorld comWorld;
GameWorldMp gameWorldMp;
