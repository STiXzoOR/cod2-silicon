#include "common_types.h"
#include "imports.h"

COD2_ASSERT_FIELD(XModelParts, numBones, 0x0);
COD2_ASSERT_FIELD(XModelParts, skel.mat, 0x44);

extern const char *SL_ConvertToString(unsigned int stringValue);

static TestLod g_testLods[4];

const char *XModelGetName(const XModel *model);
unsigned char XModelGetFlags(const XModel *model);
const char *XModelGetSurfaceName(const XModel *model, int subMatIndex, int lod);
int XModelGetSurfaces(const XModel *model, struct XSurface_s ***surfaces, int lod, int **partBits);
int XModelGetNumLods(const XModel *model);
int XModelNumBones(const XModel *model);
const DObjAnimMat *XModelGetBasePose(const XModel *model);
const DObjAnimMat *XModelGetBasePoseBone(const XModel *model, int skelMatBoneOffset);
void XModelSetTestLods(int lodLevel, float dist);
float XModelGetLodOutDist(const XModel *model);
int XModelGetLodForDist(const XModel *model, float dist);

const char *XModelGetName(const XModel *model)
{
    return model->name;
}

unsigned char XModelGetFlags(const XModel *model)
{
    return model->flags;
}

const char *XModelGetSurfaceName(const XModel *model, int subMatIndex, int lod)
{
    unsigned short *materialNames = model->lodInfo[lod].surfNames;
    unsigned short name = materialNames[subMatIndex];
    if (name == 0)
        return "DEFAULT";
    return SL_ConvertToString(name);
}

int XModelGetSurfaces(const XModel *model, struct XSurface_s ***surfaces, int lod, int **partBits)
{
    XModelSurfs *surfData = model->lodInfo[lod].surfs;
    *surfaces = *(struct XSurface_s ***)(surfData);
    *partBits = (int *)((byte *)surfData + 4);
    return model->lodInfo[lod].numsurfs;
}

int XModelGetNumLods(const XModel *model)
{
    return model->numLods;
}

int XModelNumBones(const XModel *model)
{
    return ((const XModelParts *)model->parts)->numBones;
}

const DObjAnimMat *XModelGetBasePose(const XModel *model)
{
    return ((const XModelParts *)model->parts)->skel.mat;
}

const DObjAnimMat *XModelGetBasePoseBone(const XModel *model, int skelMatBoneOffset)
{
    byte *base = (byte *)((const XModelParts *)model->parts)->skel.mat;
    return (const DObjAnimMat *)(base + ((unsigned int)skelMatBoneOffset >> 1));
}

void XModelSetTestLods(int lodLevel, float dist)
{
    *(float *)((byte *)&g_testLods[lodLevel] + 4) = dist;
    *(byte *)&g_testLods[lodLevel] = (dist >= 0.0f) ? 1 : 0;
}

float XModelGetLodOutDist(const XModel *model)
{
    int lastLod = model->numLods - 1;
    if (*(byte *)&g_testLods[lastLod] != 0)
        return *(float *)((byte *)&g_testLods[lastLod] + 4);
    return model->lodInfo[lastLod].dist;
}

int XModelGetLodForDist(const XModel *model, float dist)
{
    int numLods = model->numLods;
    int i;

    for (i = 0; i < numLods; i++) {
        float lodDist;
        if (*(byte *)&g_testLods[i] != 0)
            lodDist = *(float *)((byte *)&g_testLods[i] + 4);
        else
            lodDist = model->lodInfo[i].dist;

        if (lodDist == 0.0f || lodDist > dist)
            return i;
    }
    return -1;
}
