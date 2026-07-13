// Link scaffolding for the staged iOS boot smoke.
//
// The real archive members (com_memory, dvar, cmd, q_shared, com_math,
// huffman, msvc_crt_compat) retain references to engine subsystems that have
// not graduated into this app link. Dependencies used by the boot path get
// small, real implementations below; anything else aborts with its symbol
// name if reached. This makes an accidental expansion of the smoke path loud.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sched.h>

// Minimal ABI-identical dvar layout. Only current.enabled is read by the
// hunk bootstrap globals, but keeping the complete layout prevents a false
// success if the real object reads another field later.
union DvarValue
{
    bool enabled;
    int integer;
    uint32_t unsignedInt;
    float value;
    float vector[4];
    const char *string;
    unsigned char color[4];
};

struct DvarLimits_Enumeration
{
    int stringCount;
    const char **strings;
};

struct DvarLimits_Integer { int min; int max; };
struct DvarLimits_Value { float min; float max; };
struct DvarLimits_Vector { float min; float max; };

union DvarLimits
{
    DvarLimits_Enumeration enumeration;
    DvarLimits_Integer integer;
    DvarLimits_Value value;
    DvarLimits_Vector vector;
};

struct dvar_s
{
    const char *name;
    const char *description;
    unsigned short flags;
    unsigned char type;
    bool modified;
    DvarValue current;
    DvarValue latched;
    DvarValue reset;
    DvarLimits domain;
    bool (*domainFunc)(dvar_s *, DvarValue);
    dvar_s *hashNext;
};

using dvar_t = dvar_s;

static_assert(sizeof(DvarValue) == 16, "arm64 DvarValue ABI drift");
static_assert(sizeof(DvarLimits) == 16, "arm64 DvarLimits ABI drift");
static_assert(sizeof(dvar_s) == 104, "arm64 dvar_s ABI drift");

struct FastCriticalSection
{
    volatile uint32_t readCount;
    volatile uint32_t writeCount;
};

enum FsListBehavior_e : int {};
enum XAssetType : int {};
enum MapProfileTrackedValue : int {};
enum snd_stopsounds_arg_t : int {};
union XAssetHeader { void *data; };
struct parseInfo_t;
struct XAnim_s;
struct XAnimParts;
struct XModelPartsLoad;
struct fileData_s;

[[noreturn]] static void BootScaffoldAbort(const char *symbol)
{
    fprintf(stderr, "boot scaffold reached unexpected dependency: %s\n", symbol);
    abort();
}

#define BOOT_UNREACHED(symbol) { BootScaffoldAbort(symbol); }

// -------------------------------------------------------------------------
// Real-minimal dependencies reached by the staged boot.
// -------------------------------------------------------------------------

void Dvar_AddCommands() {}
bool Sys_IsRenderThread() { return false; }

// Pure-server IWD checksum state (multiplayer anticheat). com_files.cpp's
// only boot-lane reference is the FS_Restart error-retry path, which calls
// it with empty strings immediately before Com_Error aborts the boot.
// Reaching it in the headless no-assets boot therefore means boot has
// already failed — abort loud rather than pretend the pure state changed.
void __cdecl FS_PureServerSetLoadedIwds(char *iwdSums, char *iwdNames)
{
    (void)iwdSums;
    (void)iwdNames;
    BootScaffoldAbort("FS_PureServerSetLoadedIwds(boot-failure retry path)");
}

// Map-load profiling is observational only. Real file I/O return values and
// byte contents, not these counters, earn the Wave 1 marker.
void ProfLoad_Begin(const char *) {}
void ProfLoad_End() {}
void ProfLoad_BeginTrackedValue(MapProfileTrackedValue) {}
void ProfLoad_EndTrackedValue(MapProfileTrackedValue) {}

void NET_Sleep(int msec)
{
    if (msec <= 0) {
        sched_yield();
        return;
    }
    timespec delay = { msec / 1000, (msec % 1000) * 1000000L };
    nanosleep(&delay, nullptr);
}

