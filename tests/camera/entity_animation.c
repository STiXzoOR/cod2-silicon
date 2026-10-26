#include "players_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

extern void CG_DObjUpdateInfo(DObj *obj);
extern void CG_ProcessClientNoteTracks(int clientNum);
extern void CG_CalcEntityLerpPositions(centity_t *cent);
extern void CG_AdjustPositionForMover(const vec_t *, int, int, int, vec_t *, vec_t *);

cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static centity_t entities[1024];
centity_t *cg_entities = entities;
void *imp_cg_entities = &cg_entities;
scr_const_t scr_const;
byte *cg_pmove_mins;
static snapshot_t snapshot;
static DObj object;
static XAnimNotify notes[2];
static int updates, goals, scenes, notify_count;
static float elapsed;
static char calls[32];
static int count;
static clientInfo_t *expected_ci;
static void Call(char c) { assert(count < sizeof(calls) - 1); calls[count++] = c; }

DObj *Com_GetClientDObj(int handle, int client) { assert(handle >= 0 && handle < 1024 && client == 0); return &object; }
void DObjUpdateClientInfo(DObj *obj, float dt)
{
    assert(obj == &object); ++updates; elapsed += dt; Call('U');
    notes[0].type = 1; notes[0].name = "anim_gunhand = \"left\"";
    notify_count = 1;
}
int DObjGetClientNotifyList(XAnimNotify **list) { *list = notes; Call('N'); return notify_count; }
void BG_PlayerAnimation(const DObj *obj, entityState_t *es, clientInfo_t *ci)
{ assert(obj == &object && ci == expected_ci); ++goals; Call('G'); }
void BG_UpdatePlayerDObj(DObj *obj, entityState_t *es, clientInfo_t *ci, int flag)
{ assert(obj == &object && ci == expected_ci && flag == 0); Call('D'); }
void CG_AddCEntityToScene(const GfxEntity *body, const DObj *obj, const centity_t *cent)
{ assert(obj == &object); ++scenes; Call('S'); }
void CG_AddPlayerWeapon(GfxEntity *body, playerState_t *ps, centity_t *cent, qboolean flag) { assert(0); }
void BG_EvaluateTrajectory(const trajectory_t *tr, int time, vec_t *out)
{ for (int i = 0; i < 3; ++i) out[i] = tr->trBase[i] + (tr->trType == TR_LINEAR ? tr->trDelta[i] * (float)time : 0.0f); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }

/* Unused turret branches are still linked from the complete production player TU. */
void Com_Printf(const char *fmt, ...) { assert(0); }
void Com_Error(int level, const char *fmt, ...) { assert(0); }
WeaponDef *BG_GetWeaponDef(int index) { assert(0); return NULL; }
DObjAnimMat *CG_DObjGetLocalTagMatrix(const centity_t *cent, DObj *obj, unsigned int tag) { assert(0); return NULL; }
void CG_TraceCapsule(byte *trace, float *start, float *mins, float *maxs, float *end, int ent, int mask) { assert(0); }
void XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, unsigned int index, float time) { assert(0); }
int XAnimSetGoalWeight(XAnimTree *tree, unsigned int index, float weight, float time, float rate, unsigned int name, unsigned int type, int restart) { assert(0); return 0; }
float XAnimGetWeight(const XAnimTree *tree, unsigned int index) { assert(0); return 0; }
int XAnimGetNumChildren(const XAnim *anims, unsigned int index) { assert(0); return 0; }
unsigned int XAnimGetChildAt(const XAnim *anims, unsigned int parent, unsigned int child) { assert(0); return 0; }
const char *XAnimGetAnimDebugName(const XAnim *anims, unsigned int index) { assert(0); return NULL; }
void XAnimCalcAbsDelta(XAnimTree *tree, unsigned int index, float *rot, float *trans) { assert(0); }

static void TestBody(int corpse)
{
    centity_t *cent = &entities[corpse ? 64 : 2];
    cg->snap = cg->nextSnap = &snapshot;
    cg->frametime = 20;
    cent->nextState.number = corpse ? 64 : 2;
    cent->nextState.clientNum = 2;
    cent->nextState.eType = corpse ? 2 : 1;
    cent->nextState.eFlags = 2; /* Render the body but not the separate weapon. */
    expected_ci = corpse ? &cgs->corpseinfo[0] : &cg->bgs.clientinfo[2];
    /* The active-frame fixture owns snapshot-loop wiring. This fixture executes
     * its real preparation and rendering consumers to catch duplicate updates. */
    CG_DObjUpdateInfo(&object);
    CG_ProcessClientNoteTracks(cent->nextState.number);
    if (corpse) CG_Corpse(cent); else CG_Player(cent);
    assert(updates == 1 && goals == 1 && scenes == 1);
    assert(fabsf(elapsed - 0.02f) < 0.000001f);
    assert(strcmp(calls, corpse ? "UDGS" : "UNGS") == 0);
    if (!corpse) assert(expected_ci->leftHandGun == 1 && expected_ci->dobjDirty == 1);
}

