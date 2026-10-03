#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include "Mac/DirectX_9/CDirect3DVolumeTexture.c"
#include "PC/gfx_d3d/r_image_load_common.c"
DxGlobals dx;
int alwaysfails;
void __ZdlPv(void *p) { free(p); }
int MacDisplay_GetCardType(void) { return 0; }
int MacDisplay_IsGLExtensionSupported(const char *name) { (void)name; return 1; }
UINT32 MacDisplay_GetPCPixelShaderVersion(void) { return 0; }
int main(void)
{
    CGLPixelFormatAttribute attrs[] = {kCGLPFAAccelerated, 0};
    CGLPixelFormatObj format; CGLContextObj context; GLint count;
    assert(!CGLChoosePixelFormat(attrs, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context));
    CGLDestroyPixelFormat(format); assert(!CGLSetCurrentContext(context));
    CDirect3DVolumeTextureClean texture = {0};
    CDirect3DVolumeTexture_CDirect3DVolumeTexture((void *)&texture, 2, 2, 2, 1, 0, D3DFMT_A8R8G8B8);
    void *deviceVtable = (void *)1; dx.device = (void *)&deviceVtable;
    byte source[32];
    for (int i = 0; i < 8; ++i) {
        byte color[] = {11, 37, 149, 255};
        memcpy(source + i * 4, color, 4);
    }
    GfxImage image = {0};
    image.mapType = 4; image.width = image.height = image.depth = 2;
    image.texture.volmap = (void *)&texture;
    Image_UploadData(&image, D3DFMT_A8R8G8B8, 0, 0, source);
    assert(!memcmp(texture.pixelData, source, sizeof(source)));
    CDirect3DVolumeClean *volume;
    assert(!((HRESULT (*)(void *, UINT, void **))texture.primaryVtable[18])(&texture, 0, (void **)&volume));
    assert(volume->vtable && volume->vtable[9] && volume->vtable[10]);
    D3DLOCKED_BOX box;
    assert(!((HRESULT (*)(void *, D3DLOCKED_BOX *, const D3DBOX *, DWORD))volume->vtable[9])(volume, &box, NULL, 0));
    assert(box.RowPitch == 8 && box.SlicePitch == 16);
    for (int i = 0; i < 8; ++i) {
        byte color[] = {11, 37, 149, 255};
        memcpy((byte *)box.pBits + i * 4, color, 4);
    }
    assert(!((HRESULT (*)(void *))volume->vtable[10])(volume));
    assert(CDirect3DVolume_IsDirty(volume));
    GLuint sentinel; glGenTextures(1, &sentinel); glBindTexture(GL_TEXTURE_3D, sentinel);
    CDirect3DVolumeTexture_UpdateOpenGLSurfaces((void *)&texture);
    GLint bound; glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound); assert(bound == sentinel);
    assert(!CDirect3DVolume_IsDirty(volume));
    glBindTexture(GL_TEXTURE_3D, texture.texIDStorage);
    byte pixels[32]; glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    assert(!glGetError());
    for (int i = 0; i < 8; ++i) {
        assert(pixels[i * 4] == 149 && pixels[i * 4 + 1] == 37);
        assert(pixels[i * 4 + 2] == 11 && pixels[i * 4 + 3] == 255);
    }
    CDirect3DVolume_Release(volume);
    ZN22CDirect3DVolumeTextureD1Ev(&texture);
    glDeleteTextures(1, &sentinel);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    puts("dirty native volume uploads and restored binding: passed");
}
