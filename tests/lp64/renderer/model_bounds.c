#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#include "PC/xanim/dobj.c"
#include "PC/gfx_d3d/r_model.c"
#include "PC/gfx_d3d/r_init.c"

DxGlobals dx;
refimport_t ri;
r_global_permanent_t rgp;
GfxScene scene;
GfxBackEndData *frontEndDataOut;
r_globals_t rg;
void *imp_rg = &rg;
static dvar_t developerValue;
const dvar_t *developer = &developerValue;
static const XModel *expectedModel;
static int badCalls;
static vec3_t zero;
void *imp_vec3_origin = zero;

int XModelBad(const XModel *model)
{
    assert(model == expectedModel);
    ++badCalls;
    return model->bad;
}

void CG_DObjCalcPose(const centity_t *cent, const DObj *obj, int *partBits) { assert(0); }
void ClearBounds(float *mins, float *maxs)
{
    for (int i = 0; i < 3; ++i) { mins[i] = FLT_MAX; maxs[i] = -FLT_MAX; }
}
void GetRotatedBounds(vec3_t *bounds, const vec_t *origin, vec3_t *axis, vec3_t *out)
{
    /* Keep the entity transform identity to isolate the production bone
     * bounds calculation. Its output still includes the entity origin. */
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) assert(axis[i][j] == (i == j));
        out[0][i] = bounds[0][i] + origin[i];
        out[1][i] = bounds[1][i] + origin[i];
    }
}
void MatrixTransformVector(const vec_t *in, const void *matrix, vec_t *out) { assert(0); }
void MatrixTransformVectorQuatTrans(const vec_t *in, const DObjAnimMat *matrix, vec_t *out) { assert(0); }
void R_AddDebugLine(DebugGlobals *debug, const vec_t *start, const vec_t *end, const vec_t *color) { assert(0); }
void Com_Error(errorParm_t code, const char *format, ...) { assert(0); }
void Com_Printf(const char *format, ...) { assert(0); }
unsigned int SL_FindString(const char *value) { assert(0); return 0; }
int XModelGetBoneIndex(const XModel *model, unsigned int name) { assert(0); return -1; }
void XModelGetBounds(const XModel *model, vec_t *mins, vec_t *maxs) { assert(0); }
float Vec3Distance(const void *a, const void *b) { return 0; }
int XModelGetLodForDist(const XModel *model, float dist)
{
    assert(model == expectedModel);
    return 0;
}
const char *XModelGetName(const XModel *model) { assert(model == expectedModel); return "synthetic"; }
int XModelGetSurfaces(const XModel *model, XSurface ***surfaces, int lod, int **partBits) { assert(0); return 0; }

int InterlockedCompareExchange(volatile int *value, int exchange, int comparand)
{
    int old = *value;
    if (old == comparand) *value = exchange;
    return old;
}

