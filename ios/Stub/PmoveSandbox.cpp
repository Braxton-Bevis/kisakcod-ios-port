// PmoveSandbox.cpp — BMK4: the REAL CoD4 player-movement code (src/bgame/
// bg_pmove.cpp, compiled unmodified into libkisakcod.a) executing against a
// synthetic flat-ground world on iOS.
//
// The world model lives here: CoD4's pmove reaches the world exclusively
// through pmoveHandlers[pm->handler].trace (Quake lineage), so implementing
// G_TraceCapsule / CG_TraceCapsule as a swept-AABB clip against the z=0
// halfspace gives the genuine physics an infinite flat floor to run on.
//
// Owned by the pmove-sandbox task. Engine-symbol stubs the closure needs live
// in PmoveScaffold.cpp; generic ones BootScaffold.cpp might also define live
// in PmoveScaffoldShared.cpp (drop that file at app-link time on collision).

#include <bgame/bg_public.h>
#include <bgame/bg_local.h>

#include <cstdio>
#include <cstring>
#include <cmath>

// Real engine entry point (defined in src_bgame_bg_pmove.cpp.o):
//   void __cdecl Pmove(pmove_t *pm);
void __cdecl Pmove(pmove_t *pm);
// Dvar-seeding registrars from the real bg_jump.cpp / bg_mantle.cpp objects.
void __cdecl Jump_RegisterDvars();
void __cdecl Mantle_RegisterDvars();

// ---------------------------------------------------------------------------
// Flat world: solid halfspace z <= 0, walkable normal (0,0,1).
// ---------------------------------------------------------------------------
static const float kGroundZ = 0.0f;
static const float kClipEpsilon = 0.03125f; // Quake SURFACE_CLIP_EPSILON

static void Sandbox_FlatTrace(
    trace_t *results,
    const float *start,
    const float *mins,
    const float *maxs,
    const float *end,
    int /*passEntityNum*/,
    int contentMask)
{
    (void)maxs;
    memset(results, 0, sizeof(*results));
    results->fraction = 1.0f;
    results->hitType = TRACE_HITTYPE_NONE;

    if ((contentMask & 1 /*CONTENTS_SOLID*/) == 0)
        return;

    const float lo = mins ? mins[2] : 0.0f;      // bottom of the swept box
    const float d0 = (start[2] + lo) - kGroundZ; // signed dist at start
    const float d1 = (end[2] + lo) - kGroundZ;   // signed dist at end

    if (d0 >= 0.0f && d1 >= 0.0f)
        return;                                   // fully above: no hit

    results->normal[2] = 1.0f;
    results->contents = 1;                        // CONTENTS_SOLID
    results->walkable = true;                     // normal.z >= 0.7
    results->hitType = TRACE_HITTYPE_ENTITY;      // world hits report
    results->hitId = ENTITYNUM_WORLD;             //   ENTITYNUM_WORLD (CG_Trace)

    if (d0 < 0.0f)
    {
        // began inside the floor
        results->startsolid = true;
        if (d1 < 0.0f)
            results->allsolid = true;
        results->fraction = 0.0f;
        return;
    }

    float f = (d0 - kClipEpsilon) / (d0 - d1);    // back off so endpos rests
    if (f < 0.0f) f = 0.0f;                       // just above the plane
    if (f > 1.0f) f = 1.0f;
    results->fraction = f;
}

// The pmoveHandlers[] table (static const in bg_public.h, instantiated per-TU
// inside bg_pmove.cpp.o) references these three by name; the sandbox bodies
// below are what bind the engine's movement to the flat world.
void __cdecl G_TraceCapsule(
    trace_t *results, const float *start, const float *mins,
    const float *maxs, const float *end, int passEntityNum, int contentmask)
{
    Sandbox_FlatTrace(results, start, mins, maxs, end, passEntityNum, contentmask);
}

