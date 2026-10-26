#include "snapshot_source.h"
#include "playerstate_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

cg_t cgArray[1];
_Alignas(cgs_t) unsigned char cgsArray[sizeof(cgs_t)];
static centity_t entities[1024];
centity_t *cg_entities = entities;
static dvar_t nopredict, synchronous;
static char *nopredict_ptr = (char *)&nopredict;
static char *synchronous_ptr = (char *)&synchronous;
char **cg_dvar1 = &nopredict_ptr;
char **cg_dvar2 = &synchronous_ptr;
const dvar_t *cg_hudDamageIconTime;
void *imp_legacyHacks;
static snapshot_t old_snapshot, new_snapshot;
static int events, notifications, solid_lists, clones;
static unsigned source_tree = 1234, corpse_tree = 5678;

void BG_EvaluateTrajectory(const trajectory_t *tr, int time, vec_t *out)
{ (void)time; memcpy(out, tr->trBase, sizeof(vec3_t)); }
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *es, int extrapolate, int snap)
{
    assert(ps == &new_snapshot.ps && extrapolate == 0 && snap == 0);
    es->number = ps->clientNum;
    es->eType = 0;
    es->eFlags = 0;
}
void CG_ResetPlayerEntity(centity_t *cent) { (void)cent; assert(0); }
void XAnimCloneAnimTree(const XAnimTree *src, XAnimTree *dest)
{
    assert(src == (const XAnimTree *)&source_tree && dest == (XAnimTree *)&corpse_tree);
    ++clones;
    memcpy(dest, src, sizeof(source_tree));
}
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void I_strncpyz(char *dst, const char *src, int size)
{ assert(size > 0); snprintf(dst, (size_t)size, "%s", src); }
void CL_ResetSkeletonCache(int value) { assert(value == 0); }
void CG_SetFrameInterpolation(void) {}
void CG_ExecuteNewServerCommands(int seq) { assert(seq == 42); }
void CG_CheckOpenWaitingScriptMenu(void) {}
void CG_BuildSolidList(void) { ++solid_lists; }
void CG_ClearSolidList(void) { assert(0); }
void CG_PlaySmokeGrenadesAtTime(int time) { (void)time; assert(0); }
void CG_SetEquippedOffHand(int index) { (void)index; assert(0); }
void CG_CheckEvents(centity_t *cent) { (void)cent; assert(0); }
void CG_UpdatePlayerDObj(centity_t *cent) { (void)cent; assert(0); }
void CG_UpdateHandViewmodels(const char *value) { (void)value; assert(0); }
const char *CL_GetConfigString(int index) { (void)index; assert(0); return ""; }
void CG_GameMessage(const char *msg) { (void)msg; assert(0); }
const char *UI_SafeTranslateString(const char *msg) { (void)msg; assert(0); return ""; }
const char *va(const char *format, ...) { (void)format; assert(0); return ""; }
XAnim *XAnimGetAnims(const XAnimTree *tree) { (void)tree; assert(0); return NULL; }
Bool XAnimIsLooped(const XAnim *anims, unsigned int index)
{ (void)anims; (void)index; assert(0); return 0; }
int XAnimGetNumChildren(const XAnim *anims, unsigned int index)
{ (void)anims; (void)index; assert(0); return 0; }
void XAnimSetTime(XAnimTree *tree, unsigned int index, float time)
{ (void)tree; (void)index; (void)time; assert(0); }
void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up)
{ (void)angles; (void)forward; (void)right; (void)up; assert(0); }
const float AngleNormalize360(const float angle) { assert(0); return angle; }
float randomf(void) { assert(0); return 0; }
void CL_SetADS(int value) { (void)value; assert(0); }
void CG_HoldBreathInit(void) { assert(0); }
void CG_ResetLowHealthOverlay(void) { assert(0); }
void CG_MenuShowNotify(int menu) { assert(menu == 0); ++notifications; }
void CG_EntityEvent(centity_t *cent, int event)
{
    assert(cent == &cg->predictedPlayerEntity);
    assert(event == 73 && cent->nextState.eventParm == 92);
    ++events;
}

