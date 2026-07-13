// PmoveScaffoldShared.cpp — generic engine-wide scaffolding for the pmove
// sandbox (BMK4): print/assert/error handlers, Dvar_Register*, tiny Sys_*.
//
// These are exactly the symbols the boot scaffold (ios/Stub/BootScaffold.cpp)
// is likely to provide too. Rule for the combined app link: THEIRS WINS —
// drop this entire file (or individual definitions) if it collides; nothing
// in PmoveSandbox.cpp/PmoveScaffold.cpp depends on these specific bodies
// beyond the documented behavior:
//   - Dvar_Register* must return a dvar whose current value equals the
//     requested default (the real dvar system also guarantees this).
//   - Com_* print handlers may do anything; asserts/errors must not return.

#include <universal/q_shared.h>
#include <qcommon/qcommon.h>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <pthread.h>

#if defined(PMOVE_STANDALONE_SCAFFOLD)

void MyAssertHandler(const char *file, int line, int /*type*/, const char *fmt, ...)
{
    fprintf(stderr, "ENGINE ASSERT %s:%d: ", file, line);
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    abort();
}

void Com_Error(errorParm_t code, const char *fmt, ...)
{
    fprintf(stderr, "Com_Error(%d): %s\n", (int)code, fmt);
    abort();
}

void Com_Printf(int /*channel*/, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

void Com_DPrintf(int /*channel*/, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

void Com_PrintError(int /*channel*/, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

void Com_PrintWarning(int /*channel*/, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

// Exact engine semantics: nearest-even float-to-integer snapping.
void __cdecl Sys_SnapVector(float *v)
{
    v[0] = SnapFloat(v[0]);
    v[1] = SnapFloat(v[1]);
    v[2] = SnapFloat(v[2]);
}

// Minimal dvar registry: hand back a static dvar seeded with the caller's
// default so Jump_RegisterDvars()/Mantle_RegisterDvars() populate jump_*/
// mantle_* with the engine's own values (jump_height 39, stepSize 18, ...).
static dvar_t *sharedAllocDvar(const char *name)
{
    static dvar_t pool[64];
    static int used = 0;
    dvar_t *d = &pool[used++ % 64];
    memset(d, 0, sizeof(*d));
    d->name = name;
    return d;
}

const dvar_s *__cdecl Dvar_RegisterFloat(
    const char *dvarName, float value, DvarLimits /*min*/,
    uint16_t /*flags*/, const char * /*description*/)
{
    dvar_t *d = sharedAllocDvar(dvarName);
    d->current.value = value;
    d->latched.value = value;
    d->reset.value = value;
    return d;
}

const dvar_s *__cdecl Dvar_RegisterBool(
    const char *dvarName, bool value, uint16_t /*flags*/,
    const char * /*description*/)
{
    dvar_t *d = sharedAllocDvar(dvarName);
    d->current.enabled = value;
    d->latched.enabled = value;
    d->reset.enabled = value;
    return d;
}

#else

// common.cpp now owns the Com_* print bodies; keep only helpers that remain
// genuinely absent from the combined archive closure.
// Exact engine semantics from universal/win_shared.cpp: SnapFloat uses the
// same nearest-even conversion as the original x87/SSE implementation.
void __cdecl Sys_SnapVector(float *v)
{
    v[0] = SnapFloat(v[0]);
    v[1] = SnapFloat(v[1]);
    v[2] = SnapFloat(v[2]);
}

#endif
