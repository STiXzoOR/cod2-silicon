#include "PC/cgame_mp/cg_players_mp.c"
#include <assert.h>

cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)];
static centity_t entities[1024];
centity_t *cg_entities = entities;
static XAnimTree_s playerTrees[64];
static int expectedClient, updates, resets;
static dvar_t debugPosition;
const dvar_t *cg_debugPosition = &debugPosition;
void Com_Printf(const char *format, ...) { (void)format; }

struct DObj_s *Com_GetClientDObj(int handle, int localClientNum)
{
    assert(handle == expectedClient && localClientNum == 0);
    return NULL;
}

void BG_UpdatePlayerDObj(struct DObj_s *obj, entityState_t *state, clientInfo_t *info, int flags)
{
    assert(!obj && !flags && state->clientNum == expectedClient);
    assert(info == &cg->bgs.clientinfo[expectedClient]);
    assert(info->pXAnimTree == &playerTrees[expectedClient]);
    ++updates;
}

void XAnimClearTreeGoalWeights(XAnimTree *tree, unsigned int index, float blend)
{
    assert(tree == &playerTrees[expectedClient]);
    (void)index; (void)blend;
    ++resets;
}

void XAnimSetCompleteGoalWeight(XAnimTree_s *tree, unsigned int index, float weight, float time,
                               float rate, unsigned int name, unsigned int type, int restart)
{
    assert(tree == &playerTrees[expectedClient]);
    (void)index; (void)weight; (void)time; (void)rate; (void)name; (void)type; (void)restart;
}

int main(void)
{
    for (int i = 0; i < 64; ++i) {
        cg->bgs.clientinfo[i].pXAnimTree = &playerTrees[i];
        cg->bgs.clientinfo[i].playerAngles[1] = (float)i;
        cg->bgs.clientinfo[i].dobjDirty = 1;
    }
    for (expectedClient = 0; expectedClient < 64; ++expectedClient) {
        centity_t *entity = &entities[expectedClient];
        entity->nextValid = 1;
        entity->nextState.clientNum = expectedClient;
        CG_UpdatePlayerDObj(entity);
        CG_ResetPlayerEntity(entity);
        assert(cg->bgs.clientinfo[expectedClient].legs.yawAngle == (float)expectedClient);
        assert(cg->bgs.clientinfo[expectedClient].torso.yawAngle == (float)expectedClient);
        assert(cg->bgs.clientinfo[expectedClient].dobjDirty == 1);
    }
    assert(updates == 64 && resets == 64);
    puts("PASS: all 64 native player records retain their own animation tree on update/reset");
}
