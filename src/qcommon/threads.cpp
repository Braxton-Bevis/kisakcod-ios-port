#include "threads.h"

#include <Windows.h>

#ifdef KISAK_IOS
// Real pthreads implementation of the Win32 threading surface this file uses
// (DEPENDENCY_MAP §3). <Windows.h> above resolves to DXVK's windows_base.h
// shim (DWORD/HANDLE/BOOL/WAIT_TIMEOUT); everything behavioral lives here.
#include <pthread.h>
#include <pthread/qos.h>   // pthread_set_qos_class_self_np / pthread_get_qos_class_np
#include <mach/mach.h>     // thread_suspend/thread_resume for runtime Suspend/ResumeThread
#include <unistd.h>        // usleep, sysconf(_SC_NPROCESSORS_ONLN)
#include <sched.h>         // sched_yield for Sleep(0)
#include <errno.h>         // ETIMEDOUT
#include <time.h>          // clock_gettime for pthread_cond_timedwait deadlines
#include <stdlib.h>
#endif

#include <universal/assertive.h>

#include <gfx_d3d/rb_drawprofile.h>
#include <gfx_d3d/r_init.h>
#include <win32/win_local.h>

#ifdef KISAK_MP
#include <client_mp/client_mp.h>
#elif KISAK_SP
#include <client/client.h>
#endif
#include <gfx_d3d/rb_backend.h>

uint32_t Win_InitThreads();


// NOTE(mrsteyk): keep in mind this is 4 elements long.
static thread_local void **g_threadLocals;

//static uint32_t s_affinityMaskForProcess;
//static uint32_t s_cpuCount;
//static uint32_t s_affinityMaskForCpu[4];

#ifdef KISAK_SP
int isDoingDatabaseInit;

void *wakeServerEvent;
void *serverCompletedEvent;
void *allowSendClientMessagesEvent;
void *serverSnapshotEvent;
void *clientMessageReceived;
void *g_saveHistoryEvent;
void *g_saveHistoryDoneEvent;

volatile int g_timeout;
#endif

typedef void (*ThreadFuncFn)(uint32_t);
static ThreadFuncFn threadFunc[THREAD_CONTEXT_COUNT];

void *g_threadValues[THREAD_CONTEXT_COUNT][4];
DWORD threadId[THREAD_CONTEXT_COUNT];
HANDLE threadHandle[THREAD_CONTEXT_COUNT];
uint32_t s_affinityMaskForProcess;
uint32_t s_cpuCount;
uint32_t s_affinityMaskForCpu[4];

#ifdef KISAK_IOS
// ---------------------------------------------------------------------------
// iOS threading shims (DEPENDENCY_MAP §3). File-local functions carrying the
// Win32 names keep every decomp call site below byte-identical:
//   CreateEventA/SetEvent/ResetEvent/WaitForSingleObject
//                       -> pthread_mutex_t + pthread_cond_t + signaled flag
//   CreateThread(CREATE_SUSPENDED) + first ResumeThread
//                       -> pthread_create + a start-gate condvar
//   runtime SuspendThread/ResumeThread (worker parking in r_workercmds.cpp,
//   Sys_SuspendOtherThreads fatal-error freeze)
//                       -> mach thread_suspend/thread_resume. Counted and
//                          asynchronous exactly like Win32 SuspendThread —
//                          including its suspended-while-holding-a-lock
//                          hazard, which the engine already lives with.
//   SetThreadPriority   -> QoS classes, applied on the thread itself (Darwin
//                          is self-only) or latched while parked at the gate
//   Interlocked*        -> __atomic builtins, seq_cst (the decomp was written
//                          against x86-TSO; don't relax orderings for it)
// ---------------------------------------------------------------------------

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF // winbase.h; absent from DXVK's windows_base.h
#endif

struct SysEventIOS
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;
    bool manualReset;
};

static HANDLE CreateEventA(void *lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName)
{
    (void)lpEventAttributes;
    (void)lpName;
    SysEventIOS *event = (SysEventIOS *)malloc(sizeof(SysEventIOS));
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);
    event->signaled = bInitialState != 0;
    event->manualReset = bManualReset != 0;
    return event; // engine events live for the whole process; never destroyed
}

static BOOL SetEvent(HANDLE hEvent)
{
    SysEventIOS *event = (SysEventIOS *)hEvent;
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    if (event->manualReset)
        pthread_cond_broadcast(&event->cond);
    else
        pthread_cond_signal(&event->cond); // auto-reset releases one waiter
    pthread_mutex_unlock(&event->mutex);
    return 1;
}

static BOOL ResetEvent(HANDLE hEvent)
{
    SysEventIOS *event = (SysEventIOS *)hEvent;
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
    return 1;
}

static DWORD WaitForSingleObject(HANDLE hEvent, DWORD timeoutMsec)
{
    SysEventIOS *event = (SysEventIOS *)hEvent;
    pthread_mutex_lock(&event->mutex);
    if (timeoutMsec == INFINITE)
    {
        while (!event->signaled)
            pthread_cond_wait(&event->cond, &event->mutex);
    }
    else if (timeoutMsec != 0 && !event->signaled)
    {
        timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeoutMsec / 1000;
        deadline.tv_nsec += (long)(timeoutMsec % 1000) * 1000000;
        if (deadline.tv_nsec >= 1000000000)
        {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000;
        }
        while (!event->signaled)
        {
            if (pthread_cond_timedwait(&event->cond, &event->mutex, &deadline) == ETIMEDOUT)
                break;
        }
    }
    // timeoutMsec == 0 is the engine's polling idiom: just sample the state.
    DWORD result;
    if (event->signaled)
    {
        if (!event->manualReset)
            event->signaled = false; // auto-reset consumes the signal
        result = 0;                  // WAIT_OBJECT_0
    }
    else
    {
        result = WAIT_TIMEOUT;
    }
    pthread_mutex_unlock(&event->mutex);
    return result;
}

