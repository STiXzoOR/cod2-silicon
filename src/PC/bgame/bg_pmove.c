#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <math.h>
#if defined(COD2_X64)
#include "PC/qcommon/port_debug.h"
#endif

#if defined(COD2_CODX) && COD2_CODX
#include "../qcommon/cod2x.h"
extern const float AngleNormalize180(const float angle);
#endif

extern int CG_PointContents(const vec_t *point, int passEntityNum, int contentmask);
extern void CG_TraceCapsule(trace_t *result, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int skipNumber, int mask);
extern void G_PlayerEvent(int clientNum, int event);
extern void G_TraceCapsule(trace_t *result, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int passEntityNum, int contentmask);
extern int SV_PointContents(const vec_t *point, int passEntityNum, int contentmask);
extern pmoveHandler_t pmoveHandlers[2];
extern viewLerpWaypoint_t viewLerp_CrouchProne[13];
extern viewLerpWaypoint_t viewLerp_CrouchStand[10];
extern viewLerpWaypoint_t viewLerp_ProneCrouch[8];
extern viewLerpWaypoint_t viewLerp_StandCrouch[10];

#define BG_PRONE_TURNED_ABI COD2_REGPARM(2) COD2_SSEREGPARM
#define PM_ACCELERATE_ABI COD2_REGPARM(3) COD2_SSEREGPARM
#define PM_REGPARM2_ABI COD2_REGPARM(2) BM_NOINLINE

extern const float AngleDelta(const float angle1, const float angle2);
extern const float AngleNormalize360Accurate(float angle);
extern const float AngleNormalize180Accurate(float angle);
extern const float vectoyaw(const vec_t *vec);
extern const dvar_t *bg_prone_yawcap;
extern const dvar_t *bg_ladder_yawcap;
extern qboolean BG_CheckProne(int passEntityNum, const vec_t *vPos, const float fSize, const float fHeight, const float fYaw, float *pfTorsoHeight, float *pfTorsoPitch, float *pfWaistPitch, const qboolean bAlreadyProne, const qboolean bOnGround, vec_t *vGroundNormal, int handler, proneCheckType_t proneCheckType, float prone_feet_dist);
extern const vec_t Vec3Normalize(vec_t *v);
extern const vec_t Vec2Normalize(vec_t *v);
extern void Com_Printf(const char *fmt, ...);

extern const dvar_t *friction;
extern const dvar_t *stopspeed;
extern const dvar_t *inertiaMax;
extern const dvar_t *inertiaAngle;
extern const dvar_t *inertiaDebug;
extern const dvar_t *player_view_pitch_up;
extern const dvar_t *player_view_pitch_down;
extern const dvar_t *player_spectateSpeedScale;
extern const dvar_t *player_footstepsThreshhold;
static const vec3_t CorrectSolidDeltas[26] = {

    { 0.0f, 0.0f, 1.0f },
    { -1.0f, 0.0f, 1.0f },
    { 0.0f, -1.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 1.0f },
    { -1.0f, 0.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f },
    { -1.0f, 0.0f, -1.0f },
    { 0.0f, -1.0f, -1.0f },
    { 1.0f, 0.0f, -1.0f },
    { 0.0f, 1.0f, -1.0f },
    { -1.0f, -1.0f, 1.0f },
    { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { -1.0f, 1.0f, 1.0f },
    { -1.0f, -1.0f, 0.0f },
    { 1.0f, -1.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f },
    { -1.0f, -1.0f, -1.0f },
    { 1.0f, -1.0f, -1.0f },
    { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },
};

void PM_trace(pmove_t *pm, trace_t *results, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int passEntityNum, int contentMask);
void PM_AddTouchEnt(pmove_t *pm, int entityNum);
void PM_AddEvent(playerState_t *ps, int newEvent);
int PM_GetEffectiveStance(playerState_t *ps);
int PM_GroundSurfaceType(pml_t *pml);
int PM_GetViewHeightLerpTime(const playerState_t *ps, int iTarget, qboolean bDown);
void PM_SetProneMovementOverride(playerState_t *ps);
float BG_GetSpeed(const playerState_t *ps, int time);
static qboolean BG_PRONE_TURNED_ABI BG_CheckProneTurned(byte *ps_bytes, int handler, float newProneYaw);
qboolean PM_ShouldMakeFootsteps(pmove_t *pm);
void PM_ClipVelocity(const vec_t *in, const vec_t *normal, vec_t *out);
static void PM_ACCELERATE_ABI PM_Accelerate(playerState_t *ps, pml_t *pml, const vec_t *wishdir, float wishspeed, float accel);
static void PM_REGPARM2_ABI PM_Friction(playerState_t *ps, pml_t *pml);
#if defined(COD2_X64)
void PM_UpdateLean(playerState_t *ps, float msec, usercmd_t *cmd, void (*capsuleTrace)(trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, int, int));
#else
void PM_UpdateLean(playerState_t *ps, float msec, usercmd_t *cmd, void (*capsuleTrace)());
#endif
void PM_UpdateViewAngles(playerState_t *ps, float msec, usercmd_t *cmd, int handler);
void PM_UpdatePronePitch(pmove_t *pm, pml_t *pml);
void PM_playerTrace(pmove_t *pm, trace_t *results, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int passEntityNum, int contentMask);
static void PM_REGPARM2_ABI PM_SetMovementDir(pmove_t *pm, pml_t *pml);
static void PM_REGPARM2_ABI PM_AirMove(pmove_t *pm, pml_t *pml);
static void PM_WalkMove(pmove_t *pm, pml_t *pml);
static void PM_NoclipMove(pmove_t *pm, pml_t *pml);
static void PM_REGPARM2_ABI PM_LadderMove(pmove_t *pm, pml_t *pml);
static void PM_REGPARM2_ABI PM_ViewHeightAdjust(pmove_t *pm, pml_t *pml);
static void PM_REGPARM2_ABI PM_CheckDuck(pmove_t *pm, pml_t *pml);
void PM_FootstepEvent(pmove_t *pm, pml_t *pml, int iOldBobCycle, int iNewBobCycle, qboolean bFootStep);
static void PM_REGPARM2_ABI PM_Footsteps(pmove_t *pm, pml_t *pml);
static void PM_REGPARM2_ABI PM_GroundTrace(pmove_t *pm, pml_t *pml);
void Pmove(pmove_t *pm);

extern void PM_StepSlideMove(pmove_t *pm, pml_t *pml, qboolean gravity);
extern void PM_AdjustAimSpreadScale(pmove_t *pm, pml_t *pml);
extern void PM_UpdateAimDownSightFlag(pmove_t *pm, pml_t *pml);
extern void PM_UpdateAimDownSightLerp(pmove_t *pm, pml_t *pml);
extern void PM_Weapon(pmove_t *pm, pml_t *pml);
extern void PM_ResetWeaponState(playerState_t *ps);
extern void BG_AnimUpdatePlayerStateConditions(pmove_t *pmove);
extern int BG_AnimScriptEvent(playerState_t *ps, scriptAnimEventTypes_t event, qboolean isContinue, qboolean force);
extern Bool Jump_Check(pmove_t *pm, pml_t *pml);
extern void Jump_ClearState(playerState_t *ps);
extern float Jump_ReduceFriction(playerState_t *ps);
extern void Mantle_CapView(playerState_t *ps);
extern void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up);
extern void Sys_SnapVector(vec_t *v);

extern void Jump_ActivateSlowdown(playerState_t *ps);
extern void PM_ExitAimDownSight(playerState_t *ps);
extern int BG_PlayAnim(playerState_t *ps, int animNum, animBodyPart_t bodyPart, int forceDuration, qboolean setTimer, qboolean isContinue, qboolean force);
extern float PitchForYawOnNormal(const float fYaw, const vec_t *normal);
extern const dvar_t *bg_fallDamageMinHeight;
extern const dvar_t *bg_fallDamageMaxHeight;

void PM_trace(pmove_t *pm, trace_t *results, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int passEntityNum, int contentMask)
{

    unsigned char handlerByte = pm->handler;
    pmove_trace handler = pmoveHandlers[handlerByte].trace;
    handler(results, start, mins, maxs, end, passEntityNum, contentMask);
}

void PM_AddTouchEnt(pmove_t *pm, int entityNum)
{
    int numtouch;
    int i;

    if (entityNum == 0x3fe)
        return;

    numtouch = pm->numtouch;

    if (numtouch == 0x20)
        return;

    for (i = 0; i < numtouch; i++) {
        if (pm->touchents[i] == entityNum)
            return;
    }

    pm->touchents[numtouch] = entityNum;
    numtouch += 1;
    pm->numtouch = numtouch;
}

void BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm, playerState_t *ps);

void PM_AddEvent(playerState_t *ps, int newEvent)
{
    BG_AddPredictableEventToPlayerstate(newEvent, 0, ps);
}

int PM_GetEffectiveStance(playerState_t *ps)
{
    int val = ps->viewHeightTarget;

    if (val == 0x28)
        return 2;
    return val == 0xb;
}

int PM_GroundSurfaceType(pml_t *pml)
{

    int val = pml->groundTrace.surfaceFlags;

    if (val & 0x2000)
        return 0;
    return (val & 0x1f00000) >> 20;
}

int PM_GetViewHeightLerpTime(const playerState_t *ps, int iTarget, qboolean bDown)
{

    if (iTarget == 0xb)
        return 0x190;

    if (iTarget == 0x28) {
        if (bDown)
            return 0xc8;
        return 0x190;
    }

    return 0xc8;
}

void PM_SetProneMovementOverride(playerState_t *ps)
{
    int flags = ps->pm_flags;
    if (flags & 1)
        ps->pm_flags = flags | 0x800;
}

float BG_GetSpeed(const playerState_t *ps, int time)
{

    if (ps->pm_flags & 0x20) {

        int elapsed = time - ps->jumpTime;
        if (elapsed > 0x1f3)
            return ps->velocity[2];
        return 0.0f;
    }

    {
        float vx = ps->velocity[0];
        float vy = ps->velocity[1];
        return sqrtf(vx * vx + vy * vy);
    }
}

static qboolean BG_CheckProneTurned_impl(byte *ps_bytes, int handler, float newProneYaw)
{
    playerState_t *ps = (playerState_t *)ps_bytes;
    float oldProneYaw = ps->viewangles[1];
    float delta = AngleDelta(newProneYaw, oldProneYaw);
    float absDelta = (float)fabs(delta);
    float t = absDelta / 240.0f;
    float scale = 1.0f - t;
    float adjustedYaw = newProneYaw - (delta * scale);
    float normalizedYaw = AngleNormalize360Accurate(adjustedYaw);
    float proneFeetDist = t * 45.0f + scale * 66.0f;

    return BG_CheckProne(
        ps->clientNum,
        ps->origin,
        ps->maxs[0],
        30.0f,
        normalizedYaw,
        &ps->fTorsoHeight,
        &ps->fTorsoPitch,
        &ps->fWaistPitch,
        1,
        ps->groundEntityNum != 0x3ff ? 1 : 0,
        NULL,
        (unsigned char)handler,
        (proneCheckType_t)0,
        proneFeetDist
    );
}

static qboolean BG_PRONE_TURNED_ABI BG_CheckProneTurned(byte *ps_bytes, int handler, float newProneYaw)
{
    return BG_CheckProneTurned_impl(ps_bytes, handler, newProneYaw);
}

qboolean PM_ShouldMakeFootsteps(pmove_t *pm)
{
    playerState_t *ps = pm->ps;
    int flags, hasSprintFlag, stance;

    flags = ps->pm_flags;
    hasSprintFlag = flags & 0x100;

    stance = ps->viewHeightTarget;
    if (stance == 0x28)
        return 0;
    if (stance == 0xb)
        return 0;

    if (hasSprintFlag)
        return 0;

    {
        float threshold = (player_footstepsThreshhold)->current.value;
        return pm->xyspeed >= threshold;
    }
}

void PM_ClipVelocity(const vec_t *in, const vec_t *normal, vec_t *out)
{
    float dot, scale, overbounce;

    dot = in[0] * normal[0] + in[1] * normal[1] + in[2] * normal[2];

    scale = dot + fabsf(dot) * (-0.001f);
    overbounce = -scale;

    out[0] = in[0] + normal[0] * overbounce;
    out[1] = in[1] + normal[1] * overbounce;
    out[2] = in[2] + normal[2] * overbounce;
}

#define PMF_PRONE 0x00000001
#define PMF_DUCKED 0x00000002
#define PMF_MANTLE 0x00000004
#define PMF_LADDER 0x00000020
#define PMF_ADS 0x00000040
#define PMF_JUMPING 0x00080000
#define ENTITYNUM_WORLD 0x3fe
#define ENTITYNUM_NONE 0x3ff

static float PM_DvarFloat(const dvar_t *dvar, float fallback)
{
    return dvar ? dvar->current.value : fallback;
}

static int PM_DvarInt(const dvar_t *dvar, int fallback)
{
    return dvar ? dvar->current.integer : fallback;
}

