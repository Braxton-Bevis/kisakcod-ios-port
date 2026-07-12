// PmoveScaffold.cpp — link scaffolding for the pmove sandbox (BMK4).
//
// Satisfies bg_pmove.cpp.o / bg_jump.o / bg_slidemove.o / bg_mantle.o
// references into engine TUs that have not graduated to iOS yet. Three
// categories, mirroring the boot-scaffold method:
//   [real-minimal] tiny bodies with the engine's actual semantics
//   [no-op]        animation/weapon-visual hooks irrelevant to movement math
//   [data]         dvar-backed movement tunables, statically seeded with the
//                  engine's own registration defaults (from bg_misc.cpp)
//
// Symbols BootScaffold.cpp is likely to also provide (Com_Printf, asserts,
// Dvar_Register*, CRT-ish Sys_*) intentionally live in PmoveScaffoldShared.cpp
// so that file can be dropped wholesale at combined-link time.
//
// Owned by the pmove-sandbox task; bgame/xanim signatures come from the real
// headers so mangling matches the archive objects exactly.

#include <bgame/bg_public.h>
#include <bgame/bg_local.h>
#include <game_mp/g_main_mp.h>
#include <xanim/xanim.h>
#include <xanim/dobj_utils.h>
#include <qcommon/qcommon.h>

#include <cstring>

// ---------------------------------------------------------------------------
// [data] statically seeded dvars (defaults lifted from BG_RegisterDvars in
// bg_misc.cpp / bg_animation_mp.cpp — the TUs that own these pointers).
// ---------------------------------------------------------------------------
static dvar_t *pmoveMakeDvar(const char *name, float v, int i, bool b)
{
    // dvar_s can't be default-constructed (DvarValue has no default ctor),
    // so build zeroed storage and assign the union lanes explicitly.
    static dvar_t pool[96];
    static int used = 0;
    dvar_t *d = &pool[used++ % 96];
    memset(d, 0, sizeof(*d));
    d->name = name;
    d->current.value = v;
    if (i) d->current.integer = i;
    if (b) d->current.enabled = b;
    d->latched = d->current;
    d->reset = d->current;
    return d;
}
static dvar_t *dvF(const char *n, float v) { return pmoveMakeDvar(n, v, 0, false); }
static dvar_t *dvI(const char *n, int v) { return pmoveMakeDvar(n, 0.0f, v, v != 0); }
static dvar_t *dvB(const char *n, bool v) { return pmoveMakeDvar(n, 0.0f, v ? 1 : 0, v); }

