/* Turret allocation and teleport contracts from the retail Mac call boundaries. */
#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern void G_SpawnTurret(gentity_t *, const char *);
extern void SP_turret(gentity_t *);
extern void G_InitTurrets(void);
extern void TeleportPlayer(gentity_t *, vec_t *, vec_t *);
extern void SP_info_null(gentity_t *);
extern void SP_info_notnull(gentity_t *);
extern void SP_light(gentity_t *);
extern void SP_misc_model(gentity_t *);
extern void SP_corona(gentity_t *);
level_locals_t level;
turretInfo_s turretInfo[32];
static gentity_t entity;
static gclient_t client;
static WeaponDef weapon;
static jmp_buf fatal;
static char errorText[256];
static int weaponIndex, registered, floatCalls, soundCalls, linkCalls, freeCalls;
static int overrides, missingInfo, phase, teleporting;
static vec3_t destination = {10.0f, 20.0f, 30.0f}, view = {1.0f, 2.0f, 3.0f};

void Com_Error(int code, const char *format, ...)
{
    va_list ap; assert(code == 1); va_start(ap, format);
    vsnprintf(errorText, sizeof(errorText), format, ap); va_end(ap); longjmp(fatal, 1);
}
void Scr_Error(const char *text) { snprintf(errorText,sizeof(errorText),"%s",text); longjmp(fatal,1); }
const char *va(const char *format, ...)
{
    static char text[256]; va_list ap; va_start(ap,format);
    vsnprintf(text,sizeof(text),format,ap); va_end(ap); return text;
}
int G_GetWeaponIndexForName(const char *name)
{
    assert(!strcmp(name,"fixture_turret"));
    assert(entity.pTurretInfo && entity.pTurretInfo->inuse == 1);
    return weaponIndex;
}
WeaponDef *BG_GetWeaponDef(int index) { assert(index == 7); return &weapon; }
qboolean IsItemRegistered(int index) { assert(index == 7); return registered; }
SoundAlias G_SoundAliasIndex(const char *name)
{
    const char *names[] = {"loop", "playerloop", "stop", "playerstop"};
    assert(soundCalls < 4 && !strcmp(name,names[soundCalls])); return (SoundAlias)(10 + soundCalls++);
}
qboolean G_SpawnFloat(const char *key, const char *def, float *value)
{
    const char *keys[] = {"rightarc","leftarc","toparc","bottomarc","playerSpread"};
    const float values[] = {-11.0f,-12.0f,-13.0f,-14.0f,-2.0f};
    assert(floatCalls < 5 && !strcmp(key,keys[floatCalls]));
    assert(!strcmp(def,floatCalls == 4 ? "1" : ""));
    *value = values[floatCalls++]; return overrides;
}
qboolean G_SpawnInt(const char *key, const char *def, int *value)
{ assert(!strcmp(key,"damage") && !strcmp(def,"0")); *value=-9; return overrides; }
qboolean G_SpawnString(const char *key, const char *def, const char **value)
{ assert(!strcmp(key,"weaponinfo") && !strcmp(def,"")); *value="fixture_turret"; return !missingInfo; }
void G_DObjUpdate(gentity_t *ent)
{
    assert(ent == &entity && phase++ == 0);
    assert(ent->clipmask == 1 && ent->r.contents == 0x200004);
    assert(ent->s.eType == 9 && ent->flags == 0x1008);
}
void G_SetOrigin(gentity_t *ent, const vec_t *origin)
{
    assert(ent == &entity && origin == ent->r.currentOrigin);
    if (phase) { assert(phase++ == 1); assert(ent->r.mins[0] == -32 && ent->r.maxs[2] == 56); }
}
void G_SetAngle(gentity_t *ent, const vec_t *angle)
{ assert(ent == &entity && angle == ent->r.currentAngles && phase++ == 2); }
void SV_LinkEntity(gentity_t *ent)
{
    assert(ent == &entity); ++linkCalls;
    if (teleporting) { assert(phase++ == 3); assert(!memcmp(ent->r.currentOrigin,client.ps.origin,sizeof(vec3_t))); }
    else { assert(phase++ == 3); assert(ent->handler == 13 && ent->nextthink == 1050); }
}
void G_FreeEntity(gentity_t *ent) { assert(ent == &entity); ++freeCalls; }
void SV_UnlinkEntity(gentity_t *ent) { assert(ent == &entity && phase++ == 0); ent->r.linked=0; }
void SetClientViewAngle(gentity_t *ent, const vec_t *angles)
{
    assert(ent == &entity && angles == view && phase++ == 1);
    assert(client.ps.origin[0] == 10 && client.ps.origin[1] == 20 && client.ps.origin[2] == 31);
    assert(client.ps.eFlags == 0x42);
}
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *state, qboolean snap, int handler)
{ assert(ps == &client.ps && state == &entity.s && snap == 1 && handler == 1 && phase++ == 2); }

