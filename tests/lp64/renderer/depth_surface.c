/* Synthetic triangles through the production indexed draw, in a private CGL FBO. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* The existing fallback uses a core-profile VAO entrypoint in a legacy context.
 * Record its known GL_INVALID_OPERATION separately; every later GL error is fatal.
 * The call itself still reaches the driver, and all pixel assertions stay active. */
#define glBindVertexArray TraceBindVertexArray
#include "CDirect3DDevice.c"
#undef glBindVertexArray
void TraceBindVertexArray(unsigned int array)
{
    extern void glBindVertexArray(unsigned int);
    assert(glGetError() == GL_NO_ERROR);
    glBindVertexArray(array);
    GLenum error = glGetError();
    fprintf(stderr, "core VAO bind error: %x\n", error);
    assert(error == GL_NO_ERROR || error == GL_INVALID_OPERATION);
}


static GLuint vertexProgram, fragmentProgram;
static materialCommands_t commands;
void *imp_tess = &commands;
void *imp_backEnd;
GfxWorld s_world;
void *vtbl_CDirect3DTexture[1], *vtbl_CDirect3DCubeTexture[1], *vtbl_CDirect3DVolumeTexture[1];
GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *shader) { assert(shader); return vertexProgram; }
GLuint CDirect3DTexture_GetGLName(const void *texture) { assert(!"unexpected texture"); return 0; }
GLuint CDirect3DCubeTexture_GetGLName(const void *texture) { assert(!"unexpected texture"); return 0; }
GLuint CDirect3DVolumeTexture_GetGLName(const void *texture) { assert(!"unexpected texture"); return 0; }
void CDirect3DTexture_UpdateOpenGLSurfaces(const CDirect3DTexture *t) { assert(!"unexpected texture update"); }
void CDirect3DVolumeTexture_UpdateOpenGLSurfaces(const CDirect3DVolumeTexture *t) { assert(!"unexpected volume update"); }
void MacDisplay_GetVideoMemoryInfo(int *video, int *texture) { *video = *texture = 0; }
int MacPlatform_GetRenderSize(int *w, int *h) { *w = *h = 32; return 1; }
int MacDisplay_GetSupportsAnisotropicFiltering(void) { return 0; }
float MacDisplay_GetMaxSupportedAnisotropy(void) { return 1; }
D3DMATRIX *RB_GetActiveWorldMatrix(void) { assert(!"unexpected view matrix"); return NULL; }
D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *a, const D3DXMATRIX *b, const D3DXMATRIX *c) { assert(!"unexpected matrix multiply"); return a; }
int MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(float *m, float w, float h) { assert(!"unexpected projection"); return 0; }

static void EnablePixel(const void *shader)
{
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragmentProgram);
}

static void Set(DeviceImpl *dev, D3DRENDERSTATETYPE state, DWORD value)
{
    assert(!CDirect3DDevice_SetRenderState((const void *)dev, state, value));
}

static void Surface(DeviceImpl *dev, int present)
{
    static int surface;
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev,
        present ? (IDirect3DSurface9 *)&surface : NULL));
}

static void State(int depth, int write, int stencil, unsigned mask)
{
    GLint actual;
    assert(!!glIsEnabled(GL_DEPTH_TEST) == depth);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &actual);
    assert(!!actual == write);
    assert(!!glIsEnabled(GL_STENCIL_TEST) == stencil);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &actual);
    assert((actual & 255) == mask);
}

static void Draw(DeviceImpl *dev)
{
    assert(!CDirect3DDevice_DrawIndexedPrimitive((const void *)dev, D3DPT_TRIANGLELIST, 0, 0, 3, 0, 1));
    assert(glGetError() == GL_NO_ERROR);
}

