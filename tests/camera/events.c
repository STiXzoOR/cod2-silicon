/* Exercise the complete production event dispatcher; synthetic outputs only. */
#include "events_source.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cod2_native_trace_enabled;
cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static centity_t entities[1024];
centity_t *cg_entities = entities;
static snapshot_t next_snapshot;
static weaponInfo_t weapons[128];
static weaponInfo_t *weapons_ptr = weapons;
void *imp_cg_weapons = &weapons_ptr;
static itemInfo_t items[8];
static itemInfo_t *items_ptr = items;
void *imp_cg_items = &items_ptr;
static gitem_t item_defs[8];
void *imp_bg_itemlist = item_defs;
static int item_count = 8;
void *imp_bg_numItems = &item_count;
static char *event_names[256];
void *imp_eventnames = event_names;
static char ui[32], *ui_ptr = ui;
char **cg_uiglob = &ui_ptr;
static dvar_t zero, enabled;
const dvar_t *cg_debugEvents = &zero, *cg_footsteps = &enabled;
const dvar_t *bg_fallDamageMinHeight = &zero, *bg_fallDamageMaxHeight = &zero;
const dvar_t *cg_nopredict = &zero, *cg_synchronousClients = &zero;
static WeaponDef definitions[128];
static snd_alias_list_t aliases[16];
static EffectTemplate effect;
static snd_alias_list_t *heard[8];
static int heard_entity[8], heard_count, prep_count, use_count, selected, offhand;
static int quake_count, quake_duration, fx_count, oriented_count, center_count, death_count;
static float quake_scale, quake_radius, quake_origin[3], fx_origin[3], fx_forward[3], fx_up[3];
static char center[256], death_attacker[64], death_target[64], death_icon[64];
static float death_width, death_height;
static int death_flip, lerp_count;

WeaponDef *BG_GetWeaponDef(int n) { assert(n >= 0 && n < 128); return &definitions[n]; }
int BG_WeaponIsClipOnly(int weapon) { (void)weapon; return 0; }
int CG_PlayEntitySoundAlias(int ent, snd_alias_list_t *alias)
{ assert(heard_count < 8); heard_entity[heard_count] = ent; heard[heard_count++] = alias; return 1; }
int CG_PlaySoundAlias(int ent, const vec_t *origin, snd_alias_list_t *alias)
{ (void)origin; return CG_PlayEntitySoundAlias(ent, alias); }
int CG_PlaySoundAliasByName(int ent, const vec_t *origin, const char *name)
{ (void)ent; (void)origin; (void)name; abort(); }
int CG_PlaySoundAliasAsMasterByName(int ent, const vec_t *origin, const char *name)
{ (void)ent; (void)origin; (void)name; abort(); }
void CG_PrepOffHand(entityState_t *es, int event, int parm)
{ assert(es == &entities[3].nextState && event == 0xa5 && parm == 17); ++prep_count; }
void CG_UseOffHand(centity_t *cent, int event, int parm)
{ assert(cent == &entities[3] && event == 0xa6 && parm == 17); ++use_count; }
void CG_SetEquippedOffHand(int weapon) { offhand = weapon; }
void CG_SelectWeaponIndex(int weapon) { selected = weapon; }
void CG_FireWeapon(centity_t *cent, int weapon, int hand) { (void)cent; (void)weapon; (void)hand; abort(); }
void CG_EjectWeaponBrass(entityState_t *es, int weapon) { (void)es; (void)weapon; abort(); }
void CG_OutOfAmmoChange(void) { abort(); }
void CG_SwitchOffHandCmd(void) { abort(); }
void CG_MenuShowNotify(int val) { (void)val; abort(); }
void CL_SetADS(int val) { (void)val; abort(); }
void CG_StartShakeCamera(float scale, int duration, const vec_t *origin, float radius)
{ ++quake_count; quake_scale = scale; quake_duration = duration; quake_radius = radius; memcpy(quake_origin, origin, sizeof(quake_origin)); }
void CG_BulletHitEvent(int ent, vec_t *start, vec_t *pos, vec_t *normal, int surface, int event)
{ (void)ent; (void)start; (void)pos; (void)normal; (void)surface; (void)event; abort(); }
void CG_BulletHitClientEvent(int ent, vec_t *pos, int surface, int event)
{ (void)ent; (void)pos; (void)surface; (void)event; abort(); }
void CG_CompassAddWeaponPingInfo(centity_t *cent, const vec_t *origin, int duration)
{ (void)cent; (void)origin; (void)duration; abort(); }
void FX_PlayEffect(EffectTemplate *fx, const vec_t *origin, const vec_t *forward)
{ assert(fx == &effect); ++fx_count; memcpy(fx_origin, origin, sizeof(fx_origin)); memcpy(fx_forward, forward, sizeof(fx_forward)); }
void FX_PlayOrientedEffect(EffectTemplate *fx, const vec_t *origin, const vec_t *forward, const vec_t *up)
{ FX_PlayEffect(fx, origin, forward); ++oriented_count; memcpy(fx_up, up, sizeof(fx_up)); }
void FX_PlayEntityEffect(EffectTemplate *fx, const vec_t *origin, vec3_t *axis, const FxBoltInfo *bolt)
{ (void)fx; (void)origin; (void)axis; (void)bolt; abort(); }
void FX_WarpTime(int time) { (void)time; abort(); }
int FX_GetBoneIndex(int model, unsigned int bone) { (void)model; (void)bone; abort(); }
const char *CL_GetConfigString(int n) { (void)n; abort(); }
unsigned int SL_GetString(const char *s, unsigned int user) { (void)s; (void)user; abort(); }
void Scr_SetString(scr_string_t *s, unsigned int value) { (void)s; (void)value; abort(); }
void CG_CalcEntityLerpPositions(centity_t *cent) { assert(cent == &entities[3]); ++lerp_count; }
void CG_CheckOpenWaitingScriptMenu(void) { abort(); }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void I_strncpyz(char *dest, const char *src, int size) { assert(size > 0); snprintf(dest, (size_t)size, "%s", src); }
void I_strncat(char *dest, int size, const char *src) { size_t n = strlen(dest); assert(n < (size_t)size); snprintf(dest + n, size - n, "%s", src); }
const char *va(const char *fmt, ...)
{ static char text[512]; va_list args; va_start(args, fmt); vsnprintf(text, sizeof(text), fmt, args); va_end(args); return text; }
void CG_DrawScoreboard_GetTeamColor(int team, vec_t *color)
{ color[0] = team == 1; color[1] = team == 2; color[2] = 0.0f; color[3] = 1.0f; }
void CG_PriorityCenterPrint(const char *text, float scale, int priority)
{ assert(scale == 9.6f && priority == 1); ++center_count; snprintf(center, sizeof(center), "%s", text); }
void CL_DeathMessagePrint(const char *attacker, const float *ac, const char *target, const float *vc,
                         const char *icon, float width, float height, const float *ic, int flip)
{
    assert(ac && vc && ic); ++death_count;
    snprintf(death_attacker, sizeof(death_attacker), "%s", attacker);
    snprintf(death_target, sizeof(death_target), "%s", target);
    snprintf(death_icon, sizeof(death_icon), "%s", icon);
    death_width = width; death_height = height; death_flip = flip;
}