struct BootString
{
    char *value;
    uint32_t refs;
};

static pthread_mutex_t s_stringLock = PTHREAD_MUTEX_INITIALIZER;
static BootString s_strings[128];

uint32_t SL_GetString_(const char *str, uint32_t, int)
{
    if (!str)
        BootScaffoldAbort("SL_GetString_(null)");

    pthread_mutex_lock(&s_stringLock);
    for (uint32_t i = 0; i < 128; ++i) {
        if (s_strings[i].value && strcmp(s_strings[i].value, str) == 0) {
            ++s_strings[i].refs;
            pthread_mutex_unlock(&s_stringLock);
            return i + 1;
        }
    }
    for (uint32_t i = 0; i < 128; ++i) {
        if (!s_strings[i].value) {
            const size_t length = strlen(str) + 1;
            s_strings[i].value = static_cast<char *>(malloc(length));
            if (!s_strings[i].value) {
                pthread_mutex_unlock(&s_stringLock);
                BootScaffoldAbort("SL_GetString_(out of memory)");
            }
            memcpy(s_strings[i].value, str, length);
            s_strings[i].refs = 1;
            pthread_mutex_unlock(&s_stringLock);
            return i + 1;
        }
    }
    pthread_mutex_unlock(&s_stringLock);
    BootScaffoldAbort("SL_GetString_(table full)");
}

const char *SL_ConvertToString(uint32_t stringValue)
{
    if (stringValue == 0 || stringValue > 128 || !s_strings[stringValue - 1].value)
        BootScaffoldAbort("SL_ConvertToString(invalid id)");
    return s_strings[stringValue - 1].value;
}

uint32_t SL_FindString(const char *str)
{
    if (!str)
        BootScaffoldAbort("SL_FindString(null)");
    pthread_mutex_lock(&s_stringLock);
    for (uint32_t i = 0; i < 128; ++i) {
        if (s_strings[i].value && strcmp(s_strings[i].value, str) == 0) {
            pthread_mutex_unlock(&s_stringLock);
            return i + 1;
        }
    }
    pthread_mutex_unlock(&s_stringLock);
    return 0;
}

void SL_RemoveRefToString(uint32_t stringValue)
{
    if (stringValue == 0 || stringValue > 128)
        BootScaffoldAbort("SL_RemoveRefToString(invalid id)");
    pthread_mutex_lock(&s_stringLock);
    BootString &slot = s_strings[stringValue - 1];
    if (!slot.value || slot.refs == 0) {
        pthread_mutex_unlock(&s_stringLock);
        BootScaffoldAbort("SL_RemoveRefToString(unreferenced id)");
    }
    --slot.refs;
    pthread_mutex_unlock(&s_stringLock);
}

static dvar_t s_reflectionProbeGenerate = {};
const dvar_t *r_reflectionProbeGenerate = &s_reflectionProbeGenerate;

void R_ReflectionProbeRegisterDvars()
{
    s_reflectionProbeGenerate.current.enabled = false;
}

// Allocation tracking is diagnostic-only; the real allocator behavior is in
// com_memory.cpp and remains fully exercised by BootSmoke.cpp.
void track_PrintAllInfo() {}
void track_PrintInfo() {}
void track_hunk_ClearToMarkHigh(int) {}
void track_hunk_ClearToMarkLow(int) {}
void track_hunk_ClearToStart() {}
void track_hunk_alloc(int, int, const char *, int) {}
void track_hunk_allocLow(int, int, const char *, int) {}
void track_set_hunk_size(int) {}
void track_temp_alloc(int, int, int, const char *) {}
void track_temp_free(int, int, const char *) {}
void track_temp_high_alloc(int, int, int, const char *) {}
void track_temp_high_clear(int) {}
void track_z_commit(int, int) {}

