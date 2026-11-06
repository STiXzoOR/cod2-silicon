/* Observable drop results through complete item and math translation units. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern gentity_t *Drop_Weapon(gentity_t *, int, unsigned int);
extern void DroppedItemClearOwner(gentity_t *);
gentity_t g_entities[1024];
gitem_t bg_itemlist[256];
void *imp_bg_itemlist = bg_itemlist;
qboolean itemRegistered[256];
level_locals_t level;
static gclient_t client;
static WeaponDef weaponDef;
static dvar_t forwardSpeed, upBase, upRandom, maxDropped;
const dvar_t *g_dropForwardSpeed = &forwardSpeed;
const dvar_t *g_dropUpSpeedBase = &upBase;
const dvar_t *g_dropUpSpeedRand = &upRandom;
const dvar_t *g_maxDroppedWeapons = &maxDropped;
static int randomCalls, weaponNumber, tookWeapon, linked, angleCalls, tagFound;
static vec3_t observedAngles;

int rand(void)
{
    /* Drop launch consumes the first draw; angular draws are then Z, Y, X. */
    static const int draws[] = {0, 0, 1073741824, 1610612736};
    assert(randomCalls < 4);
    return draws[randomCalls++];
}
int BG_GetNumWeapons(void) { return 64; }
WeaponDef *BG_GetWeaponDef(int number) { assert(number == weaponNumber); return &weaponDef; }
int BG_AmmoForWeapon(int number) { assert(number == weaponNumber); return 0; }
int BG_ClipForWeapon(int number) { assert(number == weaponNumber); return 0; }
int BG_GetAmmoClipSize(int index) { assert(index == 0); return 20; }
qboolean BG_TakePlayerWeapon(playerState_t *ps, int number)
{
    assert(ps == &client.ps && number == weaponNumber);
    ps->weapons[number >> 5] &= ~(1u << (number & 31));
    ++tookWeapon; return 1;
}
gentity_t *G_Spawn(void) { return &g_entities[64]; }
void G_FreeEntity(gentity_t *ent) { abort(); }
int G_ModelIndex(const char *name) { abort(); }
void G_SetModel(gentity_t *ent, const char *name)
{ assert(ent == &g_entities[64] && !strcmp(name, "model")); }
void G_DObjUpdate(gentity_t *ent) { assert(ent == &g_entities[64]); }
void G_SetConstString(scr_string_t *out, const char *name)
{ assert(!strcmp(name, "weapon_test")); *out = 7; }
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args; va_start(args, format);
    int result = vsnprintf(dest, size, format, args); va_end(args); return result;
}
char *va(const char *format, ...) { abort(); }
void Scr_Error(const char *message) { abort(); }
void G_SetOrigin(gentity_t *ent, const vec_t *origin)
{
    assert(ent == &g_entities[64]);
    memcpy(ent->r.currentOrigin, origin, sizeof(vec3_t));
    memcpy(ent->s.pos.trBase, origin, sizeof(vec3_t));
}
void G_SetAngle(gentity_t *ent, const vec_t *angles)
{
    assert(ent == &g_entities[64]);
    memcpy(observedAngles, angles, sizeof(vec3_t));
    memcpy(ent->r.currentAngles, angles, sizeof(vec3_t));
    ++angleCalls;
}
void SV_LinkEntity(gentity_t *ent) { assert(ent == &g_entities[64]); ++linked; }
qboolean G_DObjGetWorldTagMatrix(gentity_t *ent, unsigned int tag, vec3_t *matrix)
{
    assert(ent == &g_entities[0] && tag == 9);
    memset(matrix, 0, sizeof(vec3_t) * 4);
    matrix[0][0] = matrix[1][1] = matrix[2][2] = 1;
    matrix[3][0] = 100; matrix[3][1] = 200; matrix[3][2] = 300;
    return tagFound;
}
void G_TraceCapsule(trace_t *trace, const vec_t *start, const vec_t *mins,
                    const vec_t *maxs, const vec_t *end, int pass, int mask)
{
    assert(pass == 0 && mask == 0x491);
    assert(start[0] == 10 && start[1] == 20 && start[2] == 60);
    assert(mins[0] == -1 && maxs[2] == 1 && end[0] == 100);
    memset(trace, 0, sizeof(*trace)); trace->fraction = 0.5f;
}
static void Reset(int weapon)
{
    memset(g_entities, 0, sizeof(g_entities));
    memset(&level, 0, sizeof(level)); memset(&client, 0, sizeof(client));
    memset(&weaponDef, 0, sizeof(weaponDef));
    memset(bg_itemlist, 0, sizeof(bg_itemlist));
    memset(itemRegistered, 0, sizeof(itemRegistered));
    weaponNumber = weapon; itemRegistered[weapon] = 1;
    bg_itemlist[weapon].giType = IT_WEAPON;
    bg_itemlist[weapon].world_model[0] = "model";
    weaponDef.szInternalName = "test";
    client.ps.weapons[weapon >> 5] = 1u << (weapon & 31);
    client.ps.ammo[0] = 11; client.ps.ammoclip[0] = 7;
    g_entities[0].client = &client;
    memcpy(g_entities[0].r.currentOrigin, (vec3_t){10,20,30}, sizeof(vec3_t));
    memcpy(g_entities[0].r.currentAngles, (vec3_t){10,20,30}, sizeof(vec3_t));
    memcpy(g_entities[0].r.mins, (vec3_t){-15,-15,0}, sizeof(vec3_t));
    memcpy(g_entities[0].r.maxs, (vec3_t){15,15,60}, sizeof(vec3_t));
    maxDropped.current.integer = 16; forwardSpeed.current.value = 10;
    upBase.current.value = 20; upRandom.current.value = 5; level.time = 1000;
    randomCalls = tookWeapon = linked = angleCalls = 0; tagFound = 1;
}
static gentity_t *Drop(unsigned int tag)
{
    gentity_t *result = Drop_Weapon(&g_entities[0], weaponNumber, tag);
    assert(result == &g_entities[64] && linked == 1 && tookWeapon == 1);
    assert(client.ps.ammo[0] == 0 && client.ps.ammoclip[0] == 0);
    assert(result->count == 11 && result->item.count2 == 7);
    assert(level.droppedWeaponCue[0] == result && result->s.clientNum == 0);
    return result;
}
static void Angles(void)
{
    for (int found = 0; found < 2; ++found) {
        Reset(1); tagFound = found; gentity_t *drop = Drop(9);
        assert(angleCalls == 1);
        assert(observedAngles[0] == 10 && observedAngles[1] == 20 && observedAngles[2] == 30);
        if (found) {
            assert(drop->r.currentOrigin[0] == 55 && drop->r.currentOrigin[1] == 110);
            assert(drop->r.currentOrigin[2] == 180 && drop->s.pos.trTime == 1000);
        }
    }
}
static void AngularRandom(void)
{
    Reset(1); gentity_t *drop = Drop(9);
    assert(randomCalls == 4);
    assert(drop->s.apos.trType == TR_LINEAR && drop->s.apos.trTime == 1000);
    assert(drop->s.apos.trDelta[0] == 25);
    assert(drop->s.apos.trDelta[1] == 0 && drop->s.apos.trDelta[2] == -60);
}
static void Weapon31(void) { Reset(31); Drop(0); assert(randomCalls == 1 && angleCalls == 0); }
static void Controls(void)
{
    Reset(1); gentity_t *drop = Drop(0);
    assert(randomCalls == 1 && angleCalls == 0 && drop->s.pos.trType == TR_GRAVITY);
    assert(drop->nextthink == 2000 && drop->flags == 0x10 && drop->handler == 15);
    DroppedItemClearOwner(drop); assert(drop->s.clientNum == 1022);
    Reset(1); client.ps.weapons[0] = 0;
    assert(Drop_Weapon(&g_entities[0], 1, 0) == NULL && tookWeapon == 1 && linked == 0);
    Reset(1); weaponDef.bClipOnly = 1; client.ps.ammoclip[0] = 0;
    assert(Drop_Weapon(&g_entities[0], 1, 0) == NULL && tookWeapon == 1 && linked == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "angles")) Angles();
    else if (!strcmp(argv[1], "angular-random")) AngularRandom();
    else if (!strcmp(argv[1], "weapon31")) Weapon31();
    else if (!strcmp(argv[1], "controls")) Controls();
    else abort();
    printf("item drop %s: pass\n", argv[1]); return 0;
}