static void Reset(void)
{
    memset(&entity,0,sizeof(entity)); memset(&client,0,sizeof(client)); memset(&weapon,0,sizeof(weapon));
    memset(&level,0,sizeof(level)); memset(turretInfo,0,sizeof(turretInfo));
    entity.client=&client; entity.flags=8; weaponIndex=7; registered=1;
    weapon.weapClass=WEAPCLASS_TURRET; weapon.stance=2; weapon.rightArc=11;
    weapon.leftArc=12; weapon.topArc=13; weapon.bottomArc=14; weapon.damage=25; weapon.playerSpread=1.5f;
    weapon.szFireLoopSound="loop"; weapon.szFireLoopSoundPlayer="playerloop";
    weapon.szFireStopSound="stop"; weapon.szFireStopSoundPlayer="playerstop";
    level.time=1000; level.initializing=1; phase=0; floatCalls=soundCalls=linkCalls=0;
}
static void SpawnControl(int mode)
{
    Reset(); overrides=mode==2; level.spawnVar.spawnVarsValid=mode!=0;
    turretInfo[0].inuse=1; turretInfo[1].inuse=1;
    entity.health=mode==2 ? 77 : 0;
    G_SpawnTurret(&entity,"fixture_turret");
    turretInfo_s *info=entity.pTurretInfo;
    assert(info == &turretInfo[2] && info->inuse == 1);
    assert(turretInfo[0].inuse && turretInfo[1].inuse && !turretInfo[3].inuse);
    assert(info->stance == 2 && info->prevStance == -1 && info->flags == 3);
    assert(info->dropPitch == -90 && !info->fireTime && !info->fireSndDelay);
    assert(info->fireSnd == 10 && info->fireSndPlayer == 11 && info->stopSnd == 12 && info->stopSndPlayer == 13);
    assert(info->arcmin[0] == (overrides ? 0 : -13) && info->arcmin[1] == (overrides ? 0 : -11));
    assert(info->arcmax[0] == (overrides ? 0 : 14) && info->arcmax[1] == (overrides ? 0 : 12));
    assert(info->playerSpread == (overrides ? 0 : 1.5f) && entity.damage == (overrides ? 0 : 25));
    assert(entity.health == (overrides ? 77 : 100));
    assert(floatCalls == (mode ? 5 : 0) && phase == 4 && linkCalls == 1);
    assert(entity.s.apos.trType == TR_LINEAR_STOP && !entity.takedamage);
}
int main(int argc, char **argv)
{
    assert(argc == 2); Reset();
    if (!strcmp(argv[1],"defaults")) SpawnControl(0);
    else if (!strcmp(argv[1],"fallback")) SpawnControl(1);
    else if (!strcmp(argv[1],"overrides")) SpawnControl(2);
    else if (!strcmp(argv[1],"teleport")) {
        teleporting=1;
        for (int linked=0; linked<2; ++linked) {
            Reset(); entity.r.linked=linked; client.ps.eFlags=0x40;
            TeleportPlayer(&entity,destination,view); assert(linkCalls == linked && phase == 3+linked);
            assert(!memcmp(entity.r.currentOrigin,client.ps.origin,sizeof(vec3_t)));
        }
    } else if (!strcmp(argv[1],"helpers")) {
        memset(turretInfo,0x5a,sizeof(turretInfo)); G_InitTurrets();
        for (int i=0;i<32;++i) { assert(!turretInfo[i].inuse); assert(turretInfo[i].fireTime == 0x5a5a5a5a); }
        SP_info_null(&entity); SP_light(&entity); SP_misc_model(&entity); SP_corona(&entity); assert(freeCalls == 4);
        SP_info_notnull(&entity);
    } else {
        const char *expected;
        if (!strcmp(argv[1],"exhausted")) {
            for(int i=0;i<32;++i) turretInfo[i].inuse=1;
            expected="\025G_SpawnTurret: max number of turrets (32) exceeded";
        } else if (!strcmp(argv[1],"weapon")) { weaponIndex=0; expected="\025bad weaponinfo 'fixture_turret' specified for turret"; }
        else if (!strcmp(argv[1],"class")) { weapon.weapClass=WEAPCLASS_RIFLE; expected="G_SpawnTurret: weapon 'fixture_turret' isn't a turret. This usually indicates that the weapon failed to load."; }
        else if (!strcmp(argv[1],"precache")) { level.initializing=0; registered=0; expected="turret 'fixture_turret' not precached"; }
        else { assert(!strcmp(argv[1],"spawn-entry")); missingInfo=1; expected="\025no weaponinfo specified for turret"; }
        if (!setjmp(fatal)) { if(missingInfo) SP_turret(&entity); else G_SpawnTurret(&entity,"fixture_turret"); assert(!"expected fatal error"); }
        assert(!strcmp(errorText,expected));
    }
    return 0;
}