static float PM_DotProduct(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float PM_VectorLength2D(const vec_t *v)
{
    return sqrtf(v[0] * v[0] + v[1] * v[1]);
}

static void PM_VectorCopy(const vec_t *src, vec_t *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void PM_VectorClear(vec_t *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

static int PM_AbsInt(int value)
{
    return value < 0 ? -value : value;
}

static float PM_CmdScale(const playerState_t *ps, const usercmd_t *cmd)
{
#if defined(COD2_X64)
    float total = sqrtf((float)(cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove));
    int maxMove = PM_AbsInt(cmd->forwardmove);
    if (PM_AbsInt(cmd->rightmove) > maxMove)
        maxMove = PM_AbsInt(cmd->rightmove);
    if (!maxMove)
        return 0.0f;
    float scale = (float)ps->speed * (float)maxMove / (127.0f * total);
    if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
        scale *= 0.4f;
    if (ps->pm_type == 2)
        scale *= 3.0f;
    if (ps->pm_type == 3)
        scale *= 6.0f;
    if (ps->pm_type == 4)
        scale *= player_spectateSpeedScale->current.value;
    return scale;
#else
    int maxMove;
    float total;
    float scale;

    maxMove = PM_AbsInt(cmd->forwardmove);
    if (PM_AbsInt(cmd->rightmove) > maxMove)
        maxMove = PM_AbsInt(cmd->rightmove);

    if (!maxMove)
        return 0.0f;

    total = sqrtf((float)(cmd->forwardmove * cmd->forwardmove +
                          cmd->rightmove * cmd->rightmove));
    if (total <= 0.0f)
        return 0.0f;

    scale = (float)ps->speed * (float)maxMove / (127.0f * total);

    if ((ps->pm_flags & 0x1) || ps->leanf != 0.0f)
        scale *= 0.40f;

    if (ps->pm_type == 2)
        scale *= 3.0f;
    else if (ps->pm_type == 3)
        scale *= 6.0f;
    else if (ps->pm_type == 4)
        scale *= PM_DvarFloat(player_spectateSpeedScale, 2.0f);
    else {

        int stance = PM_GetEffectiveStance((playerState_t *)ps);
        if (stance == 1)
            scale *= 0.15f;
        else if (stance == 2)
            scale *= 0.65f;
    }

    return scale;
#endif
}
#if defined(COD2_X64)
extern bgs_t *bgs;
extern WeaponDef *BG_GetWeaponDef(int weaponIndex);
extern int BG_AnimScriptAnimation(playerState_t *ps, aistateEnum_t state, scriptAnimMoveTypes_t move, qboolean isContinue);
extern void BG_UpdateConditionValue(int client, int condition, int value, qboolean checkConversion);
extern const dvar_t *player_moveThreshhold;
extern const dvar_t *player_backSpeedScale;
extern const dvar_t *player_strafeSpeedScale;
extern const dvar_t *player_turnAnims;
extern const dvar_t *player_dmgtimer_stumbleTime;
extern const dvar_t *player_dmgtimer_maxTime;
extern const dvar_t *player_dmgtimer_minScale;
extern void Jump_ApplySlowdown(playerState_t *ps);
extern void AddLeanToPosition(vec_t *position, float yaw, float fraction, float viewRoll, float leanDist);
extern float UnGetLeanFraction(float fraction);
extern const float AngleNormalize180(const float angle);
extern const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out);
extern void vectoangles(const vec_t *v, vec_t *angles);
extern void ProjectPointOnPlane(const vec_t *point, const vec_t *normal, vec_t *out);

/* Mac 1.3 inlines this stance transition in walking scale and footsteps. */
static float PM_StanceTransition(const pmove_t *pm, int from, int to)
{
    const playerState_t *ps = pm->ps;
    if (!ps->viewHeightLerpTime || ps->viewHeightLerpTarget != to)
        return 0.0f;
    if ((from < to && ps->viewHeightLerpDown) || (from > to && !ps->viewHeightLerpDown))
        return 0.0f;
    float fraction = (float)(pm->cmd.serverTime - ps->viewHeightLerpTime) /
                     (float)PM_GetViewHeightLerpTime(ps, to, ps->viewHeightLerpDown);
    if (fraction < 0.0f)
        return 0.0f;
    return fraction > 1.0f ? 1.0f : fraction;
}

static float PM_StanceScale(const pmove_t *pm)
{
    float fraction = PM_StanceTransition(pm, 40, 11);
    if (fraction != 0.0f)
        return fraction * 0.15f + (1.0f - fraction) * 0.65f;
    fraction = PM_StanceTransition(pm, 11, 40);
    if (fraction != 0.0f)
        return fraction * 0.65f + (1.0f - fraction) * 0.15f;
    int stance = PM_GetEffectiveStance(pm->ps);
    return stance == 1 ? 0.15f : (stance == 2 ? 0.65f : 1.0f);
}

static float PM_CmdScale_Walk(const pmove_t *pm, const usercmd_t *cmd)
{
    const playerState_t *ps = pm->ps;
    float total = sqrtf((float)(cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove));
    float forward = fabsf((float)cmd->forwardmove * (cmd->forwardmove < 0 ? player_backSpeedScale->current.value : 1.0f));
    float side = fabsf((float)cmd->rightmove * player_strafeSpeedScale->current.value);
    float maxMove = fmaxf(forward, side);
    if (maxMove == 0.0f)
        return 0.0f;
    float scale = (float)ps->speed * maxMove / (127.0f * total);
    if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
        scale *= 0.4f;
    if (ps->pm_type == 2)
        scale *= 3.0f;
    else if (ps->pm_type == 3)
        scale *= 6.0f;
    else
        scale *= PM_StanceScale(pm);
    if (ps->weapon) {
        float weaponScale = BG_GetWeaponDef(ps->weapon)->fMoveSpeedScale;
        if (weaponScale > 0.0f)
            scale *= BG_GetWeaponDef(ps->weapon)->fMoveSpeedScale;
    }
    if (cmd->buttons & 0x800)
        scale *= 0.4f;
    return scale;
}
#endif

static void PM_DropTimers(playerState_t *ps, int msec)
{
#if defined(COD2_X64)
    if (ps->pm_time) {
        if (msec >= ps->pm_time) {
            ps->pm_flags &= ~0x80600;
            ps->pm_time = 0;
        } else {
            ps->pm_time -= msec;
        }
    }
    if (ps->legsTimer > 0) {
        ps->legsTimer -= msec;
        if (ps->legsTimer < 0)
            ps->legsTimer = 0;
    }
    if (ps->torsoTimer > 0) {
        ps->torsoTimer -= msec;
        if (ps->torsoTimer < 0)
            ps->torsoTimer = 0;
    }
#else
    if (ps->pm_time > 0) {
        ps->pm_time -= msec;
        if (ps->pm_time <= 0) {
            ps->pm_time = 0;
            ps->pm_flags &= ~(0x200 | 0x400 | PMF_JUMPING);
        }
    }

    if (ps->weaponTime > 0) {
        ps->weaponTime -= msec;
        if (ps->weaponTime < 0)
            ps->weaponTime = 0;
    }

    if (ps->weaponDelay > 0) {
        ps->weaponDelay -= msec;
        if (ps->weaponDelay < 0)
            ps->weaponDelay = 0;
    }

    if (ps->grenadeTimeLeft > 0) {
        ps->grenadeTimeLeft -= msec;
        if (ps->grenadeTimeLeft < 0)
            ps->grenadeTimeLeft = 0;
    }

    if (ps->weaponRestrictKickTime > 0) {
        ps->weaponRestrictKickTime -= msec;
        if (ps->weaponRestrictKickTime < 0)
            ps->weaponRestrictKickTime = 0;
    }

    if (ps->foliageSoundTime > 0) {
        ps->foliageSoundTime -= msec;
        if (ps->foliageSoundTime < 0)
            ps->foliageSoundTime = 0;
    }

    if (ps->damageTimer > 0) {
        ps->damageTimer -= msec;
        if (ps->damageTimer < 0)
            ps->damageTimer = 0;
    }

    if (ps->damageDuration > 0) {
        ps->damageDuration -= msec;
        if (ps->damageDuration < 0)
            ps->damageDuration = 0;
    }

    if (ps->holdBreathTimer > 0) {
        ps->holdBreathTimer -= msec;
        if (ps->holdBreathTimer < 0)
            ps->holdBreathTimer = 0;
    }
#endif
}

#if defined(COD2_X64)
void PM_UpdateLean(playerState_t *ps, float msec, usercmd_t *cmd, void (*capsuleTrace)(trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, int, int))
#else
void PM_UpdateLean(playerState_t *ps, float msec, usercmd_t *cmd, void (*capsuleTrace)())
#endif
{
#if defined(COD2_X64)
    int leaning = 0;
    if ((cmd->buttons & 0xc0) && !(ps->pm_flags & 0x8000) && ps->pm_type <= 5 &&
        (ps->groundEntityNum != ENTITYNUM_NONE || ps->pm_type == 1)) {
        if (cmd->buttons & 0x40)
            --leaning;
        if (cmd->buttons & 0x80)
            ++leaning;
    }
    if (ps->eFlags & 0x300)
        leaning = 0;
    float limit = PM_GetEffectiveStance(ps) == 1 ? 0.25f : 0.5f;
    float lean = ps->leanf;
    if (!leaning) {
        if (lean > 0.0f) {
            lean += msec / -280.0f * limit;
            if (lean < 0.0f)
                lean = 0.0f;
        } else if (lean < 0.0f) {
            lean += msec / 280.0f * limit;
            if (lean > 0.0f)
                lean = 0.0f;
        }
    } else if (leaning > 0) {
        if (lean < limit)
            lean += msec / 350.0f * limit;
        if (lean > limit)
            lean = limit;
    } else {
        if (lean > -limit)
            lean += msec / -350.0f * limit;
        if (lean < -limit)
            lean = -limit;
    }
    ps->leanf = lean;
    if (lean != 0.0f) {
        vec3_t start, end, mins = {-8.0f, -8.0f, -8.0f}, maxs = {8.0f, 8.0f, 8.0f};
        trace_t trace;
        PM_VectorCopy(ps->origin, start);
        start[2] += ps->viewHeightCurrent;
        PM_VectorCopy(start, end);
        float sign = lean < 0.0f ? -1.0f : 1.0f;
        AddLeanToPosition(end, ps->viewangles[1], sign, 16.0f, 20.0f);
        capsuleTrace(&trace, start, mins, maxs, end, ps->clientNum, 0x2810011);
        float fraction = UnGetLeanFraction(trace.fraction);
        if (fabsf(ps->leanf) > fraction)
            ps->leanf = fraction * (ps->leanf < 0.0f ? -1.0f : 1.0f);
    }
#else
    (void)msec;
    (void)cmd;
    (void)capsuleTrace;
    ps->leanf = 0.0f;
#endif
}

void PM_UpdateViewAngles(playerState_t *ps, float msec, usercmd_t *cmd, int handler)
{
    int i;
    int minPitch;
    int maxPitch;
    float oldViewYaw;
    float newViewYaw;
    int proneBlocked;
    const float shortToAngle = 360.0f / 65536.0f;
    const float angleToShort = 65536.0f / 360.0f;

    if (!ps || !cmd)
        return;

    if (ps->pm_type == 5)
        return;

    if (ps->pm_type > 5) {

        int yaw = (short)(cmd->angles[1] + ps->delta_angles[1]);
        if (ps->stats[1] == 0x3e7)
            ps->stats[1] = (int)((float)yaw * shortToAngle);
        goto do_lean;
    }

    oldViewYaw = ps->viewangles[1];

    minPitch = (int)(PM_DvarFloat(player_view_pitch_up, 85.0f) * angleToShort) & 0xffff;
    maxPitch = (int)(PM_DvarFloat(player_view_pitch_down, 85.0f) * angleToShort) & 0xffff;

    for (i = 0; i < 3; ++i) {
        int angle = (short)(cmd->angles[i] + ps->delta_angles[i]);

        if (i == 0) {
            if (angle > maxPitch) {
                angle = maxPitch;
                ps->delta_angles[0] = angle - cmd->angles[0];
            } else if (angle < -minPitch) {
                angle = -minPitch;
                ps->delta_angles[0] = angle - cmd->angles[0];
            }
        }

        ps->viewangles[i] = (float)angle * shortToAngle;
    }

    newViewYaw = ps->viewangles[1];

    if (ps->eFlags & 0x300) {

        for (i = 0; i < 2; ++i) {
            float delta = AngleDelta(ps->viewAngleClampBase[i], ps->viewangles[i]);
            float range = ps->viewAngleClampRange[i];
            float x;

            if (delta > range)
                x = delta - range;
            else if (-range <= delta)
                continue;
            else
                x = range + delta;

            ps->delta_angles[i] += (int)(x * angleToShort) & 0xffff;
            if (x <= 0.0f)
                ps->viewangles[i] = AngleNormalize360Accurate(
                    ps->viewAngleClampBase[i] + range);
            else
                ps->viewangles[i] = AngleNormalize360Accurate(
                    ps->viewAngleClampBase[i] - range);
        }
        return;
    }

    if (ps->pm_flags & PMF_MANTLE) {
        Mantle_CapView(ps);
        return;
    }

    if ((ps->pm_flags & PMF_LADDER) && ps->groundEntityNum == 0x3ff &&
        bg_ladder_yawcap->current.value != 0.0f) {
        float yaw0 = vectoyaw(ps->vLadderVec) + 180.0f;
        float delta = AngleDelta(yaw0, ps->viewangles[1]);
        float cap = bg_ladder_yawcap->current.value;
        float x;

        if (delta > cap)
            x = delta - cap;
        else if (-cap <= delta)
            goto prone_check;
        else
            x = cap + delta;

        ps->delta_angles[1] += (int)(x * angleToShort) & 0xffff;
        if (x <= 0.0f)
            ps->viewangles[1] = AngleNormalize360Accurate(
                yaw0 + bg_ladder_yawcap->current.value);
        else
            ps->viewangles[1] = AngleNormalize360Accurate(
                yaw0 - bg_ladder_yawcap->current.value);
    }

prone_check:

    if ((ps->pm_flags & 0x1) && !(ps->eFlags & 0x300)) {
        float proneDelta = AngleDelta(ps->proneDirection, ps->viewangles[1]);
        float yawcapM5 = bg_prone_yawcap->current.value - 5.0f;
        float newProneYaw;

        if (proneDelta <= yawcapM5 && -yawcapM5 <= proneDelta &&
            (*(short *)&cmd->forwardmove == 0 ||
             (proneDelta == proneDelta && proneDelta == 0.0f))) {
            proneBlocked = 0;
        } else {

            float t70 = msec * 55.0f;
            float t72 = t70 * 0.001f;

            if (t72 <= (float)fabs(proneDelta)) {
                if (proneDelta <= 0.0f)
                    newProneYaw = ps->proneDirection + t72;
                else
                    newProneYaw = ps->proneDirection + t70 * -0.001f;
            } else {
                newProneYaw = ps->viewangles[1];
            }

            proneBlocked = 0;
            i = 1;
            for (;;) {
                if (BG_CheckProneTurned((byte *)ps, handler, newProneYaw)) {

                    if (BG_CheckProne(ps->clientNum, ps->origin, ps->maxs[0],
                                      30.0f, ps->viewangles[1], 0, 0, 0, 1,
                                      ps->groundEntityNum != 0x3ff, 0, handler,
                                      (proneCheckType_t)0, 45.0f)) {
                        if (BG_CheckProne(ps->clientNum, ps->origin, ps->maxs[0],
                                          30.0f, newProneYaw, 0, 0, 0, 1,
                                          ps->groundEntityNum != 0x3ff, 0,
                                          handler, (proneCheckType_t)0, 45.0f))
                            ps->proneDirection = newProneYaw;
                        else
                            proneBlocked = 1;
                    } else {
                        proneBlocked = 1;
                    }
                    break;
                }
                if (i == 0)
                    break;
                {

                    float d = AngleDelta(ps->proneDirection, newProneYaw);
                    float step;
                    i = ((float)fabs(d) > 1.0f) ? 1 : 0;
                    if (i) {
                        step = (d <= 0.0f) ? -1.0f : 1.0f;
                    } else {
                        proneBlocked = 1;
                        step = d;
                    }
                    newProneYaw = AngleNormalize360Accurate(step + newProneYaw);
                }
            }
        }

        {
            float proneDelta2 = AngleDelta(ps->proneDirection, ps->viewangles[1]);
            if (proneDelta2 != 0.0f) {
                float runYaw = ps->proneDirection;
                int bRetry;
                i = 1;
                for (;;) {

                    bRetry = BG_CheckProne(ps->clientNum, ps->origin, ps->maxs[0],
                                           30.0f, runYaw, 0, 0, 0, 1,
                                           ps->groundEntityNum != 0x3ff, 0,
                                           handler, (proneCheckType_t)0, 45.0f);
                    if (bRetry) {
                        if (BG_CheckProneTurned((byte *)ps, handler, runYaw))
                            ps->proneDirection = runYaw;
                        else
                            goto resolve_step;
                        break;
                    }
                resolve_step:
                    if (i == 0)
                        break;
                    {
                        float step, stepShort;
                        i = ((float)fabs(proneDelta2) > 1.0f) ? 1 : 0;
                        if (i) {
                            if (proneDelta2 <= 0.0f) {
                                step = -1.0f;
                                stepShort = -angleToShort;
                            } else {
                                step = 1.0f;
                                stepShort = angleToShort;
                            }
                        } else {
                            step = proneDelta2;
                            stepShort = proneDelta2 * angleToShort;
                        }

                        ps->delta_angles[1] += (int)stepShort & 0xffff;
                        ps->viewangles[1] = AngleNormalize360Accurate(
                            step + ps->viewangles[1]);

                        proneDelta2 = AngleDelta(ps->proneDirection, ps->viewangles[1]);
                        if (bRetry == 0)
                            runYaw = AngleNormalize360Accurate(runYaw + proneDelta2);
                    }
                    proneBlocked = 1;
                }
            }
        }

        {
            float proneDelta3 = AngleDelta(ps->proneDirection, ps->viewangles[1]);
            float yawcap = bg_prone_yawcap->current.value;
            float x;
            if (proneDelta3 > yawcap)
                x = proneDelta3 - yawcap;
            else if (-yawcap <= proneDelta3)
                goto prone_smooth;
            else
                x = proneDelta3 + yawcap;

            ps->delta_angles[1] += (int)(x * angleToShort) & 0xffff;
            if (x <= 0.0f)
                ps->viewangles[1] = AngleNormalize360Accurate(
                    ps->proneDirection + bg_prone_yawcap->current.value);
            else
                ps->viewangles[1] = AngleNormalize360Accurate(
                    ps->proneDirection - bg_prone_yawcap->current.value);
        }

    prone_smooth:

        if (proneBlocked) {
            float deltaYaw1;
            ps->pm_flags |= 0x10000;
            deltaYaw1 = AngleDelta(oldViewYaw, ps->viewangles[1]);
            if ((float)fabs(deltaYaw1) <= 1.0f) {
                float d2 = AngleDelta(newViewYaw, ps->viewangles[1]);
                if (d2 * deltaYaw1 > 0.0f) {
                    float adj = deltaYaw1 * 0.98f;
                    ps->viewangles[1] = AngleNormalize360Accurate(
                        adj + ps->viewangles[1]);
                    ps->delta_angles[1] += (int)(adj * angleToShort) & 0xffff;
                }
            }
        }

        {
            float pitchDelta = AngleDelta(ps->proneTorsoPitch, ps->viewangles[0]);
            float x;
            if (pitchDelta > 45.0f)
                x = pitchDelta - 45.0f;
            else if (pitchDelta >= -45.0f)
                goto type_check;
            else
                x = pitchDelta + 45.0f;

            ps->delta_angles[0] += (int)(x * angleToShort) & 0xffff;
            if (x <= 0.0f)
                ps->viewangles[0] = AngleNormalize180Accurate(
                    45.0f + ps->proneTorsoPitch);
            else
                ps->viewangles[0] = AngleNormalize180Accurate(
                    ps->proneTorsoPitch - 45.0f);
        }
    }

type_check:

    if (ps->pm_type == 3 || ps->pm_type == 2 || ps->pm_type == 4)
        return;

do_lean:

#if defined(COD2_X64)
    PM_UpdateLean(ps, msec, cmd, pmoveHandlers[(unsigned char)handler].trace);
#else
    PM_UpdateLean(ps, msec, cmd, (void (__cdecl *)(void))(pmoveHandlers[(unsigned char)handler].trace));
#endif
}

void PM_playerTrace(pmove_t *pm, trace_t *results, const vec_t *start,
                    const vec_t *mins, const vec_t *maxs, const vec_t *end,
                    int passEntityNum, int contentMask)
{
    int entityNum;
    int numtouch;
    int i;

    pmoveHandlers[(unsigned char)pm->handler].trace(results, start, mins, maxs,
                                                    end, passEntityNum, contentMask);

    if (!results->startsolid || !(results->contents & 0x02000000))
        return;

    entityNum = results->entityNum;
    if (entityNum != 0x3fe) {
        numtouch = pm->numtouch;
        if (numtouch != 0x20) {
            for (i = 0; i < numtouch; i++) {
                if (pm->touchents[i] == entityNum)
                    goto retry;
            }
            pm->touchents[numtouch] = entityNum;
            numtouch += 1;
            pm->numtouch = numtouch;
        }
    }

retry:
    pm->tracemask &= ~0x02000000;

    pmoveHandlers[(unsigned char)pm->handler].trace(results, start, mins, maxs, end,
                                                    passEntityNum,
                                                    contentMask & ~0x02000000);
}

static void PM_ACCELERATE_ABI PM_Accelerate(playerState_t *ps, pml_t *pml, const vec_t *wishdir,
                                            float wishspeed, float accel)
{
    float addspeed, accelspeed;

    if (ps->pm_flags & 0x20) {
        vec3_t push;
        float pushLen, t;
        push[0] = wishspeed * wishdir[0] - ps->velocity[0];
        push[1] = wishspeed * wishdir[1] - ps->velocity[1];
        push[2] = wishspeed * wishdir[2] - ps->velocity[2];
        pushLen = Vec3Normalize(push);
        t = fminf(pushLen, accel * pml->frametime * wishspeed);
        ps->velocity[0] += t * push[0];
        ps->velocity[1] += t * push[1];
        ps->velocity[2] += t * push[2];
        return;
    }

    addspeed = wishspeed - (ps->velocity[0] * wishdir[0] + ps->velocity[1] * wishdir[1]
                            + ps->velocity[2] * wishdir[2]);
    if (addspeed <= 0.0f)
        return;

    accelspeed = fminf(addspeed, fmaxf(stopspeed->current.value, wishspeed) * (accel * pml->frametime));

    if (ps->pm_type != 2 && inertiaMax->current.value < accelspeed
        && (ps->oldVelocity[0] * ps->oldVelocity[0] + ps->oldVelocity[1] * ps->oldVelocity[1]) >= 0.0001) {
        vec2_t newVel, oldVel;
        float dot;
        newVel[0] = ps->velocity[0] + accelspeed * wishdir[0];
        newVel[1] = ps->velocity[1] + accelspeed * wishdir[1];
        oldVel[0] = ps->oldVelocity[0];
        oldVel[1] = ps->oldVelocity[1];
        Vec2Normalize(oldVel);
        Vec2Normalize(newVel);
        dot = oldVel[0] * newVel[0] + oldVel[1] * newVel[1];
        if (dot < inertiaAngle->current.value) {
            if (inertiaDebug->current.enabled) {
                Com_Printf("angle is %f (oldVel is (%f,%f), vel is (%f, %f))\n",
                           (double)dot, (double)oldVel[0], (double)oldVel[1],
                           (double)newVel[0], (double)newVel[1]);
                Com_Printf("clamping acceleration from %f to %f\n",
                           (double)accelspeed, (double)inertiaMax->current.value);
            }
            accelspeed = inertiaMax->current.value;
        }
    }

    ps->velocity[0] += accelspeed * wishdir[0];
    ps->velocity[1] += accelspeed * wishdir[1];
    ps->velocity[2] += accelspeed * wishdir[2];
}

static void PM_REGPARM2_ABI PM_Friction(playerState_t *ps, pml_t *pml)
{
    float speed;
    float control;
    float drop;
    float newspeed;
    float vx, vy, vz;

    vx = ps->velocity[0];
    vy = ps->velocity[1];
    vz = ps->velocity[2];
    if (pml->walking)
        vz = 0.0f;
    speed = sqrtf(vx * vx + vy * vy + vz * vz);

    if (speed < 1.0f) {
        ps->velocity[0] = 0.0f;
        ps->velocity[1] = 0.0f;
        ps->velocity[2] = 0.0f;
        return;
    }

    drop = 0.0f;

    if (pml->walking && !(pml->groundTrace.surfaceFlags & 2) &&
        !(ps->pm_flags & 0x400)
#if !defined(COD2_X64)
        && ps->pm_type != 4) {
#else
        ) {
#endif
        control = speed;
        if (PM_DvarFloat(stopspeed, 100.0f) > control)
            control = PM_DvarFloat(stopspeed, 100.0f);

#if defined(COD2_X64)
        if (ps->pm_flags & 0x200)
            control *= 0.3f;
        else if (ps->pm_flags & PMF_JUMPING)
            control *= Jump_ReduceFriction(ps);
#else
        if (ps->pm_flags & PMF_JUMPING)
            control *= Jump_ReduceFriction(ps);
        if (ps->pm_flags & 0x200)
            control *= 0.3f;
#endif

        drop += control * PM_DvarFloat(friction, 5.5f) * pml->frametime;
    }

    if (ps->pm_type == 4)
        drop += speed * 5.0f * pml->frametime;

    newspeed = speed - drop;
    if (newspeed < 0.0f)
        newspeed = 0.0f;
    newspeed /= speed;

    ps->velocity[0] *= newspeed;
    ps->velocity[1] *= newspeed;
    ps->velocity[2] *= newspeed;
}

static void PM_REGPARM2_ABI PM_CheckDuck(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    int wasCrouched;
    trace_t trace;
    vec3_t vEnd;
    vec3_t vPoint;
    float frac;
    float delta;
    vec3_t normal;

    pm->proneChange = 0;

    if (ps->pm_type == 4) {

        pm->mins[0] = -8.0f;
        pm->mins[1] = -8.0f;
        pm->mins[2] = -8.0f;
        pm->maxs[0] = 8.0f;
        pm->maxs[1] = 8.0f;
        pm->maxs[2] = 16.0f;
        ps->pm_flags &= ~0x3;
        if (pm->cmd.buttons & 0x100) {
            pm->cmd.buttons &= ~0x100;
            BG_AddPredictableEventToPlayerstate(140, 0, ps);
        }
        ps->viewHeightTarget = 0;
        ps->viewHeightCurrent = 0.0f;
        return;
    }

    wasCrouched = ps->pm_flags & 0x1;

    pm->mins[0] = ps->mins[0];
    pm->mins[1] = ps->mins[1];
    pm->maxs[0] = ps->maxs[0];
    pm->maxs[1] = ps->maxs[1];
    pm->mins[2] = ps->mins[2];

    if (ps->pm_type > 5) {
        pm->maxs[2] = ps->maxs[2];
        ps->viewHeightTarget = 8;
        PM_ViewHeightAdjust(pm, pml);
        return;
    }

    if (ps->eFlags & 0x300) {

        if (ps->eFlags & 0x100) {
            if (ps->eFlags & 0x200) {
                ps->pm_flags &= ~0x3;
                goto compute_target;
            }
            ps->pm_flags = (ps->pm_flags | 0x1) & ~0x2;
            goto compute_target;
        }

        ps->pm_flags = (ps->pm_flags | 0x2) & ~0x1;
        goto compute_target;
    }

    if ((short)ps->pm_flags < 0) {

        goto compute_target;
    }

    if (ps->pm_flags & 0x20) {

        if (pm->cmd.buttons & 0x300) {
            pm->cmd.buttons &= ~0x300;
            BG_AddPredictableEventToPlayerstate(140, 0, ps);
        }
    }

    if (pm->cmd.buttons & 0x100) {

        if (ps->pm_flags & 0x1)
            goto set_prone;
        if (ps->groundEntityNum == 0x3ff)
            goto compute_target;
        if (BG_CheckProne(ps->clientNum, ps->origin, pm->maxs[0],
                          30.0f, ps->viewangles[1],
                          &ps->fTorsoHeight, &ps->fTorsoPitch, &ps->fWaistPitch,
                          0, 1, NULL, (unsigned char)pm->handler, (proneCheckType_t)0, 66.0f))
            goto set_prone;
        if (ps->groundEntityNum != 0x3ff) {
            ps->pm_flags |= 0x10000;
            if (!(pm->cmd.buttons & 0x2000))
                BG_AddPredictableEventToPlayerstate((ps->pm_flags & 0x2) ? 141 : 140, 0, ps);
        }
        goto compute_target;
    }

    if (pm->cmd.buttons & 0x200) {

        if (ps->pm_flags & 0x1) {

            pm->maxs[2] = 50.0f;
            PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs,
                           ps->origin, ps->clientNum, pm->tracemask & ~0x02000000);
            if (!trace.allsolid) {
                /* CoD2x animation.cpp:1389-1393 uses controller stance transitions. */
#if defined(COD2_CODX) && COD2_CODX
                if (Cod2x_GameVersion() < 3)
#endif
                BG_AnimScriptEvent(ps, ANIM_ET_PRONE_TO_CROUCH, 0, 0);
                ps->pm_flags = (ps->pm_flags & ~0x1) | 0x2;
            } else if (!(pm->cmd.buttons & 0x2000)) {
                BG_AddPredictableEventToPlayerstate(142, 2, ps);
            }
        } else {
            /* CoD2x animation.cpp:1389-1393 uses controller stance transitions. */
#if defined(COD2_CODX) && COD2_CODX
            if (Cod2x_GameVersion() < 3)
#endif
            BG_AnimScriptEvent(ps, ANIM_ET_STAND_TO_CROUCH, 0, 0);
            ps->pm_flags |= 0x2;
        }
        goto compute_target;
    }

    if (ps->pm_flags & 0x1) {

        pm->maxs[2] = ps->maxs[2];
        PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs,
                       ps->origin, ps->clientNum, pm->tracemask & ~0x02000000);
        if (!trace.allsolid) {
            /* CoD2x animation.cpp:1389-1393 uses controller stance transitions. */
#if defined(COD2_CODX) && COD2_CODX
            if (Cod2x_GameVersion() < 3)
#endif
            BG_AnimScriptEvent(ps, ANIM_ET_PRONE_TO_STAND, 0, 0);
            ps->pm_flags &= ~0x3;
        } else {
            pm->maxs[2] = 50.0f;
            PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs,
                           ps->origin, ps->clientNum, pm->tracemask & ~0x02000000);
            if (!trace.allsolid)
                ps->pm_flags = (ps->pm_flags & ~0x1) | 0x2;
            else if (!(pm->cmd.buttons & 0x2000))
                BG_AddPredictableEventToPlayerstate(142, 1, ps);
        }
        goto compute_target;
    }

    if (ps->pm_flags & 0x2) {

        pm->maxs[2] = ps->maxs[2];
        PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs,
                       ps->origin, ps->clientNum, pm->tracemask & ~0x02000000);
        if (!trace.allsolid) {
            /* CoD2x animation.cpp:1389-1393 uses controller stance transitions. */
#if defined(COD2_CODX) && COD2_CODX
            if (Cod2x_GameVersion() < 3)
#endif
            BG_AnimScriptEvent(ps, ANIM_ET_CROUCH_TO_STAND, 0, 0);
            ps->pm_flags &= ~0x2;
        } else if (!(pm->cmd.buttons & 0x2000)) {
            BG_AddPredictableEventToPlayerstate(141, 1, ps);
        }
    }
    goto compute_target;