static void transition(const char *which)
{
    cg->snap = cg->nextSnap = &old_snapshot;
    cg->time = 1000;
    old_snapshot.serverTime = 900;
    old_snapshot.ps.eventSequence = 4;
    new_snapshot.ps.eventSequence = 5;
    new_snapshot.ps.events[0] = 73;
    new_snapshot.ps.eventParms[0] = 92;
    new_snapshot.ps.damageEvent = 1;
    new_snapshot.ps.damageCount = 10;
    new_snapshot.ps.damageYaw = new_snapshot.ps.damagePitch = 255;
    new_snapshot.serverCommandSequence = 42;
    /* Keep the old player present so the high flag does not force an entity reset. */
    entities[0].nextValid = 1;
    int expected = 0;
    if (!strcmp(which, "transition-high")) { new_snapshot.ps.pm_flags = 0x400000; expected = 1; }
    else if (!strcmp(which, "transition-low")) new_snapshot.ps.pm_flags = 0x40;
    else if (!strcmp(which, "transition-demo")) { cg->demoType = 1; expected = 1; }
    else if (!strcmp(which, "transition-nopredict")) { nopredict.current.enabled = 1; expected = 1; }
    else if (!strcmp(which, "transition-synchronous")) { synchronous.current.enabled = 1; expected = 1; }
    else if (!strcmp(which, "transition-null-dvars")) cg_dvar1 = cg_dvar2 = NULL;
    else assert(!strcmp(which, "transition-ordinary"));
    CG_SetNextSnap(&new_snapshot);
    assert(cg->nextSnap == &new_snapshot && cg->snap == &old_snapshot);
    assert(solid_lists == 1);
    assert(events == expected && notifications == expected);
    assert(cg->attackerTime == (expected ? 1000 : 0));
    assert(cg->v_dmg_time == (expected ? 1500 : 0));
    assert(cg->damageTime == (expected ? 900 : 0));
    assert(cg->v_dmg_pitch == (expected ? -5.0f : 0.0f));
    assert(old_snapshot.ps.eventSequence == 4);
}

static void corpse_case(const char *which)
{
    centity_t *cent = &entities[64];
    clientInfo_t *source = &cg->bgs.clientinfo[3];
    clientInfo_t *dest = &cgs->corpseinfo[0];
    source->clientNum = dest->clientNum = 3;
    strcpy(source->model, "new-player-model");
    strcpy(dest->model, "retained-corpse-model");
    strcpy(source->attachModelNames[0], "backpack");
    strcpy(source->attachTagNames[0], "J_Spine4");
    strcpy(source->attachModelNames[1], "helmet");
    strcpy(source->attachTagNames[1], "J_Head");
    source->pXAnimTree = (XAnimTree *)&source_tree;
    dest->pXAnimTree = (XAnimTree *)&corpse_tree;
    cent->nextState.number = 64;
    cent->nextState.clientNum = 3;
    cent->nextState.eType = 2;
    cent->nextState.eventSequence = 17;
    cent->previousEventSequence = 99;
    int clone = !strcmp(which, "corpse-high");
    int copy = clone;
    if (clone) cent->nextState.eFlags = 0x80000;
    else if (!strcmp(which, "corpse-low")) cent->nextState.eFlags = 8;
    else if (!strcmp(which, "corpse-empty")) { dest->model[0] = 0; copy = 1; }
    else if (!strcmp(which, "corpse-mismatch")) { dest->clientNum = 4; copy = 1; }
    else assert(!strcmp(which, "corpse-ordinary"));
    CG_ResetEntity((char *)cent);
    assert(!strcmp(dest->model, copy ? "new-player-model" : "retained-corpse-model"));
    assert(dest->clientNum == 3 && dest->dobjDirty == 1);
    assert(dest->pXAnimTree == (XAnimTree *)&corpse_tree);
    assert(source->pXAnimTree == (XAnimTree *)&source_tree);
    assert(clones == clone && corpse_tree == (clone ? 1234 : 5678));
    assert(cent->previousEventSequence == (clone ? 0 : 17));
    assert(cent->currentState.eFlags == cent->nextState.eFlags);
    if (copy) {
        assert(dest->attachModelNames[0][0] == 0 && dest->attachTagNames[0][0] == 0);
        assert(!strcmp(dest->attachModelNames[1], "helmet"));
        assert(!strcmp(dest->attachTagNames[1], "J_Head"));
        assert(!strcmp(source->attachModelNames[0], "backpack"));
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strncmp(argv[1], "transition-", 11)) transition(argv[1]);
    else corpse_case(argv[1]);
    puts("snapshot behavior passed");
    return 0;
}
