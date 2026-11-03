/* Mac turret control-flow contracts; service mocks do not implement game data. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static bgs_t level_bgs;
static scr_const_t scr_const;
static gentity_t entities[1024];
static void *imp_g_entities = entities;
static gclient_t client;
static turretInfo_s info;
static WeaponDef weapon;
static animation_s animation;
static DObjAnimMat tag;
static char treeStorage[16], animStorage[16];
static const vec3_t g_misc_vec3_origin = {0,0,0};
static int errorKind, warnings, tagExists, flashExists, bullets, rockets, events;
static int links, traces, stops, sounds, clearCount, goalCount;
static float yaw, traceFraction;
static jmp_buf fatal;
static char errorText[256];
static gentity_t *lastAttacker;
static struct { unsigned int anim; float weight, time; } goals[32];

static void Com_Error(int code, const char *fmt, ...)
{
    assert(code == 1);
    va_list ap; va_start(ap,fmt); vsnprintf(errorText,sizeof(errorText),fmt,ap); va_end(ap);
    longjmp(fatal,1);
}
static void Com_Printf(const char *fmt, ...) { assert(!strcmp(fmt,"WARNING: aborting player positioning on turret since 'tag_weapon' does not exist\n")); ++warnings; }
static float AngleSubtract(float a, float b) { return a-b; }
static DObjAnimMat *G_DObjGetLocalTagMatrix(gentity_t *ent, unsigned int name) { assert(ent==&entities[10] && name==42); return tagExists ? &tag : NULL; }
static qboolean G_DObjGetWorldTagMatrix(gentity_t *ent, unsigned int name, vec3_t *axis)
{
    assert(ent==&entities[10] && name==43);
    memset(axis,0,12*sizeof(float)); axis[3][0]=3;axis[3][1]=4;
    return flashExists;
}
static const char *SL_ConvertToString(unsigned int string) { assert(string==17); return "fixture_turret"; }
static void G_GetPlayerViewOrigin(const gentity_t *ent, vec_t *out) { lastAttacker=(gentity_t *)ent; memset(out,0,sizeof(vec3_t)); }
static void G_GetPlayerViewDirection(const gentity_t *ent, vec_t *forward, vec_t *right, vec_t *up)
{
    assert(ent==lastAttacker); memset(forward,0,sizeof(vec3_t)); memset(right,0,sizeof(vec3_t)); memset(up,0,sizeof(vec3_t)); forward[0]=1;right[1]=1;up[2]=1;
}
static float Vec3Normalize(vec_t *v) { float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); if(n)for(int i=0;i<3;++i)v[i]/=n;return n; }
static WeaponDef *BG_GetWeaponDef(int index) { assert(index==7);return &weapon; }
static void Bullet_Fire(gentity_t *attacker,float spread,weaponParms *wp,gentity_t *ent,int time)
{
    assert(attacker==lastAttacker && ent==&entities[10] && time==1234 && spread==2.5f);
    assert(wp->weapDef==&weapon && wp->forward[0]==1 && wp->right[1]==1 && wp->up[2]==1);
    assert(wp->gunForward[0]==1 && wp->muzzleTrace[0]==5 && wp->muzzleTrace[1]==0 && wp->muzzleTrace[2]==0); ++bullets;
}
static void Weapon_RocketLauncher_Fire(gentity_t *ent,float spread,weaponParms *wp) { assert(ent==&entities[10] && spread==0 && wp->weapDef==&weapon && wp->muzzleTrace[0]==5);++rockets; }
static void G_AddEvent(gentity_t *ent,int event,int parm) { assert(ent==&entities[10] && event==0xaf && parm==lastAttacker->s.number);++events; }
static void ConvertQuatToMat(const DObjAnimMat *mat,float axis[3][3]) { assert(mat==&tag);memset(axis,0,9*sizeof(float));axis[0][0]=axis[1][1]=axis[2][2]=1; }
static float vectosignedyaw(const vec3_t v) { assert(v[0]==1);return yaw; }
static void AnglesToAxis(const vec_t *angles,vec3_t *axis) { assert(angles==entities[10].r.currentAngles);memset(axis,0,9*sizeof(float));axis[0][0]=axis[1][1]=axis[2][2]=1; }
static void XAnimClearTreeGoalWeightsStrict(XAnimTree *tree,unsigned int anim,float time) { assert(tree==(XAnimTree *)treeStorage && anim==1 && time==0);++clearCount; }
static int XAnimGetNumChildren(const XAnim *anims,unsigned int anim) { assert(anims==(XAnim *)animStorage);return anim==1 ? (errorKind==1 ? 0 : 2) : (errorKind==2 ? 0 : 3); }
static const char *XAnimGetAnimDebugName(const XAnim *anims,unsigned int anim) { assert(anims==(XAnim *)animStorage);return anim==1 ? "base" : "height"; }
static unsigned int XAnimGetChildAt(const XAnim *anims,unsigned int anim,unsigned int child)
{
    assert(anims==(XAnim *)animStorage);if(anim==1){assert(child<2);return 10+child;}
    assert((anim==10 || anim==11) && child<3);return anim*10+child;
}
static int XAnimSetGoalWeight(XAnimTree *tree,unsigned int anim,float weight,float time,float rate,unsigned int notify,unsigned int type,int restart)
{
    assert(tree==(XAnimTree *)treeStorage && rate==1 && notify==0 && type==0 && restart==0);
    assert(goalCount<32);goals[goalCount].anim=anim;goals[goalCount].weight=weight;goals[goalCount++].time=time;return 0;
}
static float XAnimGetWeight(const XAnimTree *tree,unsigned int anim) { assert(tree==(XAnimTree *)treeStorage && anim);return 0; }
static void XAnimCalcAbsDelta(XAnimTree *tree,unsigned int anim,float *rot,float *trans)
{
    assert(tree==(XAnimTree *)treeStorage);rot[0]=0;rot[1]=1;
    trans[0]=anim==1 ? 2 : 0;trans[1]=anim==1 ? 3 : 0;trans[2]=anim==10 ? 0 : 10;
}
static void VectorAngleMultiply(float *v,float angle) { assert(angle==yaw && v[0]==2 && v[1]==3); }
static float RotationToYaw(const vec2_t rot) { assert(rot[0]==0 && rot[1]==1);return 0; }
static void YawToAxis(float angle,vec3_t axis[3]) { assert(angle==yaw);memset(axis,0,9*sizeof(float));axis[0][0]=axis[1][1]=axis[2][2]=1; }
static void MatrixMultiply43(const float (*a)[3],const float (*b)[3],float (*out)[3]) { memcpy(out,a,12*sizeof(float));for(int i=0;i<3;++i)out[3][i]+=b[3][i]; }
static void G_TraceCapsule(trace_t *result,const vec_t *start,const vec_t *mins,const vec_t *maxs,const vec_t *end,int pass,int mask)
{
    assert(start[0]==3 && start[1]==5 && start[2]==client.ps.origin[2]+40);
    assert(end[0]==3 && end[1]==5 && end[2]==client.ps.origin[2]-60);
    for(int i=0;i<3;++i)assert(mins[i]==0 && maxs[i]==0);
    assert(pass==2 && mask==0x2810011);result->fraction=traceFraction;++traces;
}
static void BG_PlayerStateToEntityState(playerState_t *ps,entityState_t *s,qboolean snap,int handler) { assert(ps==&client.ps && s==&entities[2].s && snap==1 && handler==1); }
static void AxisToAngles(vec3_t *axis,vec_t *angles) { assert(axis[3][0]==3 && axis[3][1]==5);angles[0]=10;angles[1]=20;angles[2]=30; }
static void SV_LinkEntity(gentity_t *ent) { assert(ent==&entities[2]);++links; }
static void G_ClientStopUsingTurret(gentity_t *ent) { assert(ent==&entities[10]);++stops; }
static void turret_UpdateSound(gentity_t *ent) { assert(ent==&entities[10]);++sounds; }
#include "misc_fire_body.inc"

static void Reset(void)
{
    memset(&level,0,sizeof(level));memset(&level_bgs,0,sizeof(level_bgs));memset(entities,0,sizeof(entities));memset(&client,0,sizeof(client));memset(&info,0,sizeof(info));memset(&weapon,0,sizeof(weapon));memset(&animation,0,sizeof(animation));memset(&tag,0,sizeof(tag));
    for(int i=0;i<1024;++i)entities[i].s.number=i;
    entities[10].pTurretInfo=&info;entities[10].s.weapon=7;entities[10].classname=17;entities[10].r.ownerNum=2;
    entities[2].client=&client;entities[2].s.clientNum=0;entities[2].active=1;entities[2].r.currentOrigin[2]=5;
    client.ps.viewHeightCurrent=40;level.time=1234;level.frametime=50;weapon.iFireTime=100;weapon.fAnimHorRotateInc=30;info.playerSpread=2.5f;
    info.arcmin[0]=-20;info.arcmax[0]=20;info.arcmin[1]=-40;info.arcmax[1]=40;
    level_bgs.clientinfo[0].legs.animation=&animation;level_bgs.clientinfo[0].legs.animationNumber=1;animation.flags=4;
    level_bgs.clientinfo[0].pXAnimTree=(XAnimTree *)treeStorage;level_bgs.animScriptData.animTree.anims=(XAnim *)animStorage;
    tag.trans[0]=1;tag.trans[1]=2;scr_const.tag_weapon=42;scr_const.tag_flash=43;
    errorKind=warnings=bullets=rockets=events=links=traces=stops=sounds=clearCount=goalCount=0;
    tagExists=flashExists=1;yaw=30;traceFraction=0.25f;
}
static void FatalCase(const char *name)
{
    if(setjmp(fatal)==0){
        if(!strcmp(name,"flash-error")){flashExists=0;weaponParms wp;Turret_FillWeaponParms(&entities[10],&entities[2],&wp);}
        else {errorKind=!strcmp(name,"vertical-error") ? 1 : 2;G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);}
        abort();
    }
    const char *expected=!strcmp(name,"flash-error") ? "\x15" "Couldn't find tag_flash on turret (entity 10, classname 'fixture_turret').\n" : errorKind==1 ? "\x15" "Player anim 'base' has no children" : "\x15" "Player anim 'height' has no children";
    assert(!strcmp(errorText,expected));
}
static void FireCase(void)
{
    Fire_Lead(&entities[10],&entities[2]);assert(bullets==1 && rockets==0 && events==1);
    weapon.weapType=1;Fire_Lead(&entities[10],&entities[2]);assert(rockets==1 && events==2);
    weapon.weapType=0;Fire_Lead(&entities[10],&entities[1023]);assert(lastAttacker==&entities[1022] && bullets==2 && events==3);
    turret_shoot_internal(&entities[10],&entities[2]);assert(info.fireSndDelay==300 && client.ps.viewlocked==2 && events==4);
    turret_shoot_internal(&entities[10],&entities[3]);assert(events==4 && info.fireSndDelay==300);
}
static void AimCase(void)
{
    client.ps.viewangles[0]=-30;client.ps.viewangles[1]=60;info.flags=0x804;entities[10].s.eFlags=0x40;
    turret_clientaim(&entities[10],&entities[2]);assert(client.ps.viewlocked==1 && client.ps.viewlocked_entNum==10);assert(entities[10].s.angles2[0]==-20 && entities[10].s.angles2[1]==40 && entities[10].s.angles2[2]==0);assert(info.flags==4 && entities[10].s.eFlags==0x42);
    client.ps.viewangles[0]=5;client.ps.viewangles[1]=7;turret_clientaim(&entities[10],&entities[2]);assert(entities[10].s.angles2[0]==5 && entities[10].s.angles2[1]==7 && entities[10].s.eFlags==0x42);
}
static void PositionCase(void)
{
    G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);assert(clearCount==2 && traces==1 && links==1 && goalCount==12);
    assert(client.ps.origin[0]==3 && client.ps.origin[1]==5 && client.ps.origin[2]==20);
    assert(entities[2].r.currentOrigin[2]==20 && entities[2].r.currentAngles[1]==20);
    assert(goals[8].anim==11 && goals[8].weight==0.5f && goals[8].time==0.1f);
    assert(goals[9].anim==10 && goals[9].weight==0.5f && goals[9].time==0.1f);
}
static void PositionLimits(void)
{
    level_bgs.clientinfo[0].legs.animationNumber=0;G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);assert(!links);
    level_bgs.clientinfo[0].legs.animationNumber=1;tagExists=0;G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);assert(warnings==1 && !links);
    Reset();entities[2].r.currentOrigin[2]=-5;traceFraction=1;G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);assert(links==1 && client.ps.origin[2]==-5 && goalCount==6);
    Reset();entities[2].r.currentOrigin[2]=20;traceFraction=1;G_PlayerTurretPositionAndBlend(&entities[2],&entities[10]);assert(links==1 && client.ps.origin[2]==20 && goalCount==9);
}
static void TrackCase(void)
{
    level_bgs.clientinfo[0].legs.animationNumber=0;info.fireTime=100;client.buttons=1;entities[10].s.eFlags=0x40;
    turret_think_client(&entities[10]);assert(info.fireTime==50 && !events && sounds==1 && !(entities[10].s.eFlags&0x40));
    turret_think_client(&entities[10]);assert(info.fireTime==100 && events==1 && sounds==2 && client.ps.viewlocked==2 && (entities[10].s.eFlags&0x40));
    client.buttons=0;info.fireTime=30;turret_think_client(&entities[10]);assert(info.fireTime==0 && events==1 && client.ps.viewlocked==1);
    entities[2].active=0;turret_think_client(&entities[10]);assert(stops==1 && sounds==3);
    entities[2].active=1;client.sess.sessionState=SESS_STATE_SPECTATOR;turret_think_client(&entities[10]);assert(stops==2 && sounds==3);
}
int main(int argc,char **argv)
{
    assert(argc==2);Reset();
    if(strstr(argv[1],"-error"))FatalCase(argv[1]);
    else if(!strcmp(argv[1],"fire"))FireCase();
    else if(!strcmp(argv[1],"aim"))AimCase();
    else if(!strcmp(argv[1],"position"))PositionCase();
    else if(!strcmp(argv[1],"position-limits"))PositionLimits();
    else if(!strcmp(argv[1],"track"))TrackCase();
    else abort();
    return 0;
}