set_prone:
    ps->pm_flags = (ps->pm_flags | 0x1) & ~0x2;
    goto compute_target;

compute_target:
    if (ps->viewHeightLerpTime == 0) {
        if (ps->pm_flags & 0x1) {

            if (ps->viewHeightTarget == 60) {
                ps->viewHeightTarget = 40;
            } else if (ps->viewHeightTarget != 11) {
                ps->viewHeightTarget = 11;
                pm->proneChange = 1;
                BG_PlayAnim(ps, 0, ANIM_BP_TORSO, 0, 0, 1, 1);
                Jump_ActivateSlowdown(ps);
            }
        } else if (ps->viewHeightTarget == 11) {
            ps->viewHeightTarget = 40;
            pm->proneChange = 1;
            BG_PlayAnim(ps, 0, ANIM_BP_TORSO, 0, 0, 1, 1);
        } else {

            ps->viewHeightTarget = (ps->pm_flags & 0x2) ? 40 : 60;
        }
    }

    PM_ViewHeightAdjust(pm, pml);

    if (ps->viewHeightTarget == 40) {
        pm->maxs[2] = 50.0f;
        ps->eFlags = (ps->eFlags | 0x4) & ~0x8;
        ps->pm_flags = (ps->pm_flags | 0x2) & ~0x1;
    } else if (ps->viewHeightTarget == 11) {
        pm->maxs[2] = 30.0f;
        ps->eFlags = (ps->eFlags | 0x8) & ~0x4;
        ps->pm_flags = (ps->pm_flags | 0x1) & ~0x2;
    } else {
        pm->maxs[2] = ps->maxs[2];
        ps->eFlags &= ~0xc;
        ps->pm_flags &= ~0x3;
    }

    if (!(ps->pm_flags & 0x1) || wasCrouched != 0)
        return;

    if (pm->cmd.forwardmove != 0 || pm->cmd.rightmove != 0) {
        ps->pm_flags &= ~0x8;
        PM_ExitAimDownSight(ps);
    }

    vEnd[0] = ps->origin[0];
    vEnd[1] = ps->origin[1];
    vEnd[2] = ps->origin[2] + 10.0f;

    PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs, vEnd,
                   ps->clientNum, pm->tracemask & ~0x02000000);
    frac = trace.fraction;
    vEnd[0] = ps->origin[0] + (vEnd[0] - ps->origin[0]) * frac;
    vEnd[1] = ps->origin[1] + (vEnd[1] - ps->origin[1]) * frac;
    vEnd[2] = ps->origin[2] + (vEnd[2] - ps->origin[2]) * frac;

    PM_playerTrace(pm, &trace, vEnd, pm->mins, pm->maxs, ps->origin,
                   ps->clientNum, pm->tracemask & ~0x02000000);
    frac = trace.fraction;
    ps->origin[0] = vEnd[0] + (ps->origin[0] - vEnd[0]) * frac;
    ps->origin[1] = vEnd[1] + (ps->origin[1] - vEnd[1]) * frac;
    ps->origin[2] = vEnd[2] + (ps->origin[2] - vEnd[2]) * frac;

    ps->proneDirection = ps->viewangles[1];

    vPoint[0] = ps->origin[0];
    vPoint[1] = ps->origin[1];
    vPoint[2] = ps->origin[2] - 0.25f;

    PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs, vPoint,
                   ps->clientNum, pm->tracemask & ~0x02000000);

    normal[0] = trace.normal[0];
    normal[1] = trace.normal[1];
    normal[2] = trace.normal[2];
    if (trace.startsolid || trace.fraction >= 1.0f)
        ps->proneDirectionPitch = 0.0f;
    else
        ps->proneDirectionPitch = PitchForYawOnNormal(ps->proneDirection, normal);

    delta = AngleDelta(ps->proneDirectionPitch, ps->viewangles[0]);
    if (delta < -45.0f)
        ps->proneTorsoPitch = ps->viewangles[0] - 45.0f;
    else if (delta > 45.0f)
        ps->proneTorsoPitch = 45.0f + ps->viewangles[0];
    else
        ps->proneTorsoPitch = ps->proneDirectionPitch;
}

