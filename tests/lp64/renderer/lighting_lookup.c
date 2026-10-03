#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_image_load_obj.c"

static byte uploaded[6][32 * 32 * 4];
static int uploadCount;
void Image_Setup(GfxImage *image, int w, int h, int d, int flags, int usage, int format)
{
    assert(w == 32 && h == 32 && d == 1 && (flags == 3 || flags == 7));
    assert(!usage && format == 21);
    image->width = w; image->height = h;
}
int Image_CubemapFace(int face) { return face; }
void Image_UploadData(GfxImage *image, int format, int face, int mip, byte *data)
{
    assert(image->width == 32 && format == 21 && face < 6 && !mip);
    memcpy(uploaded[face], data, sizeof(uploaded[face]));
    ++uploadCount;
}
float Vec2Normalize(float *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1]);
    assert(isfinite(length) && length > 0);
    v[0] /= length; v[1] /= length;
    return length;
}
int Vec3MajorAxis(const float *v)
{
    int axis = 0;
    if (fabsf(v[1]) > fabsf(v[axis])) axis = 1;
    if (fabsf(v[2]) > fabsf(v[axis])) axis = 2;
    return axis;
}
void AxisTransformVector(vec3_t *axes, float x, float y, float z, float *out)
{
    for (int i = 0; i < 3; ++i)
        out[i] = axes[0][i] * x + axes[1][i] * y + axes[2][i] * z;
}

int main(void)
{
    GfxImage image = {0};
    byte cube0[sizeof(uploaded)], cube1[sizeof(uploaded)];
    Image_LoadLightmapWeights(&image);
    assert(uploadCount == 1);
    int directional = 0;
    for (int i = 0; i < 32 * 32; ++i) {
        const byte *p = uploaded[0] + i * 4;
        int sum = p[0] + p[1] + p[2] + p[3];
        assert(sum >= 254 && sum <= 256);
        if (!p[0]) ++directional;
    }
    assert(directional > 200);  /* Directions outside the central lobe. */
    assert(uploaded[0][(15 * 32 + 15) * 4] > 240);

    uploadCount = 0;
    Image_GenerateCubemapFunction(&image, cube0, 32, 0, Image_GetLightGridWeightsForVector);
    Image_GenerateCubemapFunction(&image, cube1, 32, 1, Image_GetLightGridWeightsForVector);
    assert(uploadCount == 12);
    for (int i = 0; i < 6 * 32 * 32; ++i) {
        int sum = 0;
        for (int channel = 0; channel < 4; ++channel)
            sum += cube0[i * 4 + channel] + cube1[i * 4 + channel];
        assert(sum >= 252 && sum <= 258);
    }
    /* Opposing cubemap directions must not collapse to identical textures. */
    assert(memcmp(cube0, cube0 + 32 * 32 * 4, 32 * 32 * 4));
    puts("native generated lighting weights: finite, normalized, distinct cubemap faces");
}