void __cdecl CG_TraceCapsule(
    trace_t *results, const float *start, const float *mins,
    const float *maxs, const float *end, int passEntityNum, int contentMask)
{
    Sandbox_FlatTrace(results, start, mins, maxs, end, passEntityNum, contentMask);
}

void __cdecl G_PlayerEvent(int /*clientNum*/, int /*event*/) {}

// ---------------------------------------------------------------------------
// Sandbox state: one player standing at the origin of the flat world.
// ---------------------------------------------------------------------------
static playerState_s s_ps;
static pmove_t s_pm;
static double s_timeMs;               // accumulated sim time (float dt in)
static const int kStartTimeMs = 10000;

extern "C" void kisak_pmove_init(void)
{
    // Seed the dvar-backed movement tunables owned by the real bg_jump.o /
    // bg_mantle.o (jump_height=39, jump_stepSize=18, mantle_enable, ...).
    // The combined app reaches the real dvar registry through BootComInit;
    // PMOVE_STANDALONE_SCAFFOLD provides the same defaults to the CLI harness.
    static bool dvarsDone = false;
    if (!dvarsDone)
    {
        Jump_RegisterDvars();
        Mantle_RegisterDvars();
        dvarsDone = true;
    }

    memset(&s_ps, 0, sizeof(s_ps));
    s_ps.commandTime = kStartTimeMs;
    s_ps.pm_type = PM_NORMAL;
    s_ps.pm_flags = 0;
    s_ps.gravity = 800;                     // g_gravity default
    s_ps.speed = 190;                       // g_speed default
    s_ps.origin[0] = 0.0f;
    s_ps.origin[1] = 0.0f;
    s_ps.origin[2] = 0.0f;                  // standing on the z=0 floor
    s_ps.groundEntityNum = ENTITYNUM_WORLD;
    s_ps.clientNum = 0;
    s_ps.viewHeightTarget = 60;             // standing view height
    s_ps.viewHeightCurrent = 60.0f;
    s_ps.stats[0] = 100;                    // health
    s_ps.stats[2] = 100;                    // maxhealth
    s_ps.weapon = 0;                        // unarmed: weaponDef paths inert
    s_ps.moveSpeedScaleMultiplier = 1.0f;   // PM_CmdScale multiplies by this
    s_ps.holdBreathScale = 1.0f;

    memset(&s_pm, 0, sizeof(s_pm));
    s_pm.ps = &s_ps;
    s_pm.tracemask = 0x2810011;             // server-side alive tracemask
    s_pm.handler = 1;                       // pmoveHandlers[1] = G_TraceCapsule
    s_pm.cmd.serverTime = kStartTimeMs;
    s_pm.oldcmd = s_pm.cmd;

    s_timeMs = 0.0;
}

static char clampToMoveChar(float v)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (char)(int)(v * 127.0f);
}

extern "C" const char *kisak_pmove_frame(
    float forwardmove, float rightmove, int jump, int sprint, float dtMs)
{
    static char status[192];

    if (!std::isfinite(dtMs) || dtMs <= 0.0f)
        dtMs = 1.0f;
    if (dtMs > 50.0f)
        dtMs = 50.0f;

    s_timeMs += (double)dtMs;
    int serverTime = kStartTimeMs + (int)(s_timeMs + 0.5);
    if (serverTime <= s_ps.commandTime)
        serverTime = s_ps.commandTime + 1;

    // usercmd_s uses the -127..127 char convention for moves and BUTTON_*
    // bits (bg_pmove checks 0x400 for jump via Jump_Check, 0x2 for sprint).
    s_pm.cmd.serverTime = serverTime;
    s_pm.cmd.forwardmove = clampToMoveChar(forwardmove);
    s_pm.cmd.rightmove = clampToMoveChar(rightmove);
    s_pm.cmd.buttons = 0;
    if (jump)
        s_pm.cmd.buttons |= 0x400;          // BUTTON_JUMP
    if (sprint)
        s_pm.cmd.buttons |= 0x002;          // BUTTON_SPRINT
    s_pm.cmd.weapon = 0;
    s_pm.cmd.offHandIndex = 0;
    s_pm.cmd.angles[0] = 0;                 // look straight ahead (+X)
    s_pm.cmd.angles[1] = 0;
    s_pm.cmd.angles[2] = 0;

    Pmove(&s_pm);                            // REAL engine movement

    float speed2d = sqrtf(s_ps.velocity[0] * s_ps.velocity[0] +
                          s_ps.velocity[1] * s_ps.velocity[1]);
    snprintf(status, sizeof(status),
             "org=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) speed=%.0f ground=%d",
             s_ps.origin[0], s_ps.origin[1], s_ps.origin[2],
             s_ps.velocity[0], s_ps.velocity[1], s_ps.velocity[2],
             speed2d, s_ps.groundEntityNum != ENTITYNUM_NONE ? 1 : 0);
    return status;
}

