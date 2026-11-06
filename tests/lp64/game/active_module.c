/* Synthetic Mac 1.3 contracts through complete production gameplay/math TUs. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void G_TouchTriggers(gentity_t *);
extern qboolean StuckInClient(gentity_t *);
extern void P_DamageFeedback(gentity_t *);
extern void vectoangles(const vec_t *, vec_t *);
level_locals_t level;
gentity_t g_entities[3];
static gclient_t clients[3];
scr_const_t scr_const;
static entityHandler_t handlers[2];
void *imp_entityHandlers = handlers;
void *imp_level = &level;
static dvar_t eject;
const dvar_t *g_playerCollisionEjectSpeed = &eject;
static int areaCount, areaCalls, contact, itemTouch, script, itemCalls, contactCalls;
static int actions[8], actionCount, randomCalls;
int cod2_native_trace_enabled;
void Com_Printf(const char *fmt, ...) { abort(); }

/* External randomness is fixed; normalization, angles and random scaling are real. */
int rand(void) { ++randomCalls; return 0x40000000; }
static void Record(int action) { assert(actionCount < 8); actions[actionCount++] = action; }
static void Touch(gentity_t *self, gentity_t *other, int touched)
{
    assert(self == &g_entities[1] && other == &g_entities[0] && touched == 1);
    Record(5);
}
int CM_AreaEntities(const vec_t *mins, const vec_t *maxs, int *list, int capacity, int mask)
{
    assert(capacity == 1024 && mask == 0x405c0008);
    assert(mins[0] == -30 && mins[1] == -20 && mins[2] == -22);
    assert(maxs[0] == 50 && maxs[1] == 60 && maxs[2] == 82);
    ++areaCalls;
    list[0] = 1;
    return areaCount;
}
qboolean BG_PlayerTouchesItem(playerState_t *ps, entityState_t *es, int time)
{
    assert(ps == &clients[0].ps && es == &g_entities[1].s && time == 2000);
    ++itemCalls;
    return itemTouch;
}
qboolean SV_EntityContact(const vec_t *mins, const vec_t *maxs, const gentity_t *ent)
{
    assert(ent == &g_entities[1]);
    assert(mins[0] == -6 && mins[1] == 4 && mins[2] == 30);
    assert(maxs[0] == 26 && maxs[1] == 36 && maxs[2] == 86);
    ++contactCalls;
    return contact;
}
int Scr_IsSystemActive(int system) { assert(system == 1); return script; }
void Scr_AddEntity(gentity_t *ent) { Record(ent == &g_entities[0] ? 1 : 3); }
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int count)
{
    assert(name == 42 && count == 1);
    Record(ent == &g_entities[1] ? 2 : 4);
}

