#include "PC/EffectsCore/Fxexport.c"
#include <assert.h>
#include <math.h>
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
/* The opt-in receipt hook shares the engine's console and helper imports. */
static FxHelper *fixtureHelper;
void *imp_theFxHelper = &fixtureHelper;
void Com_Printf(const char *format, ...) {}
#endif
static FxScheduler scheduler;
static FxScheduler *schedulerPointer=&scheduler;
byte *fx_scheduler_ptr=(byte *)&schedulerPointer;
static EffectTemplate effect;
static int calls;
void FxScheduler_PlayEffect(const FxScheduler *s,const EffectTemplate *fx,const float *origin,MediaHandles *(*axes)[4],const FxBoltInfo *bolt)
{
    const float (*axis)[3]=(const float (*)[3])axes;
    assert(s==&scheduler && fx==&effect && !bolt && origin[0]==3);
    if (axis) {
        for (int i=0;i<3;i++) {
            float length=0;
            for (int j=0;j<3;j++)length+=axis[i][j]*axis[i][j];
            assert(fabsf(length-1)<.001f);
        }
        assert(axis[0][0]==1 && axis[2][2]==1);
    }
    ++calls;
}
int main(void)
{
    const vec3_t origin={3,4,5},forward={1,0,0},up={0,0,1};
    FX_PlaySimpleEffect(&effect,origin);
    FX_PlayEffect(&effect,origin,forward);
    FX_PlayOrientedEffect(&effect,origin,forward,up);
    assert(calls==3);
    puts("native fixed-argument effect calls, NULL bolt and complete orthonormal axes: PASS");
}
