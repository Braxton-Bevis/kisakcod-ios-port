#include <win32/win_local.h>
#include <win32/win_net.h>

#include <universal/assertive.h>

#include <qcommon/qcommon.h>
#include <qcommon/threads.h>

#ifdef KISAK_IOS
// <direct.h>/<io.h> are MSVC-only; POSIX equivalents for everything below:
#include <unistd.h>      // getcwd, rmdir
#include <sys/stat.h>    // mkdir, stat (directory-bit checks)
#include <dirent.h>      // opendir/readdir replace _findfirst64i32/_findnext64i32
#include <sys/sysctl.h>  // hw.logicalcpu for Win_InitThreads
#include <pthread.h>     // owner tracking for recursive critical sections
#include <atomic>
#include <ios/sys_ios.h> // Sys_iOS_BundlePath for Sys_DefaultInstallPath
#else
#include <direct.h>
#include <io.h>
#endif
#include "com_memory.h"
#include "profile.h"

#if defined(_WIN32)
_RTL_CRITICAL_SECTION s_criticalSections[CRITSECT_COUNT];
#elif defined(KISAK_IOS)
// CRITICAL_SECTION is recursive; std::mutex (the extern type win_local.h
// publishes) is not, and the engine relies on re-entrancy (mem_track's alloc
// lock is explicitly re-entrant — see mem_track.cpp:14). Emulate recursion
// with an owner/depth side table so nested Sys_EnterCriticalSection calls
// from the owning thread don't deadlock.
std::mutex s_criticalSections[CRITSECT_COUNT];
static std::atomic<pthread_t> s_critSectOwner[CRITSECT_COUNT];
static uint32_t s_critSectDepth[CRITSECT_COUNT];
#else
#include <mutex>
std::mutex s_criticalSections[CRITSECT_COUNT];
uint32_t s_criticalSectionsCount[CRITSECT_COUNT] = { 0 };
#endif

#ifdef KISAK_IOS
// Win32 InterlockedIncrement/Decrement return the NEW value; macro aliases
// keep the FastCriticalSection call sites below untouched.
#define InterlockedIncrement(p) __sync_add_and_fetch((p), 1)
#define InterlockedDecrement(p) __sync_sub_and_fetch((p), 1)