static void Reset(void)
{
    memset(&level, 0, sizeof(level));
    memset(g_entities, 0, sizeof(g_entities));
    memset(clients, 0, sizeof(clients));
    memset(handlers, 0, sizeof(handlers));
    level.time = 2000;
    level.maxclients = 2;
    level.num_entities = 2;
    eject.current.integer = 80;
    for (int i = 0; i < 3; ++i) {
        g_entities[i].client = &clients[i];
        g_entities[i].r.inuse = 1;
        g_entities[i].takedamage = 1;
        g_entities[i].health = 100;
        g_entities[i].r.contents = 0x2000000;
        for (int j = 0; j < 3; ++j) {
            g_entities[i].r.absmin[j] = -16;
            g_entities[i].r.absmax[j] = 16;
            g_entities[i].r.mins[j] = -16;
            g_entities[i].r.maxs[j] = 16;
        }
        clients[i].ps.pm_flags = 0x800020;
        clients[i].ps.eFlags = 0x40;
        clients[i].ps.speed = 190 + i;
        clients[i].ps.velocity[2] = 17 + i;
        clients[i].sess.maxHealth = 100;
    }
    g_entities[1].r.currentOrigin[0] = 10;
    areaCount = itemTouch = contact = script = 1;
    areaCalls = contactCalls = itemCalls = actionCount = randomCalls = 0;
    scr_const.touch = 42;
}
static void TouchSetup(void)
{
    Reset();
    clients[0].ps.origin[0] = 10;
    clients[0].ps.origin[1] = 20;
    clients[0].ps.origin[2] = 30;
    g_entities[0].r.mins[2] = 0;
    g_entities[0].r.maxs[2] = 56;
    g_entities[1].handler = 1;
    handlers[1].touch = (fn_touch)Touch;
}
static void TouchItem(void)
{
    TouchSetup();
    g_entities[1].s.eType = 3;
    contact = 0; /* Item pickup volume is larger than its contact volume. */
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 5);
    for (int i = 0; i < 5; ++i) assert(actions[i] == i + 1);
    assert(itemCalls == 1 && contactCalls == 0);
}
static void TouchControls(void)
{
    TouchSetup();
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 5 && contactCalls == 1 && itemCalls == 0);
    TouchSetup(); contact = 0;
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 0 && contactCalls == 1);
    TouchSetup(); g_entities[1].s.eType = 3; itemTouch = 0;
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 0 && contactCalls == 0 && itemCalls == 1);
    TouchSetup(); script = 0;
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 1 && actions[0] == 5);
    TouchSetup(); handlers[1].touch = NULL;
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 0 && contactCalls == 0 && itemCalls == 0);
    TouchSetup(); handlers[0].touch = (fn_touch)Touch; handlers[1].touch = NULL;
    G_TouchTriggers(&g_entities[0]);
    assert(actionCount == 4); /* Entity touch enables notices but is not invoked. */
    TouchSetup(); clients[0].ps.pm_type = 2;
    G_TouchTriggers(&g_entities[0]);
    assert(areaCalls == 0 && actionCount == 0);
    TouchSetup(); areaCount = 0;
    G_TouchTriggers(&g_entities[0]);
    assert(areaCalls == 1 && actionCount == 0);
}
static void Push(float selfSpeed, float otherSpeed)
{
    assert(StuckInClient(&g_entities[0]) == 1);
    assert(randomCalls == 2);
    assert(clients[0].ps.velocity[0] == -selfSpeed);
    assert(clients[1].ps.velocity[0] == otherSpeed);
    assert(clients[0].ps.velocity[1] == 0 && clients[1].ps.velocity[1] == 0);
    assert(clients[0].ps.velocity[2] == 17 && clients[1].ps.velocity[2] == 18);
    assert(clients[0].ps.pm_time == 300 && clients[1].ps.pm_time == 300);
}
static void StuckInuse(void)
{
    Reset(); g_entities[1].takedamage = 0;
    Push(190, 191);
    Reset(); g_entities[1].r.inuse = 0;
    assert(StuckInClient(&g_entities[0]) == 0 && randomCalls == 0);
}
static void StuckMaxclients(void)
{
    Reset(); level.maxclients = 1;
    assert(StuckInClient(&g_entities[0]) == 0 && randomCalls == 0);
    Reset(); level.num_entities = 3;
    g_entities[1].r.inuse = 0; g_entities[1].takedamage = 0;
    assert(StuckInClient(&g_entities[0]) == 0 && randomCalls == 0);
}
static void StuckFlags(void)
{
    Reset(); Push(190, 191);
    for (int i = 0; i < 2; ++i) {
        assert(clients[i].ps.pm_flags == 0x800220);
        assert(clients[i].ps.eFlags == 0x40);
    }
}
static void StuckSpeed(void)
{
    Reset(); clients[0].ps.velocity[1] = 1;
    Push(80, 0);
    Reset(); clients[1].ps.velocity[1] = 1;
    Push(0, 80);
    Reset(); clients[0].ps.velocity[0] = clients[1].ps.velocity[0] = 1;
    Push(80, 80);
    Reset(); eject.current.integer = 0;
    clients[0].ps.velocity[0] = clients[1].ps.velocity[0] = 1;
    Push(190, 191);
    Reset(); g_entities[0].r.contents = g_entities[1].r.contents = 0x4000000;
    Push(190, 191);
}
static void StuckControls(void)
{
    for (int mode = 0; mode < 12; ++mode) {
        Reset();
        switch (mode) {
        case 0: clients[0].ps.pm_flags = 0; break;
        case 1: clients[0].sess.sessionState = 1; break;
        case 2: g_entities[0].r.contents = 1; break;
        case 3: clients[1].ps.pm_flags = 0; break;
        case 4: clients[1].sess.sessionState = 1; break;
        case 5: g_entities[1].health = 0; break;
        case 6: g_entities[1].r.contents = 1; break;
        case 7: g_entities[1].r.currentOrigin[0] = 33; break;
        case 8: level.maxclients = level.num_entities = 0; break;
        case 9: g_entities[1].r.absmin[0] = 17; break;
        case 10: g_entities[1].r.absmin[1] = 17; break;
        case 11: g_entities[1].r.absmin[2] = 17; break;
        }
        assert(StuckInClient(&g_entities[0]) == 0);
        assert(randomCalls == 0 && clients[0].ps.pm_time == 0);
    }
    Reset(); g_entities[1].r.currentOrigin[0] = 32;
    Push(190, 191); /* Radius equality still overlaps. */
}
static float FloatBits(unsigned int bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static void DamageAngle(int pitch)
{
    Reset();
    clients[0].damage_blood = 10;
    clients[0].damage_from[0] = 1.0f;
    clients[0].damage_from[pitch ? 2 : 1] = FloatBits(0x3dfc98c1) * (pitch ? -1 : 1);
    vec3_t angles;
    vectoangles(clients[0].damage_from, angles);
    assert(angles[pitch ? 0 : 1] == FloatBits(0x40e0ffff));
    P_DamageFeedback(&g_entities[0]);
    assert((pitch ? clients[0].ps.damagePitch : clients[0].ps.damageYaw) == 4);
    assert(clients[0].ps.damageCount == 10 && clients[0].ps.damageEvent == 1);
    assert(clients[0].damageTime == 1980 && clients[0].damage_blood == 0);
    assert(clients[0].ps.aimSpreadScale == 10);
}
static void DamageControls(void)
{
    Reset(); clients[0].ps.pm_type = 6; clients[0].ps.damageCount = 77;
    clients[0].damage_blood = 20;
    P_DamageFeedback(&g_entities[0]);
    assert(clients[0].ps.damageCount == 77 && clients[0].damage_blood == 20);
    Reset(); clients[0].ps.damageCount = 77;
    P_DamageFeedback(&g_entities[0]);
    assert(clients[0].ps.damageCount == 0 && clients[0].ps.damageEvent == 0);
    Reset(); clients[0].sess.maxHealth = 0; clients[0].damage_blood = 20;
    P_DamageFeedback(&g_entities[0]);
    assert(clients[0].ps.damageEvent == 0 && clients[0].damage_blood == 20);
    Reset(); clients[0].damage_fromWorld = 1; clients[0].damage_blood = 1;
    P_DamageFeedback(&g_entities[0]);
    assert(clients[0].v_dmg_pitch == -5 && clients[0].v_dmg_roll == 0);
    assert(clients[0].ps.damagePitch == 255 && clients[0].ps.damageYaw == 255);
    assert(clients[0].damage_fromWorld == 0 && clients[0].ps.damageCount == 1);
    Reset(); clients[0].damage_fromWorld = 1; clients[0].damage_blood = 1000;
    clients[0].ps.aimSpreadScale = 250;
    P_DamageFeedback(&g_entities[0]);
    assert(clients[0].ps.damageCount == 127 && clients[0].ps.aimSpreadScale == 255);
    assert(clients[0].v_dmg_pitch == -(127.0f * 0.2f));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "touch-item")) TouchItem();
    else if (!strcmp(argv[1], "touch-controls")) TouchControls();
    else if (!strcmp(argv[1], "stuck-inuse")) StuckInuse();
    else if (!strcmp(argv[1], "stuck-maxclients")) StuckMaxclients();
    else if (!strcmp(argv[1], "stuck-flags")) StuckFlags();
    else if (!strcmp(argv[1], "stuck-speed")) StuckSpeed();
    else if (!strcmp(argv[1], "stuck-controls")) StuckControls();
    else if (!strcmp(argv[1], "damage-yaw")) DamageAngle(0);
    else if (!strcmp(argv[1], "damage-pitch")) DamageAngle(1);
    else if (!strcmp(argv[1], "damage-controls")) DamageControls();
    else abort();
    puts("active-module contract passed");
    return 0;
}
