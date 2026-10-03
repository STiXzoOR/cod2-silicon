#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>

extern void CDirect3DTexture_CDirect3DTexture(const CDirect3DTexture *, UINT32, UINT32, UINT32, DWORD, D3DFORMAT);
extern HRESULT CDirect3DTexture_LockRect(const CDirect3DTexture *, UINT, D3DLOCKED_RECT *, const RECT *, DWORD);
extern HRESULT CDirect3DTexture_UnlockRect(const CDirect3DTexture *, UINT);
extern DWORD CDirect3DTexture_GetLevelCount(const CDirect3DTexture *);
extern GLuint CDirect3DTexture_GetGLName(const void *);
extern void ZN16CDirect3DTextureD1Ev(void *);

void *vtbl_CDirect3DTexture_secondary[3];
void *vtbl_CDirect3DCubeTexture[1];
bool g_NoTextureID, g_WarmOff;
void __ZdlPv(void *pointer) { free(pointer); }
GLuint CDirect3DCubeTexture_GetGLName(const void *texture) { (void)texture; return 0; }
int MacDisplay_GetCardType(void) { return 0; }
int MacDisplay_IsGLExtensionSupported(const char *extension) { (void)extension; return 1; }
UINT32 MacDisplay_GetPCPixelShaderVersion(void) { return 0; }

static void DrawTransparentTexture(UINT32 levels)
{
    /* Synthetic DXT5: white RGB and zero alpha throughout each 4x4 block. */
    static const byte block[16] = {0, 1, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0};
    CDirect3DTexture *texture = calloc(1, 256);
    byte pixel[4];
    GLuint glName;
    UINT32 mip;

    assert(texture);
    CDirect3DTexture_CDirect3DTexture(texture, 8, 8, levels, 0, D3DFMT_DXT5);
    for (mip = 0; mip < CDirect3DTexture_GetLevelCount(texture); ++mip) {
        D3DLOCKED_RECT locked;
        UINT32 width = 8 >> mip;
        UINT32 blocks = (width + 3) / 4;
        UINT32 index;
        if (!blocks)
            blocks = 1;
        assert(!CDirect3DTexture_LockRect(texture, mip, &locked, NULL, 0));
        for (index = 0; index < blocks * blocks; ++index)
            memcpy((byte *)locked.pBits + index * sizeof(block), block, sizeof(block));
        assert(!CDirect3DTexture_UnlockRect(texture, mip));
    }
    glName = CDirect3DTexture_GetGLName(texture);
    assert(glName);
    glBindTexture(GL_TEXTURE_2D, glName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glClearColor(.2f, .4f, .6f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(glGetError() == GL_NO_ERROR);
    printf("mip levels %u: transparent HUD pixel = %u,%u,%u,%u\n",
           levels, pixel[0], pixel[1], pixel[2], pixel[3]);
    fflush(stdout);
    /* A partial mip chain must preserve the background, never become a white quad. */
    assert(abs((int)pixel[0] - 51) <= 1);
    assert(abs((int)pixel[1] - 102) <= 1);
    assert(abs((int)pixel[2] - 153) <= 1);
    assert(pixel[3] == 255);
    ZN16CDirect3DTextureD1Ev(texture);
    free(texture);
}

int main(void)
{
    CGLPixelFormatAttribute attributes[] = {kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0};
    CGLPixelFormatObj format;
    CGLContextObj context;
    GLint count;
    GLuint framebuffer, target;

    assert(!CGLChoosePixelFormat(attributes, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context));
    CGLDestroyPixelFormat(format);
    assert(!CGLSetCurrentContext(context));
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0, 0, 32, 32);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);
    DrawTransparentTexture(1);
    DrawTransparentTexture(2);
    DrawTransparentTexture(0);
    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target);
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(context);
    puts("partial-mip HUD texture transparency: passed");
    return 0;
}
