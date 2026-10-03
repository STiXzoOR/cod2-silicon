/* Native primitive creation/update/draw uses the real FX implementations. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern void FX_AddParticle(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddLine(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddTail(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddCylinder(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddEmitter(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddOrientedParticle(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddLight(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddFlash(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddCloud(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_UpdateAllBolt(void);
extern void FX_AddDecal(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddFxRunner(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FX_AddCameraShake(EffectPrimitive *, vec3_t *, const vec_t *, int, int);
extern void FxBoltFrame_Release(const FxBoltFrame *);
extern const orientation_t *FxBoltFrame_GetOrientation(const FxBoltFrame *);
extern const FxBoltFramePtr FxBoltFrame_Acquire(const FxBoltInfo *);
extern Bool FX_GetBoneOrientation(const FxBoltInfo *, orientation_t *);
extern void FX_UpdateAllNonBolt(void);
extern void FX_DrawAll(void);

FxHelper theFxHelpers[1];
FxHelper *theFxHelper = theFxHelpers;
Bool g_rendererExists = 1;
int g_effectVisArrayCount;
EffectVisInfo g_effectVisArray[1800];
int effectActiveCountBolt, effectActiveCountNonBolt;
int privateEffectActiveCountBolt, privateEffectActiveCountNonBolt;
int initialEffectActiveCountBolt, initialEffectActiveCountNonBolt;
int cullEffectCountNonBolt, cullEffectCountBolt;
int visibleEffectCountNonBolt, visibleEffectCountBolt;
int effectClusterCount, effectActiveCount, effectBlockSightCount;
static EffectCluster clusters[1800];
static Effect *boltList[1800], *nonBoltList[1800];
EffectCluster *effectClusters = clusters;
Effect **effectListBolt = boltList, **effectListNonBolt = nonBoltList;
int *clusterSort;
byte *__ZN11FxBoltFrame12g_mFrameListE;
FxBoltFrame *FxBoltFrame_g_mFrameList;
static dvar_t enabled, disabled;
static dvar_t *enabledPtr = &enabled, *disabledPtr = &disabled;
static int cameraValid = 1;
void *imp_theFxHelper = &theFxHelper;
void *imp_fx_camera_valid = &cameraValid;
void *imp_fx_cull = &enabledPtr, *imp_fx_sort = &disabledPtr;
void *imp_fx_draw = &enabledPtr, *imp_fx_enable = &enabledPtr;
void *imp_fx_debug = &disabledPtr;
void *imp_g_effectVisArrayCount = &g_effectVisArrayCount;
void *imp_g_effectVisArray = g_effectVisArray;
static vec3_t zero;
void *imp_vec3_origin = zero;
void *imp_colorBlue, *imp_colorGreen, *imp_colorRed;
static clientActive_t client;
static clientActive_t *clientPtr = &client;
void *imp_cl = &clientPtr;
static FxScheduler globalScheduler;
static FxScheduler *schedulerPtr = &globalScheduler;
void *imp_theFxScheduler = &schedulerPtr;
void *imp_fx_debugBolt, *imp_fxSchedulers;
void *imp_g_rendererExists = &g_rendererExists;
void *imp_effectActiveCountBolt = &effectActiveCountBolt;
void *imp_effectActiveCountNonBolt = &effectActiveCountNonBolt;
void *imp_effectListBolt = &effectListBolt;
void *imp_effectListNonBolt = &effectListNonBolt;
const dvar_t *fx_debugBolt = &disabled, *fx_visMinTraceDist = &disabled;
static Material material;
static EffectTemplate childEffect;
static DObjAnimMat bone = {.quat = {0,0,0,1}, .trans = {2,3,4}, .transWeight = 2};
void CG_GetDObjOrientation(int entity, orientation_t *out)
{ (void)entity; *out = (orientation_t){.origin = {10,20,30}, .axis = {{0,1,0},{-1,0,0},{0,0,1}}}; }
DObj *Com_GetClientDObj(int handle, int clientNum) { (void)handle; (void)clientNum; return (DObj *)&bone; }
int DObjNumBones(const DObj *dobj) { (void)dobj; return 1; }
void CG_DObjCalcBoneGeneric(int entity, int clientNum, int index) { (void)entity; (void)clientNum; assert(index == 0); }
DObjAnimMat *DObjGetRotTransArray(const DObj *dobj) { (void)dobj; return &bone; }
void CL_AddDebugLine(const vec_t *a, const vec_t *b, const vec_t *color, int depth, int duration, int server)
{ (void)a; (void)b; (void)color; (void)depth; (void)duration; (void)server; abort(); }
Bool FxHelper_CullSphere(const FxHelper *h, const float *p, float r, int n)
{ (void)h; (void)p; (void)r; (void)n; return 0; }
Bool FxHelper_CullCylinder(const FxHelper *h, const float *p, const float *q, float r0, float r1, int n)
{ (void)h; (void)p; (void)q; (void)r0; (void)r1; (void)n; return 0; }
/* Archiving is outside this lifecycle fixture; accidental entry must fail. */
void FxArchive_ReadData(const FxArchive *a, void *p, int n) { (void)a; (void)p; (void)n; abort(); }
void FxArchive_WriteData(const FxArchive *a, const void *p, int n) { (void)a; (void)p; (void)n; abort(); }
void FxArchive_ArchiveChannelInstance(const FxArchive *a, FxChannelInstance *p) { (void)a; (void)p; abort(); }
void FxArchive_ArchiveEffect(const FxArchive *a, const EffectTemplate **p) { (void)a; (void)p; abort(); }
void FxArchive_ArchiveFxGfxEntity(const FxArchive *a, FxGfxEntity *p) { (void)a; (void)p; abort(); }
void FxArchive_ArchiveMaterial(const FxArchive *a, Material **p) { (void)a; (void)p; abort(); }
void FxArchive_ArchiveModel(const FxArchive *a, XModel **p) { (void)a; (void)p; abort(); }
static int secondaryEffects, decals, shakes;
static vec3_t secondaryOrigin;
void FxScheduler_PlayEffect(void *scheduler, void *effect, vec_t *origin, vec3_t *axis, void *bolt)
{
    (void)scheduler; (void)origin; (void)bolt;
    assert(effect == &childEffect);
    memcpy(secondaryOrigin, origin, sizeof(secondaryOrigin));
    if (axis) for (int i = 0; i < 3; ++i) {
        float length = axis[i][0]*axis[i][0] + axis[i][1]*axis[i][1] + axis[i][2]*axis[i][2];
        assert(fabsf(length - 1.0f) < .00001f);
    }
    ++secondaryEffects;
}
void FxScheduler_CreateDecalEffect(void *s, void *p, vec_t *o, vec3_t *a)
{ (void)s; (void)p; (void)o; (void)a; ++decals; }
void FxHelper_CameraShake(const FxHelper *h, vec_t *p, float strength, int radius, int time)
{ (void)h; (void)p; (void)strength; (void)radius; (void)time; ++shakes; }
#include "fx_orientation_source.h"
extern void AxisCopy(vec3_t *, vec3_t *);
extern float flrand(float, float);
extern void RotatePointAroundVector(vec_t *, const vec_t *, const vec_t *, float);
extern void Vec3Cross(const vec_t *, const vec_t *, vec_t *);
#include "fx_creation_source.h"

