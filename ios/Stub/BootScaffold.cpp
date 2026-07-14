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
#include <unistd.h>

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
enum netsrc_t : int {};
enum netadrtype_t : int {};
enum print_msg_dest_t : int {};
enum uiMenuCommand_t : int {};
union XAssetHeader { void *data; };
struct alignas(4) netadr_t
{
    netadrtype_t type;
    unsigned char ip[4];
    unsigned short port;
    unsigned char ipx[10];
};
static_assert(sizeof(netadr_t) == 20, "arm64 MP netadr_t ABI drift");
struct parseInfo_t;
struct XAnim_s;
struct XAnimParts;
struct XModelPartsLoad;
struct fileData_s;
struct msg_t;
struct sysEvent_t;
struct XZoneInfo;

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

// Exact iOS behavior from threads.cpp's Sys_iOS_InitThreads/Sys_GetCpuCount.
// This accessor is reached by Com_InitDvars before the B2 fence, so an abort
// stub would make the marker impossible and a hard-coded default would lie.
uint32_t Sys_GetCpuCount()
{
    long cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpuCount < 1)
        cpuCount = 1;
    if (cpuCount > 4)
        cpuCount = 4;
    s_cpuCount = static_cast<uint32_t>(cpuCount);
    return s_cpuCount;
}

// common.cpp carries direct references to four data owners only in functions
// beyond the guarded B2 return. Data cannot be abort-loud, so give each symbol
// exact-size, non-benign poison storage. The marker requires the fence before
// any read; real client/server owners delete these definitions in their waves.
template <size_t Size>
struct alignas(16) B2PoisonStorage
{
    unsigned char bytes[Size];
    B2PoisonStorage() { memset(bytes, 0xA5, sizeof(bytes)); }
};

B2PoisonStorage<0x10> clientUIActives;
B2PoisonStorage<0x2DD070> cls;
B2PoisonStorage<0x5FC60> sv;
int updateScreenCalled = -1;
static_assert(sizeof(clientUIActives) == 0x10, "clientUIActives poison size drift");
static_assert(sizeof(cls) == 0x2DD070, "cls poison size drift");
static_assert(sizeof(sv) == 0x5FC60, "sv poison size drift");

// -------------------------------------------------------------------------
// Abort-loud dependencies that must stay off the staged boot path.
// Definitions already supplied by EngineSmoke.cpp are intentionally omitted.
// -------------------------------------------------------------------------

// B2 common.cpp whole-object closure. The functions are post-fence except the
// client/system console frontends, whose pre-fence call sites have explicit
// iOS headless guards. Group comments name the real-owner wave that must delete
// each scaffold; reaching any one aborts with its symbol.

// Common/config/profile owners: common tail and B4/M15 dvar command wave.
void BG_ShutdownWeaponDefFiles() BOOT_UNREACHED("BG_ShutdownWeaponDefFiles")
void CCS_InitConstantConfigStrings() BOOT_UNREACHED("CCS_InitConstantConfigStrings")
void CM_Shutdown() BOOT_UNREACHED("CM_Shutdown")
void Com_CheckSetRecommended(int) BOOT_UNREACHED("Com_CheckSetRecommended")
void Com_CleanupBsp() BOOT_UNREACHED("Com_CleanupBsp")
void Com_DvarDump(int, const char *) BOOT_UNREACHED("Com_DvarDump")
void Com_HasPlayerProfile() BOOT_UNREACHED("Com_HasPlayerProfile")
void Com_InitDObj() BOOT_UNREACHED("Com_InitDObj")
void Com_InitPlayerProfiles(int) BOOT_UNREACHED("Com_InitPlayerProfiles")
void Com_ResetParseSessions() BOOT_UNREACHED("Com_ResetParseSessions")
void Com_ShutdownDObj() BOOT_UNREACHED("Com_ShutdownDObj")
void Com_ShutdownWorld() BOOT_UNREACHED("Com_ShutdownWorld")
void Con_InitChannels() BOOT_UNREACHED("Con_InitChannels")
void Con_IsChannelVisible(print_msg_dest_t, unsigned int, int) BOOT_UNREACHED("Con_IsChannelVisible")
void Con_WriteFilterConfigString(int) BOOT_UNREACHED("Con_WriteFilterConfigString")
void Dvar_SetA_f() BOOT_UNREACHED("Dvar_SetA_f(B4 dvar_cmds owner)")
void Dvar_Set_f() BOOT_UNREACHED("Dvar_Set_f(B4 dvar_cmds owner)")
void FS_ShutdownServerIwdNames() BOOT_UNREACHED("FS_ShutdownServerIwdNames")
void FS_ShutdownServerReferencedFFs() BOOT_UNREACHED("FS_ShutdownServerReferencedFFs")
void FS_ShutdownServerReferencedIwds() BOOT_UNREACHED("FS_ShutdownServerReferencedIwds")
void FakeLag_Init() BOOT_UNREACHED("FakeLag_Init")
void FakeLag_Shutdown() BOOT_UNREACHED("FakeLag_Shutdown")
void ProfLoad_Deactivate() BOOT_UNREACHED("ProfLoad_Deactivate")
void ProfLoad_Init() BOOT_UNREACHED("ProfLoad_Init")
void ProfLoad_IsActive() BOOT_UNREACHED("ProfLoad_IsActive")
void Profile_Guard(int) BOOT_UNREACHED("Profile_Guard")
void Profile_Recover(int) BOOT_UNREACHED("Profile_Recover")
void Profile_Unguard(int) BOOT_UNREACHED("Profile_Unguard")
void SetAnimCheck(int) BOOT_UNREACHED("SetAnimCheck")
void StatMon_Warning(int, int, const char *) BOOT_UNREACHED("StatMon_Warning")
void getBuildNumber() BOOT_UNREACHED("getBuildNumber")

