#include "common_types.h"
#include "imports.h"

void CM_CalcTraceEntents(TraceExtents *extents)
{
    int i;
    float delta;

    for (i = 0; i < 3; i++) {
        delta = extents->start[i] - extents->end[i];
        if (delta == 0.0f)
            extents->invDelta[i] = 0.0f;
        else
            extents->invDelta[i] = 1.0f / delta;
    }
}

qboolean CM_TraceBox(const TraceExtents *extents, const vec_t *mins, const vec_t *maxs, float fraction)
{
    const vec_t *bounds;
    float tmin, tmax, t1, t2, invD;
    int i, pass;

    tmin = 0.0f;
    tmax = fraction;

    for (pass = 0; pass < 2; pass++) {
        bounds = (pass == 0) ? mins : maxs;
        for (i = 0; i < 3; i++) {
            t1 = (extents->start[i] - bounds[i]) * -1.0f;
            t2 = (extents->end[i] - bounds[i]) * -1.0f;

            if (t1 > 0.0f) {
                if (t2 > 0.0f)
                    return 1;

                invD = t1 * extents->invDelta[i] * -1.0f;
                if (invD >= tmax)
                    return 1;
                if (0.0f - invD > tmin)
                    tmin = invD;
            } else if (t2 > 0.0f) {

                invD = t1 * extents->invDelta[i] * -1.0f;
                if (invD <= 0.0f)
                    return 1;

                if (invD < tmax)
                    tmax = invD;
                else
                    tmax = tmax;
            }
        }
    }

    return (tmax == 1.0f) ? 0 : 1;
}
