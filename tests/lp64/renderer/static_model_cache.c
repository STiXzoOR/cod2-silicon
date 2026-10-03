#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#include "PC/gfx_d3d/r_staticmodelcache.c"

/* Use the real converter behind the cached-model producer. Its public
 * prototype in the renderer is opaque, so keep the typed definition local. */
#define CColorConverter_GetColorConverter Test_GetTypedColorConverter
#include "Mac/DirectX_9/CColorConverter.c"
#undef CColorConverter_GetColorConverter
enum { COLOR_BYTES_RGBA, COLOR_BYTES_BGRA, COLOR_BYTES_ARGB };
#include "Mac/DirectX_9/lp64_color_order.h"

DxGlobals dx;
r_global_permanent_t rgp;
void *imp_rgp = &rgp;
refimport_t ri;
int alwaysfails;
static dvar_t renderer;
dvar_t *r_rendererInUse = &renderer;
unsigned char sStdConverterARGB[16], sStdConverterABGR[16];
unsigned char sATI4CompsConverterARGB[16], sATI4CompsConverterABGR[16];
static unsigned char openGLState[0x804];
void *imp___ZN7COpenGL7sOpenGLE = openGLState;
void *imp___ZTV15CColorConverter;
int MacDisplay_GetCardType(void) { return 0; }
Boolean MacFeatures_IsAltiVecAvailable(UInt8 *a, UInt8 *b, UInt8 *c) { return 0; }
void __ZdlPv(void *p) { free(p); }
void *CColorConverter_GetColorConverter(int format)
{
    return (void *)Test_GetTypedColorConverter(format);
}

static byte cachedVertices[2 * 64];
static HRESULT LockCachedVertices(void *object, int offset, int size, void **data, int flags)
{
    assert(!offset && size == sizeof(cachedVertices) && flags == 0x1001);
    *data = cachedVertices;
    return 0;
}
static void UnlockCachedVertices(void *object) {}
void R_FatalLockError(HRESULT result) { assert(0); }
int XSurfaceGetBoneOffset(const XSurface *surface) { return 0; }
int RB_DeriveEntityLights(vec4_t *colors, float visible, const Material *material,
                         D3DLIGHT9 *lights, int maximum) { assert(0); return 0; }
const vec_t Vec3NormalizeTo(const vec_t *vector, vec_t *out)
{
    float length = sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
    for (int i = 0; i < 3; ++i) out[i] = vector[i] / length;
    return length;
}
void AxisTransformVector(vec3_t *axes, const vec_t x, const vec_t y, const vec_t z, vec_t *out)
{
    for (int i = 0; i < 3; ++i) out[i] = axes[0][i] * x + axes[1][i] * y + axes[2][i] * z;
}
void R_GetRigidTransform(const float *bone, const float *origin, const float *axis,
                         float scale, float *out)
{
    memcpy(out, axis, 9 * sizeof(float));
    memcpy(out + 9, origin, 3 * sizeof(float));
}
static DObjAnimMat pose;
static const DObjAnimMat *GetPose(const XModel *model, int bone) { return &pose; }

static void TestCachedVertexColors(void)
{
    static SkinBuffers buffers;
    GfxWorld world = {0};
    GfxStaticModelInstance instance = {0};
    XSurface surface = {0};
    GfxStaticModelSurfaceCached cached = {0};
    SkinStaticModelCachedCmd command = {0};
    byte source[128] = {0};
    void *table[13] = {0}, *object[1] = {table};
    const byte colors[2][4] = {{128, 128, 128, 255}, {11, 37, 109, 241}};

    table[11] = (void *)LockCachedVertices;
    table[12] = (void *)UnlockCachedVertices;
    dx.smodelCacheVb = (void *)object;
    ri.XModelGetBasePoseBone = GetPose;
    renderer.current.integer = 3;
    ((float *)&pose)[3] = 1;
    ((float *)&pose)[7] = 2;
    instance.axis[0][0] = instance.axis[1][1] = instance.axis[2][2] = 1;
    instance.scale = 1;
    instance.baseLightingCoords[0] = .375f;
    instance.baseLightingCoords[1] = .75f;
    world.smodelInsts = &instance;
    rgp.world = &world;
    for (int i = 0; i < 2; ++i) {
        ((float *)(source + i * 64))[2] = 1;
        ((float *)(source + i * 64))[4] = 1;
        ((float *)(source + i * 64))[9] = 1;
        memcpy(source + i * 64 + 12, colors[i], 4);
    }
    surface.vertCount = 2;
    surface.verts = (XVertexBuffer *)source;
    cached.xsurf = &surface;
    command.cached = &cached;
    R_SkinStaticModelCachedCmd(&command, &buffers);

    assert(MacShader_ColorByteOrder(64, 24, 3) == COLOR_BYTES_BGRA);
    for (int i = 0; i < 2; ++i) {
        const byte *pixel = cachedVertices + i * 64 + 24;
        byte rgba[4] = {pixel[2], pixel[1], pixel[0], pixel[3]};
        assert(!memcmp(rgba, colors[i], 4));
    }
    /* Skeletal streams and HUD float4 streams share the stride, but not
     * the producer or color offset. Keep those interfaces distinct. */
    assert(MacShader_ColorByteOrder(64, 12, 3) == COLOR_BYTES_ARGB);
    assert(MacShader_ColorByteOrder(64, 24, 4) == COLOR_BYTES_BGRA);
    assert(MacShader_ColorByteOrder(68, 24, 3) == COLOR_BYTES_RGBA);
}

_Static_assert(sizeof(static_model_leaf_t) == 24, "STABS leaf16: two widened pointers");
_Static_assert(offsetof(static_model_tree_t, leafs) == 144, "STABS leafs136: wider list");
_Static_assert(sizeof(static_model_tree_t) == 528, "STABS tree392: list and16 leaves");
_Static_assert(sizeof(MaterialPassDx7) == 112, "STABS Dx7 pass92: pointers and alignment");
_Static_assert(offsetof(MaterialPassDx7, samplers) == 16, "native sampler arguments");
_Static_assert(offsetof(MaterialTechnique, passArray) == 16, "STABS pass array8");

int main(void)
{
    static static_model_cache_t cache;
    XSurface xsurf = {0};
    GfxStaticSurface surface = {0};
    static_model_tree_t *tree = &cache.trees[0];
    static_model_node_list_t sentinel;
    static_model_node_list_t *freeNode = &tree->leafs[8].freenode;
    GfxStaticModelSurfaceCached *used = &tree->leafs[0].surf;
    xsurf.vertCount = 29;
    used->surface = &surface; used->xsurf = &xsurf;
    surface.cachedLods[2] = used;
    tree->nodes[0].usedVerts = 29;
    tree->nodes[1].usedVerts = 29; tree->nodes[1].inuse = 1;
    sentinel.prev = sentinel.next = (intptr_t)freeNode;
    freeNode->prev = freeNode->next = (intptr_t)&sentinel;
    cache.stats.allocatedVerts = 256; cache.stats.usedVerts = 29;
    SMC_FreeCachedSurface_r(&cache, tree, 0, 4);
    assert(cache.stats.allocatedVerts == 0 && cache.stats.usedVerts == 0);
    assert(!surface.cachedLods[2] && !tree->nodes[1].inuse);
    assert(sentinel.prev == (intptr_t)&sentinel && sentinel.next == (intptr_t)&sentinel);
    TestCachedVertexColors();
    puts("renderer static-model cache recursion, native leaf strides and producer BGRA colors: passed");
}
