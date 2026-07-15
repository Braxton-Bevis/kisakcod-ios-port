// iOS platform entry layer — replaces src/win32/win_main.cpp (plus the console
// sinks from win_syscon.cpp and the Sys_* filesystem helpers from
// universal/win_common.cpp) for arm64-apple-ios. See DEPENDENCY_MAP §2/§3/§6/§7.
//
// What this file deliberately does NOT contain:
//   - main()/UIApplicationMain — the stub app target owns process entry and the
//     CADisplayLink loop that replaces WinMain's while(1) Com_Frame().
//   - The window message pump — UIKit delivers touches/keys on the main runloop
//     and the app layer feeds them into the engine via Sys_QueEvent instead.
//   - Networking (win_net.cpp) and voice (win_voice.cpp) — separate subsystems.
#ifdef KISAK_IOS

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

// DXVK's windows_base.h now carries an __OBJC__ guard (added with the iOS WSI
// backend): ObjC++ TUs keep the ObjC runtime's BOOL and Win32 code uses
// WINBOOL. Engine declarations spelled `BOOL` resolve to the ObjC BOOL here;
// the definitions below use the same spelling so signatures keep matching.
#include <win32/win_local.h>

#include <client/client.h>

#include <qcommon/qcommon.h>
#include <qcommon/cmd.h>
#include <qcommon/threads.h>
#include <qcommon/mem_track.h>

#include <universal/com_memory.h>
#include <universal/q_parse.h>
#include <universal/timing.h>
#include <universal/profile.h>

#include <script/scr_stringlist.h> // SL_Shutdown for Sys_Quit
#undef BOOL

#include <ios/sys_ios.h>

void Sys_FindInfo(); // no header decl on win32 either; the app entry mirrors WinMain
sysEvent_t *Win_GetEvent(sysEvent_t *result); // narrow owner: sys_ios_events.cpp

char sys_cmdline[1024];
char sys_exitCmdLine[1024];

HWND g_splashWnd; // extern'd via win_local.h; stays NULL — splash is LaunchScreen.storyboard on iOS

SysInfo sys_info;

int client_state;

cmd_function_s Sys_In_Restart_f_VAR;
#ifdef KISAK_MP
cmd_function_s Sys_Net_Restart_f_VAR;
cmd_function_s Sys_Listen_f_VAR;
#endif

// ------------------------------------------------------------------
// Console window (win_syscon.cpp contract)
// No console window exists on iOS: the GUI console is an explicit no-op and
// all console text goes to stderr, which reaches Xcode/os_log/`idevicesyslog`.
// ------------------------------------------------------------------

static char s_errorString[512];

void Sys_CreateConsole()
{
    // explicit no-op: no console window on iOS
}

void Sys_DestroyConsole()
{
    // explicit no-op: no console window on iOS
}

void __cdecl Sys_ShowConsole()
{
    // explicit no-op: no console window on iOS
}

void Conbuf_AppendText(const char *msg)
{
    fputs(msg, stderr);
}

void Conbuf_AppendTextInMainThread(const char *msg)
{
    // win32 dropped non-main-thread text to keep the 'edit' control
    // single-threaded; stderr has no such constraint.
    fputs(msg, stderr);
}

void Sys_SetErrorText(const char *buf)
{
    I_strncpyz(s_errorString, buf, 512);
    // win32 raises a modal MessageBoxA("Error") here; log instead.
    NSLog(@"Sys_SetErrorText: %s", buf);
}

// ------------------------------------------------------------------
// Fatal errors, exit, quit
// ------------------------------------------------------------------

void __cdecl Sys_NormalExit()
{
    // win32 deletes the process-semaphore file used for improper-quit
    // detection; a sandboxed, single-instance iOS app has no such file.
}

void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line)
{
    Sys_EnterCriticalSection(CRITSECT_FATAL_ERROR);
    Com_Printf(16, "Out of memory: filename '%s', line %d\n", filename, line);
    // win32 shows the localized WIN_OUT_OF_MEM_* MessageBox; no modal UI here.
    NSLog(@"Out of memory: filename '%s', line %d", filename, line);
    exit(-1);
}

