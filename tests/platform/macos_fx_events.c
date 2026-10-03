/* Dispatch real CG events through native impact, weapon and configured FX data. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

cg_t cgArray[1];
byte cgsArray[sizeof(cgs_t)] __attribute__((aligned(8)));
centity_t *cg_entities;
char **cg_uiglob;
static dvar_t disabled;
const dvar_t *cg_debugEvents = &disabled, *cg_footsteps = &disabled;
const dvar_t *cg_nopredict = &disabled, *cg_synchronousClients = &disabled;
const dvar_t *bg_fallDamageMinHeight = &disabled, *bg_fallDamageMaxHeight = &disabled;
static weaponInfo_t weapons[4];
static char *weaponBase = (char *)weapons;
void *imp_cg_weapons = &weaponBase;
void *imp_bg_itemlist, *imp_bg_numItems, *imp_cg_items, *imp_eventnames;
static EffectTemplate effects[11 * 23], weaponEffect, configuredEffects[64];
static EffectTemplate *impactSlots[11 * 23];
static snd_alias_list_t aliases[3][23], weaponSound;
static FxImpactTable impactTable = {.name = "fixture", .table = (FxImpactEntry *)impactSlots};
static EffectTemplate *seenEffects[4];
static snd_alias_list_t *seenSounds[4];
static int effectsSeen, soundsSeen, times[2], timesSeen, boltSeen, noMarksExpected;
static const vec3_t expectedOrigin = {17,29,41};
static const char configString[] = "12tag_flash";

extern void CG_EntityEvent(centity_t *, int);
void FX_PlayEffect(EffectTemplate *effect, const vec_t *origin, const vec_t *forward)
{
    assert(effectsSeen < 4 && cgArray[0].nomarks == noMarksExpected);
    for (int j = 0; j < 3; ++j) assert(origin[j] == expectedOrigin[j]);
    float length = forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2];
    assert(fabsf(length - 1) < .00001f);
    seenEffects[effectsSeen++] = effect;
}
void FX_PlayEntityEffect(EffectTemplate *effect, const vec_t *origin, vec3_t *axis, const FxBoltInfo *bolt)
{
    assert(effect == &configuredEffects[12] && !axis);
    assert(bolt->dobjHandle == 43 && bolt->boneIndex == 7);
    for (int j = 0; j < 3; ++j) assert(origin[j] == expectedOrigin[j]);
    ++boltSeen;
}
void FX_WarpTime(int time) { assert(timesSeen < 2); times[timesSeen++] = time; }
int CG_PlaySoundAlias(int entity, const vec_t *origin, snd_alias_list_t *sound)
{
    assert(entity == 1022 && soundsSeen < 4);
    for (int j = 0; j < 3; ++j) assert(origin[j] == expectedOrigin[j]);
    seenSounds[soundsSeen++] = sound;
    return 0;
}
int FX_GetBoneIndex(int handle, unsigned int tag) { assert(handle == 43 && tag == 17); return 7; }
const char *CL_GetConfigString(int index) { assert(index == 910 + 3); return configString; }
unsigned int SL_GetString(const char *name, unsigned int user)
{ assert(strcmp(name, "tag_flash") == 0 && user == 0); return 17; }
void Scr_SetString(scr_string_t *string, unsigned int value) { assert(*string == 17 && value == 0); *string = 0; }
WeaponDef *BG_GetWeaponDef(int weapon) { assert(weapon == 2); static WeaponDef definition; return &definition; }

/* Any unrelated event boundary reached by these cases is a test failure. */
int BG_WeaponIsClipOnly(int weapon) { (void)weapon; abort(); }
int CG_PlayEntitySoundAlias(int entity, snd_alias_list_t *sound) { (void)entity; (void)sound; abort(); }
int CG_PlaySoundAliasByName(int entity, const vec_t *origin, const char *name)
{ (void)entity; (void)origin; (void)name; abort(); }
int CG_PlaySoundAliasAsMasterByName(int entity, const vec_t *origin, const char *name)
{ (void)entity; (void)origin; (void)name; abort(); }
void CG_FireWeapon(centity_t *cent, int weapon, int hand) { (void)cent; (void)weapon; (void)hand; abort(); }
void CG_EjectWeaponBrass(entityState_t *es, int weapon) { (void)es; (void)weapon; abort(); }
void CG_PrepOffHand(entityState_t *es, int weapon, int parm) { (void)es; (void)weapon; (void)parm; abort(); }
void CG_UseOffHand(centity_t *cent, int weapon, int parm) { (void)cent; (void)weapon; (void)parm; abort(); }
void CG_SetEquippedOffHand(int weapon) { (void)weapon; abort(); }
void CG_SelectWeaponIndex(int weapon) { (void)weapon; abort(); }
void CG_OutOfAmmoChange(void) { abort(); }
void CG_SwitchOffHandCmd(void) { abort(); }
void CG_MenuShowNotify(int value) { (void)value; abort(); }
void CG_StartShakeCamera(float scale, int time, const vec_t *origin, float radius)
{ (void)scale; (void)time; (void)origin; (void)radius; abort(); }
void CG_BulletHitEvent(int source, vec_t *start, vec_t *origin, vec_t *normal, int surface, int event)
{ (void)source; (void)start; (void)origin; (void)normal; (void)surface; (void)event; abort(); }
void CG_BulletHitClientEvent(int source, vec_t *origin, int surface, int event)
{ (void)source; (void)origin; (void)surface; (void)event; abort(); }
void CG_CompassAddWeaponPingInfo(centity_t *cent, const vec_t *origin, int time)
{ (void)cent; (void)origin; (void)time; abort(); }
void CG_PriorityCenterPrint(const char *message, float scale, int priority)
{ (void)message; (void)scale; (void)priority; abort(); }
void CG_DrawScoreboard_GetTeamColor(int team, vec_t *color) { (void)team; (void)color; abort(); }
void CL_SetADS(int value) { (void)value; abort(); }
void CG_CalcEntityLerpPositions(centity_t *cent) { (void)cent; abort(); }
void CL_DeathMessagePrint(const char *attacker, const float *ac, const char *target, const float *tc,
                          const char *icon, float w, float h, const float *ic, int flip)
{ (void)attacker; (void)ac; (void)target; (void)tc; (void)icon; (void)w; (void)h; (void)ic; (void)flip; abort(); }
void I_strncpyz(char *dest, const char *src, int size) { (void)dest; (void)src; (void)size; abort(); }
void I_strncat(char *dest, int size, const char *src) { (void)dest; (void)src; (void)size; abort(); }
const char *va(const char *format, ...) { (void)format; abort(); }
void Com_Printf(const char *format, ...) { (void)format; abort(); }
void Com_DPrintf(const char *format, ...) { (void)format; abort(); }
void Com_Error(errorParm_t code, const char *format, ...) { (void)code; (void)format; abort(); }

