#include "prediction_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

cg_t cgArray[1];
static centity_t entities[1024];
centity_t *cg_entities = entities;
static dvar_t disabled;
static const dvar_t *disabled_ptr = &disabled;
const dvar_t **dvar_nopredict = &disabled_ptr;
const dvar_t **dvar_synchronous = &disabled_ptr;
const dvar_t **dvar_developer = &disabled_ptr;
const dvar_t **dvar_autoPickup = &disabled_ptr;
const dvar_t **dvar_errorDecay = &disabled_ptr;
static snapshot_t snapshot;
static playerState_t saved_state;
static WeaponDef weapon;
static int zoom, commands_available, move_count, transition_count, publication_count;

void LargeLocal_LargeLocal(void *ll, int size) { (void)ll; assert(size == sizeof(saved_state)); }
void *LargeLocal_GetBuf(const LargeLocal *ll) { (void)ll; return &saved_state; }
void ZN10LargeLocalD1Ev(void *ll) { (void)ll; }
int CL_GetCurrentCmdNumber(void) { return 128; }
qboolean CL_GetUserCmd(int number, usercmd_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->serverTime = number == 128 ? 1000 : 800;
    return commands_available;
}
void Pmove(pmove_t *pm)
{
    ++move_count;
    pm->ps->commandTime = pm->cmd.serverTime;
    pm->ps->origin[1] += 1.0f;
}
void CG_AdjustPositionForMover(const vec_t *in, int mover, int from, int to,
                               vec_t *out, vec_t *angles)
{
    /* Synthetic platform: 100 units/s translation and 250 degrees/s yaw. */
    float elapsed = mover == 7 ? (float)(to - from) : 0.0f;
    out[0] = in[0] + elapsed * 0.1f;
    out[1] = in[1];
    out[2] = in[2];
    angles[0] = angles[2] = 0.0f;
    angles[1] = elapsed * 0.25f;
}
void CG_TransitionPlayerState(playerState_t *ps, playerState_t *old)
{
    assert(ps == &cg->predictedPlayerState && old == &saved_state);
    ++transition_count;
}
void BG_EvaluateTrajectory(const trajectory_t *tr, int time, vec_t *out)
{ (void)tr; (void)time; memset(out, 0, sizeof(vec3_t)); }
WeaponDef *BG_GetWeaponDef(int index) { (void)index; return &weapon; }
qboolean CG_GetWeapReticleZoom(float *value) { *value = 1.0f; return zoom; }
float flrand(float min, float max) { (void)min; return max; }
float randomf(void) { return 0.0f; }
const float AngleNormalize360(const float angle) { return angle; }
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *es, int a, int b)
{
    assert(ps == &cg->predictedPlayerState && es == &cg->predictedPlayerEntity.nextState);
    assert(a == 0 && b == 0);
    ++publication_count;
    es->eFlags = 0x40;
}
void PM_UpdateViewAngles(playerState_t *ps, float time, usercmd_t *cmd, int mask)
{ (void)ps; (void)time; (void)cmd; (void)mask; assert(0); }
const float LerpAngle(const float from, const float to, const float frac)
{ return from + (to - from) * frac; }
qboolean BG_PlayerTouchesItem(playerState_t *ps, entityState_t *item, int time)
{ (void)ps; (void)item; (void)time; assert(0); return 0; }
qboolean BG_CanItemBeGrabbed(const entityState_t *ent, const playerState_t *ps, qboolean touched)
{ (void)ent; (void)ps; (void)touched; assert(0); return 0; }
void BG_AddPredictableEventToPlayerstate(int event, int param, playerState_t *ps)
{ (void)event; (void)param; (void)ps; assert(0); }
int CM_BoxTrace(trace_t *trace, const vec_t *start, const vec_t *end,
                const vec_t *mins, const vec_t *maxs, clipHandle_t model, int mask)
{ (void)trace; (void)start; (void)end; (void)mins; (void)maxs; (void)model; (void)mask; assert(0); return 0; }
void Com_Printf(const char *format, ...) { (void)format; }