void Sys_Error(const char *error, ...)
{
    char string[4100]; // [esp+20h] [ebp-1008h] BYREF
    va_list va; // [esp+1034h] [ebp+Ch] BYREF

    va_start(va, error);
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    Com_PrintStackTrace();
    com_errorEntered = 1;
    Sys_SuspendOtherThreads();
    vsnprintf(string, 0x1000u, error, va);
    va_end(va);

    // win32 shows the console window and pumps a modal GetMessage loop until
    // the user closes it; iOS has neither, so log everything and terminate.
    NSLog(@"Sys_Error: %s", string);
    Conbuf_AppendText("\n\n");
    Conbuf_AppendText(string);
    Conbuf_AppendText("\n");
    Sys_SetErrorText(string);
    exit(0);
}

void __cdecl Sys_QuitAndStartProcess(const char *exeName, const char *parameters)
{
    // iOS cannot spawn processes (CreateProcessA has no equivalent in the
    // sandbox), so the SP<->MP exe relaunch is impossible; honor the quit half
    // so callers (cl_main_mp.cpp) still get a clean shutdown.
    Com_Printf(16, "Sys_QuitAndStartProcess: cannot start '%s' on iOS; quitting only\n", exeName);
    Cbuf_AddText(0, "quit\n");
}

void __cdecl Sys_Quit()
{
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    // win32-only steps dropped: timeEndPeriod(1) (scheduler granularity),
    // Sys_SpawnQuitProcess (no process spawning on iOS),
    // Win_ShutdownLocalization (win_localize.cpp side-file is not compiled).
    IN_Shutdown();
    Key_Shutdown();
    Sys_DestroyConsole();
    Sys_NormalExit();
    RefreshQuitOnErrorCondition();
    Dvar_Shutdown();
    Cmd_Shutdown();
    KISAK_NULLSUB();
    KISAK_NULLSUB();
    Sys_ShutdownEvents();
    SL_Shutdown();
    if (!com_errorEntered)
        track_shutdown(0);
    Con_ShutdownChannels();
    exit(0);
}

void __cdecl Sys_Print(const char *msg)
{
    Conbuf_AppendTextInMainThread(msg);
}

// ------------------------------------------------------------------
// URL / clipboard
// ------------------------------------------------------------------

void __cdecl Sys_OpenURL(const char *url, int doexit)
{
    const char *v2; // eax

    NSString *str = url ? [NSString stringWithUTF8String:url] : nil;
    NSURL *nsurl = str ? [NSURL URLWithString:str] : nil;
    if (!nsurl)
    {
        v2 = va("EXE_ERR_COULDNT_OPEN_URL", url);
        Com_Error(ERR_DROP, v2);
        return;
    }
    // UIApplication may only be touched on the main thread; the engine calls
    // this from its own threads, so hop over via dispatch_async.
    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] openURL:nsurl options:@{} completionHandler:nil];
    });
    if (doexit)
    {
        // win32 queues "quit\n" here. DEPENDENCY_MAP §7: the "and quit" variant
        // must not self-terminate on iOS; the URL opens over the app instead.
        Com_Printf(16, "Sys_OpenURL: ignoring doexit on iOS\n");
    }
}

char *__cdecl Sys_GetClipboardData()
{
    char *data; // [esp+4h] [ebp-8h]

    data = 0;
    @autoreleasepool
    {
        NSString *cliptext = [UIPasteboard generalPasteboard].string;
        const char *utf8 = cliptext ? [cliptext UTF8String] : NULL;
        if (utf8)
        {
            size_t size = strlen(utf8) + 1;
            data = (char *)Z_Malloc((int)size, "Sys_GetClipboardData", 10);
            I_strncpyz(data, utf8, size);
            strtok(data, "\n\r\b"); // win32 truncates at the first control char too
        }
    }
    return data;
}

int __cdecl Sys_SetClipboardData(const char *text)
{
    if (!text)
        return 0;
    @autoreleasepool
    {
        NSString *str = [NSString stringWithUTF8String:text];
        if (!str)
            return 0;
        [UIPasteboard generalPasteboard].string = str;
    }
    return 1;
}

// The real event queue is isolated in sys_ios_events.cpp. Keeping that narrow
// owner separate lets the headless B4 link use it without pulling this file's
// filesystem surface, which intentionally overlaps win_common.cpp.

