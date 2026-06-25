#include "common_types.h"
#include "imports.h"

extern void *Hunk_AllocInternal(int size);
extern struct XModel *XModelPrecache(const char *name, float *(*allocXModel)(int), float *(*allocXModelColl)(int));
extern void *XModelGetBasePose(struct XModel *model);
extern int XModelTraceLine(struct XModel *model, trace_t *results, void *basePose, const vec_t *localStart, const vec_t *localEnd, int contentmask);
extern void MatrixTransformVector(const vec_t *in, const vec_t *matrix, vec_t *out);
extern void MatrixTransposeTransformVector(const vec_t *in, const vec_t *matrix, vec_t *out);
extern float Vec3Normalize(vec_t *v);

float *CM_Hunk_AllocXModel(int size)
{
    return (float *)Hunk_AllocInternal(size);
}

static float *CM_Hunk_AllocXModelColl(int size)
{
    return (float *)Hunk_AllocInternal(size);
}

struct XModel *CM_XModelPrecache(const char *name)
{
    return XModelPrecache(name, CM_Hunk_AllocXModel, CM_Hunk_AllocXModelColl);
}

void CM_TraceStaticModel(cStaticModel_t *sm, trace_t *results, const vec_t *start, const vec_t *end, int contentmask)
{
    vec3_t delta;
    vec3_t localStart;
    vec3_t localEnd;
    vec3_t normal;
    void *basePose;
    int hit;

    delta[0] = start[0] - sm->origin[0];
    delta[1] = start[1] - sm->origin[1];
    delta[2] = start[2] - sm->origin[2];
    MatrixTransformVector(delta, (const vec_t *)sm->invAxis, localStart);

    delta[0] = end[0] - sm->origin[0];
    delta[1] = end[1] - sm->origin[1];
    delta[2] = end[2] - sm->origin[2];
    MatrixTransformVector(delta, (const vec_t *)sm->invAxis, localEnd);

    basePose = XModelGetBasePose(sm->xmodel);
    hit = XModelTraceLine(sm->xmodel, results, basePose, localStart, localEnd, contentmask);

    if (hit >= 0) {
        results->entityNum = 0x3fe;
        MatrixTransposeTransformVector(results->normal, (const vec_t *)sm->invAxis, normal);
        Vec3Normalize(normal);
        results->normal[0] = normal[0];
        results->normal[1] = normal[1];
        results->normal[2] = normal[2];
    }
}

qboolean CM_TraceStaticModelComplete(cStaticModel_t *sm, const vec_t *start, const vec_t *end, int contentmask)
{
    trace_t localResults;
    vec3_t localEnd;
    vec3_t localStart;
    vec3_t delta;
    void *basePose;
    int hit;

    delta[0] = start[0] - sm->origin[0];
    delta[1] = start[1] - sm->origin[1];
    delta[2] = start[2] - sm->origin[2];
    MatrixTransformVector(delta, (const vec_t *)sm->invAxis, localStart);

    delta[0] = end[0] - sm->origin[0];
    delta[1] = end[1] - sm->origin[1];
    delta[2] = end[2] - sm->origin[2];
    MatrixTransformVector(delta, (const vec_t *)sm->invAxis, localEnd);

    localResults.fraction = 1.0f;

    basePose = XModelGetBasePose(sm->xmodel);
    hit = XModelTraceLine(sm->xmodel, &localResults, basePose, localStart, localEnd, contentmask);

    return (unsigned int)hit >> 31;
}