static void Pixels(int red, float depth, int stencil)
{
    byte color[4], value;
    float z;
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
    glReadPixels(16, 16, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z);
    glReadPixels(16, 16, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &value);
    assert(color[0] == red && color[1] == 0 && color[2] == 0);
    assert(fabsf(z - depth) < 0.00001f);
    assert(value == stencil);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    int arb = !strcmp(argv[1], "arb");
    const char *test = argv[2];
    CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0 };
    CGLPixelFormatObj format;
    CGLContextObj context;
    GLint count;
    GLuint framebuffer, buffers[2];
    assert(!CGLChoosePixelFormat(attrs, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context));
    CGLDestroyPixelFormat(format);
    assert(!CGLSetCurrentContext(context));
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glGenRenderbuffersEXT(2, buffers);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buffers[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 32, 32);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, buffers[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buffers[1]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, 32, 32);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffers[1]);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffers[1]);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0, 0, 32, 32);
    glClearColor(0, 0, 0, 1);
    glClearDepth(.25);
    glClearStencil(0x35);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    struct Vertex { float xyz[3]; byte color[4]; float uv[2]; } vertices[3] = {
        {{-.9f, -.9f, .5f}, {255, 0, 0, 255}, {0, 0}},
        {{ .9f, -.9f, .5f}, {255, 0, 0, 255}, {0, 0}},
        {{  0,  .9f, .5f}, {255, 0, 0, 255}, {0, 0}}
    };
    if (!arb)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) vertices[i].xyz[j] *= 4000;
    unsigned short indices[] = {0, 1, 2};
    CDirect3DVertexBufferClean vb = { .data = (byte *)vertices, .lengthBytes = sizeof(vertices) };
    CDirect3DIndexBufferClean ib = { .data = (byte *)indices, .lengthBytes = sizeof(indices), .indexSizeBytes = 2 };
    D3DVERTEXELEMENT9 elements[] = {{0, 0, 2, 0, 0, 0}, {0, 12, 4, 0, 10, 0}, {255, 0, 17, 0, 0, 0}};
    CDirect3DVertexDeclarationImpl declaration = { .elementCount = 3, .elements = elements };
    void *pixelMethods[8] = { [7] = EnablePixel };
    struct { void **vtable; } pixel = {pixelMethods};
    /* CreateDevice zeroes its storage then calls Init, with implicit GL depth. */
    assert(sizeof(DeviceImpl) <= 4096);
    DeviceImpl dev = {0};
    CDirect3DDevice_Init(&dev);
    dev.streams[0] = (void *)&vb;
    dev.streamStrides[0] = sizeof(vertices[0]);
    dev.indexBuffer = (void *)&ib;
    g_activeVertexDeclaration = (void *)&declaration;
    if (arb) {
        const char *vp = "!!ARBvp1.0\nMOV result.position, vertex.position;\nMOV result.color, vertex.attrib[5];\nEND";
        const char *fp = "!!ARBfp1.0\nMOV result.color, fragment.color.primary;\nEND";
        glGenProgramsARB(1, &vertexProgram);
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vertexProgram);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(vp), vp);
        glGenProgramsARB(1, &fragmentProgram);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragmentProgram);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(fp), fp);
        assert(glGetError() == GL_NO_ERROR);
        g_activeVertexShader = (void *)&vertexProgram;
        dev.pixelShader = (void *)&pixel;
    }
    /* Existing stencil operations isolate the surface enable/mask contract. */
    glStencilFunc(GL_ALWAYS, 0xa6, 255);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (!strcmp(test, "initial")) {
        assert(dev.stencilWriteMask == 0xffffffffu);
        Draw(&dev);
        Pixels(0, .25f, 0x35); /* Near default depth rejects the farther red triangle. */
        Surface(&dev, 1);
        Surface(&dev, 0);
        Surface(&dev, 1);
        State(1, 1, 0, 255); /* Restore the actual constructor's default mask. */
    } else {
        Surface(&dev, 1);
        dev.stencilEnable = 1;
        dev.stencilWriteMask = 255;
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
        glEnable(GL_STENCIL_TEST); glStencilMask(255);
        Surface(&dev, 0);
        if (!strcmp(test, "detach")) {
            State(0, 0, 0, 0);
        } else if (!strcmp(test, "detached-draw")) {
            Draw(&dev);
            Pixels(255, .25f, 0x35);
            State(0, 0, 0, 0);
        } else if (!strcmp(test, "reattach")) {
            Surface(&dev, 1);
            State(1, 1, 1, 255);
            Draw(&dev); Pixels(0, .25f, 0x35);
            Set(&dev, D3DRS_ZFUNC, 8 /* D3DCMP_ALWAYS */);
            Draw(&dev); Pixels(255, .75f, 0xa6);
        } else if (!strcmp(test, "disabled")) {
            Set(&dev, D3DRS_ZENABLE, 0); Set(&dev, D3DRS_ZWRITEENABLE, 0);
            Set(&dev, D3DRS_STENCILENABLE, 0); Set(&dev, D3DRS_STENCILWRITEMASK, 0);
            Surface(&dev, 1);
            State(0, 0, 0, 0);
            Draw(&dev); Pixels(255, .25f, 0x35);
        } else if (!strcmp(test, "clear-detached")) {
            Draw(&dev); Pixels(255, .25f, 0x35);
            assert(!CDirect3DDevice_Clear((const void *)&dev, 0, NULL, 7, 0, .9f, 0x77));
            State(0, 0, 0, 0);
            Pixels(0, .25f, 0x35);
            Draw(&dev); Pixels(255, .25f, 0x35);
        } else if (!strcmp(test, "clear-attached")) {
            Surface(&dev, 1);
            Set(&dev, D3DRS_ZWRITEENABLE, 0);
            Set(&dev, D3DRS_STENCILWRITEMASK, 0x0f);
            assert(!CDirect3DDevice_Clear((const void *)&dev, 0, NULL, 7, 0xff000000, .9f, 0x77));
            State(1, 0, 1, 0x0f);
            Pixels(0, .9f, 0x77);
            Draw(&dev); Pixels(255, .9f, 0x76);
        } else if (!strcmp(test, "stencil-detached")) {
            Set(&dev, D3DRS_STENCILENABLE, 0);
            Set(&dev, D3DRS_STENCILENABLE, 1);
            State(0, 0, 1, 0); /* Mac applies enable even without a surface. */
            Surface(&dev, 0);
            State(0, 0, 1, 0); /* Repeated NULL does not change effective state. */
            DWORD value = 0;
            assert(!CDirect3DDevice_GetRenderState((const void *)&dev, D3DRS_STENCILENABLE, &value));
            assert(value == 1);
            Draw(&dev); Pixels(255, .25f, 0x35);
        } else {
            assert(!strcmp(test, "changed-detached"));
            Set(&dev, D3DRS_ZENABLE, 0); Set(&dev, D3DRS_ZENABLE, 1);
            Set(&dev, D3DRS_ZWRITEENABLE, 0); Set(&dev, D3DRS_ZWRITEENABLE, 1);
            /* Mac STENCILENABLE is unconditional even when detached; unchanged here. */
            Set(&dev, D3DRS_STENCILWRITEMASK, 0x10f);
            Set(&dev, D3DRS_ZFUNC, 8 /* D3DCMP_ALWAYS */);
            Draw(&dev); Pixels(255, .25f, 0x35);
            State(0, 0, 0, 0);
            Surface(&dev, 1);
            State(1, 1, 1, 0x0f);
            DWORD value = 0;
            assert(!CDirect3DDevice_GetRenderState((const void *)&dev, D3DRS_STENCILWRITEMASK, &value));
            assert(value == 0x0f);
            Draw(&dev); Pixels(255, .75f, 0x36);
        }
    }
    assert(glGetError() == GL_NO_ERROR);
    glDeleteProgramsARB(1, &vertexProgram); glDeleteProgramsARB(1, &fragmentProgram);
    glDeleteRenderbuffersEXT(2, buffers); glDeleteFramebuffersEXT(1, &framebuffer);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    puts("depth surface: synthetic color, depth, stencil and effective state passed");
    return 0;
}
