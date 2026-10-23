#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define sinf F1_sinf
#define cosf F1_cosf
#include "f1_dobj_source.h"
#undef sinf
#undef cosf

static float trigArguments[3];
static int trigArgumentCount;
float F1_sinf(float x)
{
    if (trigArgumentCount < 3) trigArguments[trigArgumentCount++] = x;
    return sinf(x);
}
float F1_cosf(float x) { return cosf(x); }

static void Near(float value,float expected) { assert(fabsf(value-expected)<0.00001f); }
void TestSkeleton(void)
{
    DObjInit();
    for (int control=0;control<2;++control) {
        DObj object={0}; XModel model={0}; XModelParts parts={0}; XBoneHierarchy hierarchy={0};
        unsigned short names[2]={21,22}; float bind[3]={3,4,5};
        DSkel *skel=calloc(1,sizeof(DSkel)+sizeof(DObjAnimMat)); int bits[4]={3,0,0,0};
        object.numBones=2; object.numModels=1; object.models[0]=&model;
        object.modelParents[0]=255; object.skel=skel; model.parts=(void (*)())&parts;
        parts.numBones=2; parts.numRootBones=1; parts.hierarchy=&hierarchy; parts.trans=bind;
        hierarchy.parentList[0]=1; hierarchy.names=names; skel->animPartBits[0]=3;
        skel->controlPartBits[0]=control?2:0;
        skel->mat[0].quat[2]=1; skel->mat[0].quat[3]=1;
        skel->mat[0].trans[0]=10; skel->mat[0].trans[1]=20; skel->mat[0].trans[2]=30;
        skel->mat[1].quat[0]=1; skel->mat[1].quat[3]=1;
        skel->mat[1].trans[0]=1; skel->mat[1].trans[1]=2; skel->mat[1].trans[2]=3;
        /* Mac's control bit reverses quaternion order, while translation still
         * uses the parent's frame and the bind translation exactly once. */
        DObjCalcSkel(&object,bits);
        Near(skel->mat[1].quat[0],1); Near(skel->mat[1].quat[1],control?-1:1);
        Near(skel->mat[1].quat[2],1); Near(skel->mat[1].quat[3],1);
        Near(skel->mat[1].transWeight,.5f);
        Near(skel->mat[1].trans[0],4); Near(skel->mat[1].trans[1],24); Near(skel->mat[1].trans[2],38);
        DObjCalcSkel(&object,bits); Near(skel->mat[1].trans[0],4);
        free(skel);
    }
}

void TestSurfaces(void)
{
    DObj object={0}; XModel model={0}; XModelParts parts={0}; XModelSurfs surfs={0};
    DSurface surfaces[1]; int bits[4]={0}; char lods[1]={0};
    object.numModels=1; object.models[0]=&model; object.matOffset[0]=1;
    model.parts=(void (*)())&parts; parts.numBones=32;
    model.lodInfo[0].numsurfs=1; model.lodInfo[0].surfs=&surfs; surfs.partBits[0]=(int)0x80000000u;
    /* Mac 0x74520 uses SHRL: carrying bone 31 by one position sets only bone 32. */
    assert(DObjGetSurfaces(&object,surfaces,bits,lods)==1);
    assert(bits[0]==0 && bits[1]==1 && bits[2]==0 && bits[3]==0);
}

void TestTrace(void)
{
    vec3_t start={2,-2,0}, end={0,0,0}, mins={-1,-1,-1}, maxs={1,1,1};
    float frac=0, sign=0; int axis=-1, startSolid=0, endSolid=0;
    /* The Mac tests all minimum planes before maximum planes. Equal entry
     * fractions retain the minimum-Y normal, even though X precedes Y. */
    assert(DObjTraceLineAabbLocal(start,end,mins,maxs,1,&frac,&axis,&sign,&startSolid,&endSolid));
    Near(frac,.5f); assert(axis==1); Near(sign,-1); assert(!startSolid && endSolid);
}

void TestAngles(void)
{
    DObj object={0}; DSkel skel={0};
    vec3_t trans={0,0,0}, angles={0,.3f,0}; unsigned int radians;
    object.skel=&skel; trigArgumentCount=0;
    DObjSetLocalTagInternal(&object,trans,angles,0);
    assert(trigArgumentCount==3);
    /* Mac CVTSS2SD, MULSD [0x340c48], CVTSD2SS: .3 degrees has
     * radians bits 0x3b2b92a7; a float multiply produces 0x3b2b92a6. */
    memcpy(&radians,&trigArguments[0],sizeof(radians));
    assert(radians==0x3b2b92a7u);
}