// Client/UI/input owners: later client and renderer-content waves.
void CL_CharEvent(int, int) BOOT_UNREACHED("CL_CharEvent")
void CL_ConsoleFixPosition() BOOT_UNREACHED("CL_ConsoleFixPosition")
void CL_ConsolePrint(int, int, const char *, int, int, int) BOOT_UNREACHED("CL_ConsolePrint")
void CL_ControllerIndexFromClientNum(int) BOOT_UNREACHED("CL_ControllerIndexFromClientNum")
void CL_Disconnect(int) BOOT_UNREACHED("CL_Disconnect")
void CL_FlushDebugServerData() BOOT_UNREACHED("CL_FlushDebugServerData")
void CL_Frame(netsrc_t) BOOT_UNREACHED("CL_Frame")
void CL_GetLocalClientConnection(int) BOOT_UNREACHED("CL_GetLocalClientConnection")
void CL_InitKeyCommands() BOOT_UNREACHED("CL_InitKeyCommands")
void CL_KeyEvent(int, int, int, unsigned int) BOOT_UNREACHED("CL_KeyEvent")
void CL_PacketEvent(netsrc_t, netadr_t, msg_t *, int) BOOT_UNREACHED("CL_PacketEvent")
void CL_RunOncePerClientFrame(int, int) BOOT_UNREACHED("CL_RunOncePerClientFrame")
void CL_Shutdown(int) BOOT_UNREACHED("CL_Shutdown")
void CL_ShutdownAll(bool) BOOT_UNREACHED("CL_ShutdownAll")
void CL_ShutdownHunkUsers() BOOT_UNREACHED("CL_ShutdownHunkUsers")
void CL_ShutdownRef() BOOT_UNREACHED("CL_ShutdownRef")
void CL_UpdateDebugServerData() BOOT_UNREACHED("CL_UpdateDebugServerData")
void CL_UpdateSound() BOOT_UNREACHED("CL_UpdateSound")
void IN_Frame() BOOT_UNREACHED("IN_Frame")
void Key_WriteBindings(int, int) BOOT_UNREACHED("Key_WriteBindings")
void LiveStorage_Init() BOOT_UNREACHED("LiveStorage_Init")
void UI_GetMenuScreen() BOOT_UNREACHED("UI_GetMenuScreen")
void UI_GetMenuScreenForError() BOOT_UNREACHED("UI_GetMenuScreenForError")
void UI_IsFullscreen(int) BOOT_UNREACHED("UI_IsFullscreen")
void UI_SetActiveMenu(int, uiMenuCommand_t) BOOT_UNREACHED("UI_SetActiveMenu")
void UI_SetMap(char *, char *) BOOT_UNREACHED("UI_SetMap")