void __cdecl Sys_LoadingKeepAlive()
{
    sysEvent_t result; // [esp+0h] [ebp-48h] BYREF
    sysEvent_t v1; // [esp+18h] [ebp-30h]

    do
    {
        v1 = *Win_GetEvent(&result);
    } while (v1.evType);
    // win32 polls R_CheckLostDevice() here; D3D9 device loss has no Metal
    // analogue (DEPENDENCY_MAP §8), so draining the queue is all that remains.
}

// ------------------------------------------------------------------
// Sys_Init / system info
// ------------------------------------------------------------------

static int Sys_iOS_SysctlU64(const char *name, unsigned long long *out)
{
    size_t len = sizeof(*out);
    *out = 0;
    return sysctlbyname(name, out, &len, NULL, 0) == 0;
}

void Sys_FindInfo()
{
    unsigned long long physicalCpu;
    unsigned long long cpuFreq;
    unsigned long long memsize;
    double multiCpuFactor;
    char brand[128];
    size_t brandLen;

    sys_info.logicalCpuCount = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (sys_info.logicalCpuCount < 1)
        sys_info.logicalCpuCount = 1;
    if (Sys_iOS_SysctlU64("hw.physicalcpu", &physicalCpu) && physicalCpu)
        sys_info.physicalCpuCount = (int)physicalCpu;
    else
        sys_info.physicalCpuCount = sys_info.logicalCpuCount;

    // Apple exposes no core-clock sysctl on iOS (hw.cpufrequency is macOS-only);
    // try it anyway, then fall back to a fixed estimate. This only feeds the
    // autoconfigure heuristics (DEPENDENCY_MAP §4 __rdtsc/Sys_BenchmarkGHz).
    if (Sys_iOS_SysctlU64("hw.cpufrequency", &cpuFreq) && cpuFreq)
        sys_info.cpuGHz = (double)cpuFreq / 1000000000.0;
    else
        sys_info.cpuGHz = 2.0; // best-effort: every arm64 iPhone since the A10 clocks >= ~2.3 GHz

    if (Sys_iOS_SysctlU64("hw.memsize", &memsize) && memsize)
        sys_info.sysMB = (int)(memsize / 0x100000ULL);
    else
        sys_info.sysMB = 1024;
    // win32 caps at 1 GB ("System memory is %i MB (capped at 1 GB)") because
    // the x86-32 heap sizing was tuned against it; keep the cap.
    if (sys_info.sysMB > 1024)
        sys_info.sysMB = 1024;

    // KISAKTODO(ios-renderer): MTLCreateSystemDefaultDevice().name once the
    // Metal backend owns a device (DEPENDENCY_MAP §8, Sys_DetectVideoCard).
    I_strncpyz(sys_info.gpuDescription, "Apple GPU", sizeof(sys_info.gpuDescription));

    sys_info.SSE = 1; // the engine's SSE paths compile via sse2neon on arm64

    brandLen = sizeof(brand);
    memset(brand, 0, sizeof(brand));
    if (sysctlbyname("machdep.cpu.brand_string", brand, &brandLen, NULL, 0) != 0 || !brand[0])
        I_strncpyz(brand, "Apple Silicon", sizeof(brand));
    I_strncpyz(sys_info.cpuVendor, "Apple", sizeof(sys_info.cpuVendor));
    I_strncpyz(sys_info.cpuName, brand, sizeof(sys_info.cpuName));

    // Sys_SetAutoConfigureGHz's multi-CPU factor (win_configure.cpp:213)
    if (sys_info.physicalCpuCount == 1)
        multiCpuFactor = 1.0;
    else if (sys_info.physicalCpuCount == 2)
        multiCpuFactor = 1.75;
    else
        multiCpuFactor = 2.0;
    sys_info.configureGHz = sys_info.cpuGHz * multiCpuFactor;
}

