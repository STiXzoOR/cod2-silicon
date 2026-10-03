#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>

#include "PC/gfx_d3d/rb_shade.c"

DxGlobals dx;
r_global_permanent_t rgp;
materialCommands_t tess;
static r_backEndGlobals_t backend;
void *imp_rgp = &rgp;
void *imp_backEnd = &backend;
void *imp_tess = &tess;
static dvar_t shadowDvar, lightmapDvar;
const dvar_t *sc_enable = &shadowDvar;
const dvar_t *r_lightMap = &lightmapDvar;
void *imp_sc_enable = &sc_enable;
void *imp_r_lightMap = &r_lightMap;
void R_Error(int level, const char *message, ...)
{ (void)level; (void)message; assert(0); }
void Com_Error(int code, const char *format, ...)
{ (void)code; (void)format; assert(0); }

static void Check(int source, GfxImage *expected, byte expectedState)
{
    void *image = NULL;
    byte state = 0;
    RB_GetTextureFromCode_impl(source, &image, &state);
    assert(image == expected && state == expectedState);
}

int main(void)
{
    GfxImage white = { 0 }, black = { 0 }, shadow = { 0 }, images[4] = { 0 };
    GfxEntity entity = { .reType = (refEntityType_t)3 };
    rgp.whiteImage = &white;
    rgp.blackImage = &black;
    dx.renderTargets[3].image = &shadow;
    backend.currentEntity = &entity;
    shadowDvar.current.enabled = 1;
    /* Mac 1.3 0xf6930 tests current.enabled, not native dvar flags at +8. */
    Check(19, &shadow, 0x32);
    shadowDvar.flags = 1;
    shadowDvar.current.enabled = 0;
    Check(19, &white, 0x32);
    shadowDvar.current.enabled = 1;
    entity.reType = 0;
    Check(19, &white, 0x32);
    entity.renderFxFlags = 0x100;
    Check(19, &shadow, 0x32);

    GfxImage *lightmaps[1][4] = { { &images[0], &images[1], &images[2], &images[3] } };
    GfxWorld world = { .lightmaps = lightmaps };
    rgp.world = &world;
    tess.lmapIndex = 0;
    lightmapDvar.flags = 0xffff;
    lightmapDvar.type = lightmapDvar.modified = 0xff;
    for (int i = 0; i < 4; ++i) {
        lightmapDvar.current.integer = 0;
        Check(8 + i, &images[i], 0x32);
        lightmapDvar.current.integer = 1;
        Check(8 + i, &white, 1);
        lightmapDvar.current.integer = 2;
        Check(8 + i, &black, 1);
    }
    puts("native code samplers: typed shadow toggle and lightmap debug modes passed");
}
