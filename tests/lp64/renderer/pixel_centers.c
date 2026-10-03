#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include "Mac/DirectX_9/MacOpenGLUtils.c"

int MacDisplay_GetCardType(void) { return 0; }

static void Check(int width, int height)
{
    const float d3d[16] = {.9f,0,0,0, 0,1.6f,0,0, 0,0,.9995f,1, 0,0,-4,0};
    float gl[16], offsetX, offsetY;
    memcpy(gl, d3d, sizeof(gl));
    MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(gl, width, height);
    MacOpenGLUtils_GetSubPixelOffset(&offsetX, &offsetY);
    assert(offsetX == 0 && offsetY == 0);
    const float positions[][3] = {{0,0,10}, {13,-27,100}, {-93,84,1000}};
    for (int i = 0; i < 3; ++i) {
        const float *p = positions[i];
        float dc[4] = {0}, gc[4] = {0};
        for (int row = 0; row < 4; ++row) {
            dc[row] = p[0]*d3d[row] + p[1]*d3d[4+row] + p[2]*d3d[8+row] + d3d[12+row];
            gc[row] = p[0]*gl[row] + p[1]*gl[4+row] - p[2]*gl[8+row] + gl[12+row];
        }
        float dx = .5f * width * (dc[0]/dc[3] + 1);
        float dy = .5f * height * (1 - dc[1]/dc[3]);
        float gx = .5f * width * (gc[0]/gc[3] + 1);
        float gy = .5f * height * (gc[1]/gc[3] + 1);
        assert(fabsf(gx - (dx + .5f)) < .0003f);
        assert(fabsf(gy - (height - dy - .5f)) < .0003f);
        assert(fabsf((gc[2]/gc[3] + 1)*.5f - dc[2]/dc[3]) < .000001f);
    }
}

int main(void)
{
    Check(640,480); Check(1280,720); Check(1920,1080);
    puts("native D3D9/GL pixel centers: 3 viewports preserve samples and depth");
}
