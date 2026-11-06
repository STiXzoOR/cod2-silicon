/* Independent Mac turret lifecycle and finite angle-boundary contracts. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static gentity_t g_entities[1024];
static gclient_t client;
static turretInfo_s info;
static scr_const_t scr_const;
static void *imp_g_entities = g_entities;
static unsigned char priority[8];
static void *imp_bulletPriorityMap = priority;
static DObjAnimMat_s aimTag, buttTag;
static int eventCount, eventValue, teleportCount, tagCount, missingTag;
static int traceCount, hitTrace, transformCount;
static float sampledPitch;
static gentity_t *turret = &g_entities[70], *owner = &g_entities[3];

static float AngleNormalize180(float a)
{
    while (a > 180) a -= 360;
    while (a < -180) a += 360;
    return a;
}
static float AngleSubtract(float a, float b) { return AngleNormalize180(a-b); }
static float AngleNormalize360Accurate(float a)
{
    while (a < 0) a += 360;
    while (a >= 360) a -= 360;
    return a;
}
static void YawVectors(float yaw, float *forward, float *right)
{
    assert(!right);
    float radians = yaw * (float)(3.141592653589793/180.0);
    forward[0]=cosf(radians); forward[1]=sinf(radians); forward[2]=0;
}
static float Vec3Normalize(float *v)
{
    float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(n) for(int i=0;i<3;++i) v[i]/=n;
    return n;
}
static float Q_acos(float v) { assert(v >= -1 && v <= 1); return acosf(v); }
static void G_AddEvent(gentity_t *ent, int event, int parm)
{
    assert(ent==owner && parm==0 && info.fireSndDelay==0 && turret->s.loopSound==0);
    eventCount++; eventValue=event;
}
static void TeleportPlayer(gentity_t *ent, float *origin, float *angles)
{
    assert(ent==owner && origin==info.userOrigin && angles==owner->r.currentAngles);
    assert(info.prevStance==-1 && owner->active && turret->active);
    assert(info.fireSndDelay==0 && turret->s.loopSound==0);
    teleportCount++;
}
static DObjAnimMat_s *G_DObjGetLocalTagMatrix(gentity_t *ent, unsigned int tag)
{
    assert(ent==turret && turret->handler==14 && turret->nextthink==level.time+50);
    tagCount++;
    assert(tag==(tagCount==1 ? scr_const.tag_aim : scr_const.tag_butt));
    if(tagCount==missingTag) return NULL;
    return tagCount==1 ? &aimTag : &buttTag;
}
static void AnglesToAxis(const float *angles, float (*matrix)[3])
{
    memset(matrix,0,9*sizeof(float));
    for(int i=0;i<3;++i) matrix[i][i]=1;
    if(angles!=turret->r.currentAngles) {
        assert(angles[1]==0 && angles[2]==0);
        sampledPitch=angles[0];
        assert(sampledPitch==-3.0f*traceCount);
    }
}
static void MatrixTransformVector(const float *v, const float *matrix, float *out)
{
    (void)matrix;
    assert(v[0]==buttTag.trans[0]-aimTag.trans[0]);
    assert(v[1]==buttTag.trans[1]-aimTag.trans[1]);
    assert(v[2]==buttTag.trans[2]-aimTag.trans[2]);
    memcpy(out,v,3*sizeof(float)); transformCount++;
}
static void MatrixTransformVector43(const float *v, const float *matrix, float *out)
{
    for(int i=0;i<3;++i) out[i]=v[i]+matrix[9+i];
}
static void G_LocationalTrace(trace_t *trace, const float *start, const float *end,
                              int pass, int mask, unsigned char *priorities)
{
    assert(pass==70 && mask==0x811 && priorities==priority);
    for(int i=0;i<3;++i) {
        assert(start[i]==aimTag.trans[i]+turret->r.currentOrigin[i]);
        assert(end[i]==buttTag.trans[i]+turret->r.currentOrigin[i]);
    }
    trace->fraction=traceCount==hitTrace ? 0.25f : 1.0f;
    traceCount++;
}
#include "use_body.inc"

static void Reset(void)
{
    memset(g_entities,0,sizeof(g_entities)); memset(&client,0,sizeof(client));
    memset(&info,0,sizeof(info)); memset(&level,0,sizeof(level));
    turret->pTurretInfo=&info; turret->s.number=70; owner->s.number=3;
    owner->client=&client; info.inuse=1; info.prevStance=-1;
    info.arcmin[0]=-20; info.arcmax[0]=30; info.arcmin[1]=-45; info.arcmax[1]=45;
    owner->r.currentOrigin[0]=-10; client.ps.groundEntityNum=1022;
    eventCount=eventValue=teleportCount=tagCount=traceCount=transformCount=0;
    missingTag=0;hitTrace=-1;sampledPitch=0;
    level.time=300; scr_const.tag_aim=31;scr_const.tag_butt=32;
    for(int i=0;i<3;++i) {aimTag.trans[i]=i+1;buttTag.trans[i]=i+4;}
}
static void BehindBoundary(void)
{
    /* acosf of this finite normalized dot, then the Mac double degree scale. */
    float dot=0.9998999834060669f;
    float span=(float)((double)acosf(dot)*57.29577951308232);
    assert(span==0.8103586435317993f);
    info.arcmin[1]=-span;info.arcmax[1]=span;
    owner->r.currentOrigin[0]=-dot;
    owner->r.currentOrigin[1]=-sqrtf(1.0f-dot*dot);
    assert(G_IsTurretUsable(turret,owner)==1);
}
static void BehindNonfinite(void)
{
    /* Explicit arithmetic edge contract, not a normal gameplay scenario. */
    info.arcmin[1]=-180;info.arcmax[1]=180;
    owner->r.currentOrigin[0]=NAN;
    /* The retail select clamp feeds -1 to acos, never its NaN dot input. */
    assert(G_IsTurretUsable(turret,owner)==1);
    Reset();info.arcmax[1]=NAN;
    /* The final retail JA rejects only ordered angle > halfspan. */
    assert(G_IsTurretUsable(turret,owner)==1);
}
static void Usability(void)
{
    assert(G_IsTurretUsable(turret,owner)==1);
    turret->active=2;assert(!G_IsTurretUsable(turret,owner));turret->active=0;
    turret->pTurretInfo=NULL;assert(!G_IsTurretUsable(turret,owner));turret->pTurretInfo=&info;
    owner->r.currentOrigin[0]=10;assert(!G_IsTurretUsable(turret,owner));owner->r.currentOrigin[0]=-10;
    client.ps.grenadeTimeLeft=1;assert(!G_IsTurretUsable(turret,owner));client.ps.grenadeTimeLeft=0;
    client.ps.groundEntityNum=1023;assert(!G_IsTurretUsable(turret,owner));
}
static void UseStances(void)
{
    for(int stance=0;stance<3;++stance) for(int previous=0;previous<3;++previous) {
        Reset();info.stance=stance;client.ps.pm_flags=previous==2 ? 3 : previous==1 ? 2 : 0;
        client.ps.eFlags=0x10300;info.flags=4;
        owner->r.currentOrigin[1]=7;owner->r.currentOrigin[2]=8;
        client.ps.viewangles[0]=-60;client.ps.viewangles[1]=100;
        turret->r.currentAngles[0]=10;turret->r.currentAngles[1]=20;
        turret_use(turret,owner,NULL);
        assert(turret->active==1 && owner->active==1 && turret->r.ownerNum==3);
        assert(client.ps.viewlocked==1 && client.ps.viewlocked_entNum==70);
        assert(info.prevStance==previous && info.flags==0x804);
        assert((client.ps.eFlags&0x300)==(stance==2 ? 0x100 : stance==1 ? 0x200 : 0x300));
        assert(client.ps.eFlags&0x10000);
        assert(!memcmp(info.userOrigin,owner->r.currentOrigin,sizeof(vec3_t)));
        assert(owner->s.otherEntityNum==70 && turret->s.otherEntityNum==3);
        assert(turret->s.angles2[0]==-20 && turret->s.angles2[1]==45 && turret->s.angles2[2]==0);
        assert(client.ps.viewAngleClampRange[0]==25 && client.ps.viewAngleClampBase[0]==15);
        assert(client.ps.viewAngleClampRange[1]==45 && client.ps.viewAngleClampBase[1]==20);
    }
}
static void ArmStop(int previous)
{
    info.prevStance=previous;info.flags=0x805;info.fireSndDelay=99;turret->s.loopSound=9;
    turret->r.ownerNum=3;turret->active=owner->active=1;
    client.ps.eFlags=0x10301;client.ps.viewlocked=2;client.ps.viewlocked_entNum=70;
    owner->s.otherEntityNum=70;turret->s.otherEntityNum=3;
}
static void CheckStopped(int previous)
{
    assert(eventCount==(previous!=-1));
    if(previous!=-1) assert(eventValue==(previous==2 ? 142 : previous==1 ? 141 : 140));
    assert(teleportCount==1 && info.prevStance==-1);
    assert(!turret->active && !owner->active && turret->r.ownerNum==1023);
    assert(client.ps.eFlags==0x10001 && client.ps.viewlocked==0 && client.ps.viewlocked_entNum==1023);
    assert(owner->s.otherEntityNum==0 && turret->s.otherEntityNum==3 && info.flags==5);
}
static void StopStances(void)
{
    for(int previous=-1;previous<=3;++previous) {Reset();ArmStop(previous);G_ClientStopUsingTurret(turret);CheckStopped(previous);}
}
static void Free(int owned)
{
    if(owned) ArmStop(1); else {turret->r.ownerNum=1023;turret->active=1;}
    G_FreeTurret(turret);
    assert(!turret->active && !turret->pTurretInfo && !info.inuse);
    if(owned) CheckStopped(1);else assert(!eventCount && !teleportCount);
}
static void InitTags(void)
{
    for(int missing=1;missing<=2;++missing) {
        Reset();missingTag=missing;info.dropPitch=77;turret_think_init(turret);
        assert(tagCount==missing && !traceCount && info.dropPitch==77);
        assert(turret->handler==14 && turret->nextthink==350);
    }
}
static void InitTraces(void)
{
    int cases[]={0,7,30,-1};
    for(int i=0;i<4;++i) {
        Reset();hitTrace=cases[i];info.dropPitch=77;turret_think_init(turret);
        assert(traceCount==(hitTrace<0 ? 31 : hitTrace+1) && transformCount==traceCount);
        assert(info.dropPitch==(hitTrace<0 ? 77 : -3.0f*hitTrace));
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);Reset();
    if(!strcmp(argv[1],"behind-boundary")) BehindBoundary();
    else if(!strcmp(argv[1],"behind-nonfinite")) BehindNonfinite();
    else if(!strcmp(argv[1],"usability")) Usability();
    else if(!strcmp(argv[1],"use-stances")) UseStances();
    else if(!strcmp(argv[1],"stop-stances")) StopStances();
    else if(!strcmp(argv[1],"free-owner")) Free(1);
    else if(!strcmp(argv[1],"free-unowned")) Free(0);
    else if(!strcmp(argv[1],"init-tags")) InitTags();
    else if(!strcmp(argv[1],"init-traces")) InitTraces();
    else abort();
    return 0;
}
