#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#include "player_dobj_source.h"
#include "player_model_source.h"
#include "PC/gfx_d3d/r_xsurface.c"

DxGlobals dx;
static SkinBuffers skinBuffers;
void *imp_g_skinBuffers = &skinBuffers;
static dvar_t renderer;
dvar_t *r_rendererInUse = &renderer;
static unsigned char stringData[256];
#if defined(COD2_X64)
static unsigned char emptyData[17];
#endif
static unsigned char duplicateData[32];

unsigned int SL_GetStringOfLen(const char *value, unsigned int user, unsigned int len, int type)
{
    (void)user;
#if defined(COD2_X64)
    assert((type == 11 || type == 12) && len <= sizeof(stringData));
    if (type == 12) {
        assert(len == sizeof(emptyData));
        memcpy(emptyData, value, len);
        return 2;
    }
#else
    assert(type == 11 && len <= sizeof(stringData));
#endif
    memcpy(stringData, value, len);
    return 1;
}
const char *SL_ConvertToString(unsigned int value)
{
#if defined(COD2_X64)
    if (value == 2)
        return (const char *)emptyData;
#endif
    if (value == 200)
        return (const char *)duplicateData;
    assert(value == 1);
    return (const char *)stringData;
}
void SL_RemoveRefToStringOfLen(unsigned int value, unsigned int len) { assert(0); }
void Com_Printf(const char *format, ...) { assert(0); }
void Com_Error(errorParm_t code, const char *format, ...) { assert(0); }

static void CheckVec(const float *value, float x, float y, float z)
{
    if (fabsf(value[0] - x) >= 0.0001f || fabsf(value[1] - y) >= 0.0001f || fabsf(value[2] - z) >= 0.0001f)
        fprintf(stderr, "position %.6f %.6f %.6f; expected %.6f %.6f %.6f\n", value[0], value[1], value[2], x, y, z);
    assert(fabsf(value[0] - x) < 0.0001f);
    assert(fabsf(value[1] - y) < 0.0001f);
    assert(fabsf(value[2] - z) < 0.0001f);
}