static inline uint32_t InterlockedIncrement(volatile uint32_t *addend)
{
    return __atomic_add_fetch(addend, 1u, __ATOMIC_SEQ_CST);
}

static inline uint32_t InterlockedDecrement(volatile uint32_t *addend)
{
    return __atomic_sub_fetch(addend, 1u, __ATOMIC_SEQ_CST);
}

static inline void *InterlockedExchangePointer(void *volatile *target, void *value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t InterlockedCompareExchange(volatile uint32_t *dest, uint32_t exchange, uint32_t comparand)
{
    __atomic_compare_exchange_n(dest, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand; // initial value of *dest, like Win32
}

#ifdef KISAK_SP
static void Sleep(DWORD msec)
{
    if (msec)
        usleep(msec * 1000);
    else
        sched_yield(); // Win32 Sleep(0) relinquishes the timeslice
}
#endif

struct SysThreadIOS
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t pthread;
    ThreadContext_t context;
    uint32_t suspendGateCount; // CREATE_SUSPENDED emulation; >0 while parked pre-start
    bool started;              // gate passed: Suspend/Resume switch to mach from here on
    int pendingPriority;       // Win32 priority set while parked; applied at gate exit
    qos_class_t baseQos;       // QoS the thread was born with == Win32 priority 0
};

// One record per engine thread; HANDLEs in threadHandle[] point in here.
static SysThreadIOS s_iosThreads[THREAD_CONTEXT_COUNT];

static qos_class_t Sys_iOS_ThreadBirthQos()
{
    qos_class_t qos = QOS_CLASS_UNSPECIFIED;
    pthread_get_qos_class_np(pthread_self(), &qos, NULL);
    return qos == QOS_CLASS_UNSPECIFIED ? QOS_CLASS_DEFAULT : qos;
}

static void Sys_iOS_ApplyPriorityToSelf(SysThreadIOS *thread, int priority)
{
    // Win32 relative priorities used by the engine: -1 (main during load,
    // single-core database), 0 (normal), +1 (multi-core database load).
    qos_class_t qos;
    if (priority > 0)
        qos = QOS_CLASS_USER_INITIATED;
    else if (priority < 0)
        qos = QOS_CLASS_UTILITY;
    else
        qos = thread->baseQos;
    pthread_set_qos_class_self_np(qos, 0);
}

static BOOL SetThreadPriority(HANDLE hThread, int priority)
{
    SysThreadIOS *thread = (SysThreadIOS *)hThread;
    pthread_mutex_lock(&thread->mutex);
    thread->pendingPriority = priority; // latched for the gate if not yet started
    bool started = thread->started;
    pthread_mutex_unlock(&thread->mutex);
    // Darwin can only change QoS from the target thread itself. Every call in
    // this file is either self (main-thread load priorities, asserted) or on
    // a thread still parked at the start gate (database), so self + latch
    // covers the real surface; anything else is dropped.
    if (started && pthread_equal(thread->pthread, pthread_self()))
        Sys_iOS_ApplyPriorityToSelf(thread, priority);
    return 1;
}

static DWORD SuspendThread(HANDLE hThread)
{
    SysThreadIOS *thread = (SysThreadIOS *)hThread;
    pthread_mutex_lock(&thread->mutex);
    if (thread->started)
        thread_suspend(pthread_mach_thread_np(thread->pthread)); // counted, like Win32
    else
        ++thread->suspendGateCount;
    pthread_mutex_unlock(&thread->mutex);
    return 0;
}

static DWORD ResumeThread(HANDLE hThread)
{
    SysThreadIOS *thread = (SysThreadIOS *)hThread;
    pthread_mutex_lock(&thread->mutex);
    if (thread->started)
        thread_resume(pthread_mach_thread_np(thread->pthread));
    else if (thread->suspendGateCount != 0 && --thread->suspendGateCount == 0)
        pthread_cond_broadcast(&thread->cond);
    pthread_mutex_unlock(&thread->mutex);
    return 0;
}

static void *Sys_iOS_ThreadTrampoline(void *arg)
{
    SysThreadIOS *thread = (SysThreadIOS *)arg;

    // Win32 CreateThread reported the id to the creator before the thread
    // ran; here the thread records itself. Until the gate opens the slot may
    // briefly read 0, which only makes Sys_Is*Thread() answer 'no' — correct,
    // since the thread hasn't reached engine code yet.
    uint64_t tid64 = 0;
    pthread_threadid_np(NULL, &tid64);
    threadId[thread->context] = (DWORD)tid64;
    thread->baseQos = Sys_iOS_ThreadBirthQos();

    // CREATE_SUSPENDED: park until the first Sys_ResumeThread.
    pthread_mutex_lock(&thread->mutex);
    while (thread->suspendGateCount > 0)
        pthread_cond_wait(&thread->cond, &thread->mutex);
    thread->started = true;
    int pendingPriority = thread->pendingPriority;
    pthread_mutex_unlock(&thread->mutex);

    Sys_iOS_ApplyPriorityToSelf(thread, pendingPriority);
    Sys_ThreadMain(thread->context);
    return 0;
}

static HANDLE Sys_iOS_CreateEngineThread(ThreadContext_t threadContext)
{
    SysThreadIOS *thread = &s_iosThreads[threadContext];
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    thread->context = threadContext;
    thread->suspendGateCount = 1; // CREATE_SUSPENDED (dwCreationFlags = 4)
    thread->started = false;
    thread->pendingPriority = 0;
    thread->baseQos = QOS_CLASS_DEFAULT;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // iOS secondary threads default to 512KB stacks; Win32 CreateThread(0, 0)
    // gave these 1MB, and ODE's alloca scratch on the physics path needs it.
    pthread_attr_setstacksize(&attr, 0x100000);
    int result = pthread_create(&thread->pthread, &attr, Sys_iOS_ThreadTrampoline, thread);
    pthread_attr_destroy(&attr);
    return result == 0 ? (HANDLE)thread : (HANDLE)0;
}

static HANDLE Sys_iOS_RegisterMainThread()
{
    SysThreadIOS *thread = &s_iosThreads[THREAD_CONTEXT_MAIN];
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    thread->context = THREAD_CONTEXT_MAIN;
    thread->pthread = pthread_self();
    thread->suspendGateCount = 0;
    thread->started = true;
    thread->pendingPriority = 0;
    thread->baseQos = Sys_iOS_ThreadBirthQos();
    return (HANDLE)thread;
}

static void Sys_iOS_InitThreads()
{
    // Replaces Win_InitThreads (win_common.cpp), which walked the process
    // affinity mask. Same policy: the engine only schedules for 1-4 cores
    // (s_affinityMaskForCpu is 4 wide, worker count derives from s_cpuCount).
    // The masks themselves are unused on iOS — there is no hard thread
    // affinity, so Win_SetThreadLock keeps state but pins nothing.
    long cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpuCount < 1)
        cpuCount = 1;
    if (cpuCount > 4)
        cpuCount = 4;
    s_cpuCount = (uint32_t)cpuCount;
    s_affinityMaskForProcess = (1u << s_cpuCount) - 1;
    for (uint32_t cpu = 0; cpu < 4; ++cpu)
        s_affinityMaskForCpu[cpu] = 1u << cpu;
}
#endif // KISAK_IOS

