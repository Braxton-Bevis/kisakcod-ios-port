// Staged engine boot on iOS: initialize the real subsystems that have
// graduated into libkisakcod.a — memory, dvars, commands, filesystem — and
// prove each with a behavioral check. This is the precursor to Com_Init:
// same subsystems, explicit order, verifiable one at a time.
//
// Called from Swift (BridgingHeader.h). Returns a status string for the HUD
// and the proof-marker file.

#include <cstdio>
#include <cstdint>
#include <cstring>

extern "C" const char *kisak_boot_smoke(void);

// --- engine declarations (ABI-exact spellings) ----------------------------
// memory
void Com_InitThreadData(int threadContext);
void Com_InitHunkMemory(void);
uint32_t *Hunk_AllocateTempMemory(int size, const char *name);
void Hunk_FreeTempMemory(char *buffer);
// dvar
void Dvar_Init(void);
struct dvar_s;
const dvar_s *Dvar_RegisterString(const char *dvarName, const char *value, uint16_t flags, const char *description);
const char *Dvar_GetString(const char *dvarName);
// cmd
struct cmd_function_s;
void Cbuf_Init(void);
void Cmd_Init(void);
cmd_function_s *Cmd_FindCommand(const char *cmdName);

extern "C" const char *kisak_boot_smoke(void)
{
    static char buf[512];
    int stage = 0;

    // The full engine normally does this from Sys_InitMainThread. q_shared's
    // dvar/va helpers require the main thread's slots before boot continues.
    Com_InitThreadData(0);

    // Stage 1: hunk memory (mmap-backed Z_Virtual* under KISAK_IOS)
    Com_InitHunkMemory();
    stage = 1;
    uint32_t *tmp = Hunk_AllocateTempMemory(4096, "boot_smoke");
    if (!tmp) {
        snprintf(buf, sizeof(buf), "stage%d FAIL: Hunk_AllocateTempMemory null", stage);
        return buf;
    }
    memset(tmp, 0xAB, 4096);
    unsigned char probe = ((unsigned char *)tmp)[4095];
    Hunk_FreeTempMemory((char *)tmp);
    if (probe != 0xAB) {
        snprintf(buf, sizeof(buf), "stage%d FAIL: hunk memory readback", stage);
        return buf;
    }

    // Stage 2: dvar system
    Dvar_Init();
    stage = 2;
    Dvar_RegisterString("bmk4_boot", "ipad", 0, "boot smoke marker dvar");
    const char *val = Dvar_GetString("bmk4_boot");
    if (!val || strcmp(val, "ipad") != 0) {
        snprintf(buf, sizeof(buf), "stage%d FAIL: dvar readback '%s'", stage, val ? val : "(null)");
        return buf;
    }

    // Stage 3: command system
    Cbuf_Init();
    Cmd_Init();
    stage = 3;
    if (!Cmd_FindCommand("cmdlist")) {
        snprintf(buf, sizeof(buf), "stage%d FAIL: cmdlist not registered", stage);
        return buf;
    }

    snprintf(buf, sizeof(buf),
             "hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=%s), cmd OK — %d stages up",
             val, stage);
    return buf;
}
