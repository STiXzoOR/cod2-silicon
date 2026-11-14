#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_image_load_obj.c"

static int uploads;
static float reflectance, incidentZ;
const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int i = 0; i < 3; ++i) out[i] = v[i] / length;
    return length;
}
float FresnelTerm(float n0, float n1, float cosIncidentAngle)
{
    assert(n0 == 1.0f && n1 == BM_I2F(0x3faa9fbe));
    assert(cosIncidentAngle == incidentZ);
    return reflectance;
}
void Image_Setup(GfxImage *image, int width, int height, int depth, int flags, int usage, int format)
{
    assert(width == 1 && height == 1 && depth == 1 && flags == 2 && !usage && format == 22);
    image->mapType = 3;
    image->width = width;
    image->height = height;
    image->picmip.platform[0] = 0;
}
int Image_CubemapFace(int face) { return face; }
void *Hunk_AllocateTempMemoryInternal(int size) { return malloc(size); }
void Hunk_FreeTempMemory(void *data) { free(data); }
void Image_UploadData(GfxImage *image, int format, int face, int mip, byte *data)
{
    const byte bgra[] = {17, 83, 149, 255};
    assert(image->mapType == 3 && format == 22 && !face && !mip);
    assert(!memcmp(data, bgra, sizeof(bgra)));
    ++uploads;
}
int main(void)
{
    GfxImage image = {0};
    GfxImageFileHeader header = {.format = 2, .flags = 2, .dimensions = {1, 1, 1}};
    const byte bgr[] = {17, 83, 149};
    Image_LoadBitmap(&image, &header, bgr, (D3DFORMAT)22, 3);
    assert(uploads == 1);
    /* The Windows watercolor callback preserves RGB while view-dependent
       Fresnel drives texture alpha. A little-endian Mac ABGR word made the
       blue channel vary with view direction instead. */
    const float directions[][3] = {{3, 0, 4}, {0, 4, -3}, {4, 3, 0}};
    const byte expectedAlpha[] = {0, 128, 255};
    for (int i = 0; i < 3; ++i) {
        byte pixel[4];
        incidentZ = directions[i][2] / 5.0f;
        reflectance = i * 0.5f;
        Image_GetWaterColorForVector(directions[i], (int)0xff33404du, pixel);
        assert(pixel[0] == 77 && pixel[1] == 64 && pixel[2] == 51);
        assert(pixel[3] == expectedAlpha[i]);
    }
    puts("native bitmap and water Fresnel BGRA channels: passed");
}
