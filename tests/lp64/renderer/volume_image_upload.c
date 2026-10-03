#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_image_load_common.c"

DxGlobals dx;
int alwaysfails;
static byte destination[1024];
static int rowPitch, slicePitch, expectedLevel, lockCount, unlockCount;

static int LockBox(void *texture, int level, D3DLOCKED_BOX *locked, void *box, int flags)
{
    assert(texture && level == expectedLevel && !box && !flags);
    locked->RowPitch = rowPitch;
    locked->SlicePitch = slicePitch;
    locked->pBits = destination;
    ++lockCount;
    return 0;
}

static int UnlockBox(void *texture, int level)
{
    assert(texture && level == expectedLevel);
    ++unlockCount;
    return 0;
}

static void CheckUpload(int width, int height, int depth, int mip, D3DFORMAT format,
                        int sourceRowBytes, int sourceRows, int sourceDepth)
{
    void *vtable[21] = {0};
    void **texture = vtable;
    int device = 1;
    GfxImage image = {0};
    byte source[512], expected[sizeof(destination)];
    int sourceSlice = sourceRowBytes * sourceRows;
    int before = lockCount;

    assert(sourceSlice * sourceDepth <= sizeof(source));
    for (int index = 0; index < sizeof(source); ++index)
        source[index] = (byte)(index * 13 + 7);
    memset(destination, 0xcc, sizeof(destination));
    memcpy(expected, destination, sizeof(expected));
    rowPitch = sourceRowBytes + 8;
    slicePitch = rowPitch * sourceRows + 12;
    expectedLevel = mip;
    for (int z = 0; z < sourceDepth; ++z)
        for (int row = 0; row < sourceRows; ++row)
            memcpy(expected + z * slicePitch + row * rowPitch,
                   source + z * sourceSlice + row * sourceRowBytes, sourceRowBytes);
    vtable[19] = (void *)LockBox;
    vtable[20] = (void *)UnlockBox;
    dx.device = (IDirect3DDevice9 *)&device;
    image.mapType = 4;
    image.texture.volmap = (IDirect3DVolumeTexture9 *)&texture;
    image.width = width;
    image.height = height;
    image.depth = depth;
    Image_UploadData(&image, format, 0, mip, source);
    assert(lockCount == before + 1 && unlockCount == lockCount);
    /* Distinct slices must survive both destination row and slice padding. */
    assert(!memcmp(destination, expected, sizeof(destination)));
}

int main(void)
{
    CheckUpload(4, 3, 2, 0, D3DFMT_A8R8G8B8, 16, 3, 2);
    CheckUpload(8, 6, 4, 1, D3DFMT_X8R8G8B8, 16, 3, 2);
    CheckUpload(5, 5, 3, 0, D3DFMT_DXT1, 16, 2, 3);
    CheckUpload(5, 5, 3, 0, D3DFMT_DXT5, 32, 2, 3);
    CheckUpload(2, 2, 2, 3, D3DFMT_A8R8G8B8, 4, 1, 1);
    puts("native volume image upload: padded rows, slices, mips and DXT blocks passed");
    return 0;
}