static void TestChildTranslation(int animated, int delta)
{
    XModel model = {0};
    XModelParts parts = {0};
    XBoneHierarchy hierarchy = {0};
    unsigned short names[] = {11, 12};
    short bindQuat[] = {0, 0, 0, 32767};
    float bindTrans[] = {3, 4, 12};
    XAnim anims = {0};
    XAnimTree tree = {0};
    XAnimParts animation = {0};
    XAnimPart animationParts[2] = {0};
    XAnimPartQuat rootQuat = {0};
    XAnimPartTrans rootTrans = {0}, childTrans = {0};
    DObj object = {0};
    DSkel *skel = calloc(1, sizeof(DSkel) + sizeof(DObjAnimMat));
    int bits[4] = {3, 0, 0, 0};
    XVertexBuffer vertex = {0};
    XSurface surface = {0};
    GfxModelSkinnedSurface skinned = {0};
    float output[16] = {0};
    SkinXModelCmd command = {0};
    float expected[3];
    assert(skel);

    hierarchy.names = names;
    hierarchy.parentList[0] = 1;
    parts.numBones = 2;
    parts.numRootBones = 1;
    parts.hierarchy = &hierarchy;
    parts.quats = bindQuat;
    parts.trans = bindTrans;
    model.parts = (void (*)())&parts;
    object.numBones = 2;
    object.numModels = 1;
    object.models[0] = &model;
    object.modelParents[0] = 255;
    object.skel = skel;

    /* Rotate the root 90 degrees about Z. The child animation stores a
     * displacement from its bind translation, as in Mac 1.3's skeleton
     * addition at 0x75fe1-0x7602c; the expected positions are independent
     * geometric results of that rotation, not values from the implementation. */
    rootQuat.u.frame0[2] = rootQuat.u.frame0[3] = 23170;
    rootTrans.u.frame0[0] = 10;
    rootTrans.u.frame0[1] = 20;
    rootTrans.u.frame0[2] = 30;
    animationParts[0].quat = &rootQuat;
    animationParts[0].trans = &rootTrans;
    childTrans.u.frame0[0] = 1;
    childTrans.u.frame0[1] = 2;
    childTrans.u.frame0[2] = 3;
    animationParts[1].trans = delta ? &childTrans : NULL;
    animation.names = names;
    animation.boneCount = animated ? 2 : 1;
    animation.parts = animationParts;
    animation.numframes = 1;
    anims.size = 1;
    anims.entries[0].u.parts = &animation;
    tree.anims = &anims;
    object.tree = &tree;
    DObjCalcAnim(&object, bits);
    DObjCalcSkel(&object, bits);
    CheckVec(skel->mat[0].trans, 10, 20, 30);
    expected[0] = delta ? 4 : 6;
    expected[1] = delta ? 24 : 23;
    expected[2] = delta ? 45 : 42;
    CheckVec(skel->mat[1].trans, expected[0], expected[1], expected[2]);
    assert(skel->skelPartBits[0] == 3);

    /* The rendered vertex must consume the composed child transform through
     * the actual CPU skinning command in both renderer formats. */
    vertex.v.boneOffset = 64;
    surface.boneOffset = -1;
    surface.vertCount = 1;
    surface.verts = &vertex;
    skinned.surf.surfType = (surfaceType_t)3;
    skinned.surf.xsurf = &surface;
    skinned.skinnedCachedOffset = -1;
    skinned.skinnedVert.variant = output;
    command.surfs = (surfaceType_t *)&skinned;
    command.surfCount = 1;
    command.boneCount = 2;
    command.mat = skel->mat;
    memcpy(command.surfacePartBits, bits, sizeof(bits));
    for (int mode = 1; mode <= 2; ++mode) {
        renderer.current.integer = mode;
        memset(output, 0, sizeof(output));
        R_SkinXModelCmd(&command, 0);
        CheckVec(output, expected[0], expected[1], expected[2]);
    }
    /* Reusing an already completed skeleton must not add the bind offset twice. */
    DObjCalcSkel(&object, bits);
    CheckVec(skel->mat[1].trans, expected[0], expected[1], expected[2]);
    free(skel);
}


