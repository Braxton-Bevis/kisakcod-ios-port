// Post-initialization M13 behavioral probes. BootComInit.cpp owns the fresh
// initialization order; this file only re-earns the frozen hunk/dvar/command
// marker after those subsystems are already up.
//
// Called from Swift (BridgingHeader.h). Returns a status string for the HUD
// and the proof-marker file.

#include <cstdio>
#include <cstdint>
#include <cstring>

// --- engine declarations (ABI-exact spellings) ----------------------------
// memory
uint32_t *Hunk_AllocateTempMemory(int size, const char *name);
void Hunk_FreeTempMemory(char *buffer);
// dvar
struct dvar_s;
const dvar_s *Dvar_RegisterString(const char *dvarName, const char *value, uint16_t flags, const char *description);
const char *Dvar_GetString(const char *dvarName);
// cmd
struct cmd_function_s;
cmd_function_s *Cmd_FindCommand(const char *cmdName);

extern "C" const char *kisak_boot_probe_after_init(void)
{
    static char buf[512];
    int stage = 0;

    // Stage 1: hunk memory (already initialized, mmap-backed on KISAK_IOS)
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

    // Stage 2: dvar system (already initialized by the cold entry)
    stage = 2;
    Dvar_RegisterString("bmk4_boot", "ipad", 0, "boot smoke marker dvar");
    const char *val = Dvar_GetString("bmk4_boot");
    if (!val || strcmp(val, "ipad") != 0) {
        snprintf(buf, sizeof(buf), "stage%d FAIL: dvar readback '%s'", stage, val ? val : "(null)");
        return buf;
    }

    // Stage 3: command system (already initialized by the cold entry)
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
