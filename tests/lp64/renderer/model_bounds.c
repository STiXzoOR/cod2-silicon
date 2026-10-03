#include <assert.h>
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
void ClearBounds(float *mins, float *maxs) { assert(0); }
void GetRotatedBounds(vec3_t *bounds, const vec_t *origin, vec3_t *axis, vec3_t *out) { assert(0); }
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
    free(parts);
    free(model);
    puts("native model bounds: typed initialization, full parts pointer, widened skeleton and model/animated wrappers passed");
    return 0;
}
