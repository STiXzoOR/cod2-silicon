#include "common_types.h"
#include "imports.h"
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
#include <stdlib.h>
static void FX_CombatReceipt(const EffectTemplate *fx, const vec_t *org)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("COD2_MAC_COMBAT_TRACE") != NULL;
    if (!enabled || !fx || !org) return;
    extern void Com_Printf(const char *, ...);
    extern void *imp_theFxHelper;
    const FxHelper *helper = *(FxHelper **)imp_theFxHelper;
    Com_Printf("[combat-fx] time=%i effect=%s primitives=%i origin=%.9g,%.9g,%.9g\n",
              helper ? helper->mTime : -1, fx->mEffectName ? fx->mEffectName : "unnamed", fx->mPrimitiveCount,
              (double)org[0], (double)org[1], (double)org[2]);
}
#endif

extern volatile qboolean fx_camera_valid;

extern struct DObj_s * Com_GetClientDObj(int handle, int localClientNum);
extern int DObjGetBoneIndex(const DObj *obj, unsigned int boneName);
#if COD2_APPLE_SDK
extern void FxScheduler_PlayEffect(const FxScheduler *scheduler, const EffectTemplate *fx, const vec_t *org, MediaHandles *(*axis)[4], const FxBoltInfo *bolt);
extern void MakeNormalVectors(const vec_t *forward, vec_t *right, vec_t *up);
extern void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *out);
#else
extern void FxScheduler_PlayEffect(void *scheduler, EffectTemplate *fx, const vec_t *org, ...);
#endif
extern int FX_Init(int rendererExists);
extern void FX_Free(int freeAll);
extern void FxHelper_AdjustCamera(const FxHelper *_this, refdef_t *refdef, float zfar);
extern void FxHelper_AdjustTime(const FxHelper *_this, int intime);
extern void FxHelper_WarpTime(const FxHelper *_this, int intime);
extern float FxScheduler_GetEffectLength(const FxScheduler *_this, EffectTemplate *fx);

extern byte *fx_scheduler_ptr;
extern byte *fx_helper_ptr;

extern int effectActiveCountBolt;
extern int privateEffectActiveCountBolt;
extern int effectActiveCountNonBolt;
extern int privateEffectActiveCountNonBolt;

int FX_GetBoneIndex(const int entNum, unsigned int bone);
void FX_PlaySimpleEffect(EffectTemplate *fx, const vec_t *org);
void FX_PlayEffect(EffectTemplate *fx, const vec_t *org, const vec_t *fwd);
void FX_PlayEntityEffect(EffectTemplate *fx, const vec_t *org, vec3_t *axis, const FxBoltInfo *bolt);
int FX_InitSystem(int rendererExists);
void FX_FreeSystem(void);
void FX_FreeActive(void);
void FX_AdjustCamera(PrimType (*refdef)[256], float zfar);
void FX_AdjustTime(int time);
void FX_WarpTime(int time);
float FX_GetEffectLength(EffectTemplate *fx);
void Server_SwitchToValidFxScheduler(void);

int FX_GetBoneIndex(const int entNum, unsigned int bone)
{
    void *pObj = Com_GetClientDObj(entNum, 0);
    if (pObj == NULL)
        return -1;
    return DObjGetBoneIndex( (const DObj *)(pObj), bone);
}

void FX_PlaySimpleEffect(EffectTemplate *fx, const vec_t *org)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    FX_CombatReceipt(fx, org);
#endif
    /* no orientation, no bolt: pass explicit NULLs so the trailing axis/bolt args
       aren't garbage x64 registers (was UB: too few args for the 5-param callee). */
    FxScheduler_PlayEffect(*(void **)*(void **)&fx_scheduler_ptr, fx, org, (MediaHandles *(*)[4])0, (const FxBoltInfo *)0);
}

void FX_PlayEffect(EffectTemplate *fx, const vec_t *org, const vec_t *fwd)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    FX_CombatReceipt(fx, org);
#endif
#if COD2_APPLE_SDK
    vec3_t axis[3];
    memcpy(axis[0], fwd, sizeof(vec3_t));
    MakeNormalVectors(axis[0], axis[1], axis[2]);
    FxScheduler_PlayEffect(*(void **)*(void **)&fx_scheduler_ptr, fx, org, (MediaHandles *(*)[4])axis, NULL);
#else
    /* fwd used as the axis; no bolt -> pass explicit NULL bolt (x64: else garbage r9). */
    FxScheduler_PlayEffect(*(void **)*(void **)&fx_scheduler_ptr, fx, org, (MediaHandles *(*)[4])fwd, (const FxBoltInfo *)0);
#endif
}

#if COD2_APPLE_SDK
void FX_PlayOrientedEffect(EffectTemplate *fx, const vec_t *org, const vec_t *forward, const vec_t *up)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    FX_CombatReceipt(fx, org);
#endif
    vec3_t axis[3];
    memcpy(axis[0], forward, sizeof(vec3_t));
    memcpy(axis[2], up, sizeof(vec3_t));
    Vec3Cross(axis[0], axis[2], axis[1]);
    FxScheduler_PlayEffect(*(void **)*(void **)&fx_scheduler_ptr, fx, org, (MediaHandles *(*)[4])axis, NULL);
}
#endif

void FX_PlayEntityEffect(EffectTemplate *fx, const vec_t *org, vec3_t *axis, const FxBoltInfo *bolt)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    FX_CombatReceipt(fx, org);
#endif
    FxScheduler_PlayEffect(*(void **)*(void **)&fx_scheduler_ptr, fx, org, axis, bolt);
}

int FX_InitSystem(int rendererExists)
{
    return FX_Init((unsigned char)rendererExists);
}

void FX_FreeSystem(void)
{
    FX_Free(1);
}

void FX_FreeActive(void)
{
    FX_Free(0);
}

void FX_AdjustCamera(PrimType (*refdef)[256], float zfar)
{
    FxHelper_AdjustCamera( (const FxHelper *)(*(void **)*(void **)&fx_helper_ptr), (refdef_t *)(refdef), zfar);
    fx_camera_valid = 1;
}

extern void *imp_effectActiveCountBolt;
extern void *imp_effectActiveCountNonBolt;
extern void *imp_privateEffectActiveCountBolt;
extern void *imp_privateEffectActiveCountNonBolt;
void FX_AdjustTime(int time)
{
    fx_camera_valid = 0;
    FxHelper_AdjustTime( (const FxHelper *)(*(void **)*(void **)&fx_helper_ptr), time);
    *(int *)imp_privateEffectActiveCountBolt = *(int *)imp_effectActiveCountBolt;
    *(int *)imp_privateEffectActiveCountNonBolt = *(int *)imp_effectActiveCountNonBolt;
}

void FX_WarpTime(int time)
{
    FxHelper_WarpTime( (const FxHelper *)(*(void **)*(void **)&fx_helper_ptr), time);
}

float FX_GetEffectLength(EffectTemplate *fx)
{
    return FxScheduler_GetEffectLength( (const FxScheduler *)(*(void **)*(void **)&fx_scheduler_ptr), fx);
}

void Server_SwitchToValidFxScheduler(void)
{

}