static void near(float actual, float expected)
{
    if (fabsf(actual - expected) >= 0.001f)
        fprintf(stderr, "actual %g, expected %g\n", (double)actual, (double)expected);
    assert(fabsf(actual - expected) < 0.001f);
}
static void setup(void)
{
    memset(cg, 0, sizeof(*cg));
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&weapon, 0, sizeof(weapon));
    cg->snap = cg->nextSnap = &snapshot;
    cg->time = 1000;
    cg->oldTime = 950;
    snapshot.serverTime = 900;
    snapshot.ps.commandTime = 800;
    snapshot.ps.groundEntityNum = 7;
    for (int i = 0; i < 3; ++i) snapshot.ps.origin[i] = 100.0f;
    cg->predictedPlayerState = snapshot.ps;
    commands_available = 1;
    zoom = move_count = transition_count = publication_count = 0;
}

static void movement(int mover)
{
    setup();
    snapshot.ps.groundEntityNum = mover;
    cg->predictedPlayerState.origin[0] = mover == 7 ? 107.0f : 100.0f;
    CG_PredictPlayerState();
    assert(move_count == 1);
    near(cg->predictedPlayerState.origin[0], mover == 7 ? 110.0f : 100.0f);
    near(cg->predictedPlayerState.origin[1], 101.0f);
    near(cg->predictedPlayerState.origin[2], 100.0f);
    near(cg->predictedError[0], mover == 7 ? 2.0f : 0.0f);
    near(cg->predictedError[1], 0.0f);
    near(cg->predictedError[2], 0.0f);
    assert(cg->predictedPlayerState.delta_angles[1] == (mover == 7 ? 2275 : 0));
    assert(cg->predictedErrorTime == (mover == 7 ? 950 : 0));
    near(entities[0].lerpOrigin[0], cg->predictedPlayerState.origin[0]);
}

static void no_movement(int missing, int acknowledged_ahead)
{
    setup();
    snapshot.ps.commandTime = 1000 + acknowledged_ahead;
    commands_available = !missing;
    CG_PredictPlayerState();
    assert(move_count == 0);
    assert(transition_count == !missing);
    assert(publication_count == 1);
    assert(cg->predictedPlayerEntity.currentState.eFlags == 0x40);
    for (int i = 0; i < 3; ++i) {
        near(cg->predictedPlayerState.origin[i], 100.0f);
        near(cg->predictedError[i], 0.0f);
        assert(cg->predictedPlayerState.delta_angles[i] == 0);
    }
}

static void ads(const char *which)
{
    setup();
    snapshot.ps.commandTime = 1000;
    snapshot.ps.groundEntityNum = 1023;
    snapshot.ps.pm_flags = 0x800000;
    weapon.adsViewErrorMax = 4.0f;
    zoom = 1;
    CG_PredictPlayerState();
    assert(cg->adsViewErrorDone == 1);
    near(cg->offsetAngles[0], 0.0f);
    near(cg->offsetAngles[1], 4.0f);
    CG_PredictPlayerState();
    near(cg->offsetAngles[1], 4.0f);
    if (!strcmp(which, "zoom")) return;
    if (!strcmp(which, "ads-flag")) snapshot.ps.pm_flags = 0;
    else if (!strcmp(which, "ads-zero-max")) weapon.adsViewErrorMax = 0.0f;
    else zoom = 0;
    CG_PredictPlayerState();
    near(cg->offsetAngles[1], 4.0f);
    if (!strcmp(which, "ads-zero-max")) {
        assert(cg->adsViewErrorDone == 1);
        return;
    }
    if (strcmp(which, "ads-reentry")) assert(cg->adsViewErrorDone == 0);
    zoom = 1;
    snapshot.ps.pm_flags = 0x800000;
    CG_PredictPlayerState();
    assert(cg->adsViewErrorDone == 1);
    near(cg->offsetAngles[0], 0.0f);
    near(cg->offsetAngles[1], 8.0f);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "mover")) movement(7);
    else if (!strcmp(argv[1], "no-mover")) movement(1023);
    else if (!strcmp(argv[1], "no-command")) no_movement(0, 0);
    else if (!strcmp(argv[1], "acknowledged-ahead")) no_movement(0, 1);
    else if (!strcmp(argv[1], "missing-command")) no_movement(1, 0);
    else ads(argv[1]);
    puts("prediction case passed");
    return 0;
}