static void RootTransform(DObjAnimMat *m, float x, float y, float z, int rotate)
{
    memset(m, 0, sizeof(*m));
    if (rotate)
        m->quat[2] = m->quat[3] = 23170.0f / 32767.0f;
    else
        m->quat[3] = 1;
    m->trans[0]=x; m->trans[1]=y; m->trans[2]=z;
}
static void TestMultiRootMasks(void)
{
    XModel model={0}; XModelParts parts={0}; DObj object={0};
    XBoneHierarchy *h=calloc(1, sizeof(*h)+3);
    DSkel *s=calloc(1, sizeof(*s)+5*sizeof(DObjAnimMat));
    short quats[16]={0,23170,0,23170, 0,0,0,32767, 0,0,0,32767, 0,0,0,32767};
    float trans[12]={3,4,12, 1,2,3, 10,0,0, 0,0,1};
    int roots[4]={3,0,0,0}, early[4]={5,0,0,0}, full[4]={63,0,0,0};
    assert(h && s); h->parentList[0]=2; h->parentList[1]=1; h->parentList[2]=3; h->parentList[3]=1;
    parts.numBones=6; parts.numRootBones=2; parts.hierarchy=h; parts.quats=quats; parts.trans=trans;
    model.parts=(void (*)())&parts; object.numBones=6; object.numModels=1; object.models[0]=&model;
    object.modelParents[0]=255; object.skel=s;
    DObjCalcAnim(&object,roots);
    RootTransform(&s->mat[0],10,20,30,1); RootTransform(&s->mat[1],100,200,300,0);
    s->mat[3].trans[0]=789;
#if defined(COD2_X64)
    DObjCalcAnim(&object,early);
#endif
    DObjCalcSkel(&object,early);
    CheckVec(s->mat[2].trans,6,23,42); assert(s->skelPartBits[0]==5); assert(s->mat[3].trans[0]==789);
#if defined(COD2_X64)
    DObjCalcAnim(&object,full);
#endif
    DObjCalcSkel(&object,full);
    CheckVec(s->mat[0].trans,10,20,30); CheckVec(s->mat[1].trans,100,200,300);
    CheckVec(s->mat[2].trans,6,23,42); CheckVec(s->mat[3].trans,4,26,41);
    CheckVec(s->mat[4].trans,110,200,300); CheckVec(s->mat[5].trans,110,200,301);
    assert(s->skelPartBits[0]==63); DObjCalcSkel(&object,full); CheckVec(s->mat[3].trans,4,26,41);
    free(h); free(s);
}
static void TestAttachmentDuplicates(void)
{
    XModel models[3]={{0}}; XModelParts parts[3]={{0}}; XBoneHierarchy hierarchy[3]={{0}};
    short quats[3][4]={{0,0,0,32767},{0,0,0,32767},{0,0,0,32767}};
    float trans[3][3]={{3,4,12},{2,0,0},{1000,2000,3000}};
    DObj object={0}; DSkel *s=calloc(1,sizeof(*s)+5*sizeof(DObjAnimMat)); int roots[4]={1,0,0,0}, full[4]={63,0,0,0};
    assert(s); object.numBones=6; object.numModels=3; object.skel=s;
    for(int i=0;i<3;i++) {parts[i].numBones=2; parts[i].numRootBones=1; parts[i].hierarchy=&hierarchy[i];
        parts[i].quats=quats[i]; parts[i].trans=trans[i]; hierarchy[i].parentList[0]=1;
        models[i].parts=(void (*)())&parts[i]; object.models[i]=&models[i]; object.modelParents[i]=255;}
    object.modelParents[1]=1;
    memset(duplicateData,0,sizeof(duplicateData)); duplicateData[16]=5; duplicateData[17]=1;
#if defined(COD2_X64)
    duplicateData[0]=0x30;
#endif
    duplicateData[18]=6; duplicateData[19]=2; object.duplicateParts=200;
    DObjCalcAnim(&object,roots); RootTransform(&s->mat[0],10,20,30,1);
#if defined(COD2_X64)
    DObjCalcAnim(&object,full);
#endif
    DObjCalcSkel(&object,full);
    CheckVec(s->mat[1].trans,6,23,42); CheckVec(s->mat[2].trans,6,23,42); CheckVec(s->mat[3].trans,6,25,42);
    CheckVec(s->mat[4].trans,10,20,30); CheckVec(s->mat[5].trans,6,23,42);
    DObjCalcSkel(&object,full); CheckVec(s->mat[5].trans,6,23,42); free(s);
}
static void TestLocalTagNullBind(void)
{
    XModel model={0}; XModelParts parts={0}; XBoneHierarchy h={0};
    short q[4]={0,0,0,32767}; float bind[3]={3,4,12};
    DObj object={0}; DSkel *s=calloc(1,sizeof(*s)+sizeof(DObjAnimMat)); int bits[4]={3,0,0,0};
    float displacement[3]={1,2,3};
    assert(s); parts.numBones=2; parts.numRootBones=1; parts.hierarchy=&h; parts.quats=q; parts.trans=bind;
    h.parentList[0]=1; model.parts=(void (*)())&parts; object.numBones=2; object.numModels=1;
    object.models[0]=&model; object.modelParents[0]=255; object.skel=s;
    RootTransform(&s->mat[0],10,20,30,1); s->animPartBits[0]=3; s->controlPartBits[0]=2;
    DObjSetLocalTagInternal(&object,displacement,NULL,1);
    DObjCalcSkel(&object,bits); CheckVec(s->mat[1].trans,4,24,45);
    memset(s,0,sizeof(*s)+sizeof(DObjAnimMat)); parts.trans=NULL;
    RootTransform(&s->mat[0],10,20,30,1); s->animPartBits[0]=1;
#if defined(COD2_X64)
    DObjCalcAnim(&object,bits);
#endif
    DObjCalcSkel(&object,bits); CheckVec(s->mat[1].trans,10,20,30); free(s);
}