static void TestCorpseLerp(int slot)
{
    centity_t *cent = &entities[64 + slot];
    clientInfo_t *ci = &cgs->corpseinfo[slot];
    unsigned char before[sizeof(cgs_t)];
    memset(cgsArray, 0x5a, sizeof(cgsArray));
    memcpy(before, cgsArray, sizeof(before));
    cg->snap = cg->nextSnap = &snapshot;
    cg->time = 1100;
    cent->currentState.number = cent->nextState.number = 64 + slot;
    cent->currentState.eType = cent->nextState.eType = 2;
    cent->currentState.pos.trType = TR_STATIONARY;
    cent->currentState.pos.trBase[0] = 11;
    cent->currentState.pos.trBase[1] = 22;
    cent->currentState.pos.trBase[2] = 33;
    cent->currentState.apos.trBase[0] = 14;
    cent->currentState.apos.trBase[1] = 25;
    cent->currentState.apos.trBase[2] = 36;
    cent->nextState.angles2[1] = 47;
    cent->nextState.leanf = 0.75f;
    CG_CalcEntityLerpPositions(cent);
    assert(ci->lerpMoveDir == 47 && ci->lerpLean == 0.75f);
    assert(ci->playerAngles[0] == 14 && ci->playerAngles[1] == 25 && ci->playerAngles[2] == 36);
    assert(cent->lerpAngles[0] == 0 && cent->lerpAngles[1] == 25 && cent->lerpAngles[2] == 0);
    assert(cent->lerpOrigin[0] == 11 && cent->lerpOrigin[1] == 22 && cent->lerpOrigin[2] == 33);
    size_t start = (unsigned char *)&ci->lerpMoveDir - cgsArray;
    size_t end = (unsigned char *)(ci->playerAngles + 3) - cgsArray;
    assert(memcmp(cgsArray, before, start) == 0);
    assert(memcmp(cgsArray + end, before + end, sizeof(before) - end) == 0);
}

static void TestMover(const char *mode)
{
    int number = !strcmp(mode, "mover-bounds") ? 1022 : 10;
    centity_t *mover = &entities[number];
    vec3_t position = {1, 2, 3}, angles = {99, 99, 99};
    mover->currentState.eType = !strcmp(mode, "mover-enter") ? 0 : 6;
    mover->nextState.eType = !strcmp(mode, "mover-exit") ? 0 : 6;
    mover->currentState.pos.trType = mover->currentState.apos.trType = TR_LINEAR;
    mover->currentState.pos.trDelta[0] = 10;
    mover->currentState.apos.trDelta[1] = 20;
    if (!strcmp(mode, "mover-rounding")) {
        mover->currentState.pos.trBase[0] = 65536;
        mover->currentState.pos.trDelta[0] = 1;
        mover->currentState.pos.trBase[1] = mover->currentState.pos.trBase[2] = 100000000;
        position[0] = 0.00390625f;
        position[1] = 1;
        position[2] = -1;
    }
    CG_AdjustPositionForMover(position, number, 0, 1, position, angles);
    if (!strcmp(mode, "mover-rounding")) {
        unsigned int bits[3];
        memcpy(bits, position, sizeof(bits));
        /* Mac SUBSS then ADDSS: 1 + 1/256, 0 + 1, 0 - 1. */
        assert(bits[0] == 0x3f808000 && bits[1] == 0x3f800000 && bits[2] == 0xbf800000);
        assert(angles[0] == 0 && angles[1] == 20 && angles[2] == 0);
    } else if (!strcmp(mode, "mover-exit") || !strcmp(mode, "mover-bounds")) {
        assert(position[0] == 1 && position[1] == 2 && position[2] == 3);
        assert(angles[0] == 0 && angles[1] == 0 && angles[2] == 0);
    } else {
        assert(position[0] == 11 && position[1] == 2 && position[2] == 3);
        assert(angles[0] == 0 && angles[1] == 20 && angles[2] == 0);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "player")) TestBody(0);
    else if (!strcmp(argv[1], "corpse")) TestBody(1);
    else if (!strcmp(argv[1], "corpse-first")) TestCorpseLerp(0);
    else if (!strcmp(argv[1], "corpse-last")) TestCorpseLerp(7);
    else if (!strncmp(argv[1], "mover-", 6)) TestMover(argv[1]);
    else abort();
    puts("PASS");
    return 0;
}
