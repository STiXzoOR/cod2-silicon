/* Exercise production 2D matrix setup without a window or GL context. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static r_backEndGlobals_t backEnd;
static materialCommands_t tess;
static DxGlobals dx;
static void *dx_g = &dx;
static int alwaysfails;
static dvar_t renderer;
static const dvar_t *r_rendererInUse = &renderer;
static GfxViewport viewport;
static Bool RB_GetViewport(GfxViewport *out) { *out = viewport; return 1; }
static void RB_EndSurface(void) { abort(); }
static int MacDisplay_GetCardType(void) { return 0; }
const float identityMatrix44[4][4] = {
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}
};

#include "hud_matrix_source.h"

static void checkViewport(int width, int height, int stack)
{
    memset(&backEnd, 0, sizeof(backEnd));
    backEnd.codeMatrixStackLevel = stack;
    viewport = (GfxViewport){0, 0, width, height};
    GfxCodeMatrices *matrices = &backEnd.codeMatrixStack[stack];
    /* Seed the next record as a preceding 3D frame would. An oversized copy
     * from world to view otherwise copies this 1 over projection's X scale. */
    MatrixIdentity44((vec4_t *)&matrices->normalizedWorld);
    RB_Set2D();
    assert(backEnd.projection2D);
    assert(matrices->projection.matrix[0].m[0][0] == 2.0f / width);
    assert(matrices->projection.matrix[0].m[1][1] == -2.0f / height);
    assert(matrices->projection.valid[0] && !matrices->projection.valid[1]);
    assert(memcmp(&matrices->world, &matrices->view, sizeof(GfxCodeMatrix)) == 0);
    assert(memcmp(&matrices->world, &matrices->normalizedWorld, sizeof(GfxCodeMatrix)) == 0);
    assert(memcmp(&matrices->projection, &matrices->worldViewProjection,
                  sizeof(GfxCodeMatrix)) == 0);
    assert(memcmp(&matrices->worldView, &matrices->normalizedWorldViewProjection,
                  sizeof(GfxCodeMatrix)) == 0);

    /* The Mac screen-quad shader consumes c23..c26 as four DP4 rows. Its
     * MATRIX_COLUMNS metadata requests the transpose of this OpenGL matrix. */
    float rows[16];
    MatrixTranspose44((float *)&matrices->OGLworldViewProjection, rows);
    const float corners[4][4] = {
        {0, 0, 0, 1}, {(float)width, 0, 0, 1},
        {0, (float)height, 0, 1}, {(float)width, (float)height, 0, 1}
    };
    for (int vertex = 0; vertex < 4; ++vertex) {
        float clip[4] = {0};
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                clip[row] += rows[row * 4 + column] * corners[vertex][column];
        /* D3D9 integer centers map to GL half-integer centers. */
        float expectedX = (vertex & 1 ? 1.0f : -1.0f) + 1.0f / width;
        float expectedY = (vertex & 2 ? -1.0f : 1.0f) - 1.0f / height;
        assert(fabsf(clip[0] - expectedX) < .00001f);
        assert(fabsf(clip[1] - expectedY) < .00001f);
        assert(clip[2] == -1 && clip[3] == 1);
    }
}

int main(void)
{
    renderer.current.integer = 1;
    for (int stack = 0; stack < 3; ++stack) {
        checkViewport(640, 480, stack);
        checkViewport(1280, 720, stack);
        checkViewport(1920, 1080, stack);
    }
    puts("native HUD matrices: 3 viewport sizes and 3 stack levels preserve clip coordinates");
    return 0;
}