// Database/script/animation/effects owners: Stage C and later asset waves.
void DB_Cleanup() BOOT_UNREACHED("DB_Cleanup")
void DB_InitThread() BOOT_UNREACHED("DB_InitThread")
void DB_LoadXAssets(XZoneInfo *, unsigned int, int) BOOT_UNREACHED("DB_LoadXAssets")
void DB_ReleaseXAssets() BOOT_UNREACHED("DB_ReleaseXAssets")
void DB_SetInitializing(bool) BOOT_UNREACHED("DB_SetInitializing")
void DB_ShutdownXAssets() BOOT_UNREACHED("DB_ShutdownXAssets")
void DB_Update() BOOT_UNREACHED("DB_Update")
void DObjInit() BOOT_UNREACHED("DObjInit")
void DObjShutdown() BOOT_UNREACHED("DObjShutdown")
void FX_UnregisterAll() BOOT_UNREACHED("FX_UnregisterAll")
void GScr_Shutdown() BOOT_UNREACHED("GScr_Shutdown")
void PMem_BeginAlloc(const char *, unsigned int) BOOT_UNREACHED("PMem_BeginAlloc")
void PMem_EndAlloc(const char *, unsigned int) BOOT_UNREACHED("PMem_EndAlloc")
void PMem_Init() BOOT_UNREACHED("PMem_Init")
void SL_Init() BOOT_UNREACHED("SL_Init")
void Scr_CanDrawScript() BOOT_UNREACHED("Scr_CanDrawScript")
void Scr_Cleanup() BOOT_UNREACHED("Scr_Cleanup")
void Scr_DrawScript() BOOT_UNREACHED("Scr_DrawScript")
void Scr_Init() BOOT_UNREACHED("Scr_Init")
void Scr_InitVariables() BOOT_UNREACHED("Scr_InitVariables")
void Scr_Settings(int, int, int) BOOT_UNREACHED("Scr_Settings")
void Scr_Shutdown() BOOT_UNREACHED("Scr_Shutdown")
void Scr_UpdateDebugSocket() BOOT_UNREACHED("Scr_UpdateDebugSocket")
void Scr_UpdateRemoteDebugger() BOOT_UNREACHED("Scr_UpdateRemoteDebugger")
void XAnimInit() BOOT_UNREACHED("XAnimInit")
void XAnimShutdown() BOOT_UNREACHED("XAnimShutdown")

// Network/server owners: B3 loopback networking and later server waves.
void NET_GetClientPacket(netadr_t *, msg_t *) BOOT_UNREACHED("NET_GetClientPacket")
void NET_GetLoopPacket(netsrc_t, netadr_t *, msg_t *) BOOT_UNREACHED("NET_GetLoopPacket")
void NET_GetServerPacket(netadr_t *, msg_t *) BOOT_UNREACHED("NET_GetServerPacket")
void NET_RestartDebug() BOOT_UNREACHED("NET_RestartDebug")
void NET_ShutdownDebug() BOOT_UNREACHED("NET_ShutdownDebug")
void Netchan_Init(short) BOOT_UNREACHED("Netchan_Init")
void SV_AddDedicatedCommands() BOOT_UNREACHED("SV_AddDedicatedCommands")
void SV_Frame(int) BOOT_UNREACHED("SV_Frame")
void SV_PacketEvent(netadr_t, msg_t *) BOOT_UNREACHED("SV_PacketEvent")
void SV_Shutdown(const char *) BOOT_UNREACHED("SV_Shutdown")
void SV_ShutdownGameProgs() BOOT_UNREACHED("SV_ShutdownGameProgs")

// Renderer/sound/system owners: renderer-content, audio, and platform waves.
void DevGui_Update(int, float) BOOT_UNREACHED("DevGui_Update")
void R_BeginDebugFrame() BOOT_UNREACHED("R_BeginDebugFrame")
void R_ComErrorCleanup() BOOT_UNREACHED("R_ComErrorCleanup")
void R_EndDebugFrame() BOOT_UNREACHED("R_EndDebugFrame")
void R_PopRemoteScreenUpdate() BOOT_UNREACHED("R_PopRemoteScreenUpdate")
void R_SetEndTime(int) BOOT_UNREACHED("R_SetEndTime")
void R_SyncRenderThread() BOOT_UNREACHED("R_SyncRenderThread")
void R_WaitEndTime() BOOT_UNREACHED("R_WaitEndTime")
void R_WaitWorkerCmds() BOOT_UNREACHED("R_WaitWorkerCmds")
void Ragdoll_Update(int) BOOT_UNREACHED("Ragdoll_Update")
void SCR_UpdateScreen() BOOT_UNREACHED("SCR_UpdateScreen")
void SND_ErrorCleanup() BOOT_UNREACHED("SND_ErrorCleanup")
void SND_ShutdownChannels() BOOT_UNREACHED("SND_ShutdownChannels")
void Sys_DestroySplashWindow() BOOT_UNREACHED("Sys_DestroySplashWindow")
void Sys_GetEvent(sysEvent_t *) BOOT_UNREACHED("Sys_GetEvent")
void Sys_Init() BOOT_UNREACHED("Sys_Init")
void Sys_IsRemoteDebugClient() BOOT_UNREACHED("Sys_IsRemoteDebugClient")
void Sys_Print(const char *) BOOT_UNREACHED("Sys_Print")
void Sys_Quit() BOOT_UNREACHED("Sys_Quit")
void Win_UpdateThreadLock() BOOT_UNREACHED("Win_UpdateThreadLock")

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
