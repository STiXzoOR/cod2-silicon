#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PC/gfx_d3d/r_image_load_obj.c"

static byte *file;
static int decoded, uploaded;
int FS_ReadFile(const char *path, void **out)
{
    assert(!strcmp(path, "images/fixture.iwi"));
    file = calloc(1, 30);  /* FS contract: 29 file bytes plus one NUL. */
    GfxImageFileHeader header = {.format = 6, .flags = 2, .dimensions = {1, 1, 1}};
    memcpy(file, &header, sizeof(header));
    memcpy(file, "IWi\005", 4);
    file[28] = 17;
    *out = file;
    return 29;
}
void FS_FreeFile(void *data) { assert(data == file); free(data); file = NULL; }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    int result = vsnprintf(dest, size, fmt, args); va_end(args); return result;
}
void Com_Printf(const char *fmt, ...) { (void)fmt; abort(); }
void *Hunk_AllocateTempMemoryInternal(int size) { return calloc(1, size); }
void Hunk_FreeTempMemory(void *data) { free(data); }
void *__Znam(size_t size) { return malloc(size); }
void __ZdaPv(void *data) { free(data); }
void Image_Setup(GfxImage *image, int w, int h, int d, int flags, int usage, int format)
{
    assert(w == 1 && h == 1 && d == 1 && flags == 2 && !usage && format == 21);
    image->mapType = 3; image->width = w; image->height = h;
}
int Image_CubemapFace(int face) { return face; }
void Image_UploadData(GfxImage *image, int format, int face, int mip, byte *data)
{
    const byte bgra[] = {17, 83, 149, 255};
    assert(image->mapType == 3 && format == 21 && !face && !mip);
    assert(!memcmp(data, bgra, sizeof(bgra))); ++uploaded;
}
void Wavelet_DecompressLevel(byte *previous, byte *dst, WaveletDecode *decode)
{
    assert(!previous && decode->data != file + 28 && decode->data[0] == 17);
    /* Exercise the documented maximum lookahead without depending on a codec. */
    for (int i = 1; i <= 6; ++i) assert(decode->data[i] == 0);
    const byte bgra[] = {17, 83, 149, 255};
    memcpy(dst, bgra, sizeof(bgra)); ++decoded;
}
int main(void)
{
    GfxImage image = {.name = "fixture"};
    assert(Image_LoadFromFile(&image));
    assert(!file && decoded == 1 && uploaded == 1);
    puts("native wavelet loader: explicit six-byte lookahead and BGRA upload");
}
