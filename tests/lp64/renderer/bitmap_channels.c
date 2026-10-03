#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_image_load_obj.c"

static int uploads;
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
    puts("native bitmap BGR to BGRA channels: passed");
}
