#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "PC/gfx_d3d/rb_fog.c"

materialCommands_t tess;
GfxBackEndData *backEndData;
r_globals_t rg;
r_backEndGlobals_t backEnd;
struct DxState dxState;
DxGlobals dx;
int alwaysfails;
static dvar_t zfarDvar, rendererDvar;
const dvar_t *r_zfar = &zfarDvar;
dvar_t *r_rendererInUse = &rendererDvar;
static void *viewInfo = &zfarDvar;
void **g_viewInfo = &viewInfo;
static void *deviceVtable[58];
static struct { void **vtable; } device = { deviceVtable };
static int states[256], calls;

static int SetRenderState(void *object, int state, int value)
{
    assert(object == &device && state >= 0 && state < 256);
    states[state] = value;
    ++calls;
    return 0;
}

int main(void)
{
    backEndData = calloc(1, sizeof(*backEndData));
    assert(backEndData);
    GfxFog *fog = &backEndData->fogSettings;
    fog->techniqueOffset = 2;
    fog->registered = 1;
    fog->fogStart = 0;
    fog->fogEnd = 1;
    fog->density = 0.00015f;
    zfarDvar.flags = 0xffff;
    zfarDvar.type = zfarDvar.modified = 0xff;
    rendererDvar.current.integer = 0;
    zfarDvar.current.value = 0;
    RB_SetIteratorFog();
    assert(isfinite(backEnd.codeConsts[28][0]) && backEnd.codeConsts[28][0] == -1);
    assert(backEnd.codeConsts[28][1] == 1 && backEnd.codeConsts[28][2] == -0.00015f);
    zfarDvar.current.value = 100;
    fog->fogStart = 20;
    RB_SetIteratorFog();
    assert(backEnd.codeConsts[28][0] == -1.0f / 80);
    assert(backEnd.codeConsts[28][1] == 100.0f / 80);
    rendererDvar.flags = 0xffff;
    rendererDvar.type = rendererDvar.modified = 0xff;
    rendererDvar.current.integer = 2;
    deviceVtable[0xe4 / 4] = SetRenderState;
    dx.device = (IDirect3DDevice9 *)&device;
    RB_SetIteratorFog();
    assert(calls == 2 && states[0x8c] == 1);
    fog->techniqueOffset = 1;
    RB_SetIteratorFog();
    assert(calls == 5 && states[0x8c] == 3);
    free(backEndData);
    puts("native fog constants: typed r_zfar fallback, override and renderer mode passed");
}
