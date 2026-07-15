// iOS system event queue — the win_main.cpp ring-buffer contract without a
// Win32 message pump. UIKit producers call Sys_QueEvent and the real common
// loop consumes the owned payloads through Sys_GetEvent.
#ifdef KISAK_IOS

#include <cstring>

#include <qcommon/qcommon.h>
#include <universal/com_memory.h>
#include <universal/profile.h>
#include <win32/win_local.h>

sysEvent_t eventQue[0x100];
int eventHead;
int eventTail;

static_assert(sizeof(void *) == 8, "iOS event payload ownership requires arm64 pointers");
static_assert(sizeof(sysEvent_t) == 32, "sysEvent_t LP64 layout drift");

char *Sys_ConsoleInput()
{
    // No platform console line exists on iOS; the in-game console queues
    // events through the same Sys_QueEvent entry point.
    return NULL;
}

void __cdecl Sys_QueEvent(
    uint32_t time,
    sysEventType_t type,
    int value,
    int value2,
    int ptrLength,
    void *ptr)
{
    sysEvent_t *ev; // [esp+0h] [ebp-4h]

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    ev = &eventQue[(unsigned __int8)eventHead];
    if (eventHead - eventTail >= 256)
    {
        Com_Printf(16, "Sys_QueEvent: overflow\n");
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
        ++eventTail;
    }
    ++eventHead;
    if (!time)
        time = Sys_Milliseconds();
    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

sysEvent_t *__cdecl Win_GetEvent(sysEvent_t *result)
{
    PROF_SCOPED("Win_GetEvent");

    size_t v2; // [esp+0h] [ebp-50h]
    char *b; // [esp+10h] [ebp-40h]
    char *s; // [esp+34h] [ebp-1Ch]
    sysEvent_t ev; // [esp+38h] [ebp-18h] BYREF

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    if (eventHead <= eventTail)
    {
        // UIKit already ran its message pump and queued any input events.
        {
            PROF_SCOPED("Console Input");
            s = Sys_ConsoleInput();
            if (s)
            {
                v2 = strlen(s);
                b = (char *)Com_AllocEvent(v2 + 1);
                I_strncpyz(b, s, v2);
                Sys_QueEvent(0, SE_CONSOLE, 0, 0, v2 + 1, b);
            }
        }

        if (eventHead <= eventTail)
        {
            memset(&ev, 0, sizeof(ev));
            ev.evTime = Sys_Milliseconds();
        }
        else
        {
            ev = eventQue[(unsigned __int8)eventTail++];
        }
        Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
        *result = ev;
        return result;
    }
    else
    {
        ev = eventQue[(unsigned __int8)eventTail++];
        Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
        *result = ev;
        return result;
    }
}

void Sys_ShutdownEvents()
{
    sysEvent_t *ev; // [esp+0h] [ebp-4h]

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    while (eventHead > eventTail)
    {
        ev = &eventQue[(unsigned __int8)eventTail++];
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
    }
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result)
{
    PROF_SCOPED("Sys_GetEvent");

    sysEvent_t v2; // [esp+0h] [ebp-30h] BYREF
    sysEvent_t v3; // [esp+18h] [ebp-18h]

    v3 = *Win_GetEvent(&v2);
    *result = v3;
    return result;
}

#endif // KISAK_IOS