static int g_databaseThreadOwner;

static volatile PVOID smpData;

static volatile uint32_t renderPausedCount;

static WinThreadLock s_threadLock;

static void *renderPausedEvent;
static void *renderCompletedEvent;
static void *noThreadOwnershipEvent;
static void *rendererRunningEvent;
static void *backendEvent[2];
static void *ackendEvent;
static void *updateSpotLightEffectEvent;
static void *updateEffectsEvent;

#ifdef KISAK_MP
static const char* s_threadNames[THREAD_CONTEXT_COUNT] =
{
    "Main",
    "Backend",
    "Worker0",
    "Worker1",
    "Cinematic",
    "Titleserver",
    "Database",
};
#elif KISAK_SP
static const char *s_threadNames[THREAD_CONTEXT_COUNT] =
{
    "MAIN",
    "BACKEND",
    "WORKER0",
    "WORKER1",
    "WORKER2",
    "SERVER",
    "CINEMATIC",
    "TITLE_SERVER",
    "DATABASE",
    "STREAM",
    "SNDSTREAMPACKETCALLBACK",
    "SERVER_DEMO"
};
#endif

uint32_t __cdecl Sys_GetCpuCount()
{
    return s_cpuCount;
}

void __cdecl Sys_EndLoadThreadPriorities()
{
    iassert(Sys_IsMainThread());
    SetThreadPriority(threadHandle[THREAD_CONTEXT_MAIN], 0);
}

int __cdecl Sys_IsRendererReady()
{
    return Sys_WaitForSingleObjectTimeout(&renderCompletedEvent, 0);
}

void __cdecl Sys_InitMainThread()
{
#ifdef KISAK_IOS
    // Win32 DuplicateHandle turned the pseudo-handle into a real one so other
    // threads could act on Main; here the main thread just gets a SysThreadIOS
    // record like every spawned thread.
    threadId[THREAD_CONTEXT_MAIN] = Sys_GetCurrentThreadId();
    threadHandle[THREAD_CONTEXT_MAIN] = Sys_iOS_RegisterMainThread();
    Sys_iOS_InitThreads();
    g_threadLocals = g_threadValues[THREAD_CONTEXT_MAIN];
    Com_InitThreadData(0);
#else
    HANDLE process; // [esp+0h] [ebp-8h]
    HANDLE pseudoHandle; // [esp+4h] [ebp-4h]

    threadId[THREAD_CONTEXT_MAIN] = Sys_GetCurrentThreadId();
    process = GetCurrentProcess();
    pseudoHandle = GetCurrentThread();
    DuplicateHandle(process, pseudoHandle, process, threadHandle, 0, 0, 2);
    Win_InitThreads();
    //*(uint32_t*)(*((uint32_t*)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4) = g_threadValues;
    g_threadLocals = g_threadValues[THREAD_CONTEXT_MAIN];
    Com_InitThreadData(0);
#endif
}

uint32_t __cdecl Sys_GetCurrentThreadId()
{
#ifdef KISAK_IOS
    // Darwin's stable per-thread id is 64-bit; the engine stores and compares
    // 32. The low 32 bits are unique in practice (small monotonic values).
    uint64_t tid64 = 0;
    pthread_threadid_np(NULL, &tid64);
    return (uint32_t)tid64;
#else
    return GetCurrentThreadId();
#endif
}