// Globals owned by TUs outside the current leaf archive.
fileData_s *com_fileDataHashTable[1024] = {};
char info1[1024] = {};
char info2[8192] = {};
uint32_t s_affinityMaskForProcess = 1;
uint32_t s_cpuCount = 1;
uint32_t s_affinityMaskForCpu[4] = { static_cast<uint32_t>(-1), 0, 0, 0 };

// -------------------------------------------------------------------------
// Abort-loud dependencies that must stay off the staged boot path.
// Definitions already supplied by EngineSmoke.cpp are intentionally omitted.
// -------------------------------------------------------------------------

void CL_ForwardCommandToServer(int32_t, const char *) BOOT_UNREACHED("CL_ForwardCommandToServer")
const char *CL_GetUsernameForLocalClient() BOOT_UNREACHED("CL_GetUsernameForLocalClient")
void CL_InitDedicated() BOOT_UNREACHED("CL_InitDedicated")
void CL_InitOnceForAllClients() BOOT_UNREACHED("CL_InitOnceForAllClients")
void CL_Init(int32_t) BOOT_UNREACHED("CL_Init")
void CL_InitRenderer() BOOT_UNREACHED("CL_InitRenderer")
void CL_StartHunkUsers() BOOT_UNREACHED("CL_StartHunkUsers")
void Com_BeginParseSession(const char *) BOOT_UNREACHED("Com_BeginParseSession")
void Com_EndParseSession() BOOT_UNREACHED("Com_EndParseSession")
// Com_Filter: real owner com_shared.cpp joined the cominit archive (dupe
// caught by coordinator audit after CI run 29283036276 flagged it).
parseInfo_t *Com_Parse(const char **) BOOT_UNREACHED("Com_Parse")
parseInfo_t *Com_ParseOnLine(const char **) BOOT_UNREACHED("Com_ParseOnLine")
void Com_SkipRestOfLine(const char **) BOOT_UNREACHED("Com_SkipRestOfLine")
XAssetHeader DB_FindXAssetHeader(XAssetType, const char *) BOOT_UNREACHED("DB_FindXAssetHeader")
bool DB_IsMinimumFastFileLoaded() BOOT_UNREACHED("DB_IsMinimumFastFileLoaded")
int Com_BlockChecksumKey32(const uint8_t *, uint32_t, uint32_t) BOOT_UNREACHED("Com_BlockChecksumKey32")
int Com_SafeMode() BOOT_UNREACHED("Com_SafeMode")
void NET_Init() BOOT_UNREACHED("NET_Init")
void PMem_DumpMemStats() BOOT_UNREACHED("PMem_DumpMemStats")
void R_BeginRemoteScreenUpdate() BOOT_UNREACHED("R_BeginRemoteScreenUpdate")
void R_EndRemoteScreenUpdate() BOOT_UNREACHED("R_EndRemoteScreenUpdate")
void R_InitThreads() BOOT_UNREACHED("R_InitThreads")
char SND_InitDriver() BOOT_UNREACHED("SND_InitDriver")
void SND_Init() BOOT_UNREACHED("SND_Init")
void SND_StopSounds(snd_stopsounds_arg_t) BOOT_UNREACHED("SND_StopSounds")
void SV_Init() BOOT_UNREACHED("SV_Init")
int SV_GameCommand() BOOT_UNREACHED("SV_GameCommand")
void SV_WaitServer() BOOT_UNREACHED("SV_WaitServer")
void Scr_MonitorCommand(const char *) BOOT_UNREACHED("Scr_MonitorCommand")
bool Sys_IsDatabaseThread() BOOT_UNREACHED("Sys_IsDatabaseThread")
void Sys_OutOfMemErrorInternal(const char *, int) BOOT_UNREACHED("Sys_OutOfMemErrorInternal")
void XAnimFree(XAnimParts *) BOOT_UNREACHED("XAnimFree")
void XAnimFreeList(XAnim_s *) BOOT_UNREACHED("XAnimFreeList")
void XModelPartsFree(XModelPartsLoad *) BOOT_UNREACHED("XModelPartsFree")

#undef BOOT_UNREACHED