void __cdecl Sys_Init()
{
    // win32: timeBeginPeriod(1) — Darwin timers are already fine-grained (§4).
    Cmd_AddCommandInternal("in_restart", Sys_In_Restart_f, &Sys_In_Restart_f_VAR);
#ifdef KISAK_MP
    Cmd_AddCommandInternal("net_restart", Sys_Net_Restart_f, &Sys_Net_Restart_f_VAR);
    Cmd_AddCommandInternal("net_listen", Sys_Listen_f, &Sys_Listen_f_VAR);
#endif

    // WinMain fills sys_info via Sys_FindInfo before Com_Init; stay
    // self-sufficient in case the iOS app entry sequences differently.
    if (!sys_info.logicalCpuCount)
        Sys_FindInfo();

    // Replaces the wndproc's SetThreadExecutionState(ES_DISPLAY_REQUIRED):
    // a game rendering at 60 Hz must not let the screen dim and lock.
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled = YES;
    });

    Com_Printf(16, "CPU vendor is \"%s\"\n", sys_info.cpuVendor);
    Com_Printf(16, "CPU name is \"%s\"\n", sys_info.cpuName);
    if (sys_info.logicalCpuCount == 1)
        Com_Printf(16, "%i logical CPU%s reported\n", 1, "");
    else
        Com_Printf(16, "%i logical CPU%s reported\n", sys_info.logicalCpuCount, "s");
    if (sys_info.physicalCpuCount == 1)
        Com_Printf(16, "%i physical CPU%s detected\n", 1, "");
    else
        Com_Printf(16, "%i physical CPU%s detected\n", sys_info.physicalCpuCount, "s");
    Com_Printf(16, "Measured CPU speed is %.2lf GHz\n", sys_info.cpuGHz);
    Com_Printf(16, "Total CPU performance is estimated as %.2lf GHz\n", sys_info.configureGHz);
    Com_Printf(16, "System memory is %i MB (capped at 1 GB)\n", sys_info.sysMB);
    Com_Printf(16, "Video card is \"%s\"\n", sys_info.gpuDescription);
    if (sys_info.SSE)
        Com_Printf(16, "Streaming SIMD Extensions (SSE) %ssupported\n", "");
    else
        Com_Printf(16, "Streaming SIMD Extensions (SSE) %ssupported\n", "not ");
    Com_Printf(16, "\n");
    IN_Init();
}

// ------------------------------------------------------------------
// Console commands + net stubs (win_net.cpp / win_net_debug.cpp are replaced
// by the BSD-socket networking subsystem, not by this file)
// ------------------------------------------------------------------

void Sys_In_Restart_f()
{
    IN_Shutdown();
    IN_Init();
}

#ifdef KISAK_MP
void Sys_Net_Restart_f()
{
    // KISAKTODO(ios-net): call NET_Restart() once the BSD-socket layer lands
}

void __cdecl Sys_Listen_f()
{
    // KISAKTODO(ios-net): remote GSC script-debugger listener assumes a desktop
    // debugger peer; stubbed on iOS (DEPENDENCY_MAP §5).
}
#endif

void Sys_ShowIP()
{
    // KISAKTODO(ios-net): getifaddrs-based local-IP listing lands with the net layer
}

// ------------------------------------------------------------------
// Input (win_input.cpp contract) — GCController/GCKeyboard/GCMouse and the
// touch overlay come with the input milestone; these satisfy win_local.h so
// the client compiles and runs (events arrive via Sys_QueEvent meanwhile).
// ------------------------------------------------------------------

void IN_Init()
{
    // KISAKTODO(ios-input): GCController valueChangedHandler -> CL_GamepadAxisValue,
    // GCKeyboard/GCMouse for peripherals, touch overlay for mouse-look
}

void IN_Shutdown()
{
    // KISAKTODO(ios-input)
}

void IN_Frame()
{
    // KISAKTODO(ios-input): per-frame gamepad/touch sampling
}

void IN_Activate(qboolean active)
{
    // KISAKTODO(ios-input): app foreground/background activation
}

void IN_MouseEvent(int mstate)
{
    // KISAKTODO(ios-input)
}

void IN_JoystickCommands()
{
    // KISAKTODO(ios-input)
}

void IN_DeactivateWin32Mouse()
{
    // KISAKTODO(ios-input): no cursor to capture/release exists on iOS
}

void __cdecl IN_ShowSystemCursor(BOOL show) // BOOL: ObjC bool via windows_base.h __OBJC__ guard
{
    // KISAKTODO(ios-input): no system cursor on iOS (UIPointerInteraction on iPadOS later)
}