void __cdecl Sys_InitThread(ThreadContext_t threadContext)
{
    //*(uint32_t*)(*((uint32_t*)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4) = g_threadValues[threadContext];
    g_threadLocals = g_threadValues[threadContext];
    Com_InitThreadData(threadContext);
    Profile_InitContext(threadContext);
}

char __cdecl Sys_SpawnRenderThread(void(__cdecl* function)(uint32_t))
{
    Sys_CreateEvent(0, 0, &renderPausedEvent);
    Sys_CreateEvent(1, 1, &renderCompletedEvent);
    Sys_CreateEvent(1, 0, &noThreadOwnershipEvent);
    Sys_CreateEvent(1, 1, &rendererRunningEvent);
    Sys_CreateEvent(0, 0, &backendEvent[1]);
    Sys_CreateEvent(1, 0, backendEvent);
    Sys_CreateEvent(1, 1, &updateSpotLightEffectEvent);
    Sys_CreateEvent(1, 1, &updateEffectsEvent);

    Sys_CreateThread(function, THREAD_CONTEXT_BACKEND);

    if (!threadHandle[THREAD_CONTEXT_BACKEND])
        return 0;

    Sys_ResumeThread(THREAD_CONTEXT_BACKEND);

    return 1;
}

void __cdecl Sys_CreateEvent(bool manualReset, bool initialState, void** event)
{
    *event = CreateEventA(0, manualReset, initialState, 0);
}

void __cdecl Sys_CreateThread(void(__cdecl* function)(uint32_t), ThreadContext_t threadContext)
{
    iassert( threadFunc[threadContext] == NULL );
    iassert(threadContext < THREAD_CONTEXT_COUNT);
    threadFunc[threadContext] = function;
#ifdef KISAK_IOS
    // pthread_create + a start-gate condvar stands in for CREATE_SUSPENDED;
    // the trampoline records threadId[threadContext] itself. Darwin threads
    // can only name themselves, which Sys_ThreadMain already does with
    // SetThreadName(-1, ...) — the cross-thread naming call here is dropped.
    threadHandle[threadContext] = Sys_iOS_CreateEngineThread(threadContext);
#else
    threadHandle[threadContext] = CreateThread(
        0,
        0,
        (LPTHREAD_START_ROUTINE)Sys_ThreadMain,
        (LPVOID)threadContext,
        4u,
        &threadId[threadContext]);
    SetThreadName(threadId[threadContext], s_threadNames[threadContext]);
#endif
}

#define MS_VC_EXCEPTION 0x406d1388
struct tagTHREADNAME_INFO // sizeof=0x10
{                                     
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.          
};

// https://learn.microsoft.com/en-us/visualstudio/debugger/tips-for-debugging-threads?view=vs-2022&tabs=csharp
void __cdecl SetThreadName(uint32_t threadId, const char* threadName)
{
    tagTHREADNAME_INFO info; // [esp+10h] [ebp-28h] BYREF
    //CPPEH_RECORD ms_exc; // [esp+20h] [ebp-18h]

    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = threadId;
    info.dwFlags = 0;
    
#ifdef TRACY_ENABLE
    TracyCSetThreadName(threadName);
#endif

#ifdef KISAK_IOS
    // No MSVC-debugger SEH protocol on clang/arm64. pthread_setname_np can
    // only name the calling thread; every engine thread names itself from
    // Sys_ThreadMain (threadId == -1), so cross-thread requests are dropped.
    (void)info;
    if (threadId == 0xFFFFFFFF || threadId == Sys_GetCurrentThreadId())
        pthread_setname_np(threadName);
#else
    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#endif
}

uint32_t __stdcall Sys_ThreadMain(ThreadContext_t threadContext)
{
    bcassert(threadContext, THREAD_CONTEXT_COUNT);
    iassert(threadFunc[threadContext]);
    SetThreadName(0xFFFFFFFF, s_threadNames[threadContext]);
    Sys_InitThread(threadContext);
    threadFunc[threadContext](threadContext);
    return 0;
}

static void* wakeDatabaseEvent;
static void* databaseCompletedEvent;
static void* databaseCompletedEvent2;
static void* resumedDatabaseEvent;

bool dediRenderHack = false;

char __cdecl Sys_SpawnDatabaseThread(void(__cdecl* function)(uint32_t))
{
    Sys_CreateEvent(0, 0, &wakeDatabaseEvent);
    Sys_CreateEvent(1, 1, &databaseCompletedEvent);
    Sys_CreateEvent(1, 1, &databaseCompletedEvent2);
    Sys_CreateEvent(1, 1, &resumedDatabaseEvent);

    Sys_CreateThread(function, THREAD_CONTEXT_DATABASE);

    if (!threadHandle[THREAD_CONTEXT_DATABASE])
        return 0;

    SetThreadPriority(threadHandle[THREAD_CONTEXT_DATABASE], s_cpuCount > 1 ? 1 : -1);
    Sys_ResumeThread(THREAD_CONTEXT_DATABASE);

    return 1;
}

void __cdecl Sys_SuspendDatabaseThread(ThreadOwner owner)
{
    iassert( owner != THREAD_OWNER_NONE );

    if (g_databaseThreadOwner)
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1061,
            0,
            "%s\n\t(g_databaseThreadOwner) = %i",
            "(g_databaseThreadOwner == THREAD_OWNER_NONE)",
            g_databaseThreadOwner);

    g_databaseThreadOwner = owner;
    Sys_ResetEvent(&resumedDatabaseEvent);
}

void __cdecl Sys_ResetEvent(void** event)
{
    ResetEvent(*event);
}