static float Sandbox_Speed2D()
{
    return sqrtf(s_ps.velocity[0] * s_ps.velocity[0] +
                 s_ps.velocity[1] * s_ps.velocity[1]);
}

static const char kPmoveProofOK[] =
    "real bg_pmove OK: walk+jump+land+friction on synthetic z=0";

extern "C" const char *kisak_pmove_proof(void)
{
    static char result[256];
    const float dt = 1000.0f / 60.0f;
    float walkDistance = 0.0f;
    float walkSpeed = 0.0f;
    float maxZ = 0.0f;
    float landZ = 9999.0f;
    int airFrames = 0;
    int landedFrame = -1;

    kisak_pmove_init();
    for (int frame = 0; frame < 240; ++frame)
    {
        const float forward = frame < 120 ? 1.0f : 0.0f;
        const int jump = frame == 60 ? 1 : 0;
        kisak_pmove_frame(forward, 0.0f, jump, 0, dt);

        const float distance = sqrtf(s_ps.origin[0] * s_ps.origin[0] +
                                     s_ps.origin[1] * s_ps.origin[1]);
        if (frame == 59)
        {
            walkDistance = distance;
            walkSpeed = Sandbox_Speed2D();
        }
        if (s_ps.origin[2] > maxZ)
            maxZ = s_ps.origin[2];

        const bool grounded = s_ps.groundEntityNum != ENTITYNUM_NONE;
        if (!grounded)
            ++airFrames;
        else if (airFrames > 0 && landedFrame < 0)
        {
            landedFrame = frame;
            landZ = s_ps.origin[2];
        }
    }

    const float finalSpeed = Sandbox_Speed2D();
    const bool finalGrounded = s_ps.groundEntityNum != ENTITYNUM_NONE;
    const bool walked = walkDistance > 24.0f && walkSpeed > 20.0f;
    const bool jumped = maxZ > 8.0f && airFrames >= 8;
    const bool landed = landedFrame > 60
        && fabsf(landZ - kGroundZ) < 0.25f
        && fabsf(s_ps.origin[2] - kGroundZ) < 0.25f
        && finalGrounded;
    const bool stopped = finalSpeed < 1.0f;

    if (walked && jumped && landed && stopped)
        return kPmoveProofOK;

    snprintf(result, sizeof(result),
             "real bg_pmove FAIL: walk=%.2f/%.2f apex=%.2f air=%d land=%d/%.3f final=%.3f ground=%d",
             walkDistance, walkSpeed, maxZ, airFrames, landedFrame, landZ,
             finalSpeed, finalGrounded ? 1 : 0);
    return result;
}

// ---------------------------------------------------------------------------
// Standalone physics-reality test. The same invariant proof is called by the
// app and CI; the CLI exits nonzero for any behavioral failure.
// ---------------------------------------------------------------------------
#ifdef PMOVE_TEST_MAIN
int main(void)
{
    const char *result = kisak_pmove_proof();
    puts(result);
    return strcmp(result, kPmoveProofOK) == 0 ? 0 : 1;
}
#endif