// _finddata attrib & 0x10 (FILE_ATTRIBUTE_DIRECTORY) equivalent for a dirent;
// stat() fallback for filesystems that don't fill d_type.
static bool Sys_iOS_EntryIsDir(const char *dirPath, const dirent *entry)
{
    struct stat st;
    char childPath[512];

    if (entry->d_type != DT_UNKNOWN)
        return entry->d_type == DT_DIR;
    Com_sprintf(childPath, 0x200u, "%s/%s", dirPath, entry->d_name);
    return stat(childPath, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

void Sys_InitializeCriticalSections()
{
#if defined(_WIN32)
	for (int critSect = 0; critSect < CRITSECT_COUNT; critSect++) {
		InitializeCriticalSection(&s_criticalSections[critSect]);
	}
#endif
}

void Sys_EnterCriticalSection(int critSect)
{
    PROF_SCOPED("Sys_EnterCriticalSection");

	iassert(critSect >= 0 && critSect < CRITSECT_COUNT);
#if defined(_WIN32)
	EnterCriticalSection(&s_criticalSections[critSect]);
#elif defined(KISAK_IOS)
    pthread_t self = pthread_self();
    if (s_critSectOwner[critSect].load(std::memory_order_relaxed) == self)
    {
        // Recursive acquire — depth is only touched by the owning thread.
        ++s_critSectDepth[critSect];
        return;
    }
    s_criticalSections[critSect].lock();
    s_critSectOwner[critSect].store(self, std::memory_order_relaxed);
    s_critSectDepth[critSect] = 1;
#else
    s_criticalSections[critSect].lock();
    // This is a ghetto hack to see if this is re-entrant
    iassert(s_criticalSectionsCount[critSect] == 0);
    s_criticalSectionsCount[critSect]++;
#endif
}

void Sys_LeaveCriticalSection(int critSect)
{
	iassert(critSect >= 0 && critSect < CRITSECT_COUNT);
#if defined(_WIN32)
	LeaveCriticalSection(&s_criticalSections[critSect]);
#elif defined(KISAK_IOS)
    iassert(s_critSectOwner[critSect].load(std::memory_order_relaxed) == pthread_self());
    if (--s_critSectDepth[critSect] == 0)
    {
        s_critSectOwner[critSect].store((pthread_t)0, std::memory_order_relaxed);
        s_criticalSections[critSect].unlock();
    }
#else
    s_criticalSectionsCount[critSect]--;
    s_criticalSections[critSect].unlock();
#endif
}

void Sys_LockWrite(FastCriticalSection* critSect)
{
    while (1)
    {
        if (critSect->readCount == 0)
        {
            if (InterlockedIncrement(&critSect->writeCount) == 1 && critSect->readCount == 0)
            {
                break;
            }
            InterlockedDecrement(&critSect->writeCount);
        }
        NET_Sleep(0);
    }
}

void Sys_UnlockWrite(FastCriticalSection* critSect)
{
    iassert(critSect->writeCount > 0);
    InterlockedDecrement(&critSect->writeCount);
}

#ifdef KISAK_IOS
uint32_t Win_InitThreads()
{
    // iOS has no settable thread affinity, so the GetProcessAffinityMask walk
    // below collapses to a logical-core query (DEPENDENCY_MAP §7): report the
    // real core count so the engine sizes its scheduling slots correctly, and
    // hand out identity bit masks for the (unused-on-iOS) affinity hints.
    int cpuCount;
    size_t size;
    uint32_t cpu;

    cpuCount = 0;
    size = sizeof(cpuCount);
    if (sysctlbyname("hw.logicalcpu", &cpuCount, &size, NULL, 0) != 0 || cpuCount < 1)
        cpuCount = 1;
    if (cpuCount > 4)
        cpuCount = 4;   // the engine tracks at most 4 slots (s_affinityMaskForCpu)
    s_cpuCount = cpuCount;
    s_affinityMaskForProcess = (1u << cpuCount) - 1;
    for (cpu = 0; cpu < (uint32_t)cpuCount; ++cpu)
        s_affinityMaskForCpu[cpu] = 1u << cpu;
    if (cpuCount == 1)
        s_affinityMaskForCpu[0] = -1;
    return s_affinityMaskForProcess;
}
#else
uint32_t Win_InitThreads()
{
    HANDLE CurrentProcess;
    unsigned long result; 
    unsigned long cpuCount; 
    DWORD_PTR systemAffinityMask;
    unsigned long cpuOffset; 
    DWORD_PTR threadAffinityMask;
    DWORD_PTR affinityMaskBits[33];
    DWORD_PTR processAffinityMask; 

    CurrentProcess = GetCurrentProcess();
    result = GetProcessAffinityMask(CurrentProcess, &processAffinityMask, &systemAffinityMask);
    s_affinityMaskForProcess = processAffinityMask;
    cpuCount = 0;
    for (threadAffinityMask = 1; (processAffinityMask & ((DWORD_PTR)0 - threadAffinityMask)) != 0; threadAffinityMask *= 2)
    {
        if ((processAffinityMask & threadAffinityMask) != 0)
        {
            result = cpuCount;
            affinityMaskBits[cpuCount++] = threadAffinityMask;
            if (cpuCount == 32)
                break;
        }
        result = 2 * threadAffinityMask;
    }
    if (cpuCount > 1)
    {
        s_cpuCount = cpuCount;
        s_affinityMaskForCpu[0] = affinityMaskBits[0];
        result = affinityMaskBits[cpuCount - 1];
        s_affinityMaskForCpu[1] = result;
        if (cpuCount != 2)
        {
            if (cpuCount == 3)
            {
                s_affinityMaskForCpu[2] = affinityMaskBits[1];
            }
            else if (cpuCount == 4)
            {
                s_affinityMaskForCpu[2] = affinityMaskBits[1];
                result = affinityMaskBits[2];
                s_affinityMaskForCpu[3] = affinityMaskBits[2];
            }
            else
            {
                cpuOffset = (cpuCount - 2) / 3;
                iassert(1 + cpuOffset < (cpuCount - 1) - cpuOffset);
                s_affinityMaskForCpu[2] = affinityMaskBits[cpuOffset + 1];
                result = affinityMaskBits[cpuCount - 1 - cpuOffset];
                s_affinityMaskForCpu[3] = result;
                s_cpuCount = 4;
            }
        }
    }
    else
    {
        s_cpuCount = 1;
        s_affinityMaskForCpu[0] = -1;
    }
    return result;
}
#endif

// *(_DWORD *)(*(_DWORD *)(*((_DWORD *)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4)

void __cdecl Sys_Mkdir(const char *path)
{
#ifdef KISAK_IOS
    mkdir(path, 0777);
#else
    _mkdir(path);
#endif
}

#ifdef KISAK_IOS
BOOL __cdecl Sys_RemoveDirTree(const char *path)
{
    // POSIX rewrite of the _findfirst64i32 walk below; constructed child
    // paths use '/' (DEPENDENCY_MAP §6 backslash-path site).
    bool v2;
    DIR *handle;
    dirent *find;
    char childPath[256];
    bool hasError;
    bool hasTrailingSeparater;
    int length;

    length = strlen(path);
    v2 = path[length - 1] == '\\' || path[length - 1] == '/';
    hasTrailingSeparater = v2;
    handle = opendir(path);
    if (!handle)
        return rmdir(path) != -1;
    hasError = 0;
    while (!hasError && (find = readdir(handle)) != NULL)
    {
        if (find->d_name[0] != '.' || find->d_name[1] && (find->d_name[1] != '.' || find->d_name[2]))
        {
            if (hasTrailingSeparater)
                Com_sprintf(childPath, 0x100u, "%s%s", path, find->d_name);
            else
                Com_sprintf(childPath, 0x100u, "%s/%s", path, find->d_name);
            if (Sys_iOS_EntryIsDir(path, find))
                hasError = !Sys_RemoveDirTree(childPath);
            else
                hasError = remove(childPath) == -1;
        }
    }
    closedir(handle);
    return !hasError && rmdir(path) != -1;
}
#else
BOOL __cdecl Sys_RemoveDirTree(const char *path)
{
    bool v2; // [esp+8h] [ebp-250h]
    int handle; // [esp+1Ch] [ebp-23Ch]
    char childPath[256]; // [esp+20h] [ebp-238h] BYREF
    _finddata64i32_t find; // [esp+120h] [ebp-138h] BYREF
    bool hasError; // [esp+252h] [ebp-6h]
    bool hasTrailingSeparater; // [esp+253h] [ebp-5h]
    int length; // [esp+254h] [ebp-4h]

    length = strlen(path);
    v2 = path[length - 1] == 92 || path[length - 1] == 47;
    hasTrailingSeparater = v2;
    if (v2)
        Com_sprintf(childPath, 0x100u, "%s*", path);
    else
        Com_sprintf(childPath, 0x100u, "%s\\*", path);
    handle = _findfirst64i32(childPath, &find);
    if (handle == -1)
        return _rmdir(path) != -1;
    hasError = 0;
    do
    {
        if (find.name[0] != 46 || find.name[1] && (find.name[1] != 46 || find.name[2]))
        {
            if (hasTrailingSeparater)
                Com_sprintf(childPath, 0x100u, "%s%s", path, find.name);
            else
                Com_sprintf(childPath, 0x100u, "%s\\%s", path, find.name);
            if ((find.attrib & 0x10) != 0)
                hasError = !Sys_RemoveDirTree(childPath);
            else
                hasError = remove(childPath) == -1;
        }
    } while (!hasError && _findnext64i32(handle, &find) != -1);
    _findclose(handle);
    return !hasError && _rmdir(path) != -1;
}
#endif

#ifdef KISAK_IOS
void __cdecl Sys_ListFilteredFiles(
    HunkUser *user,
    const char *basedir,
    const char *subdirs,
    const char *filter,
    char **list,
    int *numfiles)
{
    // POSIX rewrite of the "%s\\%s\\*" _findfirst64i32 scan below
    // (DEPENDENCY_MAP §6 backslash-path site); listed names use '/'.
    char filename[256];
    DIR *findhandle;
    dirent *findinfo;
    char search[260];

    if (*numfiles < 0x1FFF)
    {
        if (strlen(subdirs))
            Com_sprintf(search, 0x100u, "%s/%s", basedir, subdirs);
        else
            Com_sprintf(search, 0x100u, "%s", basedir);
        findhandle = opendir(search);
        if (findhandle)
        {
            while ((findinfo = readdir(findhandle)) != NULL)
            {
                if (!Sys_iOS_EntryIsDir(search, findinfo)
                    || I_stricmp(findinfo->d_name, ".") && I_stricmp(findinfo->d_name, "..") && I_stricmp(findinfo->d_name, "CVS"))
                {
                    if (*numfiles >= 0x1FFF)
                        break;
                    if (subdirs)
                        Com_sprintf(filename, 0x100u, "%s/%s", subdirs, findinfo->d_name);
                    else
                        Com_sprintf(filename, 0x100u, "%s", findinfo->d_name);
                    if (Com_FilterPath(filter, filename, 0))
                        list[(*numfiles)++] = Hunk_CopyString(user, filename);
                }
            }
            closedir(findhandle);
        }
    }
}
#else
void __cdecl Sys_ListFilteredFiles(
    HunkUser *user,
    const char *basedir,
    const char *subdirs,
    const char *filter,
    char **list,
    int *numfiles)
{
    char filename[256]; // [esp+10h] [ebp-338h] BYREF
    _finddata64i32_t findinfo; // [esp+110h] [ebp-238h] BYREF
    int findhandle; // [esp+23Ch] [ebp-10Ch]
    char search[260]; // [esp+240h] [ebp-108h] BYREF

    if (*numfiles < 0x1FFF)
    {
        if (strlen(subdirs))
            Com_sprintf(search, 0x100u, "%s\\%s\\*", basedir, subdirs);
        else
            Com_sprintf(search, 0x100u, "%s\\*", basedir);
        findhandle = _findfirst64i32(search, &findinfo);
        if (findhandle != -1)
        {
            do
            {
                if ((findinfo.attrib & 0x10) == 0
                    || I_stricmp(findinfo.name, ".") && I_stricmp(findinfo.name, "..") && I_stricmp(findinfo.name, "CVS"))
                {
                    if (*numfiles >= 0x1FFF)
                        break;
                    if (subdirs)
                        Com_sprintf(filename, 0x100u, "%s\\%s", subdirs, findinfo.name);
                    else
                        Com_sprintf(filename, 0x100u, "%s", findinfo.name);
                    if (Com_FilterPath(filter, filename, 0))
                        list[(*numfiles)++] = Hunk_CopyString(user, filename);
                }
            } while (_findnext64i32(findhandle, &findinfo) != -1);
            _findclose(findhandle);
        }
    }
}
#endif

BOOL __cdecl HasFileExtension(const char *name, const char *extension)
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

#ifdef KISAK_IOS
char **__cdecl Sys_ListFiles(
    const char *directory,
    const char *extension,
    const char *filter,
    int *numfiles,
    int wantsubs)
{
    // POSIX rewrite of the "%s\\*.%s" _findfirst64i32 scan below; the
    // pattern-in-the-path trick becomes opendir + the same HasFileExtension
    // wildcard match win32 applies. Also LP64-corrects the two 4-byte-pointer
    // sizings of the win32 body (0x8000 list buffer, 4 * nfiles + 8 copy).
    char *v6;
    char **listCopy;
    DIR *findhandle;
    dirent *findinfo;
    bool isDir;
    bool wantDirs;
    char *(*list)[8192];
    int nfiles;
    HunkUser *user;
    int i;

    LargeLocal list_large_local(8192 * sizeof(char *));
    list = (char *(*)[8192])list_large_local.GetBuf();
    if (filter)
    {
        user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
        nfiles = 0;
        Sys_ListFilteredFiles(user, directory, "", filter, (char **)list, &nfiles);
        (*list)[nfiles] = 0;
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
    else
    {
        if (!extension)
            extension = "";
        if (*extension != '/' || extension[1])
        {
            wantDirs = 0;   // win32 flag = FILE_ATTRIBUTE_DIRECTORY: match files
        }
        else
        {
            extension = "";
            wantDirs = 1;   // extension "/" means list directories
        }
        nfiles = 0;
        findhandle = opendir(directory);
        if (!findhandle)
        {
            *numfiles = 0;
            return 0;
        }
        else
        {
            user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
            while ((findinfo = readdir(findhandle)) != NULL)
            {
                isDir = Sys_iOS_EntryIsDir(directory, findinfo);
                if ((!wantsubs && isDir == wantDirs || wantsubs && isDir)
                    && (!isDir
                        || I_stricmp(findinfo->d_name, ".") && I_stricmp(findinfo->d_name, "..") && I_stricmp(findinfo->d_name, "CVS"))
                    && (!*extension || HasFileExtension(findinfo->d_name, extension)))
                {
                    v6 = Hunk_CopyString(user, findinfo->d_name);
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
#else
char **__cdecl Sys_ListFiles(
    const char *directory,
    const char *extension,
    const char *filter,
    int *numfiles,
    int wantsubs)
{
    char *v6; // eax
    char **v7; // [esp+4h] [ebp-264h]
    _finddata64i32_t findinfo; // [esp+18h] [ebp-250h] BYREF
    int flag; // [esp+140h] [ebp-128h]
    char **listCopy; // [esp+144h] [ebp-124h]
    int findhandle; // [esp+148h] [ebp-120h]
    char *(*list)[8192]; // [esp+14Ch] [ebp-11Ch]
    int nfiles; // [esp+150h] [ebp-118h] BYREF
    HunkUser *user; // [esp+154h] [ebp-114h]
    char search[256]; // [esp+160h] [ebp-108h] BYREF
    int i; // [esp+264h] [ebp-4h]

    LargeLocal list_large_local(0x8000); // [esp+158h] [ebp-110h] BYREF
    //LargeLocal::LargeLocal(&list_large_local, 0x8000);
    //list = (char *(*)[8192])LargeLocal::GetBuf(&list_large_local);
    list = (char *(*)[8192])list_large_local.GetBuf();
    if (filter)
    {
        user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
        nfiles = 0;
        Sys_ListFilteredFiles(user, directory, "", filter, (char **)list, &nfiles);
        (*list)[nfiles] = 0;
        *numfiles = nfiles;
        if (nfiles)
        {
            listCopy = (char **)Hunk_UserAlloc(user, 4 * nfiles + 8, 4);
            *listCopy++ = (char *)user;
            for (i = 0; i < nfiles; ++i)
                listCopy[i] = (*list)[i];
            listCopy[i] = 0;
            //LargeLocal::~LargeLocal(&list_large_local);
            return listCopy;
        }
        else
        {
            Hunk_UserDestroy(user);
            //LargeLocal::~LargeLocal(&list_large_local);
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
        if (*extension)
            Com_sprintf(search, 0x100u, "%s\\*.%s", directory, extension);
        else
            Com_sprintf(search, 0x100u, "%s\\*", directory);
        nfiles = 0;
        findhandle = _findfirst64i32(search, &findinfo);
        if (findhandle == -1)
        {
            *numfiles = 0;
            //LargeLocal::~LargeLocal(&list_large_local);
            return 0;
        }
        else
        {
            user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
            do
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
            } while (_findnext64i32(findhandle, &findinfo) != -1);
            (*list)[nfiles] = 0;
            _findclose(findhandle);
            *numfiles = nfiles;
            if (nfiles)
            {
                listCopy = (char **)Hunk_UserAlloc(user, 4 * nfiles + 8, 4);
                *listCopy++ = (char *)user;
                for (i = 0; i < nfiles; ++i)
                    listCopy[i] = (*list)[i];
                listCopy[i] = 0;
                v7 = listCopy;
                //LargeLocal::~LargeLocal(&list_large_local);
                return v7;
            }
            else
            {
                Hunk_UserDestroy(user);
                //LargeLocal::~LargeLocal(&list_large_local);
                return 0;
            }
        }
    }
}
#endif


char cwd[256];
char *__cdecl Sys_Cwd()
{
#ifdef KISAK_IOS
    if (!getcwd(cwd, 255))
        cwd[0] = 0;
#else
    _getcwd(cwd, 255);
#endif
    cwd[255] = 0;
    return cwd;
}

const char *__cdecl Sys_DefaultCDPath()
{
    return "";
}

char exePath[256];
#ifdef KISAK_IOS
char *__cdecl Sys_DefaultInstallPath()
{
    // win32 derives this from the exe's directory (GetModuleFileNameA); the
    // iOS analogue is the app bundle — the only place game data can ship
    // (read-only, code-signed). Direct-path writers rooted here (sv_demo,
    // actor_aim dev log) must be redirected when those subsystems land.
    if (!exePath[0])
        I_strncpyz(exePath, Sys_iOS_BundlePath(), 256);
    return exePath;
}
#else
char *__cdecl Sys_DefaultInstallPath()
{
    char *v0; // eax
    uint32_t len; // [esp+0h] [ebp-8h]
    HINSTANCE__ *hinst; // [esp+4h] [ebp-4h]

    if (!exePath[0])
    {
        if (IsDebuggerPresent())
        {
            v0 = Sys_Cwd();
            I_strncpyz(exePath, v0, 256);
        }
        else
        {
            hinst = GetModuleHandleA(0);
            len = GetModuleFileNameA(hinst, exePath, 0x100u);
            if (len == 256)
                len = 255;
            while (len && exePath[len] != 92 && exePath[len] != 47 && exePath[len] != 58)
                --len;
            exePath[len] = 0;
        }
    }
    return exePath;
}
#endif