#include "snapshot_source.h"
#include "playerstate_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <setjmp.h>

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
static int snapshot_number, reads, interpolations;
static snapshot_t incoming_snapshot;
static jmp_buf process_error;
static int expect_time_error, expect_initial, initializations, clears;
static int legacy_hacks[3];
static void *legacy_hacks_ptr = legacy_hacks;
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
void CG_SetFrameInterpolation(void) { ++interpolations; }
void CG_ExecuteNewServerCommands(int seq) { assert(seq == 42); }
void CG_CheckOpenWaitingScriptMenu(void) {}
void CG_BuildSolidList(void) { ++solid_lists; }
void CG_ClearSolidList(void) { assert(expect_initial); ++clears; }
void CG_PlaySmokeGrenadesAtTime(int time) { assert(expect_initial && time == incoming_snapshot.serverTime); }
void CG_SetEquippedOffHand(int index) { assert(expect_initial && index == 0); }
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
void CL_SetADS(int value) { assert(expect_initial && value == 0); }
void CG_HoldBreathInit(void) { assert(expect_initial); }
void CG_ResetLowHealthOverlay(void) { assert(expect_initial); }
void CG_MenuShowNotify(int menu) { assert(menu == 0); ++notifications; }
void CG_EntityEvent(centity_t *cent, int event)
{
    assert(cent == &cg->predictedPlayerEntity);
    assert(event == 73 && cent->nextState.eventParm == 92);
    ++events;
}

void CL_GetCurrentSnapshotNumber(int *number, int *time)
{ *number = snapshot_number; *time = 1200; }
int CL_GetSnapshot(int number, snapshot_t *dest)
{
    assert(number == 2 && reads == 0);
    ++reads;
    *dest = incoming_snapshot;
    return 1;
}
void CG_AddLagometerSnapshotInfo(snapshot_t *snap)
{ assert(snap && snap->serverTime == incoming_snapshot.serverTime); }
void CG_SafeDObjFree(int entnum) { (void)entnum; assert(0); }
void CG_InitView(void) { assert(expect_initial); ++initializations; }
void SND_SetListener(int entnum, const vec_t *origin, vec3_t *axis)
{ (void)origin; (void)axis; assert(expect_initial && entnum == 0); }
void SND_FadeAllSounds(float volume, int duration)
{ assert(expect_initial && volume == 1.0f && duration == 0); }
void AnglesToAxis(const vec_t *angles, vec3_t *axis)
{ (void)angles; assert(expect_initial); memset(axis, 0, sizeof(vec3_t) * 3); }
void Com_Error(int level, const char *format, ...)
{
    assert(expect_time_error && level == 1);
    assert(strstr(format, "Server time went backwards"));
    longjmp(process_error, 1);
}
void Com_Printf(const char *format, ...) { (void)format; assert(0); }

static void process_case(const char *which)
{
    int cubemap = strstr(which, "cubemap") != NULL;
    int incoming = strstr(which, "incoming") != NULL;
    int future = strstr(which, "future") != NULL;
    int before = strstr(which, "before") != NULL;
    int replace = strstr(which, "replace") != NULL;
    int backward = strstr(which, "backward") != NULL;
    int inactive = strstr(which, "inactive") != NULL;
    int restart = strstr(which, "restart") != NULL;
    for (int face = cubemap ? CUBEMAPSHOT_UP : CUBEMAPSHOT_NONE;
         face <= (cubemap ? CUBEMAPSHOT_BACK : CUBEMAPSHOT_NONE); ++face) {
        memset(cg, 0, sizeof(*cg));
        memset(cgs, 0, sizeof(*cgs));
        memset(&old_snapshot, 0, sizeof(old_snapshot));
        memset(&new_snapshot, 0, sizeof(new_snapshot));
        memset(&incoming_snapshot, 0, sizeof(incoming_snapshot));
        reads = interpolations = solid_lists = initializations = clears = 0;
        expect_initial = restart && !replace;
        imp_legacyHacks = &legacy_hacks_ptr;
        old_snapshot.serverTime = 900;
        new_snapshot.serverTime = future ? 1100 : 1000;
        incoming_snapshot.serverTime = new_snapshot.serverTime;
        incoming_snapshot.serverCommandSequence = 42;
        if (backward) incoming_snapshot.serverTime = 850;
        if (inactive) incoming_snapshot.snapFlags = 2;
        if (restart) incoming_snapshot.snapFlags = 4;
        cg->activeSnapshots[0] = old_snapshot;
        cg->activeSnapshots[1] = new_snapshot;
        cg->snap = &cg->activeSnapshots[0];
        cg->nextSnap = incoming && !replace ? cg->snap : &cg->activeSnapshots[1];
        cg->time = before ? 800 : 1000;
        cg->bgs.time = cg->time;
        cg->cubemapShot = (CubemapShot)face;
        cg->latestSnapshotNum = cgs->processedSnapshotNum = 1;
        snapshot_number = incoming ? 2 : 1;
        expect_time_error = backward && !replace;
        if (setjmp(process_error)) {
            assert(expect_time_error && reads == 1 && solid_lists == 0);
            continue;
        }
        CG_ProcessSnapshots();
        assert(!expect_time_error);
        int transition_expected = (!cubemap || incoming) && (!future || before);
        snapshot_t *pending = &cg->activeSnapshots[1];
        assert(cg->snap == (transition_expected ? pending : &cg->activeSnapshots[0]));
        assert(cg->nextSnap == pending);
        assert(reads == incoming && cgs->processedSnapshotNum == snapshot_number);
        assert(solid_lists == incoming);
        assert(initializations == expect_initial && clears == expect_initial);
        assert(interpolations == 1 + incoming);
        assert(cg->bgs.latestSnapshotTime == 1200);
        int expected_time = before ? (transition_expected ? pending->serverTime : 900) : 1000;
        assert(cg->time == expected_time && cg->bgs.time == expected_time);
    }
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
    else if (!strncmp(argv[1], "process-", 8)) process_case(argv[1]);
    else corpse_case(argv[1]);
    puts("snapshot behavior passed");
    return 0;
}