static void Reset(void)
{
    memset(cgArray, 0, sizeof(cgArray)); memset(cgsArray, 0, sizeof(cgsArray));
    memset(&next_snapshot, 0, sizeof(next_snapshot)); memset(entities, 0, sizeof(entities));
    memset(weapons, 0, sizeof(weapons)); memset(definitions, 0, sizeof(definitions));
    memset(items, 0, sizeof(items)); memset(item_defs, 0, sizeof(item_defs));
    memset(heard, 0, sizeof(heard)); memset(fx_up, 0, sizeof(fx_up));
    heard_count = prep_count = use_count = quake_count = fx_count = oriented_count = center_count = death_count = lerp_count = 0;
    selected = offhand = 0;
    cg->nextSnap = &next_snapshot; cg->clientNum = 3;
    next_snapshot.ps.clientNum = 3; next_snapshot.ps.pm_flags = 0x400000;
    enabled.current.enabled = 1;
    entities[3].nextState.number = entities[3].nextState.clientNum = 3;
    entities[3].nextState.weapon = 1; entities[3].nextState.eventParm = 17;
    entities[3].lerpOrigin[0] = 13.0f; entities[3].lerpOrigin[1] = -7.0f; entities[3].lerpOrigin[2] = 9.5f;
    for (int i = 0; i < 128; ++i) definitions[i].killIcon = "";
    for (int i = 0; i < 64; ++i) { cg->bgs.clientinfo[i].infoValid = 1; snprintf(cg->bgs.clientinfo[i].name, 32, "client%d", i); }
}

static void Heard(snd_alias_list_t *alias)
{ assert(heard_count == 1 && heard[0] == alias && heard_entity[0] == entities[3].nextState.number); }
static void Near(float actual, float expected) { assert(isfinite(actual) && fabsf(actual - expected) < 0.00001f); }
static void Origin(const float *v) { Near(v[0], 13.0f); Near(v[1], -7.0f); Near(v[2], 9.5f); }