static int draws, lights;
void FxHelper_AddLightToScene(const FxHelper *h, float *origin, float radius, float r, float g, float b)
{ (void)h; (void)origin; assert(radius == 12 && r == .8f && g == .4f && b == .2f); ++lights; }
static GfxEntity submitted;
static Bool simulateImpact;
void * __Znam(size_t size) { return calloc(1, size); }
void __ZdaPv(void *p) { free(p); }
void FxHelper_FxHelper(const FxHelper *p) { (void)p; }
Bool FxHelper_IsMaterialRefractive(const FxHelper *p, MaterialHandle m) { (void)p; (void)m; return 0; }
int FxHelper_GetMaterialSubimageCount(const FxHelper *p, Material *m) { (void)p; (void)m; return 1; }
void *MediaHandles_GetHandle(const MediaHandles *m) { (void)m; return &material; }
EffectTemplate *MediaHandles_GetEffect(const MediaHandles *m) { (void)m; return m->mMediaList.size ? &childEffect : NULL; }
float FxRange_GetVal(const FxRange *p) { return (p->mMin + p->mMax) * 0.5f; }
void FxHelper_AddFxToScene(const FxHelper *p, GfxEntity *ent, const XModel *model)
{ (void)p; (void)model; submitted = *ent; ++draws; }
void FX_Print(const char *fmt, ...) { (void)fmt; }
void FxHelper_Trace(const FxHelper *p, trace_t *tr, vec_t *start, const vec_t *mins, const vec_t *maxs, vec_t *end, int skip, int flags)
{ (void)p; (void)start; (void)mins; (void)maxs; (void)end; (void)skip; (void)flags; memset(tr, 0, sizeof(*tr)); tr->fraction = simulateImpact ? .5f : 1.0f; tr->normal[2] = 1.0f; }

