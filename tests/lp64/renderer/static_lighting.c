#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PC/gfx_d3d/r_staticmodel_load_obj.c"
#include "PC/gfx_d3d/rb_shade.c"

static dvar_t renderer;
dvar_t *r_rendererInUse = &renderer;
unsigned char smodelLoadGlob[128];
refimport_t ri;
r_global_permanent_t rgp;
materialCommands_t tess;
DxGlobals dx;
int alwaysfails;
static r_backEndGlobals_t backend;
void *imp_backEnd = &backend;
void *imp_dx = &dx;
static byte uploaded[8 * 4 * 2 * 4];
static byte *cachePixels;
static GfxImage image;

void *Hunk_AllocateTempMemoryInternal(int size)
{
    assert(size == sizeof(uploaded));
    cachePixels = malloc(size);
    return cachePixels;
}
GfxImage *Image_Alloc(const char *name, int category, int semantic, int track)
{
    assert(!strcmp(name, "*smodel_lighting") && category == 2 && semantic == 1 && track == 4);
    return &image;
}
void Image_Generate3D(GfxImage *target, byte *pixels, int width, int height, int depth, D3DFORMAT format)
{
    assert(target == &image && width == 8 && height == 4 && depth == 2 && format == 21);
    memcpy(uploaded, pixels, sizeof(uploaded));
}
void RB_SetupEntityLighting(const GfxEntity *entity, GfxEntityLighting *lighting)
{
    (void)entity; (void)lighting;
    assert(0);
}
int RB_DeriveEntityLights(vec4_t *colors, float visibility, const Material *material,
                          D3DLIGHT9 *lights, int maximum)
{
    (void)colors; (void)visibility; (void)material; (void)lights; (void)maximum;
    assert(0);
    return 0;
}
void RB_SetCodeConstant(int constant, float x, float y, float z, float w)
{
    (void)constant; (void)x; (void)y; (void)z; (void)w;
    assert(0);
}
void Com_Memcpy(void *destination, const void *source, int count)
{
    memcpy(destination, source, count);
}

static byte Quantize(float value)
{
    int channel = (int)floorf(value * 255.0f + .5f);
    return channel < 0 ? 0 : channel > 255 ? 255 : (byte)channel;
}

int main(void)
{
    GfxWorld world = {0};
    GfxStaticModelInstance model = {0};
    GfxEntity entity = {0};
    vec4_t colors[6];
    float *channels = &colors[0][0];
    byte expected[sizeof(uploaded)];

    renderer.current.integer = 3;
    world.smodelCount = 6;
    R_PrepareStaticModelLightingCache(&world, world.smodelCount);
    memset(expected, 128, sizeof(expected));
    for (int corner = 0; corner < 8; ++corner) {
        channels[corner] = .1f + .05f * corner;
        channels[corner + 8] = .3f + .05f * corner;
        channels[corner + 16] = .6f + .05f * corner;
    }
    channels[0] = -.1f;
    channels[15] = 1.2f;
    for (int corner = 0; corner < 8; ++corner) {
        int x = 2 + (corner & 1);
        int y = 2 + ((corner >> 1) & 1);
        int z = corner >> 2;
        byte *pixel = expected + 4 * (x + 8 * (y + 4 * z));
        pixel[0] = Quantize(channels[corner + 16]);
        pixel[1] = Quantize(channels[corner + 8]);
        pixel[2] = Quantize(channels[corner]);
        pixel[3] = Quantize(.8f);
    }
    R_CacheStaticModelLighting(&world, &model, .8f, colors);
    assert(model.baseLightingCoords[0] == .375f);
    assert(model.baseLightingCoords[1] == .75f);
    assert(model.baseLightingCoords[2] == .5f);
    R_FinishStaticModelLightingCache(&world);
    assert(!memcmp(uploaded, expected, sizeof(uploaded)));
    free(cachePixels);

    rgp.world = &world;
    entity.reType = (refEntityType_t)2;
    memcpy(entity.lighting.baseCoords, model.baseLightingCoords, sizeof(vec3_t));
    backend.currentEntity = &entity;
    RB_SetupLighting_impl();
    /* Atlas rows differ from z: a widened dx7 pointer must not select z as y. */
    assert(!memcmp(backend.codeConsts[24], model.baseLightingCoords, sizeof(vec3_t)));
    assert(backend.codeConsts[24][3] == 0);
    assert(backend.codeConsts[25][0] == .0625f);
    assert(backend.codeConsts[25][1] == .125f);
    assert(backend.codeConsts[25][2] == .25f);
    puts("native static-model lighting: eight BGRA corners and distinct atlas coordinates passed");
    return 0;
}
