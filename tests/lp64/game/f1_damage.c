/* Exercise production nonclient damage and trigger damage dispatch. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void G_Damage(gentity_t *, gentity_t *, gentity_t *, const vec_t *, const vec_t *, int, int, int, hitLocation_t, int);
extern void Activate_trigger_damage(gentity_t *, gentity_t *, int, int);
extern void Use_trigger_damage(gentity_t *, gentity_t *, gentity_t *);

gentity_t g_entities[1024];
scr_const_t scr_const;
float g_fHitLocDamageMult[19];
static entityHandler_t handlers[20];
void *imp_entityHandlers = handlers;
static level_locals_t fixture_level;
void *imp_level = &fixture_level;
static dvar_t debugDamage;
const dvar_t *g_debugDamage = &debugDamage;
static int painCount, deathCount, damageNotifies, deathNotifies, freed, lastDamage;
static const vec_t *expectedPoint;
static gentity_t *expectedAttacker, *expectedInflictor;

void Com_Printf(const char *format, ...) { (void)format; }
WeaponDef *BG_GetWeaponDef(int weapon) { (void)weapon; abort(); }
void Scr_PlayerDamage(gentity_t *a, gentity_t *b, gentity_t *c, int d, int e, int f,
                      int g, const vec_t *h, const vec_t *i, hitLocation_t j, int k)
{ abort(); }
void Scr_AddEntity(gentity_t *ent) { assert(ent == expectedAttacker); }
void Scr_AddInt(int value) { lastDamage = value; }
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int count)
{
    assert(ent == &g_entities[0]);
    if (name == scr_const.damage) { assert(count == 2); ++damageNotifies; }
    else if (name == scr_const.death) { assert(count == 1); ++deathNotifies; }
    else abort();
}
int Scr_IsSystemActive(int system) { assert(system == 1); return 1; }
void G_FreeEntityDelay(gentity_t *ent) { assert(ent == &g_entities[0]); ++freed; }

static void CheckDirection(const vec_t *dir)
{
    assert(fabsf(dir[0] - 0.6f) < 0.00001f);
    assert(fabsf(dir[1] - 0.8f) < 0.00001f);
    assert(dir[2] == 0);
}
static void Pain(gentity_t *self, gentity_t *attacker, int damage, const vec_t *point,
                 int mod, const vec_t *dir, hitLocation_t location)
{
    assert(self == &g_entities[0] && attacker == expectedAttacker);
    assert(damage == 10 && point == expectedPoint && mod == 4 && location == 2);
    CheckDirection(dir);
    ++painCount;
}
static void Death(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                  int damage, int mod, int weapon, const vec_t *dir,
                  hitLocation_t location, int offset)
{
    assert(self == &g_entities[0] && inflictor == expectedInflictor);
    assert(attacker == expectedAttacker && damage == 10 && mod == 4);
    assert(weapon == 7 && location == 2 && offset == 9);
    CheckDirection(dir);
    ++deathCount;
}
static void Damage(int lethal)
{
    vec3_t direction = {3, 4, 0}, point = {10, 20, 30};
    gentity_t *target = &g_entities[0];
    expectedAttacker = &g_entities[1];
    expectedInflictor = &g_entities[2];
    expectedInflictor->s.weapon = 7;
    expectedPoint = point;
    target->takedamage = 1;
    target->health = lethal ? 5 : 50;
    handlers[0].pain = Pain;
    handlers[0].die = Death;
    scr_const.damage = 1;
    scr_const.death = 2;
    G_Damage(target, expectedInflictor, expectedAttacker, direction, point,
             10, 0, 4, (hitLocation_t)2, 9);
    assert(direction[0] == 3 && direction[1] == 4 && direction[2] == 0);
    assert(target->health == (lethal ? -5 : 40));
    assert(damageNotifies == 1 && deathNotifies == lethal && lastDamage == 10);
    assert(painCount == !lethal && deathCount == lethal);
}
static void ResetTrigger(int flags)
{
    memset(g_entities, 0, sizeof(g_entities));
    memset(&fixture_level, 0, sizeof(fixture_level));
    g_entities[0].spawnflags = flags;
    g_entities[0].health = 31900;
    g_entities[0].s.number = 10;
    g_entities[1].s.number = 11;
    g_entities[0].useCount = 12;
    g_entities[1].useCount = 13;
    freed = 0;
}
static void Use(int flags)
{
    ResetTrigger(flags | 0x200);
    Use_trigger_damage(&g_entities[0], &g_entities[1], &g_entities[2]);
    assert(g_entities[0].health == 32000 && freed == 1);
    assert(fixture_level.pendingTriggerListSize == 0);
}
static void Filters(void)
{
    /* Independent MOD membership lists, rather than reproducing bitmask math. */
    const int flags[] = {1, 2, 4, 8, 16, 32, 256};
    const int blocked[7][6] = {{1}, {2}, {3, 4, 5, 6}, {3, 4, 5, 6, 14},
                               {4, 6}, {7}, {0, 9, 10, 11, 12, 13}};
    const int counts[] = {1, 1, 4, 5, 2, 1, 6};
    for (unsigned int f = 0; f < sizeof(flags) / sizeof(flags[0]); ++f) {
        for (int mod = 0; mod <= 14; ++mod) {
            ResetTrigger(flags[f]);
            int reject = 0;
            for (int i = 0; i < counts[f]; ++i)
                if (mod == blocked[f][i])
                    reject = 1;
            Activate_trigger_damage(&g_entities[0], &g_entities[1], 10, mod);
            assert(g_entities[0].health == (reject ? 31900 : 32000));
            assert(fixture_level.pendingTriggerListSize == !reject);
            if (!reject) {
                const trigger_info_t *entry = &fixture_level.pendingTriggerList[0];
                assert(entry->entnum == 10 && entry->otherEntnum == 11);
                assert(entry->useCount == 12 && entry->otherUseCount == 13);
            }
        }
    }
}
static void Thresholds(void)
{
    for (int damage = 9; damage <= 10; ++damage) {
        ResetTrigger(0);
        g_entities[0].trigger.threshold = 10;
        Activate_trigger_damage(&g_entities[0], &g_entities[1], damage, 3);
        assert(fixture_level.pendingTriggerListSize == (damage == 10));
        assert(g_entities[0].health == (damage == 10 ? 32000 : 31900));
    }
    for (int accumulated = 99; accumulated <= 100; ++accumulated) {
        ResetTrigger(0);
        g_entities[0].trigger.accumulate = 100;
        g_entities[0].health = 32000 - accumulated;
        Activate_trigger_damage(&g_entities[0], &g_entities[1], 10, 3);
        assert(fixture_level.pendingTriggerListSize == (accumulated == 100));
        assert(g_entities[0].health == (accumulated == 100 ? 32000 : 31901));
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "pain")) Damage(0);
    else if (!strcmp(argv[1], "death")) Damage(1);
    else if (!strcmp(argv[1], "use8")) Use(8);
    else if (!strcmp(argv[1], "use256")) Use(256);
    else if (!strcmp(argv[1], "filters")) Filters();
    else if (!strcmp(argv[1], "thresholds")) Thresholds();
    else abort();
    printf("F1 damage %s: pass\n", argv[1]);
    return 0;
}
