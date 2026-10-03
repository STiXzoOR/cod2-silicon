#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include "Mac/DirectX_9/CDirect3DVolumeTexture.c"
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
    CDirect3DVolumeClean *volume;
    assert(!CDirect3DVolumeTexture_GetVolumeLevel((void *)&texture, 0, (void **)&volume));
    D3DLOCKED_BOX box;
    assert(!CDirect3DVolume_LockBox(volume, &box, NULL, 0));
    assert(box.RowPitch == 8 && box.SlicePitch == 16);
    for (int i = 0; i < 8; ++i) {
        byte color[] = {11, 37, 149, 255};
        memcpy((byte *)box.pBits + i * 4, color, 4);
    }
    assert(!CDirect3DVolume_UnlockBox(volume));
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