static void Perspective(void)
{
    /* Both local first-person flags, remote entity, and spectator/no-view flags. */
    int flags[] = { 0, 0x400000, 0x800000, 0xc00000, 0x40 };
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) for (int remote = 0; remote < 2; ++remote) {
        Reset(); next_snapshot.ps.pm_flags = flags[i]; entities[3].nextState.number += remote;
        weapons[1].reloadSoundPlayer = &aliases[0]; weapons[1].reloadEmptySoundPlayer = &aliases[1];
        weapons[1].reloadSound = &aliases[2]; weapons[1].reloadEmptySound = &aliases[3];
        int local = !remote && (flags[i] & 0xc00000);
        for (int event = 0x96; event <= 0x97; ++event) {
            for (int missing = 0; missing < 5; ++missing) {
                weapons[1].reloadSoundPlayer = missing >= 1 ? NULL : &aliases[0];
                weapons[1].reloadEmptySoundPlayer = missing >= 2 ? NULL : &aliases[1];
                weapons[1].reloadSound = missing >= 3 ? NULL : &aliases[2];
                weapons[1].reloadEmptySound = missing >= 4 ? NULL : &aliases[3];
                /* Mac branch priority, independent of the dispatcher's control flow:
                 * ordinary reload/empty, then local reload/empty, each with five
                 * progressively missing aliases. -1 means no sound call. */
                static const int expected_alias[2][2][5] = {
                    { { 2, 2, 2, 3, -1 }, { 3, 3, 3, 3, -1 } },
                    { { 0, 1, 2, 3, -1 }, { 1, 1, 3, 3, -1 } }
                };
                int alias_index = expected_alias[local != 0][event - 0x96][missing];
                snd_alias_list_t *expected = alias_index < 0 ? NULL : &aliases[alias_index];
                heard_count = 0; CG_EntityEvent(&entities[3], event);
                if (expected) Heard(expected); else assert(heard_count == 0);
            }
        }
    }
}

static void LocalActions(void)
{
    int flags[] = { 0, 0x40, 0x400000, 0x800000, 0xc00000 };
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) for (int remote = 0; remote < 2; ++remote) {
        Reset(); next_snapshot.ps.pm_flags = flags[i]; entities[3].nextState.number += remote;
        CG_EntityEvent(&entities[3], 0xa5); CG_EntityEvent(&entities[3], 0xa6);
        int wanted = !remote && (flags[i] & 0xc00000) != 0;
        assert(prep_count == wanted && use_count == wanted);
    }
}

static void WeaponAlias(int event)
{
    for (int remote = 0; remote < 2; ++remote) for (int missing = 0; missing < 3; ++missing) {
        Reset(); entities[3].nextState.number += remote;
        snd_alias_list_t *regular = missing >= 2 ? NULL : &aliases[0];
        snd_alias_list_t *player = missing >= 1 ? NULL : &aliases[1];
        if (event == 0x98) { weapons[1].reloadStartSound = regular; weapons[1].reloadStartSoundPlayer = player; weapons[1].reloadEndSound = &aliases[2]; }
        if (event == 0x99) { weapons[1].reloadEndSound = regular; weapons[1].reloadEndSoundPlayer = player; weapons[1].reloadStartSound = &aliases[2]; }
        if (event == 0xa1) { weapons[1].rechamberSound = regular; weapons[1].rechamberSoundPlayer = player; weapons[1].meleeSwipeSound = &aliases[2]; }
        CG_EntityEvent(&entities[3], event);
        snd_alias_list_t *expected = !remote && player ? player : regular;
        if (expected) Heard(expected); else assert(heard_count == 0);
    }
}

static void Quake(void)
{
    float radii[] = { 0.0f, 0.5f, 128.25f, 8192.0f };
    for (unsigned i = 0; i < sizeof(radii) / sizeof(radii[0]); ++i) {
        Reset(); entities[3].nextState.angles2[0] = 0.75f; entities[3].nextState.angles2[1] = radii[i]; entities[3].nextState.time = 1234;
        CG_EntityEvent(&entities[3], 0xc4);
        assert(quake_count == 1 && quake_duration == 1234 && quake_scale == 0.75f);
        assert(!memcmp(&quake_radius, &radii[i], sizeof(float))); Origin(quake_origin);
    }
}

static void Oriented(void)
{
    for (int roll = 0; roll < 2; ++roll) {
        Reset(); entities[3].nextState.eventParm = 7; cgs->fxs[7] = &effect;
        entities[3].nextState.apos.trBase[1] = 90.0f; entities[3].nextState.apos.trBase[2] = roll * 90.0f;
        CG_EntityEvent(&entities[3], 0xc2);
        assert(fx_count == 1 && oriented_count == 1); Origin(fx_origin);
        Near(fx_forward[0], 0.0f); Near(fx_forward[1], 1.0f); Near(fx_forward[2], 0.0f);
        Near(fx_up[0], roll ? 1.0f : 0.0f); Near(fx_up[1], 0.0f); Near(fx_up[2], roll ? 0.0f : 1.0f);
    }
    int invalid[] = { 0, 64, 255 };
    for (unsigned i = 0; i < 3; ++i) { Reset(); entities[3].nextState.eventParm = invalid[i]; CG_EntityEvent(&entities[3], 0xc2); assert(fx_count == 0); }
}

