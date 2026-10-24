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
extern void FX_DrawScheduledEffects(void);
extern void FX_UpdateScheduledEffectsNonBolt(void);
extern void FX_UpdateScheduledEffectsBolt(void);
extern void FX_AddScheduledEffects(const vec_t *, const vec_t *);

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
void *imp_fx_freeze = &disabledPtr, *imp_fx_count = &disabledPtr;
void *imp_colorYellow;
refexport_t re;
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
static int captureEmissions, emissionCount, pauseEmissionClock;
static struct {
    vec3_t origin;
    vec3_t axis[3];
    const FxBoltInfo *bolt;
    int hasAxis;
} emissions[256];
void FxScheduler_PlayEffect(void *scheduler, void *effect, vec_t *origin, vec3_t *axis, void *bolt)
{
    (void)scheduler; (void)origin; (void)bolt;
    assert(effect == &childEffect);
    memcpy(secondaryOrigin, origin, sizeof(secondaryOrigin));
    if (axis) for (int i = 0; i < 3; ++i) {
        float length = axis[i][0]*axis[i][0] + axis[i][1]*axis[i][1] + axis[i][2]*axis[i][2];
        assert(fabsf(length - 1.0f) < .00001f);
    }
    if (captureEmissions) {
        assert(emissionCount < 256);
        memcpy(emissions[emissionCount].origin, origin, sizeof(vec3_t));
        emissions[emissionCount].hasAxis = axis != NULL;
        if (axis)
            memcpy(emissions[emissionCount].axis, axis, sizeof(vec3_t) * 3);
        emissions[emissionCount].bolt = bolt;
        ++emissionCount;
        /* Observe one overflowing correction without repeating the original zero-progress loop. */
        if (pauseEmissionClock)
            theFxHelper->mTime = (-2147483647 - 1);
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
int FxHelper_GetSeed(const FxHelper *h) { (void)h; return 17; }
void Rand_Init(int seed);
void FxHelper_SetIgnorePrecacheErrors(const FxHelper *h, int ignore)
{ (void)h; (void)ignore; abort(); }
EffectTemplate *FX_RegisterEffect(const char *name) { (void)name; abort(); }
static int frustumCalls;
static Bool frustumResult;
Bool FxHelper_CullSpherePreviousFrame(const FxHelper *h, const vec_t *p, float r)
{ (void)h; (void)p; (void)r; ++frustumCalls; return frustumResult; }

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
extern const vec_t Vec3DistanceSq(const vec_t *, const vec_t *);
#include "fx_play_source.h"

static struct { int dimensions, keyCount; float keys[4]; } scalar = {1, 2, {0, 1, 1, 1}};
static struct { int dimensions, keyCount; float keys[8]; } color = {3, 2, {0, .8f, .4f, .2f, 1, .8f, .4f, .2f}};
static struct { int dimensions, keyCount; float keys[4]; } velocity = {1, 2, {0, 0, 1, 0}};
static int spawnFailures;
#define SPAWN_CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL spawn line %d: %s\n", __LINE__, #condition); ++spawnFailures; } } while (0)

static void CheckSpawnCase(PrimitiveTemplate *pt, float distance, int delay,
                           int accepted, int expectedCullCalls)
{
    EffectTemplate fx = {.mPrimitiveCount = 1};
    vec3_t origin = {distance, 0, 0};
    vec3_t axis[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    fx.mPrimitives[0] = pt;
    pt->mSpawnDelay.mMin = pt->mSpawnDelay.mMax = (float)delay;
    globalScheduler = (FxScheduler){0};
    cullEffectCountNonBolt = visibleEffectCountNonBolt = 0;
    frustumCalls = 0;
    theFxHelper->mTime += 1000;
    int start = theFxHelper->mTime;
    FxScheduler_PlayEffectUnderTest(&globalScheduler, &fx, origin,
                                   (MediaHandles *(*)[4])axis, NULL);
    SPAWN_CHECK(frustumCalls == expectedCullCalls);
    SPAWN_CHECK(effectActiveCountNonBolt == (delay ? 0 : accepted));
    SPAWN_CHECK(globalScheduler.mScheduledCount == (delay ? accepted : 0));
    if (delay) {
        theFxHelper->mTime = start + delay;
        FX_AddScheduledEffects(NULL, NULL);
        SPAWN_CHECK(effectActiveCountNonBolt == accepted);
        SPAWN_CHECK(!globalScheduler.mScheduledCount && !globalScheduler.mScheduledHead);
    }
    theFxHelper->mTime = start + delay + 201;
    FX_UpdateAllNonBolt();
    assert(!effectActiveCount && !effectClusterCount);
}

static void CheckSpawnFilters(void)
{
    PrimitiveTemplate pt = {.mType = 1, .mAttributeFlags = 0x80,
                           .mLife = {200, 200}, .mSpawnCount = {1, 1}};
    for (int i = 0; i < 24; ++i) {
        pt.mFxChannels[i].curve = (FxCurve *)(i < 2 ? (void *)&color : i >= 12 ? (void *)&velocity : (void *)&scalar);
        pt.mFxChannels[i].scaleRange.mMin = pt.mFxChannels[i].scaleRange.mMax = i == 4 ? 12 : 1;
    }
    memset(theFxHelper->mCamera.vieworg, 0, sizeof(vec3_t));
    const float distances[] = {5, 10, 15, 20, 25};
    for (int delayed = 0; delayed < 2; ++delayed) {
        pt.mSpawnRange.mMin = 10;
        pt.mSpawnRange.mMax = 20;
        pt.mSpawnFlags = 0;
        for (int zoom = 1; zoom <= 2; ++zoom) {
            theFxHelper->adsZoomFactor = (float)zoom;
            for (int i = 0; i < 5; ++i)
                CheckSpawnCase(&pt, distances[i] * zoom, delayed ? 50 : 0,
                               i >= 1 && i <= 3, 0);
        }
        pt.mSpawnRange.mMin = pt.mSpawnRange.mMax = 0;
        const int flags[] = {0, 4, 0x400};
        for (int cull = 0; cull <= 1; ++cull) {
            frustumResult = cull;
            for (int i = 0; i < 3; ++i) {
                pt.mSpawnFlags = flags[i];
                CheckSpawnCase(&pt, 10, delayed ? 50 : 0,
                               !(flags[i] == 0x400 && cull), flags[i] == 0x400);
            }
        }
    }
    frustumResult = 0;
}

extern void Particle_CreateChannelInstances(const Particle *, const PrimitiveTemplate *);
extern void Particle_CalcVelocityValue(const Particle *, float, vec_t *, const orientation_t *);
extern void Particle_CalcVelocity2Value(const Particle *, float, vec_t *, const orientation_t *);
extern void Particle_IntegrateVelocity2(const Particle *, float, vec_t *);
extern void Particle_ApplyImpact(const Particle *, const orientation_t *, float, const vec_t *, float, const vec_t *);

static int physicsFailures;
#define PHYSICS_CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL physics line %d: %s\n", __LINE__, #condition); ++physicsFailures; } } while (0)

static void CheckParticlePhysics(void)
{
    static struct { int dimensions, keyCount; float keys[4]; } random = {1, 2, {0, 3, 1, 3}};
    PrimitiveTemplate pt = {0};
    Particle particle = {0};
    orientation_t bolt = {.axis = {{0,1,0}, {-1,0,0}, {0,0,1}}};
    for (int i = 0; i < 24; ++i) {
        pt.mFxChannels[i].curve = (FxCurve *)&scalar;
        pt.mFxChannels[i].scaleRange.mMin = pt.mFxChannels[i].scaleRange.mMax = 0;
    }
    for (int channel = 0; channel < 2; ++channel) {
        for (int axis = 0; axis < 3; ++axis) {
            int index = 12 + channel * 6 + axis;
            pt.mFxChannels[index].scaleRange.mMin = pt.mFxChannels[index].scaleRange.mMax = 2 + channel * 3 + axis;
            pt.mFxChannels[index + 3].curve = (FxCurve *)&random;
        }
    }
    Particle_CreateChannelInstances(&particle, &pt);
    memcpy(particle.displayAxis, bolt.axis, sizeof(particle.displayAxis));
    particle.velocityWeightX = particle.velocity2WeightX = .25f;
    particle.velocityWeightY = particle.velocity2WeightY = .5f;
    particle.velocityWeightZ = particle.velocity2WeightZ = .75f;
    particle.base.mTimeEnd = 2000;
    for (int flags = 0; flags < 16; ++flags) {
        particle.base.mFlags = flags << 19;
        for (int bolted = 0; bolted < 2; ++bolted) {
            for (int channel = 0; channel < 2; ++channel) {
                vec3_t actual, value, expected;
                int randomFlag = channel ? 0x100000 : 0x80000;
                int worldFlag = channel ? 0x400000 : 0x200000;
                for (int axis = 0; axis < 3; ++axis)
                    value[axis] = (2 + channel * 3 + axis) *
                        ((particle.base.mFlags & randomFlag) ? 1.5f + .5f * axis : 1.0f);
                if ((particle.base.mFlags & worldFlag) && bolted) {
                    expected[0] = value[1]; expected[1] = -value[0]; expected[2] = value[2];
                } else if (!(particle.base.mFlags & worldFlag) && !bolted) {
                    expected[0] = -value[1]; expected[1] = value[0]; expected[2] = value[2];
                } else {
                    memcpy(expected, value, sizeof(expected));
                }
                if (channel)
                    Particle_CalcVelocity2Value(&particle, .5f, actual, bolted ? &bolt : NULL);
                else
                    Particle_CalcVelocityValue(&particle, .5f, actual, bolted ? &bolt : NULL);
                for (int axis = 0; axis < 3; ++axis)
                    PHYSICS_CHECK(actual[axis] == expected[axis]);
            }
        }
        /* Mac integration intentionally uses the first random-velocity flag. */
        vec3_t actual, value, expected;
        for (int axis = 0; axis < 3; ++axis)
            value[axis] = (5 + axis) * ((particle.base.mFlags & 0x80000) ? 1.5f + .5f * axis : 1.0f);
        if (particle.base.mFlags & 0x400000) {
            memcpy(expected, value, sizeof(expected));
        } else {
            expected[0] = -value[1]; expected[1] = value[0]; expected[2] = value[2];
        }
        Particle_IntegrateVelocity2(&particle, .5f, actual);
        for (int axis = 0; axis < 3; ++axis)
            PHYSICS_CHECK(fabsf(actual[axis] - expected[axis]) < .00001f);
    }
    const struct { float speed, normalZ, fraction; int stop; } impacts[] = {
        {1, 1, .5f, 1}, {0, 1, .5f, 1}, {4, 1, .5f, 0},
        {5, 1, 0, 1}, {1, 0, .5f, 0}, {1, -1, .5f, 0},
        {5, 0, 0, 1}, {5, -1, .5f, 0}
    };
    for (unsigned i = 0; i < sizeof(impacts) / sizeof(impacts[0]); ++i) {
        vec3_t velocity = {impacts[i].speed, 0, 0};
        vec3_t normal = {0, 0, impacts[i].normalZ};
        particle.base.mFlags = 0x820;
        Particle_ApplyImpact(&particle, NULL, .5f, velocity, impacts[i].fraction, normal);
        PHYSICS_CHECK(particle.base.mFlags == (impacts[i].stop ? 0x1000000 : 0x820));
    }
}

extern void Emitter_UpdateEmitFx(const Emitter *, vec_t *, const orientation_t *);
extern float crandom(void);
static int emitterFailures;
#define EMITTER_CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL emitter line %d: %s\n", __LINE__, #condition); ++emitterFailures; } } while (0)

static void CheckEmitterMotion(void)
{
    PrimitiveTemplate pt = {0};
    for (int i = 0; i < 24; ++i)
        pt.mFxChannels[i].curve = (FxCurve *)&scalar;
    const struct {
        float speed, step, bindOffset;
        int priorTime, now, bolted, count, finalTime;
        float firstX, finalX;
        int pauseClock;
    } cases[] = {
        {100, 1, 0, 1000, 1001, 0, 1, 1010, 4.2f, 4},
        {0, 0, 0, 1012, 1013, 0, 1, 1024, 3, 3},
        {100, 1, 0, 1000, 1001, 1, 1, 1010, 4.2f, 4},
        {100, 10, 0, 1000, 1013, 0, 0, 1000, 0, 3},
        {100, 1.1999998f, 0, 1000, 1001, 0, 1, 1012, 4.2f, 4.2f},
        {100, 1, 0, 1000, 1030, 0, 3, 1030, 4.2f, 6},
        {100, 1, 2, 1000, 1025, 0, 1, 1024, 6.6f, 5.4f},
        {.01f, 10, 1200, 1000, 1001, 1, 1, 1000, 3.00012f, 3, 1}
    };
    for (unsigned test = 0; test < sizeof(cases) / sizeof(cases[0]); ++test) {
        Emitter emitter = {0};
        Particle *particle = &emitter.base;
        Effect *effect = &particle->base;
        FxBoltFrame bolt = {.mBolt = {7, 3}};
        orientation_t orientation = {.origin = {10,20,30}, .axis = {{0,1,0},{-1,0,0},{0,0,1}}};
        vec3_t bindVelocity = {0, 20, 0};
        pt.mFxChannels[12].scaleRange.mMin = pt.mFxChannels[12].scaleRange.mMax = cases[test].speed;
        Particle_CreateChannelInstances(particle, &pt);
        for (int i = 0; i < 3; ++i)
            particle->displayAxis[i][i] = 1;
        memcpy(effect->mRefEnt.axis, orientation.axis, sizeof(orientation.axis));
        effect->mFlags = 0x100;
        effect->mTimeStart = 1000;
        effect->mTimeEnd = 3000;
        effect->mBolt.value = cases[test].bolted ? &bolt : NULL;
        emitter.emitFx = &childEffect;
        emitter.emitPos[0] = 3; emitter.emitPos[1] = 4; emitter.emitPos[2] = 5;
        emitter.initialVel[0] = 9; emitter.initialVel[1] = 8; emitter.initialVel[2] = 7;
        emitter.velocityDelta[0] = cases[test].bindOffset;
        emitter.emitLastTime = cases[test].priorTime;
        emitter.emitStep = cases[test].step;
        /* Keep the repeated-spacing case deterministic; other cases test jitter. */
        emitter.spawnVariance = test == 5 ? 1 : 10;
        float variance = test == 5 ? 0 : .25f;
        memcpy(emitter._tail, &variance, sizeof(variance));
        srand(17);
        float expectedStep = emitter.emitStep;
        for (int i = 0; i < cases[test].count; ++i)
            expectedStep = emitter.spawnVariance + crandom() * variance;
        int expectedNextRandom = rand();
        srand(17);
        theFxHelper->mTime = cases[test].now;
        theFxHelper->mFrameTime = 16;
        emissionCount = 0;
        captureEmissions = 1;
        pauseEmissionClock = cases[test].pauseClock;
        Emitter_UpdateEmitFx(&emitter, bindVelocity, cases[test].bolted ? &orientation : NULL);
        pauseEmissionClock = 0;
        captureEmissions = 0;
        EMITTER_CHECK(emissionCount == cases[test].count);
        EMITTER_CHECK(emitter.emitLastTime == cases[test].finalTime);
        EMITTER_CHECK(fabsf(emitter.emitPos[0] - cases[test].finalX) < .00001f);
        EMITTER_CHECK(emitter.emitPos[1] == 4 && emitter.emitPos[2] == 5);
        EMITTER_CHECK(emitter.emitStep == expectedStep);
        EMITTER_CHECK(rand() == expectedNextRandom);
        EMITTER_CHECK(emitter.velocityDelta[0] == cases[test].bindOffset);
        EMITTER_CHECK(fabsf(emitter.velocityDelta[1] - (cases[test].bolted && !cases[test].pauseClock ? .2f : 0)) < .00001f);
        EMITTER_CHECK(emitter.velocityDelta[2] == 0);
        if (!cases[test].count) {
            EMITTER_CHECK(emitter.initialVel[0] == 9 && emitter.initialVel[1] == 8 && emitter.initialVel[2] == 7);
            continue;
        }
        EMITTER_CHECK(emitter.initialVel[0] == cases[test].speed);
        EMITTER_CHECK(emitter.initialVel[1] == 0 && emitter.initialVel[2] == 0);
        if (cases[test].bolted) {
            EMITTER_CHECK(emissions[0].bolt == &bolt.mBolt && !emissions[0].hasAxis);
            EMITTER_CHECK(fabsf(emissions[0].origin[0] - 5.76f) < .00001f);
            EMITTER_CHECK(fabsf(emissions[0].origin[1] - (20 + cases[test].firstX)) < .00001f);
            EMITTER_CHECK(emissions[0].origin[2] == 35);
        } else {
            EMITTER_CHECK(!emissions[0].bolt && emissions[0].hasAxis);
            EMITTER_CHECK(fabsf(emissions[0].origin[0] - cases[test].firstX) < .00001f);
            EMITTER_CHECK(emissions[0].origin[1] == 4 && emissions[0].origin[2] == 5);
            if (cases[test].speed) {
                EMITTER_CHECK(emissions[0].axis[0][0] == 1 && emissions[0].axis[0][1] == 0 && emissions[0].axis[0][2] == 0);
            } else {
                EMITTER_CHECK(!memcmp(emissions[0].axis, orientation.axis, sizeof(orientation.axis)));
            }
        }
        if (test == 5 && emissionCount == 3) {
            EMITTER_CHECK(fabsf(emissions[1].origin[0] - 5.2f) < .00001f);
            EMITTER_CHECK(fabsf(emissions[2].origin[0] - 6.2f) < .00001f);
        }
    }
}

static void CheckEmitterUpdate(void)
{
    PrimitiveTemplate pt = {.mType = 5};
    EffectTemplate fx = {0};
    FxScheduler scheduler = {0};
    FxBoltInfo boltInfo = {1, -1};
    vec3_t axes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    vec3_t origin = {3,4,5};
    pt.mLife.mMin = pt.mLife.mMax = 1000;
    for (int i = 0; i < 24; ++i) {
        pt.mFxChannels[i].curve = (FxCurve *)(i < 2 ? (void *)&color : (void *)&scalar);
        pt.mFxChannels[i].scaleRange.mMin = pt.mFxChannels[i].scaleRange.mMax = i < 12 ? 3 : 0;
    }
    pt.mAngle2.mMin = pt.mAngle2.mMax = 90;
    for (int bolted = 0; bolted < 2; ++bolted) for (int model = 0; model < 2; ++model) {
        theFxHelper->mTime = client.skelTimeStamp = 9000;
        theFxHelper->mFrameTime = 100;
        pt.mAttributeFlags = 0x80 | (bolted ? 2 : 0) | (model ? 0x10 : 0);
        /* A zero-size model still updates its orientation without marking physics stopped. */
        pt.mFxChannels[4].scaleRange.mMin = pt.mFxChannels[4].scaleRange.mMax = model ? 0 : 3;
        FxScheduler_CreateEffect(&scheduler, &fx, &pt, bolted ? &boltInfo : NULL,
                                 origin, (MediaHandles *(*)[4])axes, 0, 0);
        Effect *effect = bolted ? boltList[0] : nonBoltList[0];
        memset(effect->mRefEnt.materialRGBA, 23, 4);
        if (bolted) FX_UpdateAllBolt(); else FX_UpdateAllNonBolt();
        const float expected[3][3] = {
            {bolted && model ? -1 : 0, bolted && model ? 0 : 1, 0},
            {bolted && model ? 0 : -1, bolted && model ? -1 : 0, 0},
            {0, 0, 1}
        };
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
            EMITTER_CHECK(fabsf(effect->mRefEnt.axis[i][j] - expected[i][j]) < .00001f);
        EMITTER_CHECK(!(effect->mFlags & 0x1000000));
        for (int i = 0; i < 4; ++i) EMITTER_CHECK(effect->mRefEnt.materialRGBA[i] == 23);
        if (model) {
            EMITTER_CHECK(effect->mRefEnt.origin[0] == 3 && effect->mRefEnt.origin[1] == 4 && effect->mRefEnt.origin[2] == 5);
            EMITTER_CHECK(effect->mRefEnt.scale == 0);
        }
        theFxHelper->mTime = 10001;
        if (bolted) FX_UpdateAllBolt(); else FX_UpdateAllNonBolt();
        assert(!effectActiveCount && !effectClusterCount);
    }
    pt.mAttributeFlags = 0x80;
    pt.mAngle2.mMin = pt.mAngle2.mMax = 0;
    pt.mAngle2Delta.mMin = pt.mAngle2Delta.mMax = 10;
    for (int moving = 0; moving < 2; ++moving) {
        pt.mFxChannels[12].scaleRange.mMin = pt.mFxChannels[12].scaleRange.mMax = moving ? 10 : 0;
        theFxHelper->mTime = 11000;
        theFxHelper->mFrameTime = 100;
        FxScheduler_CreateEffect(&scheduler, &fx, &pt, NULL, origin, (MediaHandles *(*)[4])axes, 0, 0);
        Emitter *emitter = (Emitter *)nonBoltList[0];
        theFxHelper->mTime = 11100;
        FX_UpdateAllNonBolt();
        EMITTER_CHECK(fabsf(*(float *)&emitter->_gap3[0] - (moving ? 10 : 7)) < .00001f);
        EMITTER_CHECK(fabsf(*(float *)&emitter->_gap2[0] - (moving ? 10 : 7)) < .00001f);
        EMITTER_CHECK(fabsf(emitter->base.base.mRefEnt.axis[0][0] - (moving ? .98480775f : .99254615f)) < .00001f);
        EMITTER_CHECK(fabsf(emitter->base.base.mRefEnt.axis[0][1] - (moving ? .17364818f : .12186934f)) < .00001f);
        theFxHelper->mTime = 12001;
        FX_UpdateAllNonBolt();
        assert(!effectActiveCount && !effectClusterCount);
    }
    pt.mAngle2Delta.mMin = pt.mAngle2Delta.mMax = 0;
    pt.mFxChannels[12].scaleRange.mMin = pt.mFxChannels[12].scaleRange.mMax = 0;
    pt.mEmitterFxHandles.mMediaList.size = 1;
    for (int model = 0; model < 2; ++model) {
        pt.mAttributeFlags = 0x182 | (model ? 0x10 : 0);
        theFxHelper->mTime = client.skelTimeStamp = 13000;
        FxScheduler_CreateEffect(&scheduler, &fx, &pt, &boltInfo, origin, (MediaHandles *(*)[4])axes, 0, 0);
        Emitter *emitter = (Emitter *)boltList[0];
        /* A changed cached bone position represents motion since this emitter was created. */
        emitter->base.base.mBolt.value->orientation.origin[0] += 1000;
        theFxHelper->mTime = 13001;
        captureEmissions = 1;
        emissionCount = 0;
        FX_UpdateAllBolt();
        captureEmissions = 0;
        EMITTER_CHECK(emissionCount == 1);
        EMITTER_CHECK(!emissions[0].hasAxis && emissions[0].bolt == &emitter->base.base.mBolt.value->mBolt);
        EMITTER_CHECK(fabsf(emissions[0].origin[0] - 1003) < .00001f);
        EMITTER_CHECK(fabsf(emissions[0].origin[1] - (model ? 4.012f : 4)) < .00001f);
        EMITTER_CHECK(fabsf(emissions[0].origin[2] - 5) < .00001f);
        EMITTER_CHECK(fabsf(emitter->velocityDelta[0] - (model ? 10.012f : 10)) < .00001f);
        theFxHelper->mTime = 14001;
        FX_UpdateAllBolt();
        assert(!effectActiveCount && !effectClusterCount);
    }

}

extern void Tail_CreateChannelInstances(const Tail *, const PrimitiveTemplate *);
extern Bool Tail_Update(const Tail *);
extern Bool Particle_Update(const Particle *);
static int motionFailures;
#define MOTION_CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL motion line %d: %s\n", __LINE__, #condition); ++motionFailures; } } while (0)

extern Bool OrientedParticle_Update(const OrientedParticle *);
extern Bool Line_Update(const Line *);
extern Bool Cylinder_Update(const Cylinder *);
extern Bool Cloud_Update(const Cloud *);
extern void Cloud_CreateChannelInstances(const Cloud *, const PrimitiveTemplate *);
static void CheckTailAndBoltMotion(void)
{
    PrimitiveTemplate pt = {0};
    for (int i = 0; i < 24; ++i) {
        pt.mFxChannels[i].curve = (FxCurve *)(i < 2 ? (void *)&color : (void *)&scalar);
        pt.mFxChannels[i].scaleRange.mMin = pt.mFxChannels[i].scaleRange.mMax = i < 12 ? 1 : 0;
    }
    pt.mFxChannels[8].scaleRange.mMin = pt.mFxChannels[8].scaleRange.mMax = 4;
    for (int bolted = 0; bolted < 2; ++bolted) {
        Tail tail = {0};
        Effect *effect = &tail.base.base;
        FxBoltFrame bolt = {.mBolt = {1, -1}, .cachedServerTime = 100,
            .orientation = {.origin = {100,200,300}, .axis = {{0,1,0},{-1,0,0},{0,0,1}}}};
        Tail_CreateChannelInstances(&tail, &pt);
        for (int i = 0; i < 3; ++i) {
            tail.base.displayAxis[i][i] = 1;
            effect->origin[i] = (i + 1) * 10;
            tail.endpoint[i] = (i + 1) * 100;
        }
        effect->mFlags = 0x80;
        effect->mTimeStart = 1000;
        effect->mTimeEnd = 3000;
        effect->mBolt.value = bolted ? &bolt : NULL;
        tail.base.velocityXChannelInstance.scale = 10;
        client.skelTimeStamp = 100;
        theFxHelper->mTime = 1100;
        theFxHelper->mFrameTime = 100;
        MOTION_CHECK(Tail_Update(&tail));
        MOTION_CHECK(effect->origin[0] == 11 && effect->origin[1] == 20 && effect->origin[2] == 30);
        MOTION_CHECK(tail.endpoint[0] == 10 && tail.endpoint[1] == 20 && tail.endpoint[2] == 30);
        const vec3_t firstOrigin[2] = {{11,20,30}, {80,211,330}};
        const vec3_t firstEnd[2] = {{7,20,30}, {80,207,330}};
        for (int i = 0; i < 3; ++i) {
            MOTION_CHECK(fabsf(effect->mRefEnt.origin[i] - firstOrigin[bolted][i]) < .00001f);
            MOTION_CHECK(fabsf(effect->mRefEnt.endpos[i] - firstEnd[bolted][i]) < .00001f);
        }
        /* A turn must use the preceding frame, not the original spawn endpoint. */
        tail.base.velocityXChannelInstance.scale = 0;
        tail.base.velocityYChannelInstance.scale = 10;
        theFxHelper->mTime = 1200;
        MOTION_CHECK(Tail_Update(&tail));
        MOTION_CHECK(tail.endpoint[0] == 11 && tail.endpoint[1] == 20 && tail.endpoint[2] == 30);
        const vec3_t secondOrigin[2] = {{11,21,30}, {79,211,330}};
        const vec3_t secondEnd[2] = {{11,17,30}, {83,211,330}};
        for (int i = 0; i < 3; ++i) {
            MOTION_CHECK(fabsf(effect->mRefEnt.origin[i] - secondOrigin[bolted][i]) < .00001f);
            MOTION_CHECK(fabsf(effect->mRefEnt.endpos[i] - secondEnd[bolted][i]) < .00001f);
        }
        vec3_t stationaryEnd;
        memcpy(stationaryEnd, effect->mRefEnt.endpos, sizeof(stationaryEnd));
        theFxHelper->mFrameTime = 0;
        MOTION_CHECK(Tail_Update(&tail));
        MOTION_CHECK(!memcmp(stationaryEnd, effect->mRefEnt.endpos, sizeof(stationaryEnd)));
    }
    for (int isTail = 0; isTail < 2; ++isTail) {
        for (int state = 0; state < 3; ++state) {
            Tail tail = {0};
            Effect *effect = &tail.base.base;
            FxBoltFrame bolt = {.mBolt = {state == 0 ? -1 : 1, state == 1 ? 1 : -1},
                .cachedServerTime = state == 1 ? 99 : 100,
                .orientation = {.origin = {100,200,300}, .axis = {{0,1,0},{-1,0,0},{0,0,1}}}};
            Tail_CreateChannelInstances(&tail, &pt);
            effect->mFlags = 0x80;
            effect->mTimeStart = 1000;
            effect->mTimeEnd = 3000;
            effect->mBolt.value = &bolt;
            effect->origin[0] = 10; effect->origin[1] = 20; effect->origin[2] = 30;
            effect->mRefEnt.origin[0] = 7; effect->mRefEnt.origin[1] = 8; effect->mRefEnt.origin[2] = 9;
            tail.endpoint[0] = 17; tail.endpoint[1] = 18; tail.endpoint[2] = 19;
            Bool alive = isTail ? Tail_Update(&tail) : Particle_Update(&tail.base);
            MOTION_CHECK(alive == (state == 2));
            if (state < 2) {
                MOTION_CHECK(effect->mRefEnt.origin[0] == 7 && effect->mRefEnt.origin[1] == 8 && effect->mRefEnt.origin[2] == 9);
                MOTION_CHECK(tail.endpoint[0] == 17 && tail.endpoint[1] == 18 && tail.endpoint[2] == 19);
                MOTION_CHECK(bolt.mBolt.dobjHandle == -1);
                if (state == 1)
                    MOTION_CHECK(bolt.mBolt.boneIndex == -1 && bolt.cachedServerTime == 100);
            } else {
                MOTION_CHECK(effect->mRefEnt.origin[0] == 80 && effect->mRefEnt.origin[1] == 210 && effect->mRefEnt.origin[2] == 330);
            }
        }
    }
    /* An attachment failure must stop each update before local motion or rendering. */
    for (int kind = 0; kind < 4; ++kind) for (int state = 0; state < 3; ++state) {
        union { OrientedParticle oriented; Line line; Cylinder cylinder; Cloud cloud; } storage = {0};
        Particle *particle = (Particle *)&storage;
        Effect *effect = &particle->base;
        FxBoltFrame bolt = {.mBolt = {state == 0 ? -1 : 1, state == 1 ? 1 : -1},
            .cachedServerTime = state == 1 ? 99 : 100,
            .orientation = {.origin = {100,200,300}, .axis = {{0,1,0},{-1,0,0},{0,0,1}}}};
        if (kind == 2)
            Tail_CreateChannelInstances((Tail *)&storage.cylinder, &pt);
        else if (kind == 3)
            Cloud_CreateChannelInstances(&storage.cloud, &pt);
        else
            Particle_CreateChannelInstances(particle, &pt);
        if (kind == 0) storage.oriented.normal[2] = 1;
        if (kind == 3) storage.cloud.randomDirection[2] = 1;
        for (int i = 0; i < 3; ++i) particle->displayAxis[i][i] = 1;
        particle->velocityXChannelInstance.scale = 10;
        effect->mFlags = 0x80;
        effect->mTimeStart = 1000;
        effect->mTimeEnd = 3000;
        effect->mBolt.value = &bolt;
        effect->origin[0] = 10; effect->origin[1] = 20; effect->origin[2] = 30;
        effect->mRefEnt.origin[0] = 7; effect->mRefEnt.origin[1] = 8; effect->mRefEnt.origin[2] = 9;
        FxGfxEntity before = effect->mRefEnt;
        client.skelTimeStamp = 100;
        theFxHelper->mTime = 1100;
        theFxHelper->mFrameTime = 100;
        Bool alive;
        switch (kind) {
            case 0: alive = OrientedParticle_Update(&storage.oriented); break;
            case 1: alive = Line_Update(&storage.line); break;
            case 2: alive = Cylinder_Update(&storage.cylinder); break;
            default: alive = Cloud_Update(&storage.cloud); break;
        }
        MOTION_CHECK(alive == (state == 2));
        if (state < 2) {
            MOTION_CHECK(effect->origin[0] == 10 && effect->origin[1] == 20 && effect->origin[2] == 30);
            MOTION_CHECK(!memcmp(&before, &effect->mRefEnt, sizeof(before)));
            MOTION_CHECK(bolt.mBolt.dobjHandle == -1);
            if (state == 1)
                MOTION_CHECK(bolt.mBolt.boneIndex == -1 && bolt.cachedServerTime == 100);
        } else {
            MOTION_CHECK(effect->origin[0] == 11 && effect->origin[1] == 20 && effect->origin[2] == 30);
            MOTION_CHECK(effect->mRefEnt.origin[0] == 80 && effect->mRefEnt.origin[1] == 211 && effect->mRefEnt.origin[2] == 330);
        }
    }

}

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
    FX_DrawScheduledEffects();
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
        FX_DrawScheduledEffects();
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

    CheckEmitterUpdate();

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

    /* Rifle smoke: four particles distributed over a 0..250 ms delay. */
    pt.mType = 1;
    pt.mAttributeFlags = 0x80;
    pt.mSpawnFlags = 0x200;
    pt.mSpawnCount.mMin = pt.mSpawnCount.mMax = 4;
    pt.mSpawnDelay.mMin = 0;
    pt.mSpawnDelay.mMax = 250;
    fx.mPrimitiveCount = 2;
    fx.mPrimitives[1] = &pt;
    PrimitiveTemplate empty = {.mType = 0};
    fx.mPrimitives[0] = &empty;
    theFxHelper->mTime = 6000;
    globalScheduler = (FxScheduler){0};
    FxScheduler_PlayEffectUnderTest(&globalScheduler, &fx, origin, (MediaHandles *(*)[4])axes, NULL);
    assert(effectActiveCountNonBolt == 1 && globalScheduler.mScheduledCount == 3);
    theFxHelper->mTime = 6061;
    FX_AddScheduledEffects(NULL, NULL);
    assert(effectActiveCountNonBolt == 1 && globalScheduler.mScheduledCount == 3);
    theFxHelper->mTime = 6062;
    FX_AddScheduledEffects(NULL, NULL);
    assert(effectActiveCountNonBolt == 2 && globalScheduler.mScheduledCount == 2);
    assert(nonBoltList[1]->origin[0] == 17 && nonBoltList[1]->origin[1] == 29 && nonBoltList[1]->origin[2] == 41);
    theFxHelper->mTime = 6188;
    FX_AddScheduledEffects(NULL, NULL);
    assert(effectActiveCountNonBolt == 4 && !globalScheduler.mScheduledCount && !globalScheduler.mScheduledHead);
    for (int i = 0; i < 4; ++i)
        assert(nonBoltList[i]->origin[0] == 17 && nonBoltList[i]->origin[1] == 29 && nonBoltList[i]->origin[2] == 41);
    /* Use the actual frontend/update -> render/draw path, with stock sorting on. */
    imp_fx_sort = &enabledPtr;
    FX_UpdateScheduledEffectsNonBolt();
    FX_UpdateScheduledEffectsBolt();
    draws = 0;
    FX_DrawScheduledEffects();
    assert(draws == 4);
    theFxHelper->mTime = 6400;
    FX_UpdateAllNonBolt();
    assert(!effectActiveCount && !effectClusterCount);

    /* Delayed bolted particles use the current bone origin and rotated axis. */
    pt.mAttributeFlags = 0x82;
    pt.mSpawnCount.mMin = pt.mSpawnCount.mMax = 2;
    pt.mOrigin1X.mMin = pt.mOrigin1X.mMax = 2;
    theFxHelper->mTime = client.skelTimeStamp = 7000;
    FxScheduler_PlayEffectUnderTest(&globalScheduler, &fx, origin, NULL, &boltInfo);
    assert(effectActiveCountBolt == 1 && globalScheduler.mScheduledCount == 1);
    assert(((Particle *)boltList[0])->displayAxis[0][0] == 0 && ((Particle *)boltList[0])->displayAxis[0][1] == 1);
    FX_UpdateAllBolt();
    assert(boltList[0]->mRefEnt.origin[0] == 10 && boltList[0]->mRefEnt.origin[1] == 22 && boltList[0]->mRefEnt.origin[2] == 30);
    theFxHelper->mTime = client.skelTimeStamp = 7125;
    FX_AddScheduledEffects(NULL, NULL);
    assert(effectActiveCountBolt == 2 && !globalScheduler.mScheduledCount);
    FX_UpdateAllBolt();
    assert(boltList[1]->mRefEnt.origin[0] == 10 && boltList[1]->mRefEnt.origin[1] == 22 && boltList[1]->mRefEnt.origin[2] == 30);
    assert(((FxBoltFrame *)__ZN11FxBoltFrame12g_mFrameListE)->refCount == 2);
    FX_UpdateScheduledEffectsNonBolt();
    FX_UpdateScheduledEffectsBolt();
    draws = 0;
    FX_DrawScheduledEffects();
    assert(draws == 2);
    theFxHelper->mTime = 7400;
    FX_UpdateAllBolt();
    assert(!effectActiveCount && !effectClusterCount && !__ZN11FxBoltFrame12g_mFrameListE);
    CheckSpawnFilters();
    CheckParticlePhysics();
    CheckEmitterMotion();
    CheckTailAndBoltMotion();
    if (spawnFailures || physicsFailures || emitterFailures || motionFailures)
        return 1;
    puts("native FX: nine primitive lifecycles, scheduler dispatch, bolting, impact axes, spawn filters and particle physics passed");
    return 0;
}