void __cdecl Sys_ResumeDatabaseThread(ThreadOwner owner)
{
    iassert( owner != THREAD_OWNER_NONE );
    if (g_databaseThreadOwner != owner)
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1073,
            0,
            "g_databaseThreadOwner == owner\n\t%i, %i",
            g_databaseThreadOwner,
            owner);
    g_databaseThreadOwner = THREAD_OWNER_NONE;
    Sys_SetEvent(&resumedDatabaseEvent);
}

void __cdecl Sys_SetEvent(void** event)
{
    SetEvent(*event);
}

bool __cdecl Sys_HaveSuspendedDatabaseThread(ThreadOwner owner)
{
    return g_databaseThreadOwner == owner;
}

void __cdecl Sys_WaitDatabaseThread()
{
    Sys_WaitForSingleObject(&resumedDatabaseEvent);
}

void __cdecl Sys_WaitForSingleObject(void** event)
{
    uint32_t result; // [esp+0h] [ebp-4h]

    result = WaitForSingleObject(*event, 0xFFFFFFFF);
    iassert(result == ((((uint32_t)0x00000000L)) + 0));
}

bool __cdecl Sys_SpawnWorkerThread(void(__cdecl* function)(uint32_t), uint32_t threadIndex)
{
    ThreadContext_t threadContext; // [esp+0h] [ebp-4h]

    threadContext = (ThreadContext_t)(threadIndex + 2);
    if (threadHandle[threadIndex + 2])
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1113,
            0,
            "%s\n\t(threadContext) = %i",
            "(!threadHandle[threadContext])",
            threadContext);
    Sys_CreateThread(function, threadContext);
    return threadHandle[threadContext] != 0;
}

void __cdecl Sys_SuspendThread(ThreadContext_t threadContext)
{
    iassert( threadHandle[threadContext] );
    SuspendThread(threadHandle[threadContext]);
}

void __cdecl Sys_ResumeThread(ThreadContext_t threadContext)
{
    iassert( threadHandle[threadContext] );
    ResumeThread(threadHandle[threadContext]);
}

void *__cdecl Sys_RendererSleep()
{
    return InterlockedExchangePointer(&smpData, nullptr);
}

int __cdecl Sys_RendererReady()
{
    return smpData != 0;
}

void __cdecl Sys_RenderCompleted()
{
    Sys_SetEvent(&renderCompletedEvent);
    Sys_SetWorkerCmdEvent();
}

void __cdecl Sys_FrontEndSleep()
{
    int newCount; // [esp+0h] [ebp-4h]

    KISAK_NULLSUB();
    if (!Sys_WaitForSingleObjectTimeout(&noThreadOwnershipEvent, 0))
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1206,
            0,
            "%s",
            "Sys_WaitForSingleObjectTimeout( &noThreadOwnershipEvent, 0 )");
    Sys_WaitForSingleObject(&rendererRunningEvent);
    Sys_ResetEvent(&noThreadOwnershipEvent);
    Sys_SetEvent(&backendEvent[1]);
    newCount = InterlockedDecrement(&renderPausedCount);
    if (newCount != -1 && newCount)
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1212,
            0,
            "%s\n\t(newCount) = %i",
            "((newCount == -1) || (newCount == 0))",
            newCount);
    Sys_WaitForSingleObject(&renderPausedEvent);
}

bool __cdecl Sys_WaitForSingleObjectTimeout(void** event, uint32_t msec)
{
    iassert( msec != INFINITE );
    return WaitForSingleObject(*event, msec) == 0;
}

void __cdecl Sys_WakeRenderer(void* data)
{
    Sys_ResetEvent(&renderCompletedEvent);
    const void *old = InterlockedExchangePointer(&smpData, data);
    vassert(!old, "old = %p", old);
    KISAK_NULLSUB();
    Sys_SetEvent(&backendEvent[1]);
    Sys_SetWorkerCmdEvent();
}

void __cdecl Sys_NotifyRenderer()
{
    if (!backendEvent[1])
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1266,
            0,
            "%s",
            "Sys_IsEventInitialized( backendEvent[BACKEND_EVENT_GENERIC] )");
    Sys_SetEvent(&backendEvent[1]);
}

void __cdecl Sys_DatabaseCompleted()
{
#ifdef KISAK_SP 
    Sys_EnterCriticalSection(CRITSECT_START_SERVER);
    isDoingDatabaseInit = 1;
    Sys_LeaveCriticalSection(CRITSECT_START_SERVER);
    Sys_WaitForSingleObject(&serverCompletedEvent);
#endif
    Sys_SetEvent(&databaseCompletedEvent);
}

void __cdecl Sys_WaitStartDatabase()
{
    Sys_WaitForSingleObject(&wakeDatabaseEvent);
}

bool __cdecl Sys_IsDatabaseReady()
{
    return Sys_WaitForSingleObjectTimeout(&databaseCompletedEvent, 0);
}

void __cdecl Sys_SyncDatabase()
{
    Sys_WaitForSingleObject(&databaseCompletedEvent);
}

void __cdecl Sys_WakeDatabase()
{
    Sys_ResetEvent(&databaseCompletedEvent);
}

void __cdecl Sys_NotifyDatabase()
{
    Sys_SetEvent(&wakeDatabaseEvent);
}

void __cdecl Sys_DatabaseCompleted2()
{
#ifdef KISAK_SP
    Sys_EnterCriticalSection(CRITSECT_START_SERVER);
    isDoingDatabaseInit = 0;
    Sys_LeaveCriticalSection(CRITSECT_START_SERVER);
#endif
    Sys_SetEvent(&databaseCompletedEvent2);
}