static void PM_REGPARM2_ABI PM_SetMovementDir(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    int fmove = pm->cmd.forwardmove;
    int smove = pm->cmd.rightmove;

#if defined(COD2_CODX) && COD2_CODX
    /* CoD2x src/shared/animation.cpp:1308-1355. movementDir holds degrees. */
    if (Cod2x_GameVersion() >= 3) {
        float yaw = 0.0f;
        if ((ps->pm_flags & 1) && !(ps->eFlags & 0x300)) {
            yaw = AngleDelta(ps->proneDirection, ps->viewangles[1]);
        } else if ((fmove || smove) && ps->groundEntityNum != 1023 && pml->frametime > 0.0f) {
            vec3_t displacement;
            float speedSquared = 0.0f;
            int axis;
            for (axis = 0; axis < 3; ++axis) {
                displacement[axis] = ps->origin[axis] - pml->previous_origin[axis];
                speedSquared += displacement[axis] * displacement[axis];
            }
            if (sqrtf(speedSquared) / pml->frametime > 5.0f) {
                yaw = AngleDelta(vectoyaw(displacement), ps->viewangles[1]);
                if (fmove < 0 && (yaw > 90.0f || yaw < -90.0f))
                    yaw = AngleNormalize180(yaw + 180.0f);
            }
        }
        if (yaw > 90.0f) yaw = 90.0f;
        if (yaw < -90.0f) yaw = -90.0f;
        ps->movementDir = (int)yaw;
        return;
    }
#endif

#if defined(COD2_X64)
    int yaw;
    if ((ps->pm_flags & 1) && !(ps->eFlags & 0x300)) {
        yaw = (int)AngleDelta(ps->proneDirection, ps->viewangles[1]);
    } else if (ps->pm_flags & PMF_LADDER) {
        yaw = (int)AngleDelta(vectoyaw(ps->vLadderVec) + 180.0f, ps->viewangles[1]);
    } else {
        vec3_t moved, direction;
        for (int i = 0; i < 3; ++i)
            moved[i] = ps->origin[i] - pml->previous_origin[i];
        float speed = sqrtf(PM_DotProduct(moved, moved));
        if (!(fmove || smove) || ps->groundEntityNum == ENTITYNUM_NONE || speed == 0.0f || speed <= pml->frametime * 5.0f) {
            ps->movementDir = 0;
            return;
        }
        Vec3NormalizeTo(moved, direction);
        vectoangles(direction, direction);
        yaw = (int)AngleDelta(direction[1], ps->viewangles[1]);
        if (fmove < 0)
            yaw = (int)AngleNormalize180((float)yaw + 180.0f);
    }
    if (PM_AbsInt(yaw) > 90)
        yaw = yaw > 0 ? 90 : -90;
    ps->movementDir = (signed char)yaw;
#else
    (void)pml;

    if (fmove == 0 && smove == 0) {
        ps->movementDir = 0;
    } else if (smove == 0 && fmove > 0) {
        ps->movementDir = 0;
    } else if (smove < 0 && fmove > 0) {
        ps->movementDir = 1;
    } else if (smove < 0 && fmove == 0) {
        ps->movementDir = 2;
    } else if (smove < 0 && fmove < 0) {
        ps->movementDir = 3;
    } else if (smove == 0 && fmove < 0) {
        ps->movementDir = 4;
    } else if (smove > 0 && fmove < 0) {
        ps->movementDir = 5;
    } else if (smove > 0 && fmove == 0) {
        ps->movementDir = 6;
    } else {
        ps->movementDir = 7;
    }
#endif
}

static void PM_ApplyGroundPlane(playerState_t *ps, pml_t *pml)
{
    float velDot;

    if (!pml->groundPlane)
        return;

    velDot = PM_DotProduct(ps->velocity, pml->groundTrace.normal);
    if (velDot < 0.0f)
        PM_ClipVelocity(ps->velocity, pml->groundTrace.normal, ps->velocity);
}

static void PM_WalkMove(pmove_t *pm, pml_t *pml)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    if (ps->pm_flags & PMF_JUMPING)
        Jump_ApplySlowdown(ps);
    if (Jump_Check(pm, pml)) {
        PM_AirMove(pm, pml);
        return;
    }
    PM_Friction(ps, pml);
    usercmd_t cmd = pm->cmd;
    float scale = PM_CmdScale_Walk(pm, &cmd);
    if (ps->damageTimer && player_dmgtimer_maxTime->current.value != 0.0f)
        scale *= (float)ps->damageTimer * (-player_dmgtimer_minScale->current.value / player_dmgtimer_maxTime->current.value) + 1.0f;
    ps->damageTimer -= (int)(pml->frametime * 1000.0f);
    if (ps->damageTimer <= 0)
        ps->damageTimer = 0;
    pml->forward[2] = 0.0f;
    pml->right[2] = 0.0f;
    PM_ClipVelocity(pml->forward, pml->groundTrace.normal, pml->forward);
    PM_ClipVelocity(pml->right, pml->groundTrace.normal, pml->right);
    Vec3Normalize(pml->forward);
    Vec3Normalize(pml->right);
    vec3_t wishdir, oldVelocity;
    for (int i = 0; i < 3; ++i)
        wishdir[i] = (float)cmd.forwardmove * pml->forward[i] + (float)cmd.rightmove * pml->right[i];
    float wishspeed = Vec3Normalize(wishdir) * scale;
    int stance = PM_GetEffectiveStance(ps);
    float acceleration = ((pml->groundTrace.surfaceFlags & 2) || (ps->pm_flags & 0x400)) ? 1.0f :
                         (stance == 1 ? 19.0f : (stance == 2 ? 12.0f : 9.0f));
    if (ps->pm_flags & 0x200)
        acceleration *= 0.25f;
    PM_Accelerate(ps, pml, wishdir, wishspeed, acceleration);
    if ((pml->groundTrace.surfaceFlags & 2) || (ps->pm_flags & 0x400))
        ps->velocity[2] -= (float)ps->gravity * pml->frametime;
    float speed = sqrtf(PM_DotProduct(ps->velocity, ps->velocity));
    PM_VectorCopy(ps->velocity, oldVelocity);
    PM_ClipVelocity(ps->velocity, pml->groundTrace.normal, ps->velocity);
    if (PM_DotProduct(ps->velocity, oldVelocity) > 0.0f) {
        Vec3Normalize(ps->velocity);
        for (int i = 0; i < 3; ++i)
            ps->velocity[i] *= speed;
    }
    if (ps->velocity[0] != 0.0f || ps->velocity[1] != 0.0f)
        PM_StepSlideMove(pm, pml, 0);
    PM_SetMovementDir(pm, pml);
#else
    playerState_t *ps = pm->ps;
    vec3_t wishvel;
    vec3_t wishdir;
    float scale;
    float wishspeed;

    if (Jump_Check(pm, pml)) {
        PM_AirMove(pm, pml);
        return;
    }

    PM_Friction(ps, pml);

    scale = PM_CmdScale(ps, &pm->cmd);

    wishvel[0] = pml->forward[0] * (float)pm->cmd.forwardmove +
                 pml->right[0] * (float)pm->cmd.rightmove;
    wishvel[1] = pml->forward[1] * (float)pm->cmd.forwardmove +
                 pml->right[1] * (float)pm->cmd.rightmove;
    wishvel[2] = 0.0f;

    PM_VectorCopy(wishvel, wishdir);
    wishspeed = Vec3Normalize(wishdir) * scale;

    PM_Accelerate(ps, pml, wishdir, wishspeed, 9.0f);
    PM_ApplyGroundPlane(ps, pml);
    PM_StepSlideMove(pm, pml, 0);
    PM_SetMovementDir(pm, pml);
#endif
}

static void PM_REGPARM2_ABI PM_AirMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    vec3_t wishvel;
    vec3_t wishdir;
    signed char fmove, smove;
    float ffmove, fsmove;
    int maxMove;
    float total;
    float scale;
    float wishspeed;
    int i;

    PM_Friction(ps, pml);

    fmove = pm->cmd.forwardmove;
    ffmove = (float)fmove;
    smove = pm->cmd.rightmove;
    fsmove = (float)smove;

    {
        int absf, abss;

        total = sqrtf((float)(fmove * fmove + smove * smove));
        absf = fmove < 0 ? -fmove : fmove;
        abss = smove < 0 ? -smove : smove;
        if (absf >= abss)
            abss = absf;
        maxMove = abss;
    }

    if (maxMove == 0) {
        scale = 0.0f;
    } else {
        scale = (float)ps->speed * (float)maxMove / (127.0f * total);
        if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
            scale *= 0.4f;
        if (ps->pm_type == 2)
            scale *= 3.0f;
        else if (ps->pm_type == 3)
            scale *= 6.0f;
        else if (ps->pm_type == 4)
            scale *= (player_spectateSpeedScale)->current.value;
    }

    pml->forward[2] = 0.0f;
    pml->right[2] = 0.0f;
    Vec3Normalize(pml->forward);
    Vec3Normalize(pml->right);

    for (i = 0; i < 2; i++)
        wishvel[i] = ffmove * pml->forward[i] + fsmove * pml->right[i];
    wishvel[2] = 0.0f;

    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = 0.0f;
    wishspeed = Vec3Normalize(wishdir) * scale;

    PM_Accelerate(ps, pml, wishdir, wishspeed, 1.0f);

    if (pml->groundPlane) {
        const vec_t *normal = pml->groundTrace.normal;
        vec_t *out = ps->velocity;
        float dot = ps->velocity[0] * normal[0] +
                    ps->velocity[1] * normal[1] +
                    ps->velocity[2] * normal[2];
        float overbounce = -(dot + fabsf(dot) * (-0.001f));
        out[0] = ps->velocity[0] + normal[0] * overbounce;
        out[1] = ps->velocity[1] + normal[1] * overbounce;
        out[2] = ps->velocity[2] + normal[2] * overbounce;
    }

    PM_StepSlideMove(pm, pml, 1);
    PM_SetMovementDir(pm, pml);
}

static void PM_NoclipMove(pmove_t *pm, pml_t *pml)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    ps->viewHeightTarget = 60;
    float fmove = (float)pm->cmd.forwardmove, rmove = (float)pm->cmd.rightmove;
    float umove = ((pm->cmd.buttons & 0x80) ? 127.0f : 0.0f) - ((pm->cmd.buttons & 0x40) ? 127.0f : 0.0f);
    float speed = sqrtf(PM_DotProduct(ps->velocity, ps->velocity));
    if (ps->pm_type == 3 && fmove == 0.0f && rmove == 0.0f && umove == 0.0f)
        speed = 0.0f;
    if (speed < 1.0f) {
        PM_VectorClear(ps->velocity);
    } else {
        float control = speed < stopspeed->current.value ? stopspeed->current.value : speed;
        float drop = control * (friction->current.value * 1.5f) * pml->frametime;
        float newspeed = speed - drop;
        if (newspeed < 0.0f)
            newspeed = 0.0f;
        newspeed /= speed;
        for (int i = 0; i < 3; ++i)
            ps->velocity[i] *= newspeed;
    }
    float maxMove = fmaxf(fabsf(fmove), fmaxf(fabsf(rmove), fabsf(umove)));
    float scale = 0.0f;
    if (maxMove != 0.0f) {
        scale = (float)ps->speed * maxMove / (sqrtf(fmove * fmove + rmove * rmove + umove * umove) * 127.0f);
        if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
            scale *= 0.4f;
        if (ps->pm_type == 2)
            scale *= 3.0f;
        if (ps->pm_type == 3)
            scale *= 6.0f;
    }
    vec3_t wishdir, forward, up;
    PM_VectorCopy(pml->forward, forward);
    PM_VectorCopy(pml->up, up);
    if (ps->pm_type == 3) {
        up[0] = up[1] = 0.0f;
        up[2] = 1.0f;
        extern void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *out);
        Vec3Cross(up, pml->right, forward);
    }
    for (int i = 0; i < 3; ++i)
        wishdir[i] = forward[i] * fmove + pml->right[i] * rmove + up[i] * umove;
    float wishspeed = Vec3Normalize(wishdir) * scale;
    PM_Accelerate(ps, pml, wishdir, wishspeed, 9.0f);
    for (int i = 0; i < 3; ++i)
        ps->origin[i] += pml->frametime * ps->velocity[i];
