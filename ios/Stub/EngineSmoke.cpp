// First engine code EXECUTING on iOS: the stub links libkisaksmoke.a — the
// leaf engine objects (com_math, q_shared, msg_mp, huffman, msvc_crt_compat)
// from the census-passing set — and calls real KisakCOD functions, showing
// the results in the HUD and the proof-marker file.
//
// Everything in the "link scaffolding" section exists only to satisfy the
// linker for archive members' references into engine TUs that have not yet
// graduated (common_mp, dvar, client/server state). Trivially-correct ones
// get real bodies; the rest abort loudly if ever reached — none are on the
// smoke-test path. The section shrinks as TUs graduate into the archive.

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <pthread.h>

// --- smoke targets (ABI-exact; mangling verified with nm against the .a) ---
float Vec3NormalizeTo(const float *v, float *out);        // universal/com_math.cpp
int GetMinBitCountForNum(unsigned int num);               // qcommon/msg_mp.cpp (LP64 _BitScanReverse fix)
int I_stricmpwild(const char *wild, const char *s);       // universal/q_shared.cpp

// --- link scaffolding: real-bodied where trivially correct ---------------
enum errorParm_t : int {};
struct clientActive_t;

void MyAssertHandler(const char *file, int line, int type, const char *fmt, ...)
{
    fprintf(stderr, "ENGINE ASSERT %s:%d\n", file, line);
    abort();
}

void Com_Error(errorParm_t code, const char *fmt, ...)
{
    fprintf(stderr, "Com_Error(%d): %s\n", (int)code, fmt);
    abort();
}

void Com_Printf(int channel, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

void Com_PrintError(int channel, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

char *va(const char *fmt, ...) // engine's rotating printf buffer
{
    static char buf[4][1024];
    static int slot;
    char *out = buf[slot = (slot + 1) & 3];
    va_list ap; va_start(ap, fmt); vsnprintf(out, sizeof(buf[0]), fmt, ap); va_end(ap);
    return out;
}

void Com_Memset(void *dst, int val, unsigned long len) { memset(dst, val, len); }
float Q_fabs(float f) { return fabsf(f); }
bool Sys_IsMainThread() { return pthread_main_np() != 0; }
int Sys_Milliseconds()
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
static void *sys_valueSlots[8];
void *Sys_GetValue(int idx) { return sys_valueSlots[idx & 7]; }
void Sys_SetValue(int idx, void *value) { sys_valueSlots[idx & 7] = value; }
void track_static_alloc_internal(void *ptr, int size, const char *name, int type) {}

// Never on the smoke path — abort loudly rather than lie.
#define UNREACHED_STUB(name) { fprintf(stderr, "engine stub reached: %s\n", name); abort(); }
void AngleVectors(const float *angles, float *fwd, float *right, float *up) UNREACHED_STUB("AngleVectors")
int FX_Register(const char *name) UNREACHED_STUB("FX_Register")
void *R_RegisterModel(const char *name) UNREACHED_STUB("R_RegisterModel")
void *Com_FindSoundAlias(const char *name) UNREACHED_STUB("Com_FindSoundAlias")
void *Material_RegisterHandle(const char *name, int imageTrack) UNREACHED_STUB("Material_RegisterHandle")
float *CL_GetMapCenter() UNREACHED_STUB("CL_GetMapCenter")
const char *BG_GetEntityTypeName(int type) UNREACHED_STUB("BG_GetEntityTypeName")
void CL_GetPredictedOriginForServerTime(clientActive_t *cl, int time, float *o, float *a, float *v, int *bob, int *bobCycle) UNREACHED_STUB("CL_GetPredictedOriginForServerTime")
void *MSG_GetStateFieldListForEntityType(int type) UNREACHED_STUB("MSG_GetStateFieldListForEntityType")

// Engine globals owned by not-yet-graduated TUs; zeroed, untouched by the smoke.
void *com_dedicated = nullptr;        // dvar_t*
void *cl_shownet = nullptr;           // dvar_t*
void *msg_dumpEnts = nullptr;         // dvar_t*
void *msg_printEntityNums = nullptr;  // dvar_t*
unsigned char clients[0x200000];      // server client array
unsigned char msgHuff[0x80000];       // network huffman tables
unsigned char orderInfo[0x20000];     // entity-order tables
int com_errorEntered = 0;
int g_script_error_level = 0;
// -------------------------------------------------------------------------

extern "C" const char *kisak_engine_smoke(void)
{
    static char buf[256];

    float v[3] = { 3.0f, 0.0f, 4.0f };
    float out[3] = { 0 };
    float len = Vec3NormalizeTo(v, out);            // expect 5.0, (0.6, 0, 0.8)

    int bits255 = GetMinBitCountForNum(255u);       // expect 8
    int bits1024 = GetMinBitCountForNum(1024u);     // expect 11

    int wild = I_stricmpwild("*.iwd", "IW_00.IWD"); // engine wildcard matcher

    snprintf(buf, sizeof(buf),
             "len=%.2f out=(%.2f,%.2f,%.2f) bits255=%d bits1024=%d wild=%d",
             (double)len, (double)out[0], (double)out[1], (double)out[2],
             bits255, bits1024, wild);
    return buf;
}
