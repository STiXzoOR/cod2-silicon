#include "ws39_player_use.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

level_locals_t level;
byte scr_const_ptr[sizeof(scr_const_t)];
static vec3_t zero;
byte *vec3_origin_ptr = (byte *)zero;
static gentity_t entities[3];
static gclient_t client;
static int tagQueries, touchCount = 1, fullItem;

void G_GetPlayerViewOrigin(gentity_t *ent, vec_t *origin) { memset(origin, 0, sizeof(vec3_t)); }
void G_GetPlayerViewDirection(gentity_t *ent, vec_t *forward, vec_t *right, vec_t *up)
{
    forward[0] = 1; forward[1] = forward[2] = 0;
}
int CM_AreaEntities(const vec_t *mins, const vec_t *maxs, int *list, int count, int mask)
{
    assert(mask == 0x200000);
    for (int i = 0; i < touchCount; ++i) list[i] = i + 1;
    return touchCount;
}
const vec_t Vec3Normalize(vec_t *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length) for (int i = 0; i < 3; ++i) v[i] /= length;
    return length;
}
qboolean SV_EntityContact(const vec_t *mins, const vec_t *maxs, const gentity_t *ent) { return 1; }
qboolean BG_CanItemBeGrabbed(const entityState_t *ent, const playerState_t *ps, qboolean touched)
{
    return !(fullItem && ent == &entities[fullItem].s);
}
int G_DObjGetWorldTagPos(gentity_t *ent, unsigned int tag, vec_t *pos) { ++tagQueries; return 1; }
int G_TraceCapsuleComplete(const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int pass, int mask) { return 1; }
/* The hint cases below only reach the trigger_use branch. */
qboolean G_IsTurretUsable(gentity_t *turret, gentity_t *player) { assert(0); return 0; }
WeaponDef *BG_GetWeaponDef(int iWeapon) { assert(0); return NULL; }
byte *bg_itemlist_ptr;

int main(void)
{
    useList_t list[3];
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

    /* Linux 1.3 player_use_mp.cpp: a trigger_use hint shows for its own team
       (or no team), from s.hintType (dmgFlags) and s.hintString (scale), the
       fields setCursorHint and setHintString write. */
    ((scr_const_t *)scr_const_ptr)->trigger_use = 77;
    entities[0].health = 100;
    client.sess.cs.team = 2;
    entities[1].s.eType = 0;
    entities[1].s.number = 1;
    entities[1].classname = 77;
    entities[1].r.contents = 0x200000;
    entities[1].r.svFlags = 0;
    entities[1].trigger.singleUserEntIndex = 0x3FF;
    entities[1].s.dmgFlags = 5;
    entities[1].s.scale = 3;
    entities[1].s.animMovetype = 0;
    for (int team = 0; team <= 2; ++team) {
        entities[1].team = team;
        Player_UpdateCursorHints(&entities[0]);
        if (team == 1) {
            assert(client.ps.cursorHint == 0 && client.ps.cursorHintEntIndex == 0x3FF);
        } else {
            assert(client.ps.cursorHint == 5 && client.ps.cursorHintString == 3);
            assert(client.ps.cursorHintEntIndex == 1);
        }
    }
    entities[1].team = 0;
    entities[1].s.scale = 0xFF;
    Player_UpdateCursorHints(&entities[0]);
    assert(client.ps.cursorHint == 5 && client.ps.cursorHintString == -1);
    /* Mac 0x1cca67 and Linux sort every collected entry, so a full item sinks below a grabbable
       one before it leaves the count. */
    memset(&entities[1], 0, 2 * sizeof(gentity_t));
    entities[1].s.eType = entities[2].s.eType = 3;
    entities[1].classname = entities[2].classname = 1;
    entities[1].r.absmin[0] = entities[1].r.absmax[0] = 64;
    entities[2].r.absmin[0] = entities[2].r.absmax[0] = 64;
    touchCount = 2;
    fullItem = 1;
    assert(Player_GetUseList(&entities[0], list) == 1);
    assert(list[0].ent == &entities[2]);
    puts("production use list: turret content bit, unrelated svFlags, item eligibility and trigger_use hints passed");
}