#else
    playerState_t *ps = pm->ps;
    vec3_t wishvel;
    vec3_t wishdir;
    float scale;
    float wishspeed;

    PM_Friction(ps, pml);

    scale = PM_CmdScale(ps, &pm->cmd);
    wishvel[0] = pml->forward[0] * (float)pm->cmd.forwardmove +
                 pml->right[0] * (float)pm->cmd.rightmove;
    wishvel[1] = pml->forward[1] * (float)pm->cmd.forwardmove +
                 pml->right[1] * (float)pm->cmd.rightmove;
    wishvel[2] = pml->forward[2] * (float)pm->cmd.forwardmove;

    if (pm->cmd.buttons & 0x80)
        wishvel[2] += 127.0f;
    if (pm->cmd.buttons & 0x40)
        wishvel[2] -= 127.0f;

    PM_VectorCopy(wishvel, wishdir);
    wishspeed = Vec3Normalize(wishdir) * scale;
    PM_Accelerate(ps, pml, wishdir, wishspeed, 8.0f);

    ps->origin[0] += ps->velocity[0] * pml->frametime;
    ps->origin[1] += ps->velocity[1] * pml->frametime;
    ps->origin[2] += ps->velocity[2] * pml->frametime;
    PM_SetMovementDir(pm, pml);
#endif
}

static void PM_REGPARM2_ABI PM_LadderMove(pmove_t *pm, pml_t *pml)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    if (Jump_Check(pm, pml)) {
        PM_AirMove(pm, pml);
        return;
    }
    float upscale = (pml->forward[2] + 0.25f) * 2.5f;
    if (upscale > 1.0f)
        upscale = 1.0f;
    else if (upscale < -1.0f)
        upscale = -1.0f;
    pml->forward[2] = 0.0f;
    Vec3Normalize(pml->forward);
    pml->right[2] = 0.0f;
    vec3_t right, wishvel = {0.0f, 0.0f, 0.0f}, wishdir;
    Vec3NormalizeTo(pml->right, right);
    ProjectPointOnPlane(right, ps->vLadderVec, pml->right);
    float scale = PM_CmdScale(ps, &pm->cmd);
    if (pm->cmd.forwardmove)
        wishvel[2] = upscale * 0.5f * scale * (float)pm->cmd.forwardmove;
    if (pm->cmd.rightmove) {
        float side = scale * 0.2f * (float)pm->cmd.rightmove;
        for (int i = 0; i < 3; ++i)
            wishvel[i] += side * pml->right[i];
    }
    float wishspeed = Vec3NormalizeTo(wishvel, wishdir);
    PM_Accelerate(ps, pml, wishdir, wishspeed, 9.0f);
    if (!pm->cmd.forwardmove) {
        if (ps->velocity[2] > 0.0f) {
            ps->velocity[2] -= (float)ps->gravity * pml->frametime;
            if (ps->velocity[2] < 0.0f)
                ps->velocity[2] = 0.0f;
        } else {
            ps->velocity[2] += (float)ps->gravity * pml->frametime;
            if (ps->velocity[2] > 0.0f)
                ps->velocity[2] = 0.0f;
        }
    }
    if (!pm->cmd.rightmove) {
        vec2_t side = {pml->right[0], pml->right[1]};
        Vec2Normalize(side);
        float speed = side[0] * ps->velocity[0] + side[1] * ps->velocity[1];
        if (speed != 0.0f) {
            for (int i = 0; i < 2; ++i)
                ps->velocity[i] += -speed * side[i];
            float drop = speed * pml->frametime * 16.0f;
            if (fabsf(speed) > fabsf(drop)) {
                if (fabsf(drop) < 1.0f)
                    drop = drop < 0.0f ? -1.0f : 1.0f;
                speed -= drop;
                for (int i = 0; i < 2; ++i)
                    ps->velocity[i] += speed * side[i];
            }
        }
    }
    if (!pml->walking) {
        float speed = ps->vLadderVec[0] * ps->velocity[0] + ps->vLadderVec[1] * ps->velocity[1];
        for (int i = 0; i < 2; ++i)
            ps->velocity[i] += -speed * ps->vLadderVec[i];
        if (ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1] <= ps->velocity[2] * ps->velocity[2]) {
            for (int i = 0; i < 2; ++i)
                ps->velocity[i] += -50.0f * ps->vLadderVec[i];
        }
    }
    PM_StepSlideMove(pm, pml, 0);
    int yaw = (int)AngleDelta(vectoyaw(ps->vLadderVec) + 180.0f, ps->viewangles[1]);
    if (PM_AbsInt(yaw) > 75)
        yaw = yaw > 0 ? 75 : -75;
    ps->movementDir = (signed char)yaw;
#else
    PM_AirMove(pm, pml);
#endif
}

static void PM_GetViewHeightLerp(const viewLerpWaypoint_t *table, int frac,
                                 float *pfViewHeight, float *pfOffset)
{
    int i;

    if (frac == 0) {
        *pfViewHeight = table[0].fViewHeight;
        *pfOffset = (float)table[0].iOffset;
        return;
    }

    for (i = 1;; i++) {
        const viewLerpWaypoint_t *cur = &table[i];

        if (frac == cur->iFrac) {
            *pfViewHeight = cur->fViewHeight;
            *pfOffset = (float)cur->iOffset;
            return;
        }

        if (frac < cur->iFrac) {
            const viewLerpWaypoint_t *prev = &table[i - 1];
            float t = (float)(frac - prev->iFrac) /
                      (float)(cur->iFrac - prev->iFrac);
            *pfOffset = (float)prev->iOffset +
                        (float)(cur->iOffset - prev->iOffset) * t;
            *pfViewHeight = prev->fViewHeight +
                            (cur->fViewHeight - prev->fViewHeight) * t;
            return;
        }

        if (cur->iFrac == -1)
            break;
    }

    *pfViewHeight = table[0].fViewHeight;
    *pfOffset = (float)table[0].iOffset;
}

static void PM_REGPARM2_ABI PM_ViewHeightAdjust(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    int target = ps->viewHeightTarget;
    float current = ps->viewHeightCurrent;
    int frac;
    int lerpTarget;
    qboolean bDown;
    int duration;
    float fViewHeight;
    float fOffset;
    const viewLerpWaypoint_t *table;

    if (target == 0) {
        if (ps->pm_type == 4)
            ps->viewHeightCurrent = 0.0f;
        else
            ps->viewHeightCurrent = (float)target;
        return;
    }

    if (current == (float)target && ps->viewHeightLerpTime == 0)
        return;

    if (target != 11 && target != 40 && target != 60) {

        ps->viewHeightLerpTime = 0;
        if ((float)ps->viewHeightTarget > current) {
            current += 180.0f * pml->frametime;
            ps->viewHeightCurrent = current;
            if (current >= (float)ps->viewHeightTarget)
                ps->viewHeightCurrent = (float)ps->viewHeightTarget;
        } else {
            current += -180.0f * pml->frametime;
            ps->viewHeightCurrent = current;
            if (current <= (float)ps->viewHeightTarget)
                ps->viewHeightCurrent = (float)ps->viewHeightTarget;
        }
        return;
    }

    if (ps->viewHeightLerpTime != 0) {
        bDown = ps->viewHeightLerpDown;
        lerpTarget = ps->viewHeightLerpTarget;

        if (lerpTarget == 11)
            duration = 0x190;
        else if (lerpTarget == 40)
            duration = bDown ? 0xc8 : 0x190;
        else
            duration = 0xc8;

        frac = ((pm->cmd.serverTime - ps->viewHeightLerpTime) * 100) / duration;
        if (frac < 0)
            frac = 0;
        if (frac >= 100) {

            ps->viewHeightCurrent = (float)lerpTarget;
            ps->viewHeightLerpTime = 0;
            ps->viewHeightLerpPosAdj = 0.0f;
            frac = 100;
        } else {
            if (lerpTarget == 11)
                table = viewLerp_CrouchProne;
            else if (lerpTarget == 40)
                table = bDown ? viewLerp_StandCrouch : viewLerp_ProneCrouch;
            else
                table = viewLerp_CrouchStand;

            PM_GetViewHeightLerp(table, frac, &fViewHeight, &fOffset);

            ps->viewHeightCurrent = fViewHeight;

            if (fabsf(ps->viewHeightLerpPosAdj - fOffset) > 0.05f) {
                vec3_t vFlatForward;
                float savedY = ps->velocity[1];
                float savedZ = ps->velocity[2];
                float diff = fOffset - ps->viewHeightLerpPosAdj;
                float move;

                if (ps->groundEntityNum == 0x3ff)
                    diff *= 0.5f;
                move = diff / pml->frametime;

                vFlatForward[0] = pml->forward[0];
                vFlatForward[1] = pml->forward[1];
                vFlatForward[2] = 0.0f;
                Vec3Normalize(vFlatForward);

                ps->velocity[0] = move * vFlatForward[0];
                ps->velocity[1] = move * vFlatForward[1];
                ps->velocity[2] = move * vFlatForward[2];

                PM_StepSlideMove(pm, pml, 1);

                ps->velocity[1] = savedY;
                ps->velocity[2] = savedZ;
                ps->viewHeightLerpPosAdj = fOffset;
            }
        }
    } else {
        frac = 0;
    }

    if (ps->viewHeightLerpTime == 0) {
        if ((float)ps->viewHeightTarget == ps->viewHeightCurrent)
            return;

        ps->viewHeightLerpTime = pm->cmd.serverTime;
        target = ps->viewHeightTarget;
        if (target == 11) {
            ps->viewHeightLerpDown = 1;
            ps->viewHeightLerpTarget = (ps->viewHeightCurrent <= 40.0f) ? target : 40;
        } else if (target == 40) {
            ps->viewHeightLerpDown = (ps->viewHeightCurrent > 40.0f);
            ps->viewHeightLerpTarget = 40;
        } else if (target == 60) {
            ps->viewHeightLerpDown = 0;
            ps->viewHeightLerpTarget = (ps->viewHeightCurrent >= 40.0f) ? target : 40;
        }
        return;
    }

    lerpTarget = ps->viewHeightLerpTarget;
    if (ps->viewHeightTarget == lerpTarget)
        return;
    if (ps->viewHeightTarget > lerpTarget) {
        if (ps->viewHeightLerpDown)
            goto restart_lerp;
        return;
    }

    if (ps->viewHeightLerpDown)
        return;

restart_lerp: {
    int iLerpFrac = 100 - frac;
    int newDuration;

    ps->viewHeightLerpDown ^= 1;

    if (ps->viewHeightLerpDown) {
        if (lerpTarget == 60)
            ps->viewHeightLerpTarget = 40;
        else if (lerpTarget == 40)
            ps->viewHeightLerpTarget = 11;
    } else {
        if (lerpTarget == 11)
            ps->viewHeightLerpTarget = 40;
        else if (lerpTarget == 40)
            ps->viewHeightLerpTarget = 60;
    }

    if (iLerpFrac == 100) {
        ps->viewHeightCurrent = (float)ps->viewHeightLerpTarget;
        ps->viewHeightLerpTime = 0;
        ps->viewHeightLerpPosAdj = 0.0f;
        return;
    }

    lerpTarget = ps->viewHeightLerpTarget;
    if (lerpTarget == 11)
        newDuration = 0x190;
    else if (lerpTarget == 40)
        newDuration = ps->viewHeightLerpDown ? 0xc8 : 0x190;
    else
        newDuration = 0xc8;

    ps->viewHeightLerpTime =
        pm->cmd.serverTime -
        (int)((float)iLerpFrac * 0.01f * (float)newDuration);

    if (lerpTarget == 11) {
        table = viewLerp_CrouchProne;
    } else if (lerpTarget == 40) {
        table = ps->viewHeightLerpDown ? viewLerp_StandCrouch
                                       : viewLerp_ProneCrouch;
    } else {
        table = viewLerp_CrouchStand;
    }

    PM_GetViewHeightLerp(table, iLerpFrac, &fViewHeight, &fOffset);
    ps->viewHeightLerpPosAdj = fOffset;
}
}

void PM_UpdatePronePitch(pmove_t *pm, pml_t *pml)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    if (!(ps->pm_flags & 1))
        return;
    if (ps->groundEntityNum == ENTITYNUM_NONE) {
        if (!BG_CheckProne(ps->clientNum, ps->origin, ps->maxs[0], 30.0f, ps->proneDirection,
                          &ps->fTorsoHeight, &ps->fTorsoPitch, &ps->fWaistPitch, 1, 0,
                          pml->groundPlane ? pml->groundTrace.normal : NULL, pm->handler, (proneCheckType_t)0, 66.0f)) {
            BG_AddPredictableEventToPlayerstate(141, 0, ps);
            ps->pm_flags |= 0x10000;
        }
    } else if (pml->groundPlane && pml->groundTrace.normal[2] < 0.7f) {
        BG_AddPredictableEventToPlayerstate(141, 0, ps);
    }
    for (int i = 0; i < 2; ++i) {
        float target = pml->groundPlane ? PitchForYawOnNormal(i ? ps->viewangles[1] : ps->proneDirection, pml->groundTrace.normal) : 0.0f;
        float *pitch = i ? &ps->proneTorsoPitch : &ps->proneDirectionPitch;
        float delta = AngleDelta(target, *pitch);
        if (delta != 0.0f) {
            float step = pml->frametime * 70.0f;
            *pitch += fabsf(delta) > step ? (delta < 0.0f ? -1.0f : 1.0f) * step : delta;
            *pitch = AngleNormalize180Accurate(*pitch);
        }
    }
#else
    (void)pm;
    (void)pml;
#endif
}

