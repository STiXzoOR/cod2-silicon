#include "PC/cgame_mp/cg_main_mp.c"
#include "PC/cgame_mp/cg_playerstate_mp.c"
#include <assert.h>
#include <stdlib.h>
cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)];
static centity_t entities[1024];
centity_t *cg_entities = entities;
static XAnimTree_s fixtureTrees[72];
static int allocated;
static snd_alias_list_t sound;
XAnimTree *XAnimCreateTree(XAnim *anims, Alloc_t alloc)
{
    (void)anims; (void)alloc;
    assert(allocated < 72);
    return &fixtureTrees[allocated++];
}
void *Hunk_AllocInternal(int size) { return malloc(size); }
void *Hunk_AllocAlignInternal(int size, int align) { (void)align; return malloc(size); }
snd_alias_list_t *Com_FindSoundAlias(const char *name) { assert(name && *name); return &sound; }
const char *Com_SurfaceTypeToName(int n) { assert(n >= 0 && n < 23); return "default"; }
const dvar_t *cg_hudDamageIconTime;
void CG_MenuShowNotify(int menu) { (void)menu; assert(!"no damage in player event fixture"); }
static int playerEvents;
void CG_EntityEvent(centity_t *cent, int event)
{
    assert(cent==&cg->predictedPlayerEntity && event==158);
    assert(cent->nextState.weapon==17 && cent->nextState.eventParm==42);
    ++playerEvents;
}
const float AngleNormalize360(float angle){return angle;}
float randomf(void){return .5f;}
void AngleVectors(const float *angles,float *f,float *r,float *u){(void)angles;(void)f;(void)r;(void)u;}
int main(void)
{
    memset(cg, 0, sizeof(*cg));
    memset(cgs, 0, sizeof(*cgs));
    playerState_t ps={0},old={0};
    ps.eventSequence=1;ps.events[0]=158;ps.eventParms[0]=42;
    cg->predictedPlayerEntity.nextState.weapon=17;
    CG_TransitionPlayerState(&ps,&old);
    assert(playerEvents==1);
    CG_InitXAnimTrees();
    for (int i=0; i<64; ++i) assert(cg->bgs.clientinfo[i].pXAnimTree == &fixtureTrees[i]);
    for (int i=0; i<8; ++i) assert(cgs->corpseinfo[i].pXAnimTree == &fixtureTrees[64+i]);
    CG_RegisterSounds();
    assert(cgs->media.noAmmoSound == &sound && cgs->media.playerSwapOffhand == &sound);
    for (int i=0; i<23; ++i) assert(cgs->media.grenadeBounceSound[i] == &sound && cgs->media.landSoundPlayer[i] == &sound);
    for (int i=0; i<8; ++i) assert(cgs->corpseinfo[i].pXAnimTree == &fixtureTrees[64+i]);
    for (int i=0; i<1024; ++i) { entities[i].tree=&fixtureTrees[0]; entities[i].localClientNum=-1; }
    CG_ClearEntityDObjHandles();
    for (int i=0; i<1024; ++i) assert(entities[i].tree==&fixtureTrees[0] && entities[i].localClientNum==0);
    printf("native cgame strides, sound pointer arrays and entity handles: PASS (centity=%zu clientInfo=%zu)\n", sizeof(centity_t), sizeof(clientInfo_t));
}