// ------------------------------------------------------------------
// Filesystem helpers (win_common.cpp contract, DEPENDENCY_MAP §6)
// ------------------------------------------------------------------

void __cdecl Sys_Mkdir(const char *path)
{
    mkdir(path, 0755);
}

BOOL __cdecl Sys_RemoveDirTree(const char *path)
{
    @autoreleasepool
    {
        NSFileManager *fm = [NSFileManager defaultManager];
        NSString *nsPath = [fm stringWithFileSystemRepresentation:path length:strlen(path)];
        // removeItemAtPath recurses, matching win32's manual _findfirst walk
        return nsPath && [fm removeItemAtPath:nsPath error:nil];
    }
}

// Stands in for _finddata64i32_t: the name plus the 0x10 (_A_SUBDIR) bit the
// win32 listing logic keys on.
struct iosFindData
{
    unsigned attrib;
    char name[256];
};

static int Sys_iOS_FindNext(DIR *dir, const char *dirPath, iosFindData *find)
{
    struct dirent *entry;
    struct stat st;
    char fullPath[512];

    entry = readdir(dir);
    if (!entry)
        return -1;
    I_strncpyz(find->name, entry->d_name, sizeof(find->name));
    find->attrib = 0;
    if (entry->d_type == DT_DIR)
    {
        find->attrib = 0x10;
    }
    else if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
    {
        Com_sprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, find->name);
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))
            find->attrib = 0x10;
    }
    return 0;
}

static void Sys_ListFilteredFiles(
    HunkUser *user,
    const char *basedir,
    const char *subdirs,
    const char *filter,
    char **list,
    int *numfiles)
{
    char filename[256]; // [esp+10h] [ebp-338h] BYREF
    iosFindData findinfo;
    DIR *findhandle;
    char search[260]; // [esp+240h] [ebp-108h] BYREF

    if (*numfiles < 0x1FFF)
    {
        if (strlen(subdirs))
            Com_sprintf(search, 0x100u, "%s/%s", basedir, subdirs);
        else
            Com_sprintf(search, 0x100u, "%s", basedir);
        findhandle = opendir(search);
        if (findhandle)
        {
            while (!Sys_iOS_FindNext(findhandle, search, &findinfo))
            {
                if ((findinfo.attrib & 0x10) == 0
                    || I_stricmp(findinfo.name, ".") && I_stricmp(findinfo.name, "..") && I_stricmp(findinfo.name, "CVS"))
                {
                    if (*numfiles >= 0x1FFF)
                        break;
                    if (subdirs)
                        Com_sprintf(filename, 0x100u, "%s/%s", subdirs, findinfo.name);
                    else
                        Com_sprintf(filename, 0x100u, "%s", findinfo.name);
                    if (Com_FilterPath(filter, filename, 0))
                        list[(*numfiles)++] = Hunk_CopyString(user, filename);
                }
            }
            closedir(findhandle);
        }
    }
}

static int HasFileExtension(const char *name, const char *extension)
{
    char search[260]; // [esp+0h] [ebp-108h] BYREF

    Com_sprintf(search, 0x100u, "*.%s", extension);
    return I_stricmpwild(search, name) == 0;
}

int __cdecl Sys_CountFileList(char **list)
{
    int i; // [esp+0h] [ebp-4h]

    i = 0;
    if (list)
    {
        while (*list)
        {
            ++list;
            ++i;
        }
    }
    return i;
}

