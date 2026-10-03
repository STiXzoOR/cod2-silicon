#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FxHelper helper;
static FxHelper *helperPtr = &helper;
static dvar_t enable = { .current.enabled = 1 }, freeze, count;
static dvar_t *enablePtr = &enable, *freezePtr = &freeze, *countPtr = &count;
void *imp_theFxHelper = &helperPtr;
void *imp_fx_enable = &enablePtr, *imp_fx_freeze = &freezePtr, *imp_fx_count = &countPtr;
void *imp_colorYellow;
refexport_t re;
static EffectTemplate *effectTemplateArray[256];
static int effectTemplateArrayCount, freed;
int FxHelper_GetSeed(const FxHelper *h) { return 123; }
void Rand_Init(int seed) { (void)seed; }
void FxHelper_SetIgnorePrecacheErrors(const FxHelper *h, int ignore) { (void)h; (void)ignore; }
EffectTemplate *FX_RegisterEffect(const char *name) { (void)name; abort(); }
void FX_Print(const char *format, ...) { (void)format; }
Bool FX_GetBoneOrientation(const FxBoltInfo *bolt, orientation_t *orient)
{ (void)bolt; (void)orient; abort(); }
void AxisCopy(vec3_t *in, vec3_t *out) { memcpy(out, in, sizeof(vec3_t) * 3); }
float Vec3DistanceSq(const vec_t *a, const vec_t *b) { (void)a; (void)b; return 0; }
Bool FxHelper_CullSpherePreviousFrame(const FxHelper *h, const vec_t *p, float r)
{ (void)h; (void)p; (void)r; return 0; }
float FxRange_GetVal(const FxRange *range) { return range->mMin; }
void *__Znam(size_t size) { return malloc(size); }
void __ZdaPv(void *p) { ++freed; free(p); }
void FxScheduler_CreateEffect(const FxScheduler *s, const EffectTemplate *f,
    const PrimitiveTemplate *p, const FxBoltInfo *b, const vec_t *o,
    MediaHandles *(*a)[4], int late, int index)
{ (void)s; (void)f; (void)p; (void)b; (void)o; (void)a; (void)late; (void)index; abort(); }
void FX_CleanTemplate(EffectTemplate *fx) { (void)fx; abort(); }
#include "scheduled_fx_source.h"

int main(void)
{
    FxScheduler scheduler = {0};
    PrimitiveTemplate primitive = { .mSpawnCount = {2, 2}, .mSpawnDelay = {100, 100} };
    EffectTemplate effect = { .mPrimitiveCount = 1 };
    vec3_t origin = {10, 20, 30};
    effect.mPrimitives[0] = &primitive;
    helper.mTime = 1000;
    FxScheduler_PlayEffect(&scheduler, &effect, origin, NULL, NULL);
    assert(scheduler.mScheduledCount == 2 && scheduler.mScheduledHead);
    ScheduledEffect *first = scheduler.mScheduledHead;
    ScheduledEffect *second = (ScheduledEffect *)(uintptr_t)first->mScheduledNext;
    assert(second && !second->mScheduledNext);
    assert(first->mFx == &effect && second->mFx == &effect);
    assert(first->mStartTime == 1100 && first->mOrigin[2] == 30);
    assert(first->mAxis[0][0] == 1 && first->mAxis[2][2] == 1);
    assert(first->mIndexInBatch == 1 && second->mIndexInBatch == 0);
    FxScheduler_Clean(&scheduler, 0, NULL);
    assert(freed == 2 && !scheduler.mScheduledHead && scheduler.mScheduledCount == 0);
    puts("online: delayed FX allocation, native queue links and cleanup passed");
    return 0;
}
