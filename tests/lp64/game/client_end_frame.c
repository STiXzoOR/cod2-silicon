/* Mac 1.3 movement states through the complete production active-client TU. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ClientEndFrame(gentity_t *);
level_locals_t level;
bgs_t level_bgs;
gentity_t g_entities[1];
static gclient_t client;
static dvar_t gravity, speed, smooth, debug, eject;
const dvar_t *g_gravity = &gravity;
const dvar_t *g_speed = &speed;
const dvar_t *g_smoothClients = &smooth;
const dvar_t *g_debugLocDamage = &debug;
const dvar_t *g_playerCollisionEjectSpeed = &eject;
void *imp_level = &level;

/* Controlled services leave pm_type and server damage flags to production. */
const char *G_ModelName(int index) { assert(index == 0); return ""; }
void Player_UpdateLookAtEntity(gentity_t *ent) { assert(ent == g_entities); }
void Player_UpdateCursorHints(gentity_t *ent) { assert(ent == g_entities); }
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *s, qboolean snap, int handler)
{
    assert(ps == &client.ps && s == &g_entities[0].s);
    assert(snap == 1 && handler == 1);
}
void G_GetPlayerViewOrigin(const gentity_t *ent, vec_t *origin)
{
    assert(ent == g_entities);
    memset(origin, 0, sizeof(vec3_t));
}
int G_GetNonPVSFriendlyInfo(gentity_t *ent, vec_t *origin, int previous)
{
    assert(ent == g_entities);
    return 0;
}
struct DObj_s *Com_GetServerDObj(int handle) { assert(handle == 0); return NULL; }
void BG_UpdatePlayerDObj(struct DObj_s *obj, entityState_t *es, clientInfo_t *ci, int collision)
{
    assert(!obj && es == &g_entities[0].s && ci == &level_bgs.clientinfo[0]);
}
void BG_PlayerAnimation(const struct DObj_s *obj, entityState_t *es, clientInfo_t *ci)
{
    assert(!obj && es == &g_entities[0].s && ci == &level_bgs.clientinfo[0]);
}

/* Linked but outside the selected end-frame paths. */
int G_TagIndex(const char *name) { abort(); }
const char *SL_ConvertToString(unsigned int name) { abort(); }
void I_strncpyz(char *dest, const char *src, int size) { abort(); }
void ClientSpawn(gentity_t *ent, const vec_t *origin, const vec_t *angles) { abort(); }
void G_SafeDObjFree(gentity_t *ent) { abort(); }
void turret_think_client(gentity_t *ent) { abort(); }
qboolean SV_DObjExists(gentity_t *ent) { abort(); }
int G_DObjCalcPose(gentity_t *ent) { abort(); }
void SV_XModelDebugBoxes(gentity_t *ent) { abort(); }
void SV_SetConfigstring(int index, const char *value) { abort(); }
const char *va(const char *format, ...) { abort(); }
qboolean SV_GetArchivedClientInfo(int number, int *time, playerState_t *ps, clientState_t *cs) { abort(); }
qboolean Cmd_FollowCycle_f(gentity_t *ent, int direction) { abort(); }
void StopFollowing(gentity_t *ent) { abort(); }
void HudElem_UpdateClient(gclient_t *cl, int number, int which) { abort(); }
void vectoangles(const vec_t *vec, vec_t *angles) { abort(); }
void AnglesToAxis(const vec_t *angles, vec3_t *axis) { abort(); }
const float AngleNormalize180(float angle) { abort(); }
void BG_WeaponFireRecoil(const playerState_t *ps, vec_t *speed, vec_t *kick) { abort(); }
int PM_GetEffectiveStance(playerState_t *ps) { abort(); }
int PM_GetViewHeightLerpTime(const playerState_t *ps, int target, qboolean down) { abort(); }
void *imp_singleClientEvents;
float crandom(void) { abort(); }
float Vec2Normalize(vec_t *vec) { abort(); }

static void Check(unsigned int tag, int count, int dead, int noclip, int ufo, int expected)
{
    gentity_t *ent = &g_entities[0];
    memset(ent, 0, sizeof(*ent));
    memset(&client, 0, sizeof(client));
    memset(&level, 0, sizeof(level));
    memset(&level_bgs, 0, sizeof(level_bgs));
    ent->client = &client;
    ent->s.eType = 1;
    ent->health = dead ? 0 : 100;
    ent->tagInfo = tag;
    ent->count = count;
    ent->r.svFlags = 0xa7;
    ent->takedamage = 9;
    client.sess.connected = CON_CONNECTED;
    client.sess.sessionState = dead ? SESS_STATE_DEAD : SESS_STATE_PLAYING;
    client.noclip = noclip;
    client.ufo = ufo;
    client.ps.pm_type = 123;
    client.buttonsSinceLastFrame = 42;
    gravity.current.value = 800.0f;
    speed.current.integer = 190;
    ClientEndFrame(ent);
    assert(client.ps.pm_type == expected);
    assert(ent->r.svFlags == (dead && !noclip && !ufo ? 0xa5 : 0xa6));
    assert(ent->takedamage == (dead && !noclip && !ufo ? 0 : 1));
    assert(client.buttonsSinceLastFrame == 0);
    assert(ent->tagInfo == tag && ent->count == count);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "dead-null-count0")) Check(0, 0, 1, 0, 0, 6);
    else if (!strcmp(argv[1], "dead-null-count1")) Check(0, 1, 1, 0, 0, 6);
    else if (!strcmp(argv[1], "dead-linked-count0")) Check(1, 0, 1, 0, 0, 7);
    else if (!strcmp(argv[1], "dead-linked-count1")) Check(1, 1, 1, 0, 0, 7);
    else if (!strcmp(argv[1], "dead-high-bit")) Check(0x80000000u, 1, 1, 0, 0, 7);
    else if (!strcmp(argv[1], "noclip")) Check(1, 1, 1, 1, 1, 2);
    else if (!strcmp(argv[1], "ufo")) Check(1, 1, 1, 0, 1, 3);
    else if (!strcmp(argv[1], "alive-null")) Check(0, 0, 0, 0, 0, 0);
    else if (!strcmp(argv[1], "alive-linked")) Check(1, 1, 0, 0, 0, 1);
    else abort();
    puts("client end-frame contract passed");
    return 0;
}
