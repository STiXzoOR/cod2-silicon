/* Synthetic pixels only: exercise the production saved-screen copy. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Mac/DirectX_9/CDirect3DDevice.c"

CXX_BOOL g_InhibitCopy;
#if defined(COD2_X64)
CXX_BOOL g_RenderToShadowCookie;
UINT32 g_NumBlurShadowPasses;
static GLuint savedTexture;
static void *surfaceMethods[13];
static struct { void **vtable; } destinationSurface = { surfaceMethods };
static COpenGLNative openGL;
void *imp__ZN7COpenGL7sOpenGLE = &openGL;

int CDirect3DSurface_GetNativeInfo(const void *surface, CDirect3DSurfaceNativeInfo *info)
{
    static COpenGLTextureStateNative owner;
    if (surface != &destinationSurface) return 0;
    owner.name = &savedTexture;
    owner.width = owner.height = 32;
    *info = (CDirect3DSurfaceNativeInfo){ &owner, savedTexture, GL_TEXTURE_2D, 0, 32, 32 };
    return 1;
}

unsigned int COpenGL_SetActiveTexUnit(const COpenGL *gl, UINT32 unit)
{
    ((COpenGLNative *)gl)->activeTexUnit = unit;
    glActiveTextureARB(GL_TEXTURE0_ARB + unit);
    return 0;
}

int MacDisplay_GetCardType(void) { return 0; }
UINT32 MacOpenGLUtils_GetPCPixelShaderVersion(void) { return 0; }
uint16_t MacDisplay_GetCurrentDimensions(int *w, int *h) { *w = *h = 32; return 0; }
bool CDirect3DSurface_IsDirty(const CDirect3DSurface *s) { return 0; }
void CDirect3DSurface_SetDirty(void *s, int dirty) { }
void CDirect3DSurface_MarkGPUWritten(void *s) { }
void CDirect3DSurface_UpdateOpenGLSurfaceObject(const CDirect3DSurface *s, int recreate) { }
static GLenum FixtureBuffer(GLenum buffer)
{
    /* The production context maps its BACK/AUX buffers to FBO attachments. */
    if (buffer == GL_BACK) return GL_COLOR_ATTACHMENT0_EXT;
    if (buffer == GL_AUX0 || buffer == GL_AUX1)
        return GL_COLOR_ATTACHMENT1_EXT + buffer - GL_AUX0;
    return buffer;
}
void MacGL_DrawBuffer(unsigned int buffer) { glDrawBuffer(FixtureBuffer(buffer)); }
void MacGL_ReadBuffer(unsigned int buffer) { glReadBuffer(FixtureBuffer(buffer)); }

static ULONG SurfaceRef(void *surface) { return 1; }
#else
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
#endif

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
#if !defined(COD2_X64)
    void *surfaceMethods[13] = { 0 };
#endif
    struct { void **vtable; } backBuffer = { surfaceMethods };
    DeviceImpl device = { 0 };
#if defined(COD2_X64)
    surfaceMethods[1] = surfaceMethods[2] = SurfaceRef;
#endif
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
    /* Leaving an offscreen target resolves its pixels before the next pass
     * samples that texture. This is the Gaussian ping-pong/feedback boundary. */
    memset(pixels, 0, sizeof(pixels));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, binding);
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glViewport(7, 9, 128, 64);
    device.renderTarget = (IDirect3DSurface9 *)&destinationSurface;
    assert(!CDirect3DDevice_SetRenderTarget((CDirect3DDevice *)&device, 0, device.backBuffer));
    assert(device.renderTarget == device.backBuffer);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    assert(viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 32 && viewport[3] == 32);
    assert(device.viewportW == 32 && device.viewportH == 32);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state);
    assert(state == GL_TEXTURE1_ARB);
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
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    g_InhibitCopy = 1;
    device.renderTarget = (IDirect3DSurface9 *)&destinationSurface;
    assert(!CDirect3DDevice_SetRenderTarget((CDirect3DDevice *)&device, 0, device.backBuffer));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    assert(pixels[0] == 51 && pixels[1] == 102 && pixels[2] == 153);
    g_InhibitCopy = 0;
    assert(glGetError() == GL_NO_ERROR);
    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target); glDeleteTextures(1, &savedTexture); glDeleteTextures(1, &binding);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    puts("native saved screen: copied framebuffer pixels, orientation and GL state passed");
}