static struct { int dimensions, keyCount; float keys[4]; } scalar = {1, 2, {0, 1, 1, 1}};
static struct { int dimensions, keyCount; float keys[8]; } color = {3, 2, {0, .8f, .4f, .2f, 1, .8f, .4f, .2f}};
static struct { int dimensions, keyCount; float keys[4]; } velocity = {1, 2, {0, 0, 1, 0}};
int main(void)
{
    PrimitiveTemplate pt = {0};
    EffectTemplate fx = {0};
    vec3_t axes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    vec3_t origin = {17, 29, 41};
    enabled.current.enabled = 1;
    theFxHelper->mTime = 1000;
    pt.mLife.mMin = pt.mLife.mMax = 200;
    pt.mAttributeFlags = 0x80;
    for (int i = 0; i < 24; ++i) {
        pt.mFxChannels[i].curve = (FxCurve *)(i < 2 ? (void *)&color : i >= 12 ? (void *)&velocity : (void *)&scalar);
        pt.mFxChannels[i].scaleRange.mMin = pt.mFxChannels[i].scaleRange.mMax = i == 4 ? 12 : 1;
    }
    FxScheduler scheduler = {0};
    pt.mType = 1;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    assert(effectActiveCountNonBolt == 1);
    assert(nonBoltList[0]->origin[0] == origin[0]);
    FX_UpdateAllNonBolt();
    FX_DrawAll();
    assert(draws == 1);
    assert(submitted.customMaterial == &material);
    assert(submitted.radius[0] == 12 && submitted.radius[1] == 12);
    assert(submitted.origin[0] == 17 && submitted.origin[1] == 29 && submitted.origin[2] == 41);
    assert(submitted.materialRGBA[0] == 204 && submitted.materialRGBA[1] == 102 && submitted.materialRGBA[2] == 51);
    assert(submitted.materialRGBA[3] == 255);
    theFxHelper->mTime = 1201;
    FX_UpdateAllNonBolt();
    assert(effectActiveCount == 0 && effectClusterCount == 0);
    int renderTypes[] = {8, 8, 9, 1, 7, -1, 4, 6};
    int primitiveTypes[] = {2, 3, 4, 5, 7, 9, 11, 12};
    for (unsigned i = 0; i < sizeof(primitiveTypes)/sizeof(primitiveTypes[0]); ++i) {
        draws = lights = 0;
        cullEffectCountNonBolt = visibleEffectCountNonBolt = 0;
        pt.mAttributeFlags = 0x90; /* alpha and emitter model draw */
        theFxHelper->mTime = 2000;
        pt.mType = primitiveTypes[i];
        FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
        assert(effectActiveCountNonBolt == 1);
        FX_UpdateAllNonBolt();
        FX_DrawAll();
        assert(renderTypes[i] == -1 ? lights == 1 : draws == 1);
        if (renderTypes[i] != -1) {
            assert(submitted.reType == renderTypes[i]);
            assert(submitted.customMaterial == &material || renderTypes[i] == 1);
            if (renderTypes[i] == 7) assert(submitted.axis[0][0] == 1);
            if (renderTypes[i] == 6) {
                Cloud *cloud = (Cloud *)nonBoltList[0];
                assert(fabsf(cloud->randomDirection[0]*cloud->randomDirection[0]
                    + cloud->randomDirection[1]*cloud->randomDirection[1]
                    + cloud->randomDirection[2]*cloud->randomDirection[2] - 1) < .00001f);
            }
        }
        theFxHelper->mTime = 2201;
        FX_UpdateAllNonBolt();
        assert(effectActiveCount == 0 && effectClusterCount == 0);
    }
    FxBoltInfo boltInfo = {1, 0};
    orientation_t result;
    assert(FX_GetBoneOrientation(&boltInfo, &result));
    assert(result.origin[0] == 7 && result.origin[1] == 22 && result.origin[2] == 34);
    boltInfo.boneIndex = -1;
    assert(FX_GetBoneOrientation(&boltInfo, &result));
    assert(result.origin[0] == 10 && result.origin[1] == 20 && result.origin[2] == 30);
    client.skelTimeStamp = 3000;
    theFxHelper->mTime = 3000;
    theFxHelper->mFrameTime = 16;
    pt.mType = 1;
    pt.mAttributeFlags = 0x82;
    cullEffectCountBolt = visibleEffectCountBolt = 0;
    cullEffectCountNonBolt = visibleEffectCountNonBolt = 0;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, &boltInfo, origin, (MediaHandles *(*)[4])axes, 0, 0);
    assert(effectActiveCountBolt == 1 && __ZN11FxBoltFrame12g_mFrameListE);
    assert(((FxBoltFrame *)__ZN11FxBoltFrame12g_mFrameListE)->refCount == 1);
    FX_UpdateAllBolt();
    draws = 0;
    FX_DrawAll();
    assert(draws == 1);
    assert(submitted.origin[0] == 17 && submitted.origin[1] == 29 && submitted.origin[2] == 41);
    theFxHelper->mTime = 3201;
    FX_UpdateAllBolt();
    assert(!effectActiveCount && !effectClusterCount && !__ZN11FxBoltFrame12g_mFrameListE);

    /* The local oriented normal must rotate back into the caller's world axis. */
    theFxHelper->mTime = 3500;
    pt.mType = 7;
    cullEffectCountBolt = visibleEffectCountBolt = 0;
    cullEffectCountNonBolt = visibleEffectCountNonBolt = 0;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, &boltInfo, origin, (MediaHandles *(*)[4])axes, 0, 0);
    FX_UpdateAllBolt();
    draws = 0;
    FX_DrawAll();
    assert(draws == 1 && submitted.axis[0][0] == 1 && submitted.axis[0][1] == 0);
    theFxHelper->mTime = 3701;
    FX_UpdateAllBolt();
    assert(!effectActiveCount && !__ZN11FxBoltFrame12g_mFrameListE);

    theFxHelper->mTime = 4000;
    pt.mType = 1;
    pt.mAttributeFlags = 0x80 | 0x20 | 0x800 | 0x400;
    pt.mImpactFxHandles.mMediaList.size = 1;
    simulateImpact = 1;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    FX_UpdateAllNonBolt();
    assert(secondaryEffects == 1 && effectActiveCount == 0);
    simulateImpact = 0;
    pt.mType = 6;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    assert(decals == 1);
    pt.mType = 10;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    assert(shakes == 1);
    pt.mType = 8;
    pt.mPlayFxHandles.mMediaList.size = 1;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    assert(secondaryEffects == 2);

    /* An emitter must submit its child effect at a world position. */
    pt.mType = 5;
    pt.mAttributeFlags = 0x80 | 0x100;
    pt.mEmitterFxHandles.mMediaList.size = 1;
    theFxHelper->mTime = 5000;
    cullEffectCountNonBolt = visibleEffectCountNonBolt = 0;
    FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
    theFxHelper->mTime = 5016;
    FX_UpdateAllNonBolt();
    assert(secondaryEffects > 2);
    assert(secondaryOrigin[0] == 17 && secondaryOrigin[1] == 29 && secondaryOrigin[2] == 41);
    theFxHelper->mTime = 5201;
    FX_UpdateAllNonBolt();
    assert(!effectActiveCount && !effectClusterCount);
    puts("native FX: nine primitive lifecycles, scheduler dispatch, bolting and impact axes passed");
    return 0;
}