// owned by bg_misc.cpp (not yet on iOS)
const dvar_t *friction = dvF("friction", 5.5f);
const dvar_t *stopspeed = dvF("stopspeed", 100.0f);
const dvar_t *inertiaMax = dvF("inertiaMax", 50.0f);
const dvar_t *inertiaAngle = dvF("inertiaAngle", 0.0f);
const dvar_t *inertiaDebug = dvB("inertiaDebug", false);
const dvar_t *bg_fallDamageMinHeight = dvF("bg_fallDamageMinHeight", 128.0f);
const dvar_t *bg_fallDamageMaxHeight = dvF("bg_fallDamageMaxHeight", 300.0f);
const dvar_t *bg_foliagesnd_minspeed = dvF("bg_foliagesnd_minspeed", 40.0f);
const dvar_t *bg_foliagesnd_maxspeed = dvF("bg_foliagesnd_maxspeed", 180.0f);
const dvar_t *bg_foliagesnd_slowinterval = dvI("bg_foliagesnd_slowinterval", 1500);
const dvar_t *bg_foliagesnd_fastinterval = dvI("bg_foliagesnd_fastinterval", 500);
const dvar_t *bg_foliagesnd_resetinterval = dvI("bg_foliagesnd_resetinterval", 500);
const dvar_t *bg_ladder_yawcap = dvF("bg_ladder_yawcap", 100.0f);
const dvar_t *bg_prone_yawcap = dvF("bg_prone_yawcap", 85.0f);
const dvar_t *player_backSpeedScale = dvF("player_backSpeedScale", 0.7f);
const dvar_t *player_strafeSpeedScale = dvF("player_strafeSpeedScale", 0.8f);
const dvar_t *player_strafeAnimCosAngle = dvF("player_strafeAnimCosAngle", 0.5f);
const dvar_t *player_spectateSpeedScale = dvF("player_spectateSpeedScale", 1.0f);
const dvar_t *player_moveThreshhold = dvF("player_moveThreshhold", 10.0f);
const dvar_t *player_footstepsThreshhold = dvF("player_footstepsThreshhold", 0.0f);
const dvar_t *player_meleeChargeFriction = dvF("player_meleeChargeFriction", 1200.0f);
const dvar_t *player_sprintForwardMinimum = dvI("player_sprintForwardMinimum", 105);
const dvar_t *player_sprintSpeedScale = dvF("player_sprintSpeedScale", 1.5f);
const dvar_t *player_sprintStrafeSpeedScale = dvF("player_sprintStrafeSpeedScale", 0.667f);
const dvar_t *player_sprintMinTime = dvF("player_sprintMinTime", 1.0f);
const dvar_t *player_sprintRechargePause = dvF("player_sprintRechargePause", 0.0f);
const dvar_t *player_sprintCameraBob = dvF("player_sprintCameraBob", 0.5f);
const dvar_t *player_turnAnims = dvB("player_turnAnims", false);
const dvar_t *player_view_pitch_up = dvF("player_view_pitch_up", 85.0f);
const dvar_t *player_view_pitch_down = dvF("player_view_pitch_down", 85.0f);
const dvar_t *player_dmgtimer_maxTime = dvF("player_dmgtimer_maxTime", 750.0f);
const dvar_t *player_dmgtimer_minScale = dvF("player_dmgtimer_minScale", 0.0f);
const dvar_t *player_dmgtimer_stumbleTime = dvI("player_dmgtimer_stumbleTime", 500);
const dvar_t *player_dmgtimer_flinchTime = dvI("player_dmgtimer_flinchTime", 500);
// owned by bg_animation_mp.cpp (not yet on iOS)
const dvar_t *xanim_debug = dvB("xanim_debug", false);

// ---------------------------------------------------------------------------
// [data] bgs: the background-game state bg_pmove asserts non-null in
// PM_Footsteps. Zeroed is correct for the sandbox: player_turnAnims=false
// short-circuits the only path that reads into it deeply.
// ---------------------------------------------------------------------------
static bgs_t s_sandbox_bgs;            // ~0xADD08 bytes of BSS
bgs_t *bgs = &s_sandbox_bgs;

// ---------------------------------------------------------------------------
// [real-minimal]
// ---------------------------------------------------------------------------
// exact body of cm_trace.cpp:Trace_GetEntityHitId (TU not yet on iOS)
uint16_t __cdecl Trace_GetEntityHitId(const trace_t *trace)
{
    if (trace->hitType == TRACE_HITTYPE_DYNENT_MODEL
        || trace->hitType == TRACE_HITTYPE_DYNENT_BRUSH)
        return ENTITYNUM_WORLD;
    if (trace->hitType == TRACE_HITTYPE_ENTITY)
        return trace->hitId;
    return ENTITYNUM_NONE;
}

// weapon table isn't loaded: hand back one static zeroed WeaponDef. All the
// movement-relevant fields read by PM (moveSpeedScale, adsMoveSpeedScale,
// freezeMovementWhenFiring, sprint scalers) are zero, which the engine
// treats as "no weapon effect on movement" (and ps->weapon==0 anyway).
WeaponDef *__cdecl BG_GetWeaponDef(uint32_t /*weaponIndex*/)
{
    static WeaponDef def;
    return &def;
}

