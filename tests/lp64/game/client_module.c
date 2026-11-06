/* Synthetic Mac client contracts through complete production translation units. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int G_GetNonPVSFriendlyInfo(gentity_t *, vec_t *, int);
extern void G_GetPlayerViewOrigin(const gentity_t *, vec_t *);
extern void ClientDisconnect(int);
level_locals_t level;
gentity_t g_entities[64];
static gclient_t clients[64];
scr_const_t scr_const;
static dvar_t bobMax;
const dvar_t *bg_bobMax = &bobMax;
static int inSnapshot, tagFound, scriptStringCount, notifications, frees, ranks;
static char scriptStrings[2][32], errorText[128];
static jmp_buf errorJump;

qboolean SV_inSnapshot(const vec_t *origin, int entity)
{
    assert(entity == 1 && origin[0] == 0 && origin[1] == 0);
    return inSnapshot;
}
float BG_GetBobCycle(const playerState_t *ps) { assert(ps == &clients[0].ps); return 0; }
float BG_GetSpeed(const playerState_t *ps, int time) { assert(ps == &clients[0].ps && time == 1000); return 0; }
float BG_GetVerticalBobFactor(const playerState_t *ps, float cycle, float speed, float maximum) { return 0; }
float BG_GetHorizontalBobFactor(const playerState_t *ps, float cycle, float speed, float maximum) { return 0; }
int G_DObjGetWorldTagPos(gentity_t *ent, unsigned short tag, vec_t *origin)
{
    assert(ent == &g_entities[1] && tag == 42);
    origin[0] = 7; origin[1] = 8; origin[2] = 9;
    return tagFound;
}
void Com_Error(int code, const char *format, ...)
{
    assert(code == 1);
    va_list args;
    va_start(args, format);
    vsnprintf(errorText, sizeof(errorText), format, args);
    va_end(args);
    longjmp(errorJump, 1);
}
int Scr_IsSystemActive(int system) { assert(system == 1); return 1; }
void Scr_AddString(const char *value)
{
    assert(scriptStringCount < 2);
    snprintf(scriptStrings[scriptStringCount++], sizeof(scriptStrings[0]), "%s", value);
}
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int count)
{
    assert(ent == &g_entities[0] && name == 43 && count == 2);
    ++notifications;
}
void StopFollowing(gentity_t *ent) { abort(); }
void HudElem_ClientDisconnect(gentity_t *ent) { assert(ent == &g_entities[0]); }
void Scr_PlayerDisconnect(gentity_t *ent) { assert(ent == &g_entities[0]); }
void G_FreeEntity(gentity_t *ent) { assert(ent == &g_entities[0]); ++frees; }
void CalculateRanks(void) { ++ranks; }

static void Reset(void)
{
    memset(&level, 0, sizeof(level));
    memset(g_entities, 0, sizeof(g_entities));
    memset(clients, 0, sizeof(clients));
    memset(scriptStrings, 0, sizeof(scriptStrings));
    level.clients = clients;
    level.maxclients = 2;
    level.time = 1000;
    for (int i = 0; i < 2; ++i) {
        g_entities[i].s.number = i;
        g_entities[i].client = &clients[i];
        g_entities[i].r.inuse = 1;
        clients[i].sess.cs.team = TEAM_ALLIES;
        clients[i].sess.connected = CON_CONNECTED;
    }
    g_entities[1].r.currentOrigin[0] = 10;
    g_entities[1].r.currentOrigin[1] = 20;
    scr_const.tag_player = 42;
    scr_const.menuresponse = 43;
    inSnapshot = 0;
    tagFound = 1;
    scriptStringCount = notifications = frees = ranks = 0;
}
static uint32_t Compass(int previous)
{
    vec3_t view = {0};
    return (uint32_t)G_GetNonPVSFriendlyInfo(&g_entities[0], view, previous);
}
static void CompassYaw(void)
{
    Reset();
    g_entities[1].r.currentAngles[1] = 180;
    assert(Compass(1023) == UINT32_C(0x80824081));
    g_entities[1].r.currentAngles[1] = -90;
    assert(Compass(1023) == UINT32_C(0xc0824081));
}
static void CompassTie(void)
{
    Reset();
    g_entities[1].r.currentOrigin[0] = -1042.5f;
    g_entities[1].r.currentOrigin[1] = -1042.5f;
    assert(Compass(1023) == 1); /* Both equally clipped offsets encode zero. */
}
static void CompassControls(void)
{
    Reset(); assert(Compass(1023) == UINT32_C(0x00824081));
    assert(Compass(63) == UINT32_C(0x00824081));
    inSnapshot = 1; assert(Compass(1023) == 0);
    Reset(); clients[0].sess.cs.team = TEAM_FREE; assert(Compass(1023) == 0);
    Reset(); clients[0].sess.cs.team = TEAM_SPECTATOR; assert(Compass(1023) == 0);
    Reset(); clients[1].sess.cs.team = TEAM_AXIS; assert(Compass(1023) == 0);
    Reset(); g_entities[1].r.inuse = 0; assert(Compass(1023) == 0);
    Reset(); clients[1].sess.sessionState = SESS_STATE_DEAD; assert(Compass(1023) == 0);
}
static void ViewSetup(void)
{
    Reset();
    clients[0].ps.origin[0] = 1;
    clients[0].ps.origin[1] = 2;
    clients[0].ps.origin[2] = 3;
    clients[0].ps.viewHeightCurrent = 60;
    clients[0].ps.viewlocked_entNum = 1;
}
static void ViewLean(void)
{
    ViewSetup();
    clients[0].ps.leanf = 1;
    vec3_t origin;
    G_GetPlayerViewOrigin(&g_entities[0], origin);
    assert(fabsf(origin[0] - 1) < 0.0001f);
    assert(fabsf(origin[1] - (2 - 20 * 0.96126169f)) < 0.0001f);
    assert(fabsf(origin[2] - (63 - 20 * 0.27563736f)) < 0.0001f);
}
static void ViewControls(void)
{
    ViewSetup();
    vec3_t origin;
    G_GetPlayerViewOrigin(&g_entities[0], origin);
    assert(origin[0] == 1 && origin[1] == 2 && origin[2] == 63);
    clients[0].ps.viewHeightCurrent = 0;
    G_GetPlayerViewOrigin(&g_entities[0], origin);
    assert(origin[2] == 11);
    clients[0].ps.eFlags = 0x300;
    G_GetPlayerViewOrigin(&g_entities[0], origin);
    assert(origin[0] == 7 && origin[1] == 8 && origin[2] == 9);
}
static void ViewTagError(void)
{
    ViewSetup();
    clients[0].ps.eFlags = 0x300;
    tagFound = 0;
    if (!setjmp(errorJump)) {
        vec3_t origin;
        G_GetPlayerViewOrigin(&g_entities[0], origin);
        abort();
    }
    assert(!strcmp(errorText, "\x15G_GetPlayerViewOrigin: Couldn't find [tag_player] on turret\n"));
}
static void Disconnect(void)
{
    Reset();
    ClientDisconnect(0);
    assert(scriptStringCount == 2 && !strcmp(scriptStrings[0], "disconnect"));
    assert(!strcmp(scriptStrings[1], "-1"));
    assert(notifications == 1 && frees == 1 && ranks == 1);
    assert(clients[0].sess.connected == CON_DISCONNECTED);
    clientState_t zero = {0};
    assert(!memcmp(&clients[0].sess.cs, &zero, sizeof(zero)));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "compass-yaw")) CompassYaw();
    else if (!strcmp(argv[1], "compass-tie")) CompassTie();
    else if (!strcmp(argv[1], "compass-controls")) CompassControls();
    else if (!strcmp(argv[1], "view-lean")) ViewLean();
    else if (!strcmp(argv[1], "view-controls")) ViewControls();
    else if (!strcmp(argv[1], "view-tag-error")) ViewTagError();
    else if (!strcmp(argv[1], "disconnect")) Disconnect();
    else abort();
    puts("client-module contract passed");
    return 0;
}