void PM_FootstepEvent(pmove_t *pm, pml_t *pml, int iOldBobCycle, int iNewBobCycle, qboolean bFootStep)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    if (!(((byte)(iOldBobCycle + 64) ^ (byte)(iNewBobCycle + 64)) & 128) || !bFootStep)
        return;
    if (ps->groundEntityNum != ENTITYNUM_NONE) {
        int surface = PM_GroundSurfaceType(pml);
        int event = 0;
        if (surface) {
            if (ps->pm_flags & 1)
                event = surface + 47;
            else if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
                event = surface + 24;
            else
                event = surface + 1;
        }
        PM_AddEvent(ps, event);
    } else if (ps->pm_flags & PMF_LADDER) {
        vec3_t mins, maxs, end;
        trace_t trace;
        PM_VectorCopy(pm->mins, mins);
        PM_VectorCopy(pm->maxs, maxs);
        mins[0] += 6.0f;
        mins[1] += 6.0f;
        mins[2] = 8.0f;
        maxs[0] -= 6.0f;
        maxs[1] -= 6.0f;
        if (maxs[2] < mins[2])
            maxs[2] = mins[2];
        for (int i = 0; i < 3; ++i)
            end[i] = ps->origin[i] + -31.0f * ps->vLadderVec[i];
        PM_playerTrace(pm, &trace, ps->origin, mins, maxs, end, ps->clientNum, pm->tracemask & ~0x2010000);
        int surface = (trace.surfaceFlags >> 20) & 31;
        PM_AddEvent(ps, trace.fraction == 1.0f || !surface ? 22 : surface + 1);
    }
#else
    (void)pm;
    (void)pml;
    (void)iOldBobCycle;
    (void)iNewBobCycle;
    (void)bFootStep;
#endif
}
#if defined(COD2_X64)
static void PM_AdvanceBob(pmove_t *pm, pml_t *pml, float bobmove, qboolean footstep)
{
    int old = pm->ps->bobCycle;
    pm->ps->bobCycle = (int)((float)old + (float)pml->msec * bobmove) & 255;
    if (COD2_NATIVE_TRACE_ENABLED()) {
        const WeaponDef *weapon = pm->ps->weapon ? BG_GetWeaponDef(pm->ps->weapon) : NULL;
        Com_Printf("[move] handler=%d time=%d msec=%d xyspeed=%.6f bob=%d->%d rate=%.9f origin=%.6f,%.6f,%.6f lean=%.6f flags=%x weapon=%d weaponScale=%.6f stance=%d cmd=%d,%d buttons=%x ground=%d\n",
                   pm->handler, pm->cmd.serverTime, pml->msec, (double)pm->xyspeed, old, pm->ps->bobCycle, (double)bobmove,
                   (double)pm->ps->origin[0], (double)pm->ps->origin[1], (double)pm->ps->origin[2], (double)pm->ps->leanf, pm->ps->pm_flags, pm->ps->weapon,
                   (double)(weapon ? weapon->fMoveSpeedScale : 1.0f), pm->ps->viewHeightTarget, pm->cmd.forwardmove, pm->cmd.rightmove, pm->cmd.buttons, pm->ps->groundEntityNum);
    }
    PM_FootstepEvent(pm, pml, old, pm->ps->bobCycle, footstep);
}

static scriptAnimMoveTypes_t PM_FlinchAnimation(int yaw)
{
    if (yaw < 0)
        yaw += 360;
    if (yaw >= 315 || yaw < 45)
        return ANIM_MT_FLINCH_FORWARD;
    if (yaw < 135)
        return ANIM_MT_FLINCH_LEFT;
    if (yaw < 225)
        return ANIM_MT_FLINCH_BACKWARD;
    return ANIM_MT_FLINCH_RIGHT;
}
#endif

static void PM_REGPARM2_ABI PM_Footsteps(pmove_t *pm, pml_t *pml)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    if (ps->pm_type > 5)
        return;
    clientInfo_t *ci = ps->clientNum < 64 ? &bgs->clientinfo[ps->clientNum] : NULL;
    int stumbleEnd = ps->damageDuration - player_dmgtimer_stumbleTime->current.integer;
    if (stumbleEnd < 0)
        stumbleEnd = 0;
    pm->xyspeed = PM_VectorLength2D(ps->velocity);
    int stance = PM_GetEffectiveStance(ps);
    scriptAnimMoveTypes_t idle = stance == 1 ? ANIM_MT_IDLEPRONE : (stance == 2 ? ANIM_MT_IDLECR : ANIM_MT_IDLE);
    if (ps->eFlags & 0x300) {
        idle = (ps->pm_flags & 1) ? ANIM_MT_IDLEPRONE : ((ps->pm_flags & 2) ? ANIM_MT_IDLECR : ANIM_MT_IDLE);
        BG_AnimScriptAnimation(ps, AISTATE_COMBAT, idle, 1);
        return;
    }
    int walking = (ps->pm_flags & 0x100) || ps->leanf != 0.0f;
    if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pm_type != 1) {
        if (ps->pm_flags & PMF_LADDER) {
            if (pm->cmd.serverTime - ps->jumpTime <= 299)
                return;
            float speed = ps->velocity[2];
            float rate = walking ? speed / 38.10000228881836f * 0.35f : speed / 95.25f * 0.45f;
            BG_AnimScriptAnimation(ps, AISTATE_COMBAT, speed >= 0.0f ? ANIM_MT_CLIMBUP : ANIM_MT_CLIMBDOWN, 1);
            PM_AdvanceBob(pm, pml, rate, 1);
        }
        if (stance == (ps->pm_flags & 3))
            return;
    }
    if (pm->xyspeed < player_moveThreshhold->current.value || ps->pm_type == 1) {
        if (pm->xyspeed < 1.0f)
            ps->bobCycle = 0;
        scriptAnimMoveTypes_t turn = ANIM_MT_UNUSED;
        if (ci && player_turnAnims->current.enabled) {
            if (ci->legs.yawing) {
                turn = ci->legs.yawAngle > ci->torso.yawAngle ? ANIM_MT_TURNRIGHT : ANIM_MT_TURNLEFT;
                ci->legs.yawAngle = ci->torso.yawAngle;
                ci->turnAnimType = (char)turn;
                if (ci->turnAnimEndTime < bgs->time)
                    ci->turnAnimEndTime = 0;
            } else {
                if (ci->turnAnimEndTime > bgs->time) {
                    ci->legs.yawAngle = ci->torso.yawAngle;
                    return;
                }
                if (ci->turnAnimEndTime) {
                    ci->turnAnimEndTime = 0;
                    ci->legs.yawAngle = ci->torso.yawAngle;
                }
            }
        }
        scriptAnimMoveTypes_t move = idle;
        if (stance != 1 && turn) {
            move = stance == 2 ? (turn == ANIM_MT_TURNRIGHT ? ANIM_MT_TURNRIGHTCR : ANIM_MT_TURNLEFTCR) : turn;
        } else if (stance == 0 && ps->damageTimer > stumbleEnd) {
            move = PM_FlinchAnimation(ps->flinchYaw);
            BG_AnimScriptAnimation(ps, AISTATE_COMBAT, move, 1);
            return;
        }
        int result = BG_AnimScriptAnimation(ps, AISTATE_COMBAT, move, 1);
        if (turn && stance != 1 && result > 0 && !ci->turnAnimEndTime)
            ci->turnAnimEndTime = bgs->time + ps->legsAnimDuration;
        if (result < 0) {
            if (stance == 2)
                move = ANIM_MT_IDLECR;
            else if (ps->damageTimer > stumbleEnd)
                move = ANIM_MT_FLINCH_FORWARD;
            else
                move = ANIM_MT_IDLE;
            BG_AnimScriptAnimation(ps, AISTATE_COMBAT, move, 1);
        }
        return;
    }
    float scale = (float)ps->speed;
    if (pm->cmd.forwardmove) {
        if (pm->cmd.rightmove) {
            scale *= ((player_strafeSpeedScale->current.value - 1.0f) * 0.75f + 1.0f + 1.0f) * 0.5f;
            if (pm->cmd.forwardmove < 0)
                scale *= (player_backSpeedScale->current.value + 1.0f) * 0.5f;
        } else if (pm->cmd.forwardmove < 0) {
            scale *= player_backSpeedScale->current.value;
        }
        BG_UpdateConditionValue(ps->clientNum, 8, 0, 1);
    } else if (pm->cmd.rightmove) {
        scale *= (player_strafeSpeedScale->current.value - 1.0f) * 0.75f + 1.0f;
        BG_UpdateConditionValue(ps->clientNum, 8, pm->cmd.rightmove > 0 ? 2 : 1, 1);
    }
    if (walking)
        scale *= 0.4f;
    scale *= PM_StanceScale(pm);
    int backward = (ps->pm_flags & 0x80) != 0;
    int stumble = ps->damageTimer > stumbleEnd;
    float coefficient;
    scriptAnimMoveTypes_t move;
    if (stance == 1) {
        coefficient = walking ? 0.24f : 0.25f;
        move = backward ? ANIM_MT_WALKPRONEBK : ANIM_MT_WALKPRONE;
    } else if (stance == 2) {
        coefficient = walking ? 0.315f : 0.34f;
        if (stumble)
            move = backward ? ANIM_MT_STUMBLE_CROUCH_BACKWARD : ANIM_MT_STUMBLE_CROUCH_FORWARD;
        else if (walking)
            move = backward ? ANIM_MT_WALKCRBK : ANIM_MT_WALKCR;
        else
            move = backward ? ANIM_MT_RUNCRBK : ANIM_MT_RUNCR;
    } else if (backward) {
        coefficient = walking ? 0.325f : 0.36f;
        move = stumble ? (walking ? ANIM_MT_STUMBLE_WALK_BACKWARD : ANIM_MT_STUMBLE_BACKWARD) :
                         (walking ? ANIM_MT_WALKBK : ANIM_MT_RUNBK);
    } else {
        coefficient = walking ? 0.305f : 0.335f;
        move = stumble ? (walking ? ANIM_MT_STUMBLE_WALK_FORWARD : ANIM_MT_STUMBLE_FORWARD) :
                         (walking ? ANIM_MT_WALK : ANIM_MT_RUN);
    }
    int result = BG_AnimScriptAnimation(ps, AISTATE_COMBAT, move, 1);
    int old = ps->bobCycle;
    float rate = pm->xyspeed / scale * coefficient;
    qboolean footstep = PM_ShouldMakeFootsteps(pm);
    PM_AdvanceBob(pm, pml, rate, 0);
    if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
        if (pm->xyspeed > 120.0f)
            return;
        if (stance)
            result = BG_AnimScriptAnimation(ps, AISTATE_COMBAT, idle, 1);
        if (result < 0)
            BG_AnimScriptAnimation(ps, AISTATE_COMBAT, stumble ? ANIM_MT_FLINCH_FORWARD : ANIM_MT_IDLE, 1);
        return;
    }
    if (result < 0)
        BG_AnimScriptAnimation(ps, AISTATE_COMBAT, ANIM_MT_IDLE, 1);
    PM_FootstepEvent(pm, pml, old, ps->bobCycle, footstep);
#else
    int oldBob;
    int newBob;

    if (!PM_ShouldMakeFootsteps(pm))
        return;

    oldBob = pm->ps->bobCycle;
    newBob = (oldBob + (int)(pm->xyspeed * pml->frametime * 0.75f)) & 0xff;
    pm->ps->bobCycle = newBob;

    if ((oldBob ^ newBob) & 0x40)
        PM_FootstepEvent(pm, pml, oldBob, newBob, 1);
#endif
}

