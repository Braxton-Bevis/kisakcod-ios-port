// Phase 3 Stage B4: an owned console event must cross the real iOS queue,
// Com_EventLoop, Cbuf/Cmd dispatch, and dvar command owner before this marker
// can be formatted.

#include <cstdio>
#include <cstring>

#include <qcommon/qcommon.h>
#include <win32/win_local.h>

static bool s_eventSmokeSucceeded;

static bool QueueConsoleEvent(const char *command)
{
    const int length = static_cast<int>(strlen(command)) + 1;
    char *ownedCommand = reinterpret_cast<char *>(Com_AllocEvent(length));
    if (!ownedCommand)
        return false;
    memcpy(ownedCommand, command, length);
    Sys_QueEvent(0, SE_CONSOLE, 0, 0, length, ownedCommand);
    return true;
}

extern "C" const char *kisak_boot_event_smoke(void)
{
    static char status[512];
    static bool entered;
    static const char expectedInitial[] = "before";
    static const char expectedFinal[] = "alive";
    static const char expectedWitness[] = "passed";

    if (entered)
    {
        snprintf(status, sizeof(status), "event FAIL: invoked more than once");
        return status;
    }
    entered = true;

    if (!Com_iOS_BootEventReady())
    {
        snprintf(status, sizeof(status), "event FAIL: B4 boundary not ready");
        return status;
    }

    const dvar_s *probe = Dvar_RegisterString(
        "bmk4_b4_probe", expectedInitial, DVAR_NOFLAG,
        "B4 real console-event command probe");
    const dvar_s *witness = Dvar_RegisterString(
        "bmk4_b4_after_invalid", expectedInitial, DVAR_NOFLAG,
        "B4 command execution witness after invalid input");
    const char *initial = Dvar_GetString("bmk4_b4_probe");
    const char *witnessInitial = Dvar_GetString("bmk4_b4_after_invalid");
    if (!probe || !witness || !initial || !witnessInitial
        || strcmp(initial, expectedInitial) != 0
        || strcmp(witnessInitial, expectedInitial) != 0)
    {
        snprintf(status, sizeof(status), "event FAIL: probe registration");
        return status;
    }

    // All three commands are drained by one Com_EventLoop pass. The first
    // mutates the probe; the second is invalid `set` syntax (no value); the
    // third witness proves execution continued after rejecting it.
    if (!QueueConsoleEvent("set bmk4_b4_probe alive")
        || !QueueConsoleEvent("set bmk4_b4_probe")
        || !QueueConsoleEvent("set bmk4_b4_after_invalid passed"))
    {
        snprintf(status, sizeof(status), "event FAIL: queue allocation");
        return status;
    }
    if (!Com_iOS_RunBootEventLoopOnce())
    {
        snprintf(status, sizeof(status), "event FAIL: real event pass refused");
        return status;
    }

    const char *observed = Dvar_GetString("bmk4_b4_probe");
    const char *witnessObserved = Dvar_GetString("bmk4_b4_after_invalid");
    if (!observed || strcmp(observed, expectedFinal) != 0)
    {
        snprintf(status, sizeof(status), "event FAIL: readback=%s",
                 observed ? observed : "<null>");
        return status;
    }
    if (!witnessObserved || strcmp(witnessObserved, expectedWitness) != 0)
    {
        snprintf(status, sizeof(status), "event FAIL: invalid command stopped execution");
        return status;
    }

    s_eventSmokeSucceeded = true;
    snprintf(status, sizeof(status),
             "Com_EventLoop OK — queued console event set bmk4_b4_probe=alive, invalid cmd rejected");
    return status;
}

extern "C" bool kisak_boot_event_smoke_succeeded(void)
{
    return s_eventSmokeSucceeded;
}
