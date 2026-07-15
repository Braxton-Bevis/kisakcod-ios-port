// Phase 3 Stage B5: aggregate only native, already-earned boot behaviors.
// Swift records the returned text but cannot manufacture the M15 marker.

#include <cstdio>
#include <cstring>

#include <qcommon/cmd.h>
#include <qcommon/qcommon.h>
#include <script/scr_stringlist.h>
#include <universal/com_files.h>

extern "C" bool kisak_boot_event_smoke_succeeded(void);
extern "C" bool kisak_fs_smoke_succeeded(void);

static_assert(sizeof(void *) == 8, "M15 closeout requires the arm64 pointer model");

static void CountDvar(const dvar_s *, void *userData)
{
    ++*static_cast<int *>(userData);
}

static int CountDvars()
{
    int count = 0;
    Dvar_ForEach(CountDvar, &count);
    return count;
}

static void PreserveRedirectBuffer(char *)
{
    // The filtered dvarlist output fits in the supplied buffer. Com_EndRedirect
    // still requires a callback; leaving the bytes in place makes them the
    // native behavioral witness inspected below.
}

extern "C" const char *kisak_boot_m15_closeout(void)
{
    static char status[512];
    static bool entered;
    static char listOutput[4096];
    static char expectedFooter[128];
    static const char setCommand[] = "set bmk4_b5_probe closed";
    static const char listCommand[] = "dvarlist bmk4_b5_probe";
    static const char expectedBefore[] = "before";
    static const char expectedAfter[] = "closed";
    static const char stringProbe[] = "bmk4_b5_sl_lifecycle";

    if (entered)
    {
        snprintf(status, sizeof(status), "M15 FAIL: invoked more than once");
        return status;
    }
    entered = true;

    if (!Com_iOS_BootSpineReached() || !Com_iOS_BootNetInitialized()
        || !Com_iOS_BootEventReady() || Dvar_GetBool("useFastFile")
        || Dvar_GetInt("dedicated") != 2 || !FS_iOS_HeadlessNoAssetsActive())
    {
        snprintf(status, sizeof(status), "M15 FAIL: Com_Init policy/spine");
        return status;
    }
    if (!kisak_boot_event_smoke_succeeded()
        || !kisak_fs_smoke_succeeded())
    {
        snprintf(status, sizeof(status), "M15 FAIL: retained event/filesystem proof");
        return status;
    }

    const char *eventValue = Dvar_GetString("bmk4_b4_probe");
    const char *eventWitness = Dvar_GetString("bmk4_b4_after_invalid");
    if (!eventValue || strcmp(eventValue, "alive") != 0
        || !eventWitness || strcmp(eventWitness, "passed") != 0)
    {
        snprintf(status, sizeof(status), "M15 FAIL: retained event dvar readback");
        return status;
    }

    // Exercise the newly graduated real string-list and memory-tree owners.
    // This proves allocation, lookup, conversion, reference release, and
    // reclamation instead of accepting their mere presence in the archive.
    if (SL_FindString(stringProbe) != 0)
    {
        snprintf(status, sizeof(status), "M15 FAIL: string probe already present");
        return status;
    }
    const uint32_t stringHandle = SL_GetString_(stringProbe, 0, 21);
    const char *stringReadback = stringHandle
        ? SL_ConvertToString(stringHandle) : nullptr;
    if (!stringHandle || !stringReadback
        || strcmp(stringReadback, stringProbe) != 0
        || SL_FindString(stringProbe) != stringHandle)
    {
        if (stringHandle)
            SL_RemoveRefToString(stringHandle);
        snprintf(status, sizeof(status), "M15 FAIL: real string lifecycle readback");
        return status;
    }
    SL_RemoveRefToString(stringHandle);
    if (SL_FindString(stringProbe) != 0)
    {
        snprintf(status, sizeof(status), "M15 FAIL: real string lifecycle release");
        return status;
    }

    const int countBefore = CountDvars();
    if (countBefore <= 24)
    {
        snprintf(status, sizeof(status), "M15 FAIL: dvar count=%d", countBefore);
        return status;
    }
    if (Dvar_FindVar("bmk4_b5_probe"))
    {
        snprintf(status, sizeof(status), "M15 FAIL: probe already registered");
        return status;
    }

    const dvar_s *probe = Dvar_RegisterString(
        "bmk4_b5_probe", expectedBefore, DVAR_NOFLAG,
        "M15 real set/dvarlist closeout probe");
    const char *initial = Dvar_GetString("bmk4_b5_probe");
    if (!probe || !initial || strcmp(initial, expectedBefore) != 0)
    {
        snprintf(status, sizeof(status), "M15 FAIL: command probe registration");
        return status;
    }

    Cbuf_ExecuteBuffer(0, 0, setCommand);
    const char *observed = Dvar_GetString("bmk4_b5_probe");
    if (!observed || strcmp(observed, expectedAfter) != 0)
    {
        snprintf(status, sizeof(status), "M15 FAIL: real set readback");
        return status;
    }

    const int countAfter = CountDvars();
    if (countAfter != countBefore + 1)
    {
        snprintf(status, sizeof(status),
                 "M15 FAIL: registry growth %d->%d", countBefore, countAfter);
        return status;
    }

    Com_BeginRedirect(listOutput, sizeof(listOutput), PreserveRedirectBuffer);
    Cbuf_ExecuteBuffer(0, 0, listCommand);
    Com_EndRedirect();

    snprintf(expectedFooter, sizeof(expectedFooter),
             "\n%d total dvars\n", countAfter);
    if (!strstr(listOutput, " bmk4_b5_probe \"closed\"\n")
        || !strstr(listOutput, expectedFooter))
    {
        snprintf(status, sizeof(status), "M15 FAIL: real dvarlist output");
        return status;
    }

    fprintf(stderr,
            "KISAK_M15_DETAIL dvars=%d set=closed dvarlist=1 fs=1 event=1 sl=1\n",
            countAfter);
    snprintf(status, sizeof(status),
             "Com_Init OK — 4 subsystems up, no assets");
    return status;
}