static void TestRigidRotationRounding(void)
{
    XSurface surface = {0};
    GfxModelRigidSurface rigid = {0};
    GfxEntity entity = {0};
    SkinRigidXModelCmd command = {0};
    rigid.surf.surfType = (surfaceType_t)4;
    rigid.surf.xsurf = &surface;
    entity.scale = 1;
    entity.axis[0][0] = entity.axis[1][1] = entity.axis[2][2] = 1;
    command.surfs = (surfaceType_t *)&rigid;
    command.surfCount = 1;
    command.e = &entity;
    command.mat.quat[0] = .3f;
    command.mat.quat[1] = .4f;
    command.mat.quat[2] = .5f;
    command.mat.quat[3] = sqrtf(.5f);
    command.mat.transWeight = 2;
    R_SkinRigidXModelCmd(&command);
    /* Mac adds the two squared quaternion terms before subtracting from one. */
    assert(rigid.boneAxis[0][0] == 0x1.70a3dp-3f);
    assert(rigid.boneAxis[1][1] == 0x1.47ae14p-2f);
    assert(rigid.boneAxis[2][2] == 0x1p-1f);
}

static void TestSkinArithmetic(const char *mode)
{
    struct { XVertexInfo vertex; XBlendInfo blend; } input = {0};
    DObjSkelMat bones[2] = {0};
    XSurface surface = {0};
    GfxModelSkinnedSurface skinned = {0};
    float output[16] = {0};
    int dx7 = !strcmp(mode, "dx7");
    int homogeneous = !strcmp(mode, "homogeneous");
    bones[1].axis[0][0] = bones[1].axis[2][0] = 0x1.6a09e6p-1f;
    bones[1].axis[0][2] = -0x1.6a09e6p-1f;
    bones[1].axis[1][1] = 1;
    bones[1].axis[2][2] = 0x1.6a09e6p-1f;
    bones[1].origin[0] = 10.0f;
    bones[0].origin[3] = bones[1].origin[3] = 1;
    input.vertex.numWeights = dx7 ? 0 : 1;
    input.vertex.boneWeight = 128;
    input.vertex.boneOffset = dx7 ? 64 : 0;
    input.vertex.offset[0] = input.vertex.offset[2] = .1f;
    input.blend.offset[0] = input.blend.offset[2] = .1f;
    input.blend.boneOffset = 64;
    input.blend.boneWeight = homogeneous ? 32767 : 32768;
    surface.boneOffset = -1;
    surface.vertCount = 1;
    surface.verts = (XVertexBuffer *)&input;
    skinned.surf.xsurf = &surface;
    skinned.skinnedCachedOffset = -1;
    skinned.skinnedVert.variant = output;
    renderer.current.integer = dx7 ? 2 : 1;
    R_SkinXSurfaceSkinned(&skinned, bones);
    /* Single-precision results from the original scalar/SIMD operation order. */
    if (dx7) assert(output[0] == 0x1.448686p+3f);
    else if (homogeneous) assert(output[3] == 0x1.fffep-1f);
    else assert(output[0] == 0x1.448688p+2f);
}

int main(int argc, char **argv)
{
#if defined(COD2_X64)
    DObjInit();
#endif
    if (argc > 1 && !strcmp(argv[1], "rigid")) {
        TestRigidRotationRounding();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "existing")) {
        TestSkinArithmetic(argv[1]);
        return 0;
    }
    TestChildTranslation(1, 0);
    TestChildTranslation(1, 1);
    TestChildTranslation(0, 0);
    TestMultiRootMasks();
    TestAttachmentDuplicates();
    TestLocalTagNullBind();
    puts("native player skeleton: bind offsets, masks, attachments, local tags and CPU skinning passed");
    return 0;
}