static void PM_REGPARM2_ABI PM_GroundTrace(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    vec3_t start;
    vec3_t point;
    trace_t trace;
    int i;
    float frac;

    start[0] = ps->origin[0];
    start[1] = ps->origin[1];
    point[0] = ps->origin[0];
    point[1] = ps->origin[1];
    if ((ps->eFlags & 0x300) == 0) {
        start[2] = ps->origin[2] + 0.25f;
        point[2] = ps->origin[2] - 0.25f;
    } else {
        start[2] = ps->origin[2];
        point[2] = start[2] - 1.0f;
    }

    PM_playerTrace(pm, &trace, start, pm->mins, pm->maxs, point,
                   ps->clientNum, pm->tracemask);

    pml->groundTrace = trace;

    if (trace.allsolid) {

        for (i = 0; i < 26; i++) {
            point[0] = ps->origin[0] + CorrectSolidDeltas[i][0];
            point[1] = ps->origin[1] + CorrectSolidDeltas[i][1];
            point[2] = ps->origin[2] + CorrectSolidDeltas[i][2];

            PM_playerTrace(pm, &trace, point, pm->mins, pm->maxs, point,
                           ps->clientNum, pm->tracemask);

            if (!trace.startsolid)
                break;
        }

        if (i == 26) {
            ps->groundEntityNum = ENTITYNUM_NONE;
            pml->groundPlane = 0;
            pml->almostGroundPlane = 0;
            pml->walking = 0;
            Jump_ClearState(ps);
            return;
        }

        ps->origin[0] = point[0];
        ps->origin[1] = point[1];
        ps->origin[2] = point[2];

        point[0] = ps->origin[0];
        point[1] = ps->origin[1];
        point[2] = ps->origin[2] - 1.0f;

        PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs, point,
                       ps->clientNum, pm->tracemask);

        pml->groundTrace = trace;

        frac = trace.fraction;
        ps->origin[0] += (point[0] - ps->origin[0]) * frac;
        ps->origin[1] += (point[1] - ps->origin[1]) * frac;
        ps->origin[2] += (point[2] - ps->origin[2]) * frac;
    }

    if (trace.startsolid) {

        start[2] = ps->origin[2] - 0.001f;

        PM_playerTrace(pm, &trace, start, pm->mins, pm->maxs, point,
                       ps->clientNum, pm->tracemask);

        if (trace.startsolid) {
            ps->groundEntityNum = ENTITYNUM_NONE;
            pml->groundPlane = 0;
            pml->almostGroundPlane = 0;
            pml->walking = 0;
            return;
        }

        pml->groundTrace = trace;
    }

    if (trace.fraction == 1.0f) {
        if (ps->groundEntityNum == ENTITYNUM_NONE) {
            point[0] = ps->origin[0];
            point[1] = ps->origin[1];
            point[2] = ps->origin[2] - 1.0f;

            PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs, point,
                           ps->clientNum, pm->tracemask);

            pml->almostGroundPlane = (trace.fraction != 1.0f);
        } else {
            point[0] = ps->origin[0];
            point[1] = ps->origin[1];
            point[2] = ps->origin[2] - 64.0f;

            PM_playerTrace(pm, &trace, ps->origin, pm->mins, pm->maxs, point,
                           ps->clientNum, pm->tracemask);

            if (trace.fraction == 1.0f) {
                BG_AnimScriptEvent(ps, pm->cmd.forwardmove < 0 ? ANIM_ET_JUMPBK : ANIM_ET_JUMP, 0, 1);
                pml->almostGroundPlane = 0;
                ps->groundEntityNum = ENTITYNUM_NONE;
                pml->groundPlane = 0;
                pml->walking = 0;
                return;
            }

            pml->almostGroundPlane = (trace.fraction < 0.015625f);
        }

        ps->groundEntityNum = ENTITYNUM_NONE;
        pml->groundPlane = 0;
        pml->walking = 0;
        return;
    }

    if (!(ps->pm_flags & 0x20) && ps->velocity[2] > 0.0f &&
        (ps->velocity[0] * trace.normal[0] +
         ps->velocity[1] * trace.normal[1] +
         ps->velocity[2] * trace.normal[2]) > 10.0f) {
        BG_AnimScriptEvent(ps, pm->cmd.forwardmove < 0 ? ANIM_ET_JUMPBK : ANIM_ET_JUMP, 0, 0);
        pml->almostGroundPlane = 0;
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml->groundPlane = 0;
        pml->walking = 0;
        return;
    }

    if (trace.normal[2] < 0.7f) {
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml->groundPlane = 1;
        pml->almostGroundPlane = 1;
        pml->walking = 0;
        Jump_ClearState(ps);
        return;
    }

    pml->groundPlane = 1;
    pml->almostGroundPlane = 1;
    pml->walking = 1;

    if (ps->groundEntityNum == ENTITYNUM_NONE) {

#if defined(COD2_X64)
        /* Mac 1.3 0x70678: inlined PM_CrashLand, including double sqrt/divide. */
        float vz = pml->previous_velocity[2];
        float grav = (float)ps->gravity;
        float acc = -grav;
        float halfg = acc * 0.5f;
        float disc = vz * vz + (pml->previous_origin[2] - ps->origin[2]) * (halfg * -4.0f);
        if (disc >= 0.0f) {
            float t = (float)((-(double)vz - sqrt((double)disc)) / (double)(halfg + halfg));
            float landVel = -(t * acc + vz);
            float fallHeight = landVel * landVel / (grav + grav);
            float minH = bg_fallDamageMinHeight->current.value;
            float maxH = bg_fallDamageMaxHeight->current.value;
            int damage = 0, viewDip = 0;
            int surf = PM_GroundSurfaceType(pml);
            if (minH >= maxH)
                Com_Printf("bg_fallDamageMaxHeight must be greater than bg_fallDamageMinHeight\n");
            else if (fallHeight > minH && !(pml->groundTrace.surfaceFlags & 1) && ps->pm_type <= 5) {
                damage = fallHeight >= maxH ? 100 : (int)((fallHeight - minH) / (maxH - minH) * 100.0f);
                if (damage > 100) damage = 100;
                if (damage < 0) damage = 0;
            }
            if (fallHeight > 12.0f) {
                viewDip = (int)((fallHeight - 12.0f) / 26.0f * 4.0f + 4.0f);
                if (viewDip > 24) viewDip = 24;
                BG_AnimScriptEvent(ps, ANIM_ET_LAND, 0, 1);
            }
            if (damage) {
                float scale = 0.67f;
                if (damage <= 99 && !(pml->groundTrace.surfaceFlags & 2)) {
                    int stun = damage * 35 + 500;
                    if (stun > 2000) stun = 2000;
                    scale = stun <= 500 ? 0.5f : (stun >= 1500 ? 0.2f : ((float)stun - 500.0f) / 1000.0f * -0.3f + 0.5f);
                    ps->pm_time = stun;
                    ps->pm_flags |= 0x200;
                }
                for (int axis = 0; axis < 3; ++axis) ps->velocity[axis] *= scale;
                BG_AddPredictableEventToPlayerstate(surf + 116, damage, ps);
            } else if (fallHeight > 4.0f) {
                if (fallHeight < 8.0f)
                    PM_AddEvent(ps, surf ? surf + 24 : 0);
                else if (fallHeight < 12.0f)
                    PM_AddEvent(ps, surf ? surf + 1 : 0);
                else {
                    for (int axis = 0; axis < 3; ++axis) ps->velocity[axis] *= 0.67f;
                    BG_AddPredictableEventToPlayerstate(surf + 93, viewDip, ps);
                }
            }
        }
#else
        float vz = pml->previous_velocity[2];
        float grav = (float)ps->gravity;
        float halfg = grav * 0.5f;
        float disc = vz * vz +
                     (pml->previous_origin[2] - ps->origin[2]) * (halfg * -4.0f);
        float fallHeight = 0.0f;
        int damage = 0;
        int impactEvent = 0;

        if (disc >= 0.0f) {
            float fallVel = vz + (-vz - sqrtf(disc)) / (halfg + halfg) * grav;
            fallHeight = fallVel * fallVel / (grav + grav);
        }

        if (disc >= 0.0f) {
            float maxH = bg_fallDamageMaxHeight->current.value;
            float minH = bg_fallDamageMinHeight->current.value;

            if (minH >= maxH) {
                Com_Printf("bg_fallDamageMaxHeight must be greater than "
                           "bg_fallDamageMinHeight\n");
                damage = 0;
            } else if (minH < fallHeight &&
                       !(pml->groundTrace.surfaceFlags & 0x1) &&
                       ps->pm_type <= 5) {
                if (fallHeight >= maxH) {
                    damage = 100;
                } else {
                    int d = (int)((fallHeight - minH) / (maxH - minH) * 100.0f);
                    damage = d > 100 ? 100 : d;
                    if (d < 0)
                        damage = 0;
                }
            } else {
                damage = 0;
            }

            if (fallHeight <= 12.0f) {
                impactEvent = 0;
            } else {
                int e = (int)((fallHeight - 12.0f) / 26.0f * 4.0f + 4.0f);
                BG_AnimScriptEvent(ps, ANIM_ET_LAND, 0, 1);
                impactEvent = e >= 25 ? 24 : e;
            }

            if (damage == 0) {
                int surf;

                if (fallHeight <= 4.0f) {

                } else if (fallHeight < 8.0f) {
                    surf = (pml->groundTrace.surfaceFlags & 0x1f00000) >> 0x14;
                    BG_AddPredictableEventToPlayerstate(
                        (!(pml->groundTrace.surfaceFlags & 0x20) && surf != 0)
                            ? surf + 24
                            : 0,
                        0, ps);
                } else if (fallHeight < 12.0f) {
                    surf = (pml->groundTrace.surfaceFlags & 0x1f00000) >> 0x14;
                    BG_AddPredictableEventToPlayerstate(
                        (!(pml->groundTrace.surfaceFlags & 0x20) && surf != 0)
                            ? surf + 1
                            : 0,
                        0, ps);
                } else {
                    ps->velocity[0] *= 0.67f;
                    ps->velocity[1] *= 0.67f;
                    ps->velocity[2] *= 0.67f;
                    surf = (pml->groundTrace.surfaceFlags & 0x20)
                               ? 93
                               : ((pml->groundTrace.surfaceFlags & 0x1f00000) >> 0x14) + 93;
                    BG_AddPredictableEventToPlayerstate(surf, impactEvent, ps);
                }
            } else {
                if (damage <= 99 && !(pml->groundTrace.surfaceFlags & 0x2)) {
                    int t = damage * 0x23 + 0x1f4;
                    ps->pm_time = t > 0x7d0 ? 0x7d0 : t;
                    ps->pm_flags |= 0x200;
                    ps->velocity[0] *= 0.2f;
                    ps->velocity[1] *= 0.2f;
                    ps->velocity[2] *= 0.2f;
                } else {
                    ps->velocity[0] *= 0.67f;
                    ps->velocity[1] *= 0.67f;
                    ps->velocity[2] *= 0.67f;
                }

                impactEvent = (pml->groundTrace.surfaceFlags & 0x20)
                                  ? 116
                                  : ((pml->groundTrace.surfaceFlags & 0x1f00000) >> 0x14) + 116;
                BG_AddPredictableEventToPlayerstate(impactEvent, damage, ps);
            }
        }
#endif
    }

    ps->groundEntityNum = trace.entityNum;

    {
        int entityNum = trace.entityNum;
        if (entityNum != 0x3fe) {
            int numtouch = pm->numtouch;
            if (numtouch != 0x20) {
                int j;
                for (j = 0; j < numtouch; j++) {
                    if (pm->touchents[j] == entityNum)
                        return;
                }
                pm->touchents[numtouch] = entityNum;
                pm->numtouch = numtouch + 1;
            }
        }
    }
}
#if defined(COD2_X64)
extern void Mantle_ClearHint(playerState_t *ps);
extern void Mantle_Check(pmove_t *pm, pml_t *pml);
extern void Mantle_Move(pmove_t *pm, playerState_t *ps, pml_t *pml);
extern int PM_InteruptWeaponWithProneMove(playerState_t *ps);
extern int PM_WeaponAmmoAvailable(playerState_t *ps);
extern const dvar_t *bg_foliagesnd_minspeed;
extern const dvar_t *bg_foliagesnd_maxspeed;
extern const dvar_t *bg_foliagesnd_resetinterval;
extern const dvar_t *bg_foliagesnd_fastinterval;
extern const dvar_t *bg_foliagesnd_slowinterval;

static void PM_ClearLadderFlag(playerState_t *ps)
{
    if (ps->pm_flags & PMF_LADDER)
        ps->pm_flags |= 0x40000;
    ps->pm_flags &= ~PMF_LADDER;
}

static void PM_UpdatePlayerWalkingFlag(pmove_t *pm)
{
    playerState_t *ps = pm->ps;
    ps->pm_flags &= ~0x100;
    if (ps->pm_type > 5 || !(pm->cmd.buttons & 0x1000) || (ps->pm_flags & 1) || !(ps->pm_flags & 0x40))
        return;
    if (ps->weaponstate != 5 && ps->weaponstate != 7 && ps->weaponstate != 9 && ps->weaponstate != 8 && ps->weaponstate != 6)
        ps->pm_flags |= 0x100;
}

static void PM_CheckLadderMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    if (pml->walking)
        ps->pm_flags &= ~0x40000;
    if (ps->pm_time && !(ps->pm_flags & PMF_LADDER) && (ps->pm_flags & 0x600))
        return;
    float distance = pml->walking ? 8.0f : 30.0f;
    int fellOff = (ps->pm_flags & PMF_LADDER) && ps->groundEntityNum == ENTITYNUM_NONE;
    vec3_t direction, mins, maxs, end;
    if (fellOff) {
        for (int i = 0; i < 3; ++i)
            direction[i] = -ps->vLadderVec[i];
    } else {
        direction[0] = pml->forward[0];
        direction[1] = pml->forward[1];
        direction[2] = 0.0f;
        Vec3Normalize(direction);
    }
    if (ps->pm_type > 5) {
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml->groundPlane = pml->almostGroundPlane = pml->walking = 0;
        PM_ClearLadderFlag(ps);
        return;
    }
    if ((ps->pm_flags & 0x40000) || PM_GetEffectiveStance(ps) == 1 || pm->cmd.serverTime - ps->jumpTime < 300) {
        PM_ClearLadderFlag(ps);
        return;
    }
    PM_VectorCopy(pm->mins, mins);
    PM_VectorCopy(pm->maxs, maxs);
    mins[0] += 6.0f;
    mins[1] += 6.0f;
    mins[2] = 8.0f;
    maxs[0] -= 6.0f;
    maxs[1] -= 6.0f;
    if (maxs[2] < mins[2])
        maxs[2] = mins[2];
    for (int i = 0; i < 3; ++i)
        end[i] = ps->origin[i] + distance * direction[i];
    trace_t trace;
    PM_playerTrace(pm, &trace, ps->origin, mins, maxs, end, ps->clientNum, pm->tracemask);
    if (trace.fraction < 1.0f && (trace.surfaceFlags & 8) && (!pml->walking || pm->cmd.forwardmove > 0)) {
        if (ps->pm_flags & PMF_LADDER) {
            ps->pm_flags |= PMF_LADDER;
            return;
        }
        PM_VectorCopy(trace.normal, ps->vLadderVec);
        for (int i = 0; i < 3; ++i)
            end[i] = ps->origin[i] + distance * -ps->vLadderVec[i];
        PM_playerTrace(pm, &trace, ps->origin, mins, maxs, end, ps->clientNum, pm->tracemask);
        if (trace.fraction < 1.0f && (trace.surfaceFlags & 8)) {
            ps->pm_flags |= PMF_LADDER;
            return;
        }
    }
    PM_ClearLadderFlag(ps);
    if (fellOff)
        BG_AnimScriptEvent(ps, ANIM_ET_JUMP, 0, 1);
}

static void PM_FlyMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    PM_Friction(ps, pml);
    float scale = PM_CmdScale(ps, &pm->cmd);
    vec3_t wishdir;
    for (int i = 0; i < 3; ++i)
        wishdir[i] = scale ? scale * pml->forward[i] * (float)pm->cmd.forwardmove + scale * pml->right[i] * (float)pm->cmd.rightmove : 0.0f;
    if (ps->speed) {
        float vertical = (float)ps->speed;
        if ((ps->pm_flags & 0x100) || ps->leanf != 0.0f)
            vertical *= 0.4f;
        vertical *= player_spectateSpeedScale->current.value;
        if (pm->cmd.buttons & 0x40)
            wishdir[2] -= vertical;
        if (pm->cmd.buttons & 0x80)
            wishdir[2] += vertical;
    }
    float wishspeed = Vec3Normalize(wishdir);
    PM_Accelerate(ps, pml, wishdir, wishspeed, 8.0f);
    PM_StepSlideMove(pm, pml, 0);
}

static void PM_FoliageSounds(pmove_t *pm)
{
    playerState_t *ps = pm->ps;
    if (pm->xyspeed < bg_foliagesnd_minspeed->current.value) {
        if (ps->foliageSoundTime + bg_foliagesnd_resetinterval->current.integer < pm->cmd.serverTime)
            ps->foliageSoundTime = 0;
        return;
    }
    float fraction = (pm->xyspeed - bg_foliagesnd_minspeed->current.value) / (bg_foliagesnd_maxspeed->current.value - bg_foliagesnd_minspeed->current.value);
    if (fraction > 1.0f)
        fraction = 1.0f;
    int interval = (int)((float)(bg_foliagesnd_fastinterval->current.integer - bg_foliagesnd_slowinterval->current.integer) * fraction + (float)bg_foliagesnd_slowinterval->current.integer);
    if (ps->foliageSoundTime + interval < pm->cmd.serverTime) {
        vec3_t mins, maxs;
        trace_t trace;
        for (int i = 0; i < 3; ++i) {
            mins[i] = pm->mins[i] * 0.75f;
            maxs[i] = pm->maxs[i] * 0.75f;
        }
        maxs[2] = pm->maxs[2] * 0.9f;
        PM_playerTrace(pm, &trace, ps->origin, mins, maxs, ps->origin, ps->clientNum, 2);
        if (trace.startsolid) {
            PM_AddEvent(ps, 0x8e);
            ps->foliageSoundTime = pm->cmd.serverTime;
        }
    }
}