shellshock_parms_t *__cdecl BG_GetShellshockParms(uint32_t /*index*/)
{
    static shellshock_parms_t parms;   // .movement.affect == 0
    return &parms;
}

int32_t __cdecl BG_GetMaxSprintTime(const playerState_s * /*ps*/)
{
    return 4000;                       // player_sprintTime default 4.0 s
}

void __cdecl PM_ResetWeaponState(playerState_s *ps)
{
    ps->weaponstate = WEAPON_READY;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;
}

int32_t __cdecl PM_WeaponAmmoAvailable(playerState_s * /*ps*/) { return 1; }

// flat ground, nothing overhead: prone is always spatially permitted
char __cdecl BG_CheckProne(
    int32_t, const float *, float, float, float, float *, float *,
    bool, bool, bool, uint8_t, proneCheckType_t, float)
{
    return 1;
}

int32_t __cdecl G_IsServerGameSystem(int32_t /*clientNum*/) { return 0; }

// ---------------------------------------------------------------------------
// [no-op] animation-script / weapon-visual hooks; movement math never reads
// their results in the sandbox (legs/torso anims, predictable event queue).
// ---------------------------------------------------------------------------
void __cdecl BG_AnimUpdatePlayerStateConditions(pmove_t * /*pmove*/) {}
void __cdecl BG_SetConditionValue(uint32_t, uint32_t, uint64_t) {}
void __cdecl BG_CheckThread() {}
int32_t __cdecl BG_AnimScriptEvent(playerState_s *, scriptAnimEventTypes_t, int32_t, int32_t) { return 0; }
int32_t __cdecl BG_AnimScriptAnimation(playerState_s *, aistateEnum_t, scriptAnimMoveTypes_t, int32_t) { return 0; }
int32_t __cdecl BG_PlayAnim(playerState_s *, int32_t, animBodyPart_t, int32_t, int32_t, int32_t, int32_t) { return 0; }
void __cdecl BG_AddPredictableEventToPlayerstate(entity_event_t, uint32_t, playerState_s *) {}
bool __cdecl BG_UsingSniperScope(playerState_s * /*ps*/) { return false; }
bool __cdecl BG_WeaponBlocksProne(uint32_t /*weapIndex*/) { return false; }
void __cdecl PM_Weapon(pmove_t *, pml_t *) {}
void __cdecl PM_AdjustAimSpreadScale(pmove_t *, pml_t *) {}
void __cdecl PM_UpdateAimDownSightFlag(pmove_t *, pml_t *) {}
void __cdecl PM_UpdateAimDownSightLerp(pmove_t *, pml_t *) {}
void __cdecl PM_ExitAimDownSight(playerState_s * /*ps*/) {}
int32_t __cdecl PM_InteruptWeaponWithProneMove(playerState_s * /*ps*/) { return 0; }

// xanim/dobj hooks reached only if a mantle animation actually plays; the
// flat world has no ledges, so these stay inert.
void __cdecl BG_CreateXAnim(XAnim_s *, uint32_t, const char *) {}
XAnim_s *__cdecl XAnimCreateAnims(const char *, uint32_t, void *(__cdecl *)(int)) { return nullptr; }
void __cdecl XAnimBlend(XAnim_s *, uint32_t, const char *, uint32_t, uint32_t, uint32_t) {}
int __cdecl XAnimGetLengthMsec(const XAnim_s *, uint32_t) { return 0; }
void __cdecl XAnimGetAbsDelta(const XAnim_s *, uint32_t, float *rot, float *trans, float)
{
    if (rot) { rot[0] = rot[1] = rot[2] = 0.0f; rot[3] = 1.0f; }
    if (trans) { trans[0] = trans[1] = trans[2] = 0.0f; }
}
void __cdecl DObjSetLocalTag(DObj_s *, int *, uint32_t, const float *, const float *) {}
void __cdecl DObjSetControlTagAngles(DObj_s *, int *, uint32_t, float *) {}