bool __cdecl Sys_IsDatabaseReady2()
{
    return Sys_WaitForSingleObjectTimeout(&databaseCompletedEvent2, 0);
}

void __cdecl Sys_WakeDatabase2()
{
    Sys_ResetEvent(&databaseCompletedEvent2);
}

bool __cdecl Sys_FinishRenderer()
{
    KISAK_NULLSUB();
    return !Sys_WaitForSingleObjectTimeout(&noThreadOwnershipEvent, 0);
}

int __cdecl Sys_IsMainThreadReady()
{
    return Sys_WaitForSingleObjectTimeout(&noThreadOwnershipEvent, 0);
}

void __cdecl Sys_WaitForMainThread()
{
    Sys_WaitForSingleObject(&noThreadOwnershipEvent);
}

void __cdecl Sys_StopRenderer()
{
    uint32_t newCount; // [esp+0h] [ebp-4h]

    newCount = InterlockedIncrement(&renderPausedCount);
    if (newCount > 1)
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            1734,
            0,
            "%s\n\t(newCount) = %i",
            "((newCount == 0) || (newCount == 1))",
            newCount);
    Sys_ResetEvent(&rendererRunningEvent);
    Sys_SetEvent(&renderPausedEvent);
}

void __cdecl Sys_StartRenderer()
{
    Sys_SetEvent(&rendererRunningEvent);
}

bool __cdecl Sys_IsRenderThread()
{
    return Sys_GetCurrentThreadId() == threadId[THREAD_CONTEXT_BACKEND];
}

bool __cdecl Sys_IsDatabaseThread()
{
    return Sys_GetCurrentThreadId() == threadId[THREAD_CONTEXT_DATABASE];
}

bool __cdecl Sys_IsMainThread()
{
    return Sys_GetCurrentThreadId() == threadId[THREAD_CONTEXT_MAIN];
}

#ifdef KISAK_SP
bool Sys_IsServerThread()
{
#ifdef KISAK_IOS
    return threadId[THREAD_CONTEXT_SERVER] == Sys_GetCurrentThreadId();
#else
    return threadId[THREAD_CONTEXT_SERVER] == GetCurrentThreadId();
#endif
}
#endif

void __cdecl Sys_SetValue(int valueIndex, void* data)
{
    //*(uint32_t*)(*(uint32_t*)(*((uint32_t*)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4) + 4 * valueIndex) = data;
    g_threadLocals[valueIndex] = data;
}

void* __cdecl Sys_GetValue(int valueIndex)
{
    //return *(void**)(*(uint32_t*)(*((uint32_t*)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4) + 4 * valueIndex);
    return g_threadLocals[valueIndex];
}

void __cdecl Sys_WaitForWorkerCmd()
{
    KISAK_NULLSUB();
    Sys_WaitForSingleObjectTimeout(backendEvent, 1u);
}

void __cdecl Sys_SetWorkerCmdEvent()
{
    Sys_SetEvent(backendEvent);
}

void __cdecl Sys_ResetWorkerCmdEvent()
{
    Sys_ResetEvent(backendEvent);
}

int __cdecl Sys_WaitBackendEvent()
{
    return Sys_WaitForSingleObjectTimeout(&backendEvent[1], 0);
}

void __cdecl Sys_SetUpdateSpotLightEffectEvent()
{
    Sys_SetEvent(&updateSpotLightEffectEvent);
}

void __cdecl Sys_ResetUpdateSpotLightEffectEvent()
{
    Sys_ResetEvent(&updateSpotLightEffectEvent);
}

void __cdecl Sys_WaitUpdateNonDependentEffectsCompleted()
{
    KISAK_NULLSUB();
    Sys_WaitForSingleObject(&updateEffectsEvent);
}

void __cdecl Sys_SetUpdateNonDependentEffectsEvent()
{
    Sys_SetEvent(&updateEffectsEvent);
}

void __cdecl Sys_ResetUpdateNonDependentEffectsEvent()
{
    Sys_ResetEvent(&updateEffectsEvent);
}

void __cdecl Sys_SuspendOtherThreads()
{
    uint32_t threadIndex; // [esp+0h] [ebp-8h]
    uint32_t currentThreadId; // [esp+4h] [ebp-4h]

    currentThreadId = Sys_GetCurrentThreadId();
    for (threadIndex = 0; threadIndex < THREAD_CONTEXT_COUNT; ++threadIndex)
    {
        if (threadHandle[threadIndex] && threadId[threadIndex] && threadId[threadIndex] != currentThreadId)
            SuspendThread(threadHandle[threadIndex]);
    }
}

void __cdecl Sys_ReleaseThreadOwnership()
{
    if (Sys_WaitForSingleObjectTimeout(&noThreadOwnershipEvent, 0))
        MyAssertHandler(
            ".\\qcommon\\threads.cpp",
            2000,
            0,
            "%s",
            "!Sys_WaitForSingleObjectTimeout( &noThreadOwnershipEvent, 0 )");
    Sys_SetEvent(&noThreadOwnershipEvent);
}

static void* g_cinematicsThreadOutstandingRequestEvent;
static void* g_cinematicsHostOutstandingRequestEvent;