static void Obit(int mod, int target, int attacker)
{ entities[3].nextState.eventParm = mod; entities[3].nextState.otherEntityNum = target; entities[3].nextState.attackerEntityNum = attacker; CG_EntityEvent(&entities[3], 0xc6); }
static void Icons(void)
{
    int mods[] = { 7, 8, 9, 11, 12, 13 };
    const char *icons[] = { "killiconmelee", "killiconheadshot", "killiconcrush", "killiconfalling", "killiconsuicide", "killicondied" };
    for (unsigned i = 0; i < 6; ++i) {
        Reset(); Obit(0x80 | mods[i], 4, 5);
        assert(death_count == 1 && !strcmp(death_icon, icons[i]));
        assert(!strcmp(death_attacker, "client5^7") && !strcmp(death_target, "client4^7"));
        assert(death_width == 1.4f && death_height == 1.4f && !death_flip && center_count == 0);
    }
    Reset(); definitions[1].killIcon = "synthetic-weapon"; definitions[1].wideKillIcon = 1; definitions[1].flipKillIcon = 1;
    Obit(1, 4, 5); assert(!strcmp(death_icon, "synthetic-weapon") && death_width == 2.8f && death_flip);
    Reset(); Obit(1, 4, 5); assert(!strcmp(death_icon, "killicondied"));
}

static void Messages(void)
{
    Reset(); Obit(0x80 | 12, 3, 1022);
    assert(death_count == 1 && center_count == 0 && !strcmp(death_attacker, ""));
    assert(!strcmp(death_target, "client3^7"));
    Reset(); Obit(0x80 | 8, 3, 4);
    assert(death_count == 1 && center_count == 1 && !strcmp(center, "CGAME_YOUWEREKILLED\025client4^7"));
    assert(!strcmp(cg->killerName, "client4^7"));
    Reset(); cg->bgs.clientinfo[3].oldteam = cg->bgs.clientinfo[4].oldteam = 1; Obit(0x80 | 8, 3, 4);
    assert(center_count == 1 && !strcmp(center, "CGAME_YOUWEREKILLED\025^1&&2^7 client4^7\024CGAME_TEAMMATE"));
    Reset(); Obit(0x80 | 8, 4, 3);
    assert(center_count == 1 && !strcmp(center, "CGAME_YOUKILLED\025client4^7"));
    Reset(); Obit(0x80 | 12, 3, 3); assert(center_count == 0 && death_count == 1 && !strcmp(death_attacker, ""));
    Reset(); cg->inKillCam = 1; Obit(0x80 | 8, 3, 4); assert(center_count == 0 && death_count == 0);
}

static void Controls(void)
{
    Reset(); CG_EntityEvent(&entities[3], 0); assert(!heard_count && !prep_count && !use_count);
    /* Run the unchanged event-ring integration, preserving the old event parm. */
    weapons[1].reloadStartSoundPlayer = &aliases[0]; weapons[1].reloadEndSoundPlayer = &aliases[1];
    entities[3].nextState.events[0] = 0x98; entities[3].nextState.events[1] = 0x99;
    entities[3].nextState.eventParms[0] = 2; entities[3].nextState.eventParms[1] = 3;
    entities[3].nextState.eventSequence = 2;
    CG_CheckEvents(&entities[3]);
    assert(heard_count == 2 && heard[0] == &aliases[0] && heard[1] == &aliases[1]);
    assert(entities[3].nextState.eventParm == 17 && entities[3].previousEventSequence == 2 && lerp_count == 1);
    CG_CheckEvents(&entities[3]); assert(heard_count == 2 && lerp_count == 1);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "perspective")) Perspective();
    else if (!strcmp(argv[1], "local-actions")) LocalActions();
    else if (!strcmp(argv[1], "reload-start")) WeaponAlias(0x98);
    else if (!strcmp(argv[1], "reload-end")) WeaponAlias(0x99);
    else if (!strcmp(argv[1], "rechamber")) WeaponAlias(0xa1);
    else if (!strcmp(argv[1], "quake")) Quake();
    else if (!strcmp(argv[1], "oriented-fx")) Oriented();
    else if (!strcmp(argv[1], "obituary-icons")) Icons();
    else if (!strcmp(argv[1], "obituary-message")) Messages();
    else if (!strcmp(argv[1], "controls")) Controls();
    else abort();
    printf("PASS events %s\n", argv[1]);
    return 0;
}