static void TestNonuniformBoneBounds(XModel *model, XModelParts *parts)
{
    XBoneInfo bone = {0};
    XModelSurfs surfaces = {0};
    GfxEntity entity = {0};
    GfxSceneEntity sceneEntity = {0};
    const float mins[3] = {-1, -5, -13}, maxs[3] = {3, 7, 17};
    const float quaternions[][4] = {
        {0, 0, 0, 1}, {0, 0, .70710678f, .70710678f},
        {0, 1, 0, 0}, {-.3f, .4f, .2f, .84261498f}
    };
    memcpy(bone.bounds[0], mins, sizeof(mins));
    memcpy(bone.bounds[1], maxs, sizeof(maxs));
    model->boneInfo = &bone;
    surfaces.partBits[0] = 1;
    model->lodInfo[0].surfs = &surfaces;
    model->lodInfo[0].numsurfs = 1;
    model->bad = 0;
    entity.reType = (refEntityType_t)1;
    entity.origin[0] = 17;
    entity.origin[1] = -23;
    entity.origin[2] = 41;
    entity.axis[0][0] = entity.axis[1][1] = entity.axis[2][2] = 1;
    sceneEntity.u.model = model;
    parts->skel.mat[0].trans[0] = 7;
    parts->skel.mat[0].trans[1] = -11;
    parts->skel.mat[0].trans[2] = 19;
    parts->skel.mat[0].transWeight = 2;
    for (unsigned pose = 0; pose < sizeof(quaternions) / sizeof(*quaternions); ++pose) {
        const float *q = quaternions[pose];
        double expectedMin[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
        double expectedMax[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
        memcpy(parts->skel.mat[0].quat, q, sizeof(vec4_t));
        /* Independent quaternion-vector rotation of all eight corners:
         * v' = v + 2*w*(q cross v) + 2*(q cross (q cross v)). */
        for (int corner = 0; corner < 8; ++corner) {
            double v[3], cross[3], rotated[3];
            for (int i = 0; i < 3; ++i) v[i] = (corner & (1 << i)) ? maxs[i] : mins[i];
            for (int i = 0; i < 3; ++i) {
                int j = (i + 1) % 3, k = (i + 2) % 3;
                cross[i] = q[j] * v[k] - q[k] * v[j];
            }
            for (int i = 0; i < 3; ++i) {
                int j = (i + 1) % 3, k = (i + 2) % 3;
                rotated[i] = v[i] + 2 * (q[3] * cross[i] + q[j] * cross[k] - q[k] * cross[j]);
                rotated[i] += parts->skel.mat[0].trans[i] + entity.origin[i];
                if (rotated[i] < expectedMin[i]) expectedMin[i] = rotated[i];
                if (rotated[i] > expectedMax[i]) expectedMax[i] = rotated[i];
            }
        }
        sceneEntity.cullState = 0;
        R_UpdateXModelBounds(&sceneEntity, &entity);
        assert(sceneEntity.cullState == 2);
        for (int i = 0; i < 3; ++i) {
            assert(fabs(sceneEntity.curMins[i] - expectedMin[i]) < 1e-4);
            assert(fabs(sceneEntity.curMaxs[i] - expectedMax[i]) < 1e-4);
        }
    }
    model->boneInfo = NULL;
    model->lodInfo[0].surfs = NULL;
}

int main(void)
{
    XModel *model = calloc(1, sizeof(*model));
    XModelParts *parts = calloc(1, sizeof(*parts));
    DObj wrapper = {0}, animated = {0};
    GfxEntity entity = {0};
    GfxSceneEntity sceneEntity = {0};
    assert(model && parts);
    assert((uintptr_t)model > UINT32_MAX && (uintptr_t)parts > UINT32_MAX);
    /* Exercise the actual initialization and actual DObjCreate together.
     * Fields after the legacy 100-byte scratch array must stay intact. */
    rg.stats = (trStatistics_t *)&sceneEntity;
    rg.lodParms.origin[0] = 19;
    ri.DObjCreate = DObjCreate;
    R_InitModelDObj();
    assert(rg.modelDObj != (DObj *)rg.modelDObjBuf);
    assert(rg.modelDObj->numModels == 1 && !rg.modelDObj->models[0]);
    assert(rg.modelDObj->modelParents[0] == 255 && !rg.modelDObj->matOffset[0]);
    assert(!rg.modelDObj->mins[0] && !rg.modelDObj->maxs[2]);
    assert(rg.stats == (trStatistics_t *)&sceneEntity && rg.lodParms.origin[0] == 19);
    model->parts = (void (*)())parts;
    model->bad = 1;
    parts->numBones = 1;
    parts->skel.mat[0].quat[3] = 1;
    wrapper.numModels = 1;
    wrapper.timeStamp = 1234;
    rg.modelDObj = &wrapper;
    expectedModel = model;

    DObjSetModel(&wrapper, model);
    assert(wrapper.skel == &parts->skel && wrapper.numBones == 1);
    assert(wrapper.models[0] == model && wrapper.numModels == 1);
    assert(wrapper.timeStamp == 1234);
    assert(DObjGetRotTransArray(&wrapper) == parts->skel.mat);
    assert(DObjGetRotTransArray(&wrapper)[0].quat[3] == 1);

    sceneEntity.u.model = model;
    entity.reType = (refEntityType_t)1;
    entity.origin[0] = 17;
    entity.origin[1] = -23;
    entity.origin[2] = 41;
    R_UpdateXModelBounds(&sceneEntity, &entity);
    assert(badCalls == 1 && sceneEntity.cullState == 2);
    assert(!memcmp(sceneEntity.curMins, entity.origin, sizeof(vec3_t)));
    assert(!memcmp(sceneEntity.curMaxs, entity.origin, sizeof(vec3_t)));
    assert(R_GetGfxEntityDObj(&sceneEntity, &entity) == &wrapper);

    /* A valid model continues through real DObj model/LOD/surface access. */
    model->bad = 0;
    sceneEntity.cullState = 0;
    R_UpdateXModelBounds(&sceneEntity, &entity);
    assert(badCalls == 2 && sceneEntity.cullState == 2);
    assert(!memcmp(sceneEntity.curMins, entity.origin, sizeof(vec3_t)));

    /* Animated entities already carry a DObj and must keep that object. */
    animated.numModels = 1;
    animated.models[0] = model;
    sceneEntity.u.obj = &animated;
    sceneEntity.cullState = 0;
    entity.reType = (refEntityType_t)0;
    R_UpdateXModelBounds(&sceneEntity, &entity);
    assert(badCalls == 3 && sceneEntity.cullState == 2);
    assert(R_GetGfxEntityDObj(&sceneEntity, &entity) == &animated);
    TestNonuniformBoneBounds(model, parts);
    free(parts);
    free(model);
    puts("native model bounds: typed initialization, full pointers, wrappers and four nonuniform rotated bone boxes passed");
    return 0;
}