int main(void)
{
    cgs_t *cgs = (cgs_t *)cgsArray;
    snapshot_t snapshot = {0};
    centity_t cent = {0};
    cent.nextState.weapon = 2;
    cent.nextState.number = 43;
    cent.nextState.time = 77;
    cent.nextState.eventParm = 0;
    memcpy(cent.lerpOrigin, expectedOrigin, sizeof(expectedOrigin));
    cgArray[0].nextSnap = &snapshot;
    cgArray[0].time = 100;
    cgs->media.fx = &impactTable;
    weapons[2].projExplosionEffect = &weaponEffect;
    weapons[2].projExplosionSound = &weaponSound;
    for (int i = 0; i < 11 * 23; ++i) impactSlots[i] = &effects[i];
    for (int i = 0; i < 23; ++i) {
        cgs->media.grenadeBounceSound[i] = &aliases[0][i];
        cgs->media.grenadeExplodeSound[i] = &aliases[1][i];
        cgs->media.rocketExplodeSound[i] = &aliases[2][i];
    }
    const int events[] = {0xbb,0xbc,0xbd,0xbe,0xbf,0xc5};
    const int surfaces[] = {0,7,22};
    for (int s = 0; s < 3; ++s) for (int e = 0; e < 6; ++e) {
        int event = events[e], surface = surfaces[s];
        cent.nextState.surfType = surface;
        effectsSeen = soundsSeen = timesSeen = 0;
        noMarksExpected = event == 0xbe;
        CG_EntityEvent(&cent, event);
        assert(cgArray[0].nomarks == 0);
        if (event == 0xbf) {
            assert(effectsSeen == 1 && seenEffects[0] == &weaponEffect);
            assert(soundsSeen == 1 && seenSounds[0] == &weaponSound);
            assert(timesSeen == 2 && times[0] == 77 && times[1] == 100);
        } else {
            int type = event == 0xbb ? 8 : (event == 0xbc || event == 0xc5) ? 9 : 10;
            int index = event == 0xc5 ? 0 : surface;
            int weaponExtra = event != 0xbb && event != 0xc5;
            assert(effectsSeen == 1 + weaponExtra && soundsSeen == 1 + weaponExtra);
            assert(seenEffects[0] == &effects[type * 23 + index]);
            assert(seenSounds[0] == &aliases[type - 8][index]);
            if (weaponExtra) {
                assert(seenEffects[1] == &weaponEffect && seenSounds[1] == &weaponSound);
            }
        }
    }
    for (int i = 0; i < 64; ++i) cgs->fxs[i] = &configuredEffects[i];
    for (int index = 1; index < 64; index += 31) {
        cent.nextState.eventParm = index;
        effectsSeen = 0;
        CG_EntityEvent(&cent, 0xc2);
        assert(effectsSeen == 1 && seenEffects[0] == &configuredEffects[index]);
    }
    cent.nextState.eventParm = 3;
    CG_EntityEvent(&cent, 0xc3);
    assert(boltSeen == 1);
    puts("PASS: grenade/rocket/projectile FX and sound, surface slots, warp time, nomarks, configured/bolted events");
    return 0;
}
