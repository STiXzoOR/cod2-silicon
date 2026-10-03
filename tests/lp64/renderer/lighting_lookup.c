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

/* Instruction model: Mac 1.3 0xfd152 and Windows 1.3 0x1000feb0 retain
 * normalized y between texels; only the x coordinate is reset each time. */
static void OriginalLightmapWeights(byte *pixels)
{
    for (int row = 0; row < 32; ++row) {
        float y = (row + 0.5f) * 0.03125f * 2.0f - 1.0f;
        for (int column = 0; column < 32; ++column) {
            float x = (column + 0.5f) * 0.03125f * 2.0f - 1.0f;
            float z = 1.0f - (x * x + y * y);
            if (z < 0.0f) {
                float length = sqrtf(x * x + y * y);
                x /= length;
                y /= length;
                z = 0.0f;
            }
            float phase = (float)(atan2((double)y, (double)x) * 0.477464829275686 - 0.75);
            if (phase < 0.0f) phase += 3.0f;
            else if (phase > 3.0f) phase -= 3.0f;
            float weights[4];
            weights[0] = 1.0f + acosf(sqrtf(z)) / -0.9553166031837463f;
            if (weights[0] < 0.0f) weights[0] = 0.0f;
            if (weights[0] > 1.0f) weights[0] = 1.0f;
            if (phase < 1.0f) {
                weights[1] = 1.0f - phase; weights[2] = phase; weights[3] = 0.0f;
            } else if (phase < 2.0f) {
                weights[1] = 0.0f; weights[3] = phase - 1.0f; weights[2] = 1.0f - weights[3];
            } else {
                weights[1] = phase - 2.0f; weights[3] = 1.0f - weights[1]; weights[2] = 0.0f;
            }
            for (int channel = 0; channel < 4; ++channel) {
                if (channel) weights[channel] *= 1.0f - weights[0];
                pixels[(row * 32 + column) * 4 + channel] = (byte)(int)floorf(weights[channel] * 255.0f + 0.5f);
            }
        }
    }
}

int main(void)
{
    GfxImage image = {0};
    byte cube0[sizeof(uploaded)], cube1[sizeof(uploaded)];
    byte originalWeights[sizeof(uploaded[0])];
    Image_LoadLightmapWeights(&image);
    assert(uploadCount == 1);
    OriginalLightmapWeights(originalWeights);
    assert(!memcmp(uploaded[0], originalWeights, sizeof(originalWeights)));
    /* These off-center rows distinguish persistent normalization from a
     * freshly reset direction, even though both tables partition unity. */
    assert(!memcmp(uploaded[0] + (8 * 32 + 15) * 4, (byte[]){135, 0, 64, 56}, 4));
    assert(!memcmp(uploaded[0] + (24 * 32 + 15) * 4, (byte[]){124, 126, 4, 0}, 4));
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