char __cdecl Sys_SpawnCinematicsThread(void(__cdecl* function)(uint32_t))
{
    Sys_CreateEvent(1, 1, &g_cinematicsThreadOutstandingRequestEvent);
    Sys_CreateEvent(1, 0, &g_cinematicsHostOutstandingRequestEvent);
    Sys_CreateThread(function, THREAD_CONTEXT_CINEMATIC);
    if (!threadHandle[THREAD_CONTEXT_CINEMATIC])
        return 0;
    Sys_ResumeThread(THREAD_CONTEXT_CINEMATIC);
    return 1;
}

bool __cdecl Sys_WaitForCinematicsThreadOutstandingRequestEventTimeout(uint32_t timeoutMsec)
{
    return Sys_WaitForSingleObjectTimeout(&g_cinematicsThreadOutstandingRequestEvent, timeoutMsec);
}

void __cdecl Sys_SetCinematicsThreadOutstandingRequestEvent()
{
    Sys_SetEvent(&g_cinematicsThreadOutstandingRequestEvent);
}

void __cdecl Sys_ResetCinematicsThreadOutstandingRequestEvent()
{
    Sys_ResetEvent(&g_cinematicsThreadOutstandingRequestEvent);
}

bool __cdecl Sys_WaitForCinematicsHostOutstandingRequestEventTimeout(uint32_t timeoutMsec)
{
    return Sys_WaitForSingleObjectTimeout(&g_cinematicsHostOutstandingRequestEvent, timeoutMsec);
}

void __cdecl Sys_SetCinematicsHostOutstandingRequestEvent()
{
    Sys_SetEvent(&g_cinematicsHostOutstandingRequestEvent);
}

void __cdecl Sys_ResetCinematicsHostOutstandingRequestEvent()
{
    Sys_ResetEvent(&g_cinematicsHostOutstandingRequestEvent);
}

void __cdecl Win_SetThreadLock(WinThreadLock threadLock)
{
    if (s_cpuCount != 1 && threadLock != s_threadLock)
    {
        s_threadLock = threadLock;
        iassert(s_cpuCount >= 2);

#ifdef KISAK_IOS
        // iOS has no hard thread affinity (SetThreadAffinityMask has no
        // equivalent; THREAD_AFFINITY_POLICY is a hint the scheduler is free
        // to ignore). Placement stays with the scheduler and the QoS classes,
        // so a lock-level change only updates s_threadLock for readers.
#else
        if (threadLock)
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_MAIN], s_affinityMaskForCpu[0]);
        else
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_MAIN], s_affinityMaskForProcess);

        if (threadLock)
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_BACKEND], s_affinityMaskForCpu[1]);
        else
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_BACKEND], s_affinityMaskForProcess);
        if (threadLock == THREAD_LOCK_ALL)
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_DATABASE], s_affinityMaskForCpu[2 - (s_cpuCount < 3)]);
        else
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_DATABASE], s_affinityMaskForProcess);
        if (threadLock == THREAD_LOCK_ALL)
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_TRACE_COUNT], s_affinityMaskForCpu[s_cpuCount - 1]);
        else
            SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_TRACE_COUNT], s_affinityMaskForProcess);
        if (s_cpuCount >= 3)
        {
            if (threadLock == THREAD_LOCK_ALL)
                SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_WORKER0], s_affinityMaskForCpu[2]);
            else
                SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_WORKER0], s_affinityMaskForProcess);
        }
        if (s_cpuCount >= 4)
        {
            if (threadLock == THREAD_LOCK_ALL)
                SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_WORKER1], s_affinityMaskForCpu[3]);
            else
                SetThreadAffinityMask(threadHandle[THREAD_CONTEXT_WORKER1], s_affinityMaskForProcess);
        }
#endif
    }
}

WinThreadLock __cdecl Win_GetThreadLock()
{
    return s_threadLock;
}

void Win_UpdateThreadLock()
{
    if (s_cpuCount == 1)
    {
        s_threadLock = THREAD_LOCK_ALL;
    }
    else if (RB_IsUsingAnyProfile())
    {
        Win_SetThreadLock(THREAD_LOCK_ALL);
    }
    else
    {
        WinThreadLock threadLock = (WinThreadLock)sys_lockThreads->current.integer;
        if (threadLock == THREAD_LOCK_NONE && R_IsUsingAdaptiveGpuSync())
            threadLock = THREAD_LOCK_MINIMAL;
        Win_SetThreadLock(threadLock);
    }
}

void __cdecl Sys_BeginLoadThreadPriorities()
{
    iassert(Sys_IsMainThread());
    SetThreadPriority(threadHandle[THREAD_CONTEXT_MAIN], -1);
}

#ifdef KISAK_SP
int Sys_WaitStartServer(uint32_t timeout)
{
    int v2; // r3
    int v3; // r30

    Sys_EnterCriticalSection(CRITSECT_START_SERVER);
    v2 = Sys_WaitForSingleObjectTimeout(&wakeServerEvent, timeout);
    v3 = v2;
    if (isDoingDatabaseInit)
    {
        v3 = 0;
    }
    else if (v2)
    {
        ResetEvent(serverCompletedEvent);
    }
    Sys_LeaveCriticalSection(CRITSECT_START_SERVER);
    return v3;
}

void Sys_InitServerEvents()
{
    ResetEvent(wakeServerEvent);
    ResetEvent(serverCompletedEvent);
    SetEvent(allowSendClientMessagesEvent);
    ResetEvent(serverSnapshotEvent);
    SetEvent(clientMessageReceived);
    g_timeout = 0;
}