/* Mac 1.3 folds PmoveSingle into Pmove at 0x70cf3. */
static void PmoveSingle(pmove_t *pm)
{
    playerState_t *ps = pm->ps;
    BG_AnimUpdatePlayerStateConditions(pm);
    if (ps->pm_flags & 0x8000) {
        pm->cmd.buttons &= 0x2300;
        pm->cmd.forwardmove = pm->cmd.rightmove = 0;
        PM_VectorClear(ps->velocity);
    }
    if (pm->cmd.buttons & 0x40000) {
        pm->cmd.buttons &= 0x43300;
        pm->cmd.forwardmove = pm->cmd.rightmove = 0;
    }
    ps->pm_flags &= ~0x10000;
    if (ps->pm_type > 5)
        pm->tracemask &= ~0x2000000;
    if (ps->pm_flags & 1) {
        if (PM_AbsInt(pm->cmd.forwardmove) > PM_AbsInt(pm->oldcmd.forwardmove) || PM_AbsInt(pm->cmd.rightmove) > PM_AbsInt(pm->oldcmd.rightmove)) {
            if (PM_InteruptWeaponWithProneMove(ps)) {
                ps->pm_flags &= ~0x800;
                PM_ExitAimDownSight(ps);
            }
        } else if (!(ps->pm_flags & 0x40) && (ps->weaponstate == 0 || ps->weaponstate == 1 || ps->weaponstate == 2 || ps->weaponstate == 5)) {
            ps->pm_flags &= ~0x800;
        }
    } else {
        ps->pm_flags &= ~0x800;
    }
    int stance = PM_GetEffectiveStance(ps);
    if ((ps->pm_flags & 0x40) && stance == 1)
        pm->cmd.forwardmove = pm->cmd.rightmove = 0;
    if (pm->cmd.buttons & 0x40000)
        ps->eFlags |= 0x200000;
    else
        ps->eFlags &= ~0x200000;
    ps->eFlags &= ~0x40;
    if (ps->pm_type != 5 && !(ps->pm_flags & 0x1000) && (ps->weaponstate == 0 || ps->weaponstate == 3) && PM_WeaponAmmoAvailable(ps) && (pm->cmd.buttons & 1))
        ps->eFlags |= 0x40;
    if (ps->pm_type <= 5 && !(pm->cmd.buttons & 0x4001))
        ps->pm_flags &= ~0x1000;
    pml_t pml;
    memset(&pml, 0, sizeof(pml));
    pml.msec = pm->cmd.serverTime - ps->commandTime;
    if (pml.msec < 1)
        pml.msec = 1;
    else if (pml.msec > 200)
        pml.msec = 200;
    ps->commandTime = pm->cmd.serverTime;
    PM_VectorCopy(ps->origin, pml.previous_origin);
    PM_VectorCopy(ps->velocity, pml.previous_velocity);
    pml.frametime = (float)pml.msec * 0.001f;
    PM_AdjustAimSpreadScale(pm, &pml);
    PM_UpdateViewAngles(ps, (float)pml.msec, &pm->cmd, pm->handler);
    AngleVectors(ps->viewangles, pml.forward, pml.right, pml.up);
    if (pm->cmd.forwardmove < 0)
        ps->pm_flags |= 0x80;
    else if (pm->cmd.forwardmove > 0 || pm->cmd.rightmove)
        ps->pm_flags &= ~0x80;
    if (ps->pm_type > 5 || (stance == 1 && (ps->pm_flags & 0x800)))
        pm->cmd.forwardmove = pm->cmd.rightmove = 0;
    Mantle_ClearHint(ps);
    if (ps->pm_type == 2 || ps->pm_type == 3 || ps->pm_type == 4) {
        PM_ClearLadderFlag(ps);
        PM_UpdateAimDownSightFlag(pm, &pml);
        PM_UpdatePlayerWalkingFlag(pm);
        if (ps->pm_type == 4) {
            PM_CheckDuck(pm, &pml);
            PM_FlyMove(pm, &pml);
        } else {
            PM_NoclipMove(pm, &pml);
        }
        PM_DropTimers(ps, pml.msec);
        PM_UpdateAimDownSightLerp(pm, &pml);
        return;
    }
    if (ps->pm_type == 5) {
        PM_ClearLadderFlag(ps);
        PM_UpdateAimDownSightFlag(pm, &pml);
        PM_UpdateAimDownSightLerp(pm, &pml);
        return;
    }
    if (ps->pm_type == 1 || ps->pm_type == 7 || (ps->eFlags & 0x300)) {
        PM_ClearLadderFlag(ps);
        ps->groundEntityNum = ENTITYNUM_NONE;
        PM_VectorClear(ps->velocity);
        PM_UpdateAimDownSightFlag(pm, &pml);
        PM_UpdatePlayerWalkingFlag(pm);
        PM_CheckDuck(pm, &pml);
        PM_DropTimers(ps, pml.msec);
        PM_Footsteps(pm, &pml);
        if (ps->eFlags & 0x300)
            PM_ResetWeaponState(ps);
        else
            PM_Weapon(pm, &pml);
        return;
    }
    if (!(ps->pm_flags & PMF_MANTLE)) {
        PM_CheckDuck(pm, &pml);
        PM_GroundTrace(pm, &pml);
    }
    Mantle_Check(pm, &pml);
    if (ps->pm_flags & PMF_MANTLE) {
        PM_ClearLadderFlag(ps);
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = pml.walking = 0;
        PM_UpdateAimDownSightFlag(pm, &pml);
        PM_UpdatePlayerWalkingFlag(pm);
        PM_CheckDuck(pm, &pml);
        Mantle_Move(pm, ps, &pml);
        if (COD2_NATIVE_TRACE_ENABLED())
            Com_Printf("[move-mantle] handler=%d time=%d msec=%d timer=%d trans=%d flags=%x origin=%.6f,%.6f,%.6f\n",
                       pm->handler, pm->cmd.serverTime, pml.msec, ps->mantleState.timer,
                       ps->mantleState.transIndex, ps->pm_flags,
                       (double)ps->origin[0], (double)ps->origin[1], (double)ps->origin[2]);
        PM_Weapon(pm, &pml);
        return;
    }
    PM_UpdateAimDownSightFlag(pm, &pml);
    PM_UpdatePlayerWalkingFlag(pm);
    PM_UpdatePronePitch(pm, &pml);
    if (ps->pm_type == 6 && pml.walking) {
        float speed = sqrtf(PM_DotProduct(ps->velocity, ps->velocity)) - 20.0f;
        if (speed <= 0.0f)
            PM_VectorClear(ps->velocity);
        else {
            Vec3Normalize(ps->velocity);
            for (int i = 0; i < 3; ++i)
                ps->velocity[i] *= speed;
        }
    }
    PM_CheckLadderMove(pm, &pml);
    PM_DropTimers(ps, pml.msec);
    if (ps->pm_flags & PMF_LADDER)
        PM_LadderMove(pm, &pml);
    else if (pml.walking)
        PM_WalkMove(pm, &pml);
    else
        PM_AirMove(pm, &pml);
    PM_GroundTrace(pm, &pml);
    PM_Footsteps(pm, &pml);
    PM_Weapon(pm, &pml);
    PM_FoliageSounds(pm);
    vec3_t move;
    for (int i = 0; i < 3; ++i)
        move[i] = ps->origin[i] - pml.previous_origin[i];
    float movement = PM_DotProduct(move, move) / (pml.frametime * pml.frametime);
    float velocity = PM_DotProduct(ps->velocity, ps->velocity);
    if (velocity * 0.25f > movement) {
        float scale = 1.0f / pml.frametime;
        for (int i = 0; i < 3; ++i)
            ps->velocity[i] = move[i] * scale;
    }
    for (int i = 0; i < 2; ++i)
        ps->oldVelocity[i] += (ps->velocity[i] - ps->oldVelocity[i]) * fminf(pml.frametime, 1.0f);
    Sys_SnapVector(ps->velocity);
}
#endif

void Pmove(pmove_t *pm)
{
#if defined(COD2_X64)
    playerState_t *ps = pm->ps;
    int finalTime = pm->cmd.serverTime;
    if (finalTime < ps->commandTime)
        return;
    if (finalTime > ps->commandTime + 1000)
        ps->commandTime = finalTime - 1000;
    pm->numtouch = 0;
    while (ps->commandTime != finalTime) {
        int msec = finalTime - ps->commandTime;
        if (msec > 66)
            msec = 66;
        pm->cmd.serverTime = ps->commandTime + msec;
        PmoveSingle(pm);
        pm->oldcmd = pm->cmd;
    }
#else
    playerState_t *ps;
    int finalTime;

    if (!pm || !pm->ps)
        return;

    ps = pm->ps;
    finalTime = pm->cmd.serverTime;
#ifdef PM_CMDTRACE
    {
        static int n;
        static int lastB;
        if (pm->cmd.buttons != lastB || (n++ % 240) == 0) {
            fprintf(stderr, "[cmd] buttons=0x%x fwd=%d right=%d weap=%d vhTarget=%d vhCur=%d pm_flags=0x%x pm_type=%d\n",
                    pm->cmd.buttons, pm->cmd.forwardmove, pm->cmd.rightmove, pm->cmd.weapon,
                    ps->viewHeightTarget, ps->viewHeightCurrent, ps->pm_flags, ps->pm_type);
            fflush(stderr);
            lastB = pm->cmd.buttons;
        }
    }
#endif

    if (finalTime < ps->commandTime)
        return;

    if (finalTime > ps->commandTime + 1000)
        ps->commandTime = finalTime - 1000;

    pm->numtouch = 0;

    while (ps->commandTime != finalTime) {
        pml_t pml;
        int msec;

        msec = finalTime - ps->commandTime;
        if (msec > 66)
            msec = 66;
        if (msec < 1)
            msec = 1;

        pm->cmd.serverTime = ps->commandTime + msec;

        BG_AnimUpdatePlayerStateConditions(pm);

        {
            extern int PM_WeaponAmmoAvailable(playerState_t * ps);
            if ((pm->cmd.buttons & 0x4) == 0)
                ps->eFlags &= ~0x200000;
            else
                ps->eFlags |= 0x200000;
            ps->eFlags &= ~0x40;
            if (ps->pm_type != 5) {
                if (!(ps->pm_flags & 0x1000) && (ps->weaponstate == 0 || ps->weaponstate == 3) &&
                    PM_WeaponAmmoAvailable(ps) && (pm->cmd.buttons & 0x1))
                    ps->eFlags |= 0x40;
                if (ps->pm_type <= 5 && (pm->cmd.buttons & 0x4001) == 0)
                    ps->pm_flags &= ~0x1000;
            } else {
                if ((pm->cmd.buttons & 0x4001) == 0)
                    ps->pm_flags &= ~0x1000;
            }
        }

        memset(&pml, 0, sizeof(pml));
        pml.msec = msec;
        pml.frametime = (float)msec * 0.001f;
        PM_VectorCopy(ps->origin, pml.previous_origin);
        PM_VectorCopy(ps->velocity, pml.previous_velocity);
        pml.previous_waterlevel = 0;

        PM_DropTimers(ps, msec);
        PM_AdjustAimSpreadScale(pm, &pml);
        PM_UpdateViewAngles(ps, (float)msec, &pm->cmd, pm->handler);
        AngleVectors(ps->viewangles, pml.forward, pml.right, pml.up);

        PM_CheckDuck(pm, &pml);
        PM_UpdateAimDownSightFlag(pm, &pml);

#if defined(COD2_X64)
        /* Mac 1.3 Pmove's case 2 is noclip, independent of ground contact. */
        if (ps->pm_type == 2) {
            ps->groundEntityNum = ENTITYNUM_NONE;
            ps->viewHeightTarget = 60;
            PM_NoclipMove(pm, &pml);
        } else
#endif
        if (ps->pm_type <= 5) {
            PM_GroundTrace(pm, &pml);

            if (ps->pm_flags & PMF_LADDER) {
                PM_LadderMove(pm, &pml);
            } else if (pml.walking && ps->groundEntityNum != ENTITYNUM_NONE) {
                PM_WalkMove(pm, &pml);
            } else {
                PM_AirMove(pm, &pml);
            }

            PM_GroundTrace(pm, &pml);
            PM_Footsteps(pm, &pml);
            PM_Weapon(pm, &pml);
        } else {
            ps->groundEntityNum = ENTITYNUM_NONE;
            PM_NoclipMove(pm, &pml);
            PM_ResetWeaponState(ps);
        }

        PM_UpdateAimDownSightLerp(pm, &pml);
        PM_ViewHeightAdjust(pm, &pml);

        pm->xyspeed = PM_VectorLength2D(ps->velocity);
        Sys_SnapVector(ps->velocity);

        ps->commandTime = pm->cmd.serverTime;
        pm->oldcmd = pm->cmd;
    }
#endif
}

pmoveHandler_t pmoveHandlers[2] = {
#if COD2_APPLE_SDK && defined(DEDICATED)
    { NULL, NULL, NULL },
#else
    { (pmove_trace)&CG_TraceCapsule, (pmove_pointcontents)&CG_PointContents, 0x0 },
#endif
    { (pmove_trace)&G_TraceCapsule, (pmove_pointcontents)&SV_PointContents, (pmove_PlayerEvent)&G_PlayerEvent }
};
viewLerpWaypoint_t viewLerp_CrouchProne[13] = {
    { 0x0, 40.0f, 0x0 },
    { 0xb, 38.0f, 0x0 },
    { 0x16, 33.0f, 0x0 },
    { 0x22, 25.0f, 0x0 },
    { 0x2d, 16.0f, 0x0 },
    { 0x32, 15.0f, 0x0 },
    { 0x37, 16.0f, 0x0 },
    { 0x46, 18.0f, 0x0 },
    { 0x5a, 17.0f, 0x0 },
    { 0x64, 11.0f, 0x0 },
    { 0xffffffff, 0.0f, 0x0 },
    { 0x0, 0.0f, 0x0 },
    { 0x0, 0.0f, 0x0 }
};
viewLerpWaypoint_t viewLerp_CrouchStand[10] = {
    { 0x0, 40.0f, 0x0 },
    { 0x5, 40.5f, 0x0 },
    { 0xa, 41.5f, 0x0 },
    { 0x14, 44.0f, 0x0 },
    { 0x46, 56.0f, 0x0 },
    { 0x60, 58.5f, 0x0 },
    { 0x63, 59.5f, 0x0 },
    { 0x64, 60.0f, 0x0 },
    { 0xffffffff, 0.0f, 0x0 },
    { 0x0, 0.0f, 0x0 }
};
viewLerpWaypoint_t viewLerp_ProneCrouch[8] = {
    { 0x0, 11.0f, 0x0 },
    { 0x5, 10.0f, 0x0 },
    { 0x1e, 21.0f, 0x0 },
    { 0x32, 25.0f, 0x0 },
    { 0x43, 31.0f, 0x0 },
    { 0x53, 34.0f, 0x0 },
    { 0x64, 40.0f, 0x0 },
    { 0xffffffff, 0.0f, 0x0 }
};
viewLerpWaypoint_t viewLerp_StandCrouch[10] = {
    { 0x0, 60.0f, 0x0 },
    { 0x1, 59.5f, 0x0 },
    { 0x4, 58.5f, 0x0 },
    { 0x1e, 56.0f, 0x0 },
    { 0x50, 44.0f, 0x0 },
    { 0x5a, 41.5f, 0x0 },
    { 0x5f, 40.5f, 0x0 },
    { 0x64, 40.0f, 0x0 },
    { 0xffffffff, 0.0f, 0x0 },
    { 0x0, 0.0f, 0x0 }
};
