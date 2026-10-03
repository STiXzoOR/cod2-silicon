#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "PC/gfx_d3d/rb_state.c"

struct DxState dxState;
r_backEndGlobals_t backEnd;
static float subpixelX, subpixelY;
static int constantCalls, offsetCalls;

int MacOpenGLUtils_GetSubPixelOffset(float *x, float *y)
{ *x = subpixelX; *y = subpixelY; ++offsetCalls; return 0; }
void RB_SetCodeConstant(int constant, vec_t x, vec_t y, vec_t z, vec_t w)
{
    assert(constant == 0xab || constant == 0xae || constant == 0xaf);
    float *value = backEnd.codeConsts[constant - 128];
    value[0] = x; value[1] = y; value[2] = z; value[3] = w;
    ++constantCalls;
}

static void Close(float actual, double expected)
{
    if (!isfinite(actual) || fabs(actual - expected) >= 2 * FLT_EPSILON)
        fprintf(stderr, "viewport constant: actual %.10g, expected %.10g\n", actual, expected);
    assert(isfinite(actual) && fabs(actual - expected) < 2 * FLT_EPSILON);
}

static void Check(int width, int height, GfxViewport viewport, GfxViewportBehavior behavior)
{
    dxState.renderTargetWidth = width;
    dxState.renderTargetHeight = height;
    dxState.viewportBehavior = behavior;
    backEnd.sceneViewport = viewport;
    backEnd.projection2D = 1;
    backEnd.viewportIsDirty = 0;
    constantCalls = offsetCalls = 0;
    RB_UpdateViewportConstants();
    assert(constantCalls == 3 && offsetCalls == 1);
    assert(!backEnd.projection2D && backEnd.viewportIsDirty);
    if (behavior == GFX_USE_VIEWPORT_FULL)
        viewport = (GfxViewport){ .width = width, .height = height };
    const float *bounds = backEnd.codeConsts[0xab - 128];
    const float *scale = backEnd.codeConsts[0xae - 128];
    const float *offset = backEnd.codeConsts[0xaf - 128];
    Close(bounds[0], (viewport.width - 0.5) / width);
    Close(bounds[1], (viewport.height - 0.5) / height);
    Close(bounds[2], 1.0 / width);
    Close(bounds[3], 1.0 / height);
    Close(scale[2], 0); Close(scale[3], 1);
    Close(offset[2], 0); Close(offset[3], 0);
    /* Clip corners map to the viewport rectangle inside the actual NPOT image. */
    Close(offset[0] - scale[0], (viewport.x + (double)subpixelX) / width);
    Close(offset[0] + scale[0], (viewport.x + viewport.width + (double)subpixelX) / width);
    Close(offset[1] - scale[1], (viewport.y + (double)subpixelY) / height);
    Close(offset[1] + scale[1], (viewport.y + viewport.height + (double)subpixelY) / height);
}

int main(void)
{
    GfxViewport ignored = { .x = 13, .y = 17, .width = 641, .height = 359 };
    Check(1280, 720, ignored, GFX_USE_VIEWPORT_FULL);
    Check(1280, 720, (GfxViewport){ .x = 160, .y = 90, .width = 640, .height = 360 }, GFX_USE_VIEWPORT_FOR_VIEW);
    Check(13, 7, (GfxViewport){ .x = 12, .y = 6, .width = 1, .height = 1 }, GFX_USE_VIEWPORT_FOR_VIEW);
    Check(1, 1, ignored, GFX_USE_VIEWPORT_FULL);
    subpixelX = 0.25f;
    subpixelY = -0.25f;
    Check(1280, 720, ignored, GFX_USE_VIEWPORT_FULL);
    Check(1280, 720, (GfxViewport){ .x = 79, .y = 51, .width = 641, .height = 359 }, GFX_USE_VIEWPORT_FOR_VIEW);
    puts("native viewport constants: NPOT extents, clipped viewports and subpixel offsets passed");
}