void Sys_ClientMessageReceived()
{
    SetEvent(clientMessageReceived);
}
void Sys_ClearClientMessage()
{
    ResetEvent(clientMessageReceived);
}
int Sys_SpawnServerThread(void(*function)(uint32_t))
{
    int result; // r3

    wakeServerEvent = CreateEventA(0, 1, 0, 0);
    serverCompletedEvent = CreateEventA(0, 1, 0, 0);
    allowSendClientMessagesEvent = CreateEventA(0, 1, 0, 0);
    serverSnapshotEvent = CreateEventA(0, 0, 0, 0);
    clientMessageReceived = CreateEventA(0, 1, 1, 0);
    Sys_CreateThread(function, THREAD_CONTEXT_SERVER);
    result = (int)threadHandle[THREAD_CONTEXT_SERVER];

    if (threadHandle[THREAD_CONTEXT_SERVER])
    {
        //XSetThreadProcessor(threadHandle[5], 3u);
        Sys_ResumeThread(THREAD_CONTEXT_SERVER);
        return 1;
    }

    return result;
}
void Sys_WaitClientMessageReceived()
{
    uint32_t v0; // r8

    PROF_SCOPED("wait receive msg");
    Sys_WaitForSingleObject(&clientMessageReceived);
}
void Sys_ServerSnapshotCompleted()
{
    SetEvent(serverSnapshotEvent);
}
bool Sys_WaitServerSnapshot()
{
    PROF_SCOPED("wait snapshot");
    return WaitForSingleObject(serverSnapshotEvent, 1) == 0;
}
void Sys_AllowSendClientMessages()
{
    SetEvent(allowSendClientMessagesEvent);
}
void Sys_DisallowSendClientMessages()
{
    ResetEvent(allowSendClientMessagesEvent);
}
int Sys_CanSendClientMessages()
{
    return WaitForSingleObject(allowSendClientMessagesEvent, 0) == 0;
}
void Sys_ServerCompleted()
{
    SetEvent(serverCompletedEvent);
}
int Sys_ServerTimeout()
{
    int time = g_timeout;

    if (!time)
    {
        return 1;
    }

    int timeMS = Sys_Milliseconds();
    if (timeMS - g_timeout >= 0)
    {
        int nextTimeout = timeMS - (int)(-50.0f / com_timescaleValue);

        // shitty atomic looping that is emulated from XBox360
        while (true)
        {
            int current = g_timeout;
            if (current != time)
                break;

            // linux spergs, use: __sync_bool_compare_and_swap()
            int oldVal = InterlockedCompareExchange((volatile uint32_t*)&g_timeout, nextTimeout, current);
            if (oldVal == current)
            {
                return 1;
            }
        }

        // timeout modified by someone else
        return 1;
    }


    return 0;
}
void Sys_WakeServer()
{
    SetEvent(wakeServerEvent);
}
bool Sys_WaitServer()
{
    PROF_SCOPED("wait server");
    return WaitForSingleObject(serverCompletedEvent, 1) == 0;
}
void Sys_SleepServer()
{
    bool v0; // r30

    //PIXBeginNamedEvent_Copy_NoVarArgs(0xFFFFFFFF, "sleep server");
    v0 = WaitForSingleObject(wakeServerEvent, 0) == 0;
    //PIXEndNamedEvent();
    if (v0)
    {
        Sys_EnterCriticalSection(CRITSECT_START_SERVER);
        ResetEvent(wakeServerEvent);
        Sys_LeaveCriticalSection(CRITSECT_START_SERVER);
    }
}
void Sys_Sleep(uint32_t msec)
{
    Sleep(msec);
}
void Sys_SetServerTimeout(int timeout)
{
    int timeMS; // r3

    iassert(timeout >= 0);
    iassert(com_timescaleValue);

    if (timeout)
    {
        //a12 = (int)(float)((float)__SPAIR64__(&a12, timeout) / (float)v13);
        int val = (int)((float)timeout / com_timescaleValue);
        timeMS = Sys_Milliseconds();
        if (g_timeout && timeMS - g_timeout < 0 && g_timeout - (timeMS + val) <= 0)
        {
            //PIXSetMarker(0xFFFFFFFF, "ignore server timeout: %d", a12);
        }
        else
        {
            g_timeout = timeMS + val;
            //PIXSetMarker(0xFFFFFFFF, "server timeout: %d", a12);
        }
    }
    else
    {
        g_timeout = 0;
        //PIXSetMarker(0xFFFFFFFF, "server timeout");
    }
}

bool Sys_WaitForSaveHistoryDone()
{
    return WaitForSingleObject(g_saveHistoryDoneEvent, 0x7D0u) == 0;
}

int Sys_SpawnServerDemoThread(void(*function)(uint32_t))
{
    int result; // r3

    g_saveHistoryEvent = CreateEventA(0, 0, 0, 0);
    g_saveHistoryDoneEvent = CreateEventA(0, 0, 0, 0);
    Sys_CreateThread(function, THREAD_CONTEXT_SERVER_DEMO);
    result = (int)threadHandle[THREAD_CONTEXT_SERVER_DEMO];
    if (threadHandle[THREAD_CONTEXT_SERVER_DEMO])
    {
        //XSetThreadProcessor(threadHandle[11], 2u);
        Sys_ResumeThread(THREAD_CONTEXT_SERVER_DEMO);
        return 1;
    }
    return result;
}

void Sys_SetSaveHistoryEvent()
{
    SetEvent(g_saveHistoryEvent);
}

void Sys_WaitForSaveHistory()
{
    WaitForSingleObject(g_saveHistoryEvent, INFINITE);
}

void Sys_SetSaveHistoryDoneEvent()
{
    SetEvent(g_saveHistoryDoneEvent);
}
#endif