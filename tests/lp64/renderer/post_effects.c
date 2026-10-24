#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

DxGlobals dx;
static DxGlobals *const dx_g = &dx;
DxState dxState;
r_backEndGlobals_t backEnd;
materialCommands_t tess;
r_global_permanent_t rgp;
int alwaysfails;
static dvar_t renderer, blur, disabled;
dvar_t *r_rendererInUse = &renderer;
const dvar_t *r_blur = &blur, *r_glow = &disabled, *r_fullbright = &disabled;
const dvar_t *r_showFbColorDebug = &disabled, *sc_showDebug = &disabled;
const dvar_t *r_glowBloomCutoff = &disabled, *r_glowBloomDesaturation = &disabled;
void *imp_dx = &dx, *imp_rgp = &rgp;
static int passes;
void *imp_g_TotalFilterPasses = &passes;
static const dvar_t *glowDvars[] = { &disabled, &disabled };
void *imp_r_glowBloomIntensity = glowDvars, *imp_r_glowRadius = glowDvars;
void *imp_r_glowSkyBleedIntensity = glowDvars;
static void *surfaceMethods[13], *deviceMethods[35];
static void **surface = surfaceMethods, **device = deviceMethods;
static Material feedback;
static GfxImage image;
static float filteredRadius, uv[4];
static D3DCOLOR drawnColor;
static int draws, copies;

static HRESULT Copy(void *dev, void *source, void *sourceRect, void *dest, void *destRect, int filter)
{
    assert(dev == &device && dest == &surface && !sourceRect && !destRect && filter == 2);
    ++copies;
    return 0;
}
static HRESULT Release(void *value) { assert(value == &surface); return 0; }
IDirect3DSurface9 *Image_GetSurface(GfxImage *value)
{
    return (IDirect3DSurface9 *)&surface;
}
void RB_EndSurface(void) {}
void RB_GlowFilterImage(float *radius) { assert(0); }
void RB_GaussianFilterImage(float radius, GfxRenderTargetId id)
{
    assert(id == R_RENDERTARGET_BLURRED_SCREEN);
    filteredRadius = radius;
}
void RB_SetRenderTarget(GfxRenderTargetId id)
{
    assert(id == R_RENDERTARGET_FRAME_BUFFER);
    dxState.renderTargetWidth = 1280;
    dxState.renderTargetHeight = 720;
}
float GetVirtualWidthFromRealWidth(float width) { assert(width == 3); return 3; }
void RB_DrawStretchPic(const Material *material, float x, float y, float w, float h,
                      float s0, float t0, float s1, float t1, D3DCOLOR color, GfxPrimStatsTarget stats)
{
    assert(material == &feedback && x == 0 && y == 0 && w == 1280 && h == 720 && stats == 10);
    uv[0] = s0; uv[1] = t0; uv[2] = s1; uv[3] = t1;
    drawnColor = color;
    ++draws;
}
#include "post_effects_source.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    GfxCmdApplyLatePostEffects command = {0};
    ((unsigned short *)&command)[1] = sizeof(command);
    GfxRenderCommandExecState exec = {0};
    exec.cmd = (const byte *)&command;
    dx.device = (IDirect3DDevice9 *)&device;
    deviceMethods[34] = Copy;
    surfaceMethods[2] = Release;
    dx.renderTargets[R_RENDERTARGET_BLURRED_SCREEN].image = &image;
    rgp.feedbackBlendMaterial = &feedback;
    blur.current.value = !strcmp(argv[1], "below-threshold") ? 1.5f : 4;
    RB_ApplyLatePostEffectsCmd(&exec);
    assert(copies == 1 && draws == 1 && backEnd.currentFeedbackImage == &image);
    assert(exec.cmd == (const byte *)&command + sizeof(command));
    if (!strcmp(argv[1], "native-extent")) {
        assert(uv[0] == 0 && uv[1] == 0 && uv[2] == 1 && uv[3] == 1);
    } else if (!strcmp(argv[1], "below-threshold")) {
        assert(filteredRadius == 3 && drawnColor == 0xffffff80);
    } else {
        assert(!strcmp(argv[1], "above-threshold"));
        assert(filteredRadius == 4 && drawnColor == 0xffffffff);
    }
    printf("post effects %s: passed\n", argv[1]);
}
