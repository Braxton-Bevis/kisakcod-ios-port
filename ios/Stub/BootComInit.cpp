// Phase 3 Stage B2: fresh cold-start entry into the real Com_Init owner.
// B1's LP64 preflight stays frozen; common.cpp now owns the temporary
// command/dvar/hunk spine instead of a second initializer in this file.

#include <cstdint>
#include <cstdio>
#include <cstring>

struct dvar_s;
enum DvarSetSource : int
{
    DVAR_SOURCE_EXTERNAL = 1,
};

bool Sys_IsMainThread();
void Com_InitThreadData(int threadContext);
void Dvar_Init();
const dvar_s *Dvar_RegisterEnum(const char *dvarName, const char **valueList,
                                int defaultIndex, uint16_t flags,
                                const char *description);
const char *Dvar_EnumToString(const dvar_s *dvar);
const dvar_s *Dvar_RegisterString(const char *dvarName, const char *value,
                                  uint16_t flags, const char *description);
void Dvar_SetStringFromSource(dvar_s *dvar, char *string, DvarSetSource source);
const char *Dvar_GetString(const char *dvarName);
bool Dvar_GetBool(const char *dvarName);
int Dvar_GetInt(const char *dvarName);
void FS_iOS_SetHeadlessNoAssets(bool enabled);
void Com_Init(char *commandLine);
bool Com_iOS_BootSpineReached();

static_assert(sizeof(void *) == 8, "BootComInit requires the arm64 pointer model");

extern "C" const char *kisak_boot_cominit_stage(void)
{
    static char status[512];
    static bool entered = false;
    static char emptyCommandLine[] = "";
    static char enumZero[] = "zero";
    static char enumOne[] = "one";
    static const char *enumValues[] = { enumZero, enumOne, nullptr };
    static char externalValue[] = "full-pointer-ok";

    if (entered)
    {
        snprintf(status, sizeof(status), "cold path FAIL: invoked more than once");
        return status;
    }
    entered = true;

    if (!Sys_IsMainThread())
    {
        snprintf(status, sizeof(status), "cold path FAIL: not main thread");
        return status;
    }

    // This is the WinMain-equivalent preamble required by q_shared/dvar/va.
    Com_InitThreadData(0);
    Dvar_Init();

    // Refuse a hollow LP64 pass: both source objects must actually require
    // upper pointer bits in this process before their engine lanes are tested.
    if ((reinterpret_cast<uintptr_t>(enumValues) >> 32) == 0
        || (reinterpret_cast<uintptr_t>(externalValue) >> 32) == 0)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: pointers do not exercise LP64");
        return status;
    }

    const dvar_s *enumDvar = Dvar_RegisterEnum(
        "bmk4_cominit_enum", enumValues, 1, 0, "Stage B1 arm64 enum preflight");
    if (!enumDvar || strcmp(Dvar_EnumToString(enumDvar), enumOne) != 0
        || strcmp(Dvar_GetString("bmk4_cominit_enum"), enumOne) != 0)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: enum registration/readback");
        return status;
    }
    Dvar_SetStringFromSource(
        const_cast<dvar_s *>(enumDvar), enumZero, DVAR_SOURCE_EXTERNAL);
    if (strcmp(Dvar_GetString("bmk4_cominit_enum"), enumZero) != 0)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: enum mutation");
        return status;
    }
    Dvar_SetStringFromSource(
        const_cast<dvar_s *>(enumDvar), enumOne, DVAR_SOURCE_EXTERNAL);
    if (strcmp(Dvar_GetString("bmk4_cominit_enum"), enumOne) != 0)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: enum restore");
        return status;
    }

    const dvar_s *externalDvar = Dvar_RegisterString(
        "bmk4_cominit_external", "before", 0x4000,
        "Stage B1 arm64 external-string preflight");
    if (!externalDvar)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: external registration");
        return status;
    }
    Dvar_SetStringFromSource(
        const_cast<dvar_s *>(externalDvar), externalValue, DVAR_SOURCE_EXTERNAL);
    const char *externalReadback = Dvar_GetString("bmk4_cominit_external");
    if (!externalReadback || strcmp(externalReadback, externalValue) != 0
        || (reinterpret_cast<uintptr_t>(externalReadback) >> 32) == 0)
    {
        snprintf(status, sizeof(status), "dvar preflight FAIL: external string pointer lane");
        return status;
    }

    // Explicit headless policy: common.cpp registers useFastFile=0 and
    // dedicated=2 from this request before its guarded B2 boundary returns.
    FS_iOS_SetHeadlessNoAssets(true);
    Com_Init(emptyCommandLine);
    if (!Com_iOS_BootSpineReached() || Dvar_GetBool("useFastFile")
        || Dvar_GetInt("dedicated") != 2)
    {
        snprintf(status, sizeof(status), "common spine FAIL: policy/fence postcondition");
        return status;
    }

    snprintf(status, sizeof(status),
             "dvar enum/external string OK — cold Dvar_Init path");
    return status;
}

extern "C" const char *kisak_boot_common_spine_status(void)
{
    if (!Com_iOS_BootSpineReached())
        return "common spine FAIL: Com_Init did not reach B2 fence";
    if (Dvar_GetBool("useFastFile") || Dvar_GetInt("dedicated") != 2)
        return "common spine FAIL: headless policy drift";
    return "Com_Init entered — useFastFile=0, dedicated=2, sv/cl tails fenced";
}
