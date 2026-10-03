#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
static jmp_buf failure;
static int dropped;
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...)
{
    (void)fmt; assert(code == ERR_DROP); dropped = 1; longjmp(failure, 1);
}
uintptr_t MacSystem_ImageOffset(const void *p) { return (uintptr_t)p; }
float Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    float length = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    for (int i = 0; i < 3; ++i) out[i] = length ? v[i] / length : 0;
    return length;
}
#include "trajectory_source.h"
int main(void)
{
    entityState_t ent = {0};
    vec3_t result;
    ent.pos.trType = TR_LINEAR; ent.pos.trTime = 100;
    ent.pos.trBase[0] = 2; ent.pos.trDelta[0] = 1000;
    BG_EvaluateTrajectory(&ent.pos, 103, result); assert(result[0] == 5);
    ent.pos.trType = TR_GRAVITY;
    BG_EvaluateTrajectory(&ent.pos, 1100, result); assert(result[2] == -400);
    ent.pos.trType = (trType_t)0x12345678;
    if (!setjmp(failure)) BG_EvaluateTrajectory(&ent.pos, 103, result);
    assert(dropped == 1);
    return 0;
}
