#include "ws39_player_use.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

level_locals_t level;
byte scr_const_ptr[sizeof(scr_const_t)];
static vec3_t zero;
byte *vec3_origin_ptr = (byte *)zero;
static gentity_t entities[2];
static gclient_t client;
static int tagQueries;

void G_GetPlayerViewOrigin(gentity_t *ent, vec_t *origin) { memset(origin, 0, sizeof(vec3_t)); }
void G_GetPlayerViewDirection(gentity_t *ent, vec_t *forward, vec_t *right, vec_t *up)
{
    forward[0] = 1; forward[1] = forward[2] = 0;
}
int CM_AreaEntities(const vec_t *mins, const vec_t *maxs, int *list, int count, int mask)
{
    assert(mask == 0x200000); list[0] = 1; return 1;
}
const vec_t Vec3Normalize(vec_t *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length) for (int i = 0; i < 3; ++i) v[i] /= length;
    return length;
}
qboolean SV_EntityContact(const vec_t *mins, const vec_t *maxs, const gentity_t *ent) { return 1; }
qboolean BG_CanItemBeGrabbed(const entityState_t *ent, const playerState_t *ps, qboolean touched) { return 1; }
int G_DObjGetWorldTagPos(gentity_t *ent, unsigned int tag, vec_t *pos) { ++tagQueries; return 1; }
int G_TraceCapsuleComplete(const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int pass, int mask) { return 1; }

int main(void)
{
    useList_t list[2];
    level.gentities = entities;
    entities[0].client = &client;
    entities[1].classname = 1;
    entities[1].s.eType = 9;
    entities[1].r.contents = 0x200004;
    entities[1].r.absmin[0] = entities[1].r.absmax[0] = 64;
    assert(Player_GetUseList(&entities[0], list) == 1);
    assert(list[0].ent == &entities[1] && tagQueries == 1);
    entities[1].r.contents = 4;
    entities[1].r.svFlags = 0x20;
    assert(Player_GetUseList(&entities[0], list) == 0);
    entities[1].s.eType = 3;
    assert(Player_GetUseList(&entities[0], list) == 1);
    puts("production use list: turret content bit, unrelated svFlags and item eligibility passed");
}
