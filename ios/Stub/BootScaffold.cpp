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

int FS_LoadStack() { return 0; }
void Dvar_AddCommands() {}
bool Com_LogFileOpen() { return false; }
bool Sys_IsRenderThread() { return false; }

void Com_PrintWarning(int, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void NET_Sleep(int msec)
{
    if (msec <= 0) {
        sched_yield();
        return;
    }
    timespec delay = { msec / 1000, (msec % 1000) * 1000000L };
    nanosleep(&delay, nullptr);
}

static pthread_once_t s_criticalOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_criticalSections[22];

static void BootInitCriticalSections()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    for (pthread_mutex_t &mutex : s_criticalSections)
        pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

void Sys_EnterCriticalSection(int critSect)
{
    if (critSect < 0 || critSect >= 22)
        BootScaffoldAbort("Sys_EnterCriticalSection(index)");
    pthread_once(&s_criticalOnce, BootInitCriticalSections);
    pthread_mutex_lock(&s_criticalSections[critSect]);
}

void Sys_LeaveCriticalSection(int critSect)
{
    if (critSect < 0 || critSect >= 22)
        BootScaffoldAbort("Sys_LeaveCriticalSection(index)");
    pthread_once(&s_criticalOnce, BootInitCriticalSections);
    pthread_mutex_unlock(&s_criticalSections[critSect]);
}

void Sys_LockWrite(FastCriticalSection *critSect)
{
    for (;;) {
        if (__atomic_load_n(&critSect->readCount, __ATOMIC_ACQUIRE) == 0
            && __sync_bool_compare_and_swap(&critSect->writeCount, 0, 1)) {
            if (__atomic_load_n(&critSect->readCount, __ATOMIC_ACQUIRE) == 0)
                return;
            __sync_lock_release(&critSect->writeCount);
        }
        NET_Sleep(0);
    }
}

void Sys_UnlockWrite(FastCriticalSection *critSect)
{
    if (__atomic_load_n(&critSect->writeCount, __ATOMIC_RELAXED) == 0)
        BootScaffoldAbort("Sys_UnlockWrite(unlocked)");
    __sync_lock_release(&critSect->writeCount);
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
        return 0;
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

static dvar_t s_useFastFile = {};
static dvar_t s_reflectionProbeGenerate = {};
const dvar_t *useFastFile = &s_useFastFile;
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

// Globals owned by TUs outside the current leaf archive. The smoke path only
// requires useFastFile and r_reflectionProbeGenerate to be valid dvars.
fileData_s *com_fileDataHashTable[1024] = {};
const dvar_t *com_sv_running = nullptr;
char info1[1024] = {};
char info2[8192] = {};

// -------------------------------------------------------------------------
// Abort-loud dependencies that must stay off the staged boot path.
// Definitions already supplied by EngineSmoke.cpp are intentionally omitted.
// -------------------------------------------------------------------------

void CL_ForwardCommandToServer(int32_t, const char *) BOOT_UNREACHED("CL_ForwardCommandToServer")
const char *CL_GetUsernameForLocalClient() BOOT_UNREACHED("CL_GetUsernameForLocalClient")
void Com_BeginParseSession(const char *) BOOT_UNREACHED("Com_BeginParseSession")
void Com_EndParseSession() BOOT_UNREACHED("Com_EndParseSession")
char Com_Filter(const char *, char *, int) BOOT_UNREACHED("Com_Filter")
parseInfo_t *Com_Parse(const char **) BOOT_UNREACHED("Com_Parse")
parseInfo_t *Com_ParseOnLine(const char **) BOOT_UNREACHED("Com_ParseOnLine")
void Com_SkipRestOfLine(const char **) BOOT_UNREACHED("Com_SkipRestOfLine")
XAssetHeader DB_FindXAssetHeader(XAssetType, const char *) BOOT_UNREACHED("DB_FindXAssetHeader")
bool DB_IsMinimumFastFileLoaded() BOOT_UNREACHED("DB_IsMinimumFastFileLoaded")
void FS_FreeFile(char *) BOOT_UNREACHED("FS_FreeFile")
int FS_HashFileName(const char *, int) BOOT_UNREACHED("FS_HashFileName")
const char **FS_ListFiles(const char *, const char *, FsListBehavior_e, int *) BOOT_UNREACHED("FS_ListFiles")
void FS_Printf(int, const char *, ...) BOOT_UNREACHED("FS_Printf")
int FS_ReadFile(const char *, void **) BOOT_UNREACHED("FS_ReadFile")
void PMem_DumpMemStats() BOOT_UNREACHED("PMem_DumpMemStats")
int SV_GameCommand() BOOT_UNREACHED("SV_GameCommand")
void SV_WaitServer() BOOT_UNREACHED("SV_WaitServer")
void Scr_MonitorCommand(const char *) BOOT_UNREACHED("Scr_MonitorCommand")
void Sys_OutOfMemErrorInternal(const char *, int) BOOT_UNREACHED("Sys_OutOfMemErrorInternal")
void XAnimFree(XAnimParts *) BOOT_UNREACHED("XAnimFree")
void XAnimFreeList(XAnim_s *) BOOT_UNREACHED("XAnimFreeList")
void XModelPartsFree(XModelPartsLoad *) BOOT_UNREACHED("XModelPartsFree")

#undef BOOT_UNREACHED
