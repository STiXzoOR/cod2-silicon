#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static pmoveHandler_t handlers[1];
void *imp_pmoveHandlers = handlers;
static int traceCount, entering, allsolid;

void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up)
{
    assert(angles[0] == 0 && angles[1] == -180 && angles[2] == 0);
    forward[0] = -1; forward[1] = forward[2] = 0;
    right[0] = right[2] = 0; right[1] = -1;
    up[0] = up[1] = 0; up[2] = 1;
}
const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int i = 0; i < 3; i++) out[i] = len ? v[i] / len : 0;
    return len;
}
const vec_t Vec3Normalize(vec_t *v) { return Vec3NormalizeTo(v, v); }
const float vectopitch(const vec_t *v) { (void)v; return 0; }
const float AngleSubtract(float x, float y) { return x - y; }
const float AngleNormalize180(float x) { return x; }

static void trace(trace_t *tr, const vec_t *start, const vec_t *mins, const vec_t *maxs,
                  const vec_t *end, int pass, int mask)
{
    assert(pass == 7 && mask == 0x810011);
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1; tr->normal[2] = 1;
    ++traceCount;
    if (entering && traceCount == 1) {
        tr->startsolid = 1;
        tr->allsolid = allsolid;
        return;
    }
    int bodyTrace = traceCount - entering;
    if (bodyTrace == 2) tr->fraction = 0;
    else if (bodyTrace == 3) {
        /* Mac skips deflection traces when the first body trace is clear.
           The third trace drops directly from the original rear endpoint. */
        assert(start[0] == -58.0f && start[1] == 0.0f && start[2] == 24.0f);
        assert(end[0] == -58.0f && end[1] == 0.0f && end[2] == -3.0f);
        assert(mins[0] == -6.0f && maxs[2] == 6.0f);
        tr->fraction = 0.5f;
    }
}
#include "f1_prone_source.h"

int main(void)
{
    vec3_t origin = {0}, normal = {0, 0, 1};
    float height = 9, pitch = 9, waist = 9;
    handlers[0].trace = trace;
    assert(BG_CheckProne(7, origin, 15, 30, 0, &height, &pitch, &waist,
                         1, 1, normal, 0, 0, 64));
    assert(traceCount == 5 && height == 0 && pitch == 0 && waist == 0);
    entering = 1; traceCount = 0;
    assert(BG_CheckProne(7, origin, 15, 30, 0, &height, &pitch, &waist,
                         0, 1, normal, 0, 0, 64));
    assert(traceCount == 6);
    allsolid = 1; traceCount = 0;
    assert(!BG_CheckProne(7, origin, 15, 30, 0, &height, &pitch, &waist,
                          0, 1, normal, 0, 0, 64));
    assert(traceCount == 1);
    puts("PASS F1 prone clear-body trace order and allsolid versus startsolid");
    return 0;
}