char **__cdecl Sys_ListFiles(
    const char *directory,
    const char *extension,
    const char *filter,
    int *numfiles,
    int wantsubs)
{
    char *v6; // eax
    iosFindData findinfo;
    int flag; // [esp+140h] [ebp-128h]
    char **listCopy; // [esp+144h] [ebp-124h]
    DIR *findhandle;
    char *(*list)[8192]; // [esp+14Ch] [ebp-11Ch]
    int nfiles; // [esp+150h] [ebp-118h] BYREF
    HunkUser *user; // [esp+154h] [ebp-114h]
    int i; // [esp+264h] [ebp-4h]

    // win32 sizes this 0x8000 for 8192 4-byte pointers; LP64 pointers are 8 B.
    // Same for the hunk: filenames plus the pointer copy fit 0x20000 on x86-32
    // but the pointer copy alone doubles on LP64, so double the hunk too.
    LargeLocal list_large_local(0x2000 * sizeof(char *));
    list = (char *(*)[8192])list_large_local.GetBuf();
    if (filter)
    {
        user = Hunk_UserCreate(0x40000, "Sys_ListFiles", 0, 0, 3);
        nfiles = 0;
        Sys_ListFilteredFiles(user, directory, "", filter, (char **)list, &nfiles);
        (*list)[nfiles] = 0;
        *numfiles = nfiles;
        if (nfiles)
        {
            // win32: Hunk_UserAlloc(user, 4 * nfiles + 8, 4) — pointer-sized on LP64
            listCopy = (char **)Hunk_UserAlloc(user, sizeof(char *) * (nfiles + 2), sizeof(char *));
            *listCopy++ = (char *)user; // FS_FreeFileList destroys the hunk via list[-1]
            for (i = 0; i < nfiles; ++i)
                listCopy[i] = (*list)[i];
            listCopy[i] = 0;
            return listCopy;
        }
        else
        {
            Hunk_UserDestroy(user);
            return 0;
        }
    }
    else
    {
        if (!extension)
            extension = "";
        if (*extension != 47 || extension[1])
        {
            flag = 16;
        }
        else
        {
            extension = "";
            flag = 0;
        }
        // win32 passes "<dir>/*.<ext>" to _findfirst; opendir has no pattern,
        // so the HasFileExtension check below carries the whole match.
        nfiles = 0;
        findhandle = opendir(directory);
        if (!findhandle)
        {
            *numfiles = 0;
            return 0;
        }
        else
        {
            user = Hunk_UserCreate(0x40000, "Sys_ListFiles", 0, 0, 3);
            while (!Sys_iOS_FindNext(findhandle, directory, &findinfo))
            {
                if ((!wantsubs && flag != (findinfo.attrib & 0x10) || wantsubs && (findinfo.attrib & 0x10) != 0)
                    && ((findinfo.attrib & 0x10) == 0
                        || I_stricmp(findinfo.name, ".") && I_stricmp(findinfo.name, "..") && I_stricmp(findinfo.name, "CVS"))
                    && (!*extension || HasFileExtension(findinfo.name, extension)))
                {
                    v6 = Hunk_CopyString(user, findinfo.name);
                    (*list)[nfiles++] = v6;
                    if (nfiles == 0x1FFF)
                        break;
                }
            }
            (*list)[nfiles] = 0;
            closedir(findhandle);
            *numfiles = nfiles;
            if (nfiles)
            {
                listCopy = (char **)Hunk_UserAlloc(user, sizeof(char *) * (nfiles + 2), sizeof(char *));
                *listCopy++ = (char *)user;
                for (i = 0; i < nfiles; ++i)
                    listCopy[i] = (*list)[i];
                listCopy[i] = 0;
                return listCopy;
            }
            else
            {
                Hunk_UserDestroy(user);
                return 0;
            }
        }
    }
}

static char cwd[1024];
char *__cdecl Sys_Cwd()
{
    // getcwd() inside an iOS app container returns "/", which the engine would
    // treat as a data root. Every "directory next to the exe" concept from
    // win32 means the app bundle on iOS (see FS_RegisterDvars in com_files.cpp).
    if (!cwd[0])
        I_strncpyz(cwd, Sys_iOS_BundlePath(), sizeof(cwd));
    return cwd;
}

const char *__cdecl Sys_DefaultCDPath()
{
    // win32 returns ""; FS_Startup only mounts fs_cdpath when it differs from
    // fs_basepath, so pointing both at the bundle keeps a single search path.
    return Sys_iOS_BundlePath();
}

static char exePath[1024];
char *__cdecl Sys_DefaultInstallPath()
{
    // win32 derives this from GetModuleFileNameA (the exe's directory); the
    // executable lives inside the app bundle on iOS.
    if (!exePath[0])
        I_strncpyz(exePath, Sys_iOS_BundlePath(), sizeof(exePath));
    return exePath;
}

#endif // KISAK_IOS
