/* Synthetic pixels only: exercise the production saved-screen copy. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Mac/DirectX_9/CDirect3DDevice.c"

static GLuint savedTexture;
static int destinationSurface;

int CDirect3DSurface_GetGLBlitInfo(const void *surface, unsigned int *texture,
                                  unsigned int *width, unsigned int *height)
{
    if (surface != &destinationSurface)
        return 0; /* The backbuffer has no texture owner. */
    if (texture) *texture = savedTexture;
    if (width) *width = 32;
    if (height) *height = 32;
    return 1;
}

static HRESULT SurfaceDesc(const void *surface, D3DSURFACE_DESC *desc)
{
    assert(surface);
    memset(desc, 0, sizeof(*desc));
    desc->Width = desc->Height = 32;
    return 0;
}

int main(void)
{
    CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0 };
    CGLPixelFormatObj format;
    CGLContextObj context;
    GLint count;
    GLuint target, framebuffer, binding;
    byte pixels[32 * 32 * 4] = { 0 };
    void *surfaceMethods[13] = { 0 };
    struct { void **vtable; } backBuffer = { surfaceMethods };
    DeviceImpl device = { 0 };
    surfaceMethods[12] = SurfaceDesc;
    device.backBuffer = device.renderTarget = (IDirect3DSurface9 *)&backBuffer;
    assert(!CGLChoosePixelFormat(attrs, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context));
    CGLDestroyPixelFormat(format);
    assert(!CGLSetCurrentContext(context));
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0, 0, 32, 32);
    glClearColor(.2f, .4f, .6f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 16, 32, 16);
    glClearColor(.6f, .4f, .2f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glGenTextures(1, &savedTexture);
    glBindTexture(GL_TEXTURE_2D, savedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenTextures(1, &binding);
    glBindTexture(GL_TEXTURE_2D, binding);
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    assert(!CDirect3DDevice_StretchRect((CDirect3DDevice *)&device,
        device.backBuffer, NULL, (IDirect3DSurface9 *)&destinationSurface, NULL, D3DTEXF_LINEAR));
    GLint state;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state);
    assert(state == GL_TEXTURE1_ARB);
    assert(glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST));
    glGetIntegerv(GL_DEPTH_WRITEMASK, &state);
    assert(state);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state);
    assert(state == (GLint)binding);
    glBindTexture(GL_TEXTURE_2D, savedTexture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            byte *pixel = pixels + (y * 32 + x) * 4;
            assert(pixel[0] == (y < 16 ? 51 : 153));
            assert(pixel[1] == 102 && pixel[2] == (y < 16 ? 153 : 51) && pixel[3] == 255);
        }
    }
    assert(glGetError() == GL_NO_ERROR);
    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target); glDeleteTextures(1, &savedTexture); glDeleteTextures(1, &binding);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    puts("native saved screen: copied framebuffer pixels, orientation and GL state passed");
}
