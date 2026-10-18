/* Real ABI-1 sidecars/dispatch with platform-only dependencies removed. GL
 * reference sampling uses the production format mapper, in a private context. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "Mac/Metal/metal_backend.h"
#include "Mac/Metal/shim_dispatch.h"

void *vtbl_CDirect3D[32], *vtbl_CDirect3DDevice[128], *vtbl_CDirect3DSurface[32];
void *vtbl_CDirect3DTexture[32], *vtbl_CDirect3DCubeTexture[32], *vtbl_CDirect3DVolumeTexture[32];
void *vtbl_CDirect3DVertexBuffer[32], *vtbl_CDirect3DIndexBuffer[32], *vtbl_CDirect3DVertexDeclaration[32];
void *vtbl_CDirect3DVertexShader[32], *vtbl_CDirect3DPixelShader_ARB[32], *vtbl_CDirect3DSwapChain[32];
const dvar_t *r_fullscreen;
dvar_t *Dvar_RegisterBool(const char *n, int v, int f) { (void)n; (void)v; (void)f; return NULL; }
void R_CapturePendingScreenshotBeforePresent(void) { }
void CDirect3DVertexBuffer_CDirect3DVertexBuffer(const void *p, UINT32 n, DWORD u, D3DPOOL pool) { (void)p; (void)n; (void)u; (void)pool; abort(); }
void CDirect3DIndexBuffer_CDirect3DIndexBuffer(const void *p, UINT32 n, D3DFORMAT f, DWORD u, D3DPOOL pool) { (void)p; (void)n; (void)f; (void)u; (void)pool; abort(); }
int CDirect3DVertexDeclaration_CDirect3DVertexDeclaration(const void *p, const void *d) { (void)p; (void)d; abort(); }
void CDirect3DSurface_CDirect3DSurface(const void *p, int s, unsigned int c, unsigned int l, unsigned int w, unsigned int h, int f, const void *b, void *i) { (void)p; (void)s; (void)c; (void)l; (void)w; (void)h; (void)f; (void)b; (void)i; }
static int configuredWidth, configuredHeight, configuredRefresh, allowContext;
static unsigned int gammaCalls;
static unsigned short capturedGamma[3][256];
uint16_t MacDisplay_SetGammaRamp(const unsigned short *ramp)
{ ++gammaCalls; memcpy(capturedGamma, ramp, sizeof(capturedGamma)); return 0; }
void MacPlatform_ConfigureWindow(int w, int h, int mode, int hz)
{ configuredWidth = w; configuredHeight = h; configuredRefresh = hz; (void)mode; }
void *MacDisplay_CreateScreenContext(int a, int b, int c, int d, int e, void *f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return allowContext ? (void *)(uintptr_t)1 : NULL; }
void MacDisplay_ReleaseDisplay(void) { }
int MacDisplay_SetMode(int w, int h, int depth, int hz)
{ configuredWidth = w; configuredHeight = h; configuredRefresh = hz; (void)depth; return 0; }
void SDL_GL_SwapWindowDirect(void) { }
void MacMetalBackend_Clear(unsigned int f, unsigned int c, float d, unsigned int s) { (void)f; (void)c; (void)d; (void)s; }
void MacMetalFence_Set(unsigned int n) { (void)n; }
int MacDisplay_GetCardType(void) { return 0; }
int MacDisplay_IsGLExtensionSupported(const char *p) { (void)p; return 1; }
UINT32 MacDisplay_GetPCPixelShaderVersion(void) { return 0; }
extern int MacOpenGLUtils_GetOpenGLTextureType(bool *, GLenum *, GLenum *, GLenum *, D3DFORMAT);

_Static_assert(sizeof(D3DINDEXBUFFER_DESC) == 20 && sizeof(D3DVERTEXBUFFER_DESC) == 24, "buffer descriptor ABI");
_Static_assert(sizeof(D3DSURFACE_DESC) == 32, "surface ABI");
_Static_assert(sizeof(D3DVOLUME_DESC) == 28, "volume ABI");
_Static_assert(sizeof(D3DLOCKED_RECT) == 16 && offsetof(D3DLOCKED_RECT, pBits) == 8, "rect ABI");
_Static_assert(sizeof(D3DLOCKED_BOX) == 16 && offsetof(D3DLOCKED_BOX, pBits) == 8, "box ABI");
_Static_assert(sizeof(RECT) == 16 && sizeof(D3DBOX) == 24, "regions ABI");

static CGLContextObj context;
void ResourceGL_Start(void)
{
    CGLPixelFormatAttribute attributes[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0 };
    CGLPixelFormatObj format; GLint count;
    assert(!CGLChoosePixelFormat(attributes, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context)); CGLDestroyPixelFormat(format);
    assert(!CGLSetCurrentContext(context));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ALIGNMENT, 1);
}
void ResourceGL_Stop(void) { CGLSetCurrentContext(NULL); CGLDestroyContext(context); context = NULL; }
void ResourceGL_Read(uint32_t format, unsigned int kind, unsigned int w, unsigned int h,
                     unsigned int d, unsigned int face, unsigned int z,
                     const unsigned char *data, size_t length, unsigned char *out)
{
    GLenum internal = 0, input = 0, type = 0; bool create = 0;
    MacOpenGLUtils_GetOpenGLTextureType(&create, &internal, &input, &type, format);
    assert(create && internal);
    GLenum target = kind == MAC_SHIM_TEXTURE_CUBE ? GL_TEXTURE_CUBE_MAP : kind == MAC_SHIM_TEXTURE_3D ? GL_TEXTURE_3D : GL_TEXTURE_2D;
    GLuint texture, color, framebuffer, program;
    glGenTextures(1, &texture); glBindTexture(target, texture);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (kind == MAC_SHIM_TEXTURE_CUBE) {
        for (unsigned int f = 0; f < 6; ++f) {
            if (type) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, internal, w, h, 0, input, type, data);
            else glCompressedTexImage2DARB(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, internal, w, h, 0, (GLsizei)length, data);
        }
    } else if (kind == MAC_SHIM_TEXTURE_3D) {
        if (type) glTexImage3D(target, 0, internal, w, h, d, 0, input, type, data);
        else glCompressedTexImage3DARB(target, 0, internal, w, h, d, 0, (GLsizei)length, data);
    } else {
        if (type) glTexImage2D(target, 0, internal, w, h, 0, input, type, data);
        else glCompressedTexImage2DARB(target, 0, internal, w, h, 0, (GLsizei)length, data);
    }
    assert(glGetError() == GL_NO_ERROR);
    glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersEXT(1, &framebuffer); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color, 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glBindTexture(target, texture);
    const char *source = kind == MAC_SHIM_TEXTURE_CUBE ? "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], CUBE;\nEND" :
        kind == MAC_SHIM_TEXTURE_3D ? "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 3D;\nEND" :
        "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 2D;\nEND";
    glGenProgramsARB(1, &program); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(source), source);
    assert(glGetError() == GL_NO_ERROR); glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_DITHER);
    glViewport(0, 0, w, h); glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glBegin(GL_QUADS);
    for (unsigned int i = 0; i < 4; ++i) {
        float u = i == 1 || i == 2, v = i >= 2;
        if (kind == MAC_SHIM_TEXTURE_CUBE) {
            float s = 2 * u - 1, t = 2 * v - 1;
            switch (face) {
            case 0: glTexCoord3f(1, -t, -s); break;
            case 1: glTexCoord3f(-1, -t, s); break;
            case 2: glTexCoord3f(s, 1, t); break;
            case 3: glTexCoord3f(s, -1, -t); break;
            case 4: glTexCoord3f(s, -t, 1); break;
            default: glTexCoord3f(-s, -t, -1); break;
            }
        } else glTexCoord3f(u, v, (z + .5f) / d);
        glVertex2f(2 * u - 1, 2 * v - 1);
    }
    glEnd(); glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out);
    assert(glGetError() == GL_NO_ERROR);
    glDisable(GL_FRAGMENT_PROGRAM_ARB); glDeleteProgramsARB(1, &program);
    glDeleteFramebuffersEXT(1, &framebuffer); glDeleteTextures(1, &color); glDeleteTextures(1, &texture);
}


static void DeviceCall(const char *method, void *object, const void *const *args)
{
    int32_t result = -1;
    assert(MacShim_Invoke(method, object, args, &result, sizeof(result)) && result == 0);
}
static unsigned int DeviceRelease(void *object)
{
    unsigned int refs = 99; const void *args[] = { &object };
    assert(MacShim_Invoke("CDirect3DTexture_Release", object, args, &refs, sizeof(refs)));
    return refs;
}
static void DeviceContracts(int *parameters)
{
    allowContext = 1;
    void *device = NULL; void **out = &device;
    const void *create[] = { NULL, NULL, NULL, NULL, NULL, &parameters, &out };
    DeviceCall("CDirect3D_CreateDevice", NULL, create); assert(device);
    uint32_t index = 0, type = 0;
    void *chain = NULL; out = &chain;
    const void *getChain[] = { &device, &index, &out };
    DeviceCall("CDirect3DDevice_GetSwapChain", device, getChain); assert(chain);
    void *back = NULL; out = &back;
    const void *getBack[] = { &chain, &index, &type, &out };
    DeviceCall("CDirect3DSwapChain_GetBackBuffer", chain, getBack); assert(back);
    assert(DeviceRelease(back) == 1);
    assert(DeviceRelease(chain) == 1);

    void *vertex = MacMetal_CreateObject(MAC_SHIM_VERTEX_BUFFER, NULL); assert(vertex);
    void *bound = vertex; uint32_t offset = 4, stride = 16;
    const void *setStream[] = { &device, &index, &bound, &offset, &stride };
    DeviceCall("CDirect3DDevice_SetStreamSource", device, setStream);
    void *returned = NULL; out = &returned;
    uint32_t gotOffset = 0, gotStride = 0; uint32_t *offsetOut = &gotOffset, *strideOut = &gotStride;
    const void *getStream[] = { &device, &index, &out, &offsetOut, &strideOut };
    DeviceCall("CDirect3DDevice_GetStreamSource", device, getStream);
    assert(returned == vertex && gotOffset == offset && gotStride == stride);
    assert(DeviceRelease(returned) == 1);
    bound = NULL; DeviceCall("CDirect3DDevice_SetStreamSource", device, setStream);
    assert(DeviceRelease(vertex) == 0);

    unsigned short ramp[3][256];
    for (unsigned int c = 0; c < 3; ++c) for (unsigned int i = 0; i < 256; ++i) ramp[c][i] = (unsigned short)(c * 71 + i * 97);
    const void *rampInput = ramp; uint32_t flags = 0;
    const void *gammaArgs[] = { &device, &index, &flags, &rampInput };
    unsigned long gammaResult = 99;
    assert(MacShim_Invoke("CDirect3DDevice_SetGammaRamp", device, gammaArgs, &gammaResult, sizeof(gammaResult)));
    assert(gammaResult == 0 && gammaCalls == 1 && !memcmp(ramp, capturedGamma, sizeof(ramp)));
    assert(DeviceRelease(device) == 0);
    assert(!MacMetal_FindSidecar(back) && !MacMetal_FindSidecar(chain));
    allowContext = 0;
}

void ResourceTest_PresentationParameters(void)
{
    int parameters[14] = { 0 };
    parameters[0] = 960;
    parameters[1] = 540;
    parameters[8] = 75;
    parameters[12] = 144;
    int *input = parameters;
    void *device = NULL;
    void **output = &device;
    const void *arguments[] = { NULL, NULL, NULL, NULL, NULL, &input, &output };
    HRESULT result = 0;
    assert(MacShim_Invoke("CDirect3D_CreateDevice", NULL, arguments, &result, sizeof(result)));
    /* The platform stub refuses context creation after recording the request. */
    assert(result != 0);
    assert(configuredWidth == 960 && configuredHeight == 540 && configuredRefresh == 144);

    device = MacMetal_CreateObject(MAC_SHIM_DEVICE, NULL);
    assert(device);
    parameters[0] = 1280;
    parameters[1] = 720;
    parameters[12] = 120;
    const void *resetArguments[] = { &device, &input };
    result = -1;
    assert(MacShim_Invoke("CDirect3DDevice_Reset", device, resetArguments, &result, sizeof(result)));
    assert(result == 0);
    assert(configuredWidth == 1280 && configuredHeight == 720 && configuredRefresh == 120);
    unsigned int refs = 1;
    const void *releaseArguments[] = { &device };
    assert(MacShim_Invoke("CDirect3DDevice_Release", device, releaseArguments, &refs, sizeof(refs)));
    assert(refs == 0);
    DeviceContracts(parameters);
}
