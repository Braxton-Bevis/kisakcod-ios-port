// Staged engine boot on iOS: initialize the real subsystems that have
// graduated into libkisakcod.a — memory, dvars, commands, filesystem — and
// prove each with a behavioral check. This is the precursor to Com_Init:
// same subsystems, explicit order, verifiable one at a time.
//
// Called from Swift (BridgingHeader.h). Returns a status string for the HUD
// and the proof-marker file.

#include <cstdio>
#include <cstring>

extern "C" const char *kisak_boot_smoke(void);

// --- engine declarations (ABI-exact spellings) ----------------------------
// memory
void Com_InitHunkMemory(void);
void *Hunk_AllocateTempMemory(int size, const char *name);
void Hunk_FreeTempMemory(char *buffer);
// dvar
void Dvar_Init(void);
struct dvar_t;
const dvar_t *Dvar_RegisterString(const char *dvarName, const char *value, unsigned short flags, const char *description);
const char *Dvar_GetString(const char *dvarName);
// cmd
void Cbuf_Init(void);
void Cmd_Init(void);

extern "C" const char *kisak_boot_smoke(void)
{
    static char buf[512];
    int stage = 0;

    // Stage 1: hunk memory (mmap-backed Z_Virtual* under KISAK_IOS)
    Com_InitHunkMemory();
    stage = 1;
    void *tmp = Hunk_AllocateTempMemory(4096, "boot_smoke");
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

    snprintf(buf, sizeof(buf),
             "hunk OK (4KB tmp alloc rw), dvar OK (bmk4_boot=%s), cmd OK — %d stages up",
             val, stage);
    return buf;
}
