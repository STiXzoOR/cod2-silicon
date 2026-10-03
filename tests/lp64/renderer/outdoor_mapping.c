#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PC/universal/com_math.c"
#include "PC/gfx_d3d/r_outdoor.c"
#include "PC/gfx_d3d/rb_shade.c"

r_global_permanent_t rgp;
void *imp_rgp = &rgp;
void *imp_backEnd;
static dvar_t rendererDvar, awayBiasDvar, downBiasDvar;
dvar_t *r_rendererInUse = &rendererDvar;
const dvar_t *r_outdoorAwayBias = &awayBiasDvar;
const dvar_t *r_outdoorDownBias = &downBiasDvar;
const float lightGridLookupMatrix[4][4];
static GfxImage outdoor;
static int traceCalls, uploads;

GfxImage *Image_Register(const char *name, int semantic, int track)
{ assert(!strcmp(name, "$outdoor") && semantic == 1 && track == 0); return &outdoor; }
void *Hunk_AllocateTempMemoryInternal(int size)
{ assert(size == 512 * 512); return malloc(size); }
void Hunk_FreeTempMemory(void *pixels)
{ free(pixels); }
static int Trace(trace_t *trace, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int mask)
{
    assert(model == 0 && mask == 0x2001 && mins[0] == 0 && maxs[0] == 0);
    assert(fabsf(start[0] - (10 + ((traceCalls % 512) + .5f) * 512 / 511)) < .0001f);
    assert(fabsf(start[1] - (20 + ((traceCalls / 512) + .5f) * 1024 / 511)) < .0002f);
    assert(start[2] == 257 && end[2] == -1);
    static const float fractions[] = { 0, .25f, .5f, 1 };
    trace->fraction = fractions[traceCalls % 512 < 3 ? traceCalls % 512 : 3];
    ++traceCalls;
    return 0;
}
void *imp_CM_BoxTrace = Trace;
void Image_Generate2D(GfxImage *image, byte *pixels, int width, int height, int format)
{
    assert(image == &outdoor && width == 512 && height == 512 && format == 0x32);
    for (int row = 0; row < height; ++row) {
        assert(pixels[row * width] == 255);
        assert(pixels[row * width + 1] == 191);
        assert(pixels[row * width + 2] == 127);
        for (int col = 3; col < width; ++col) assert(pixels[row * width + col] == 0);
    }
    ++uploads;
}
Bool RB_GetViewport(GfxViewport *viewport)
{ (void)viewport; assert(0); return 0; }
int MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(float *matrix, float width, float height)
{ (void)matrix; (void)width; (void)height; assert(0); return 0; }

int main(void)
{
    GfxWorld world = { 0 };
    Material materials[2] = { 0 };
    srfTriangles_t triangles[2] = { 0 };
    GfxSurface surfaces[2] = { { .material = &materials[0], .tris = &triangles[0] },
                               { .material = &materials[1], .tris = &triangles[1] } };
    materials[1].info.gameFlags = 8;
    const float bounds[2][3] = { { 10, 20, 0 }, { 522, 1044, 256 } };
    memcpy(triangles[0].bounds, bounds, sizeof(bounds));
    world.surfaceCount = 2;
    world.surfaces = surfaces;
    rgp.world = &world;
    R_RegisterOutdoorImage(&world);
    /* Mac 1.3 0x32b960 is {512,512,256}; 0x108597 interpolates from ceilZ. */
    assert(world.outdoorImage == &outdoor);
    assert(outdoorGlob.scale[0] == 511.0f / 512);
    assert(outdoorGlob.scale[1] == 511.0f / 1024);
    assert(outdoorGlob.scale[2] == 255.0f / 256);
    R_GenerateOutdoorImage(&outdoor);
    assert(traceCalls == 512 * 512 && uploads == 1);

    r_backEndGlobals_t *backend = calloc(1, sizeof(*backend));
    assert(backend);
    imp_backEnd = backend;
    GfxCodeMatrices *matrices = &backend->codeMatrixStack[0];
    MatrixIdentity44(matrices->world.matrix[0].m);
    MatrixIdentity44(matrices->view.matrix[1].m);
    matrices->world.valid[0] = matrices->view.valid[1] = 1;
    awayBiasDvar.current.value = 32;
    awayBiasDvar.flags = 0xffff;
    awayBiasDvar.type = awayBiasDvar.modified = 0xff;
    downBiasDvar.current.value = 2;
    const float *matrix = RB_GetCodeMatrix_impl(0xEC, 0);
    assert(matrix[0] == 1.0f / 512 && matrix[5] == 1.0f / 1024 && matrix[10] == 1.0f / 256);
    assert(matrix[12] == -10.0f / 512 && matrix[13] == -18.0f / 1024 && matrix[14] == -32.0f / 256);
    assert(matrix[15] == 1 && matrices->worldOutdoorLookup.valid[0]);
    free(backend);
    rendererDvar.current.integer = 2;
    R_RegisterOutdoorImage(&world);
    assert(world.outdoorImage == NULL);
    puts("native outdoor lookup: map dimensions, trace heights and typed world matrix passed");
}
