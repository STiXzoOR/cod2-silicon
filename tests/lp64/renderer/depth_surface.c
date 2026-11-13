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
#if defined(COD2_X64)
#include "Mac/DirectX_9/CDirect3DVertexShader.c"
intptr_t g_Low, g_High;
extern ULONG CDirect3DVertexBuffer_AddRef(const CDirect3DVertexBuffer *);
extern ULONG CDirect3DVertexBuffer_Release(const CDirect3DVertexBuffer *);
#endif
#if defined(COD2_X64)
/* The production initializer invalidates these original device caches. */
CXX_BOOL CDirect3DDevice_mNeedsVertexShaderValidation;
CXX_BOOL CDirect3DDevice_mNeedsTransformationValidation;
CXX_BOOL CDirect3DDevice_mNeedsRasterizationValidation;
#endif
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
Bool g_NoTextureID, g_WarmOff;
/* Isolate generated ABI import data; exercise real vertex-array initialization. */
#if defined(COD2_X64)
static void FixtureArrayDtor(const void *array) { (void)array; }
static void *fixtureArrayVtables[16] = { [2] = FixtureArrayDtor };
void *imp___ZTV14CTexCoordArray = fixtureArrayVtables;
void *imp___ZTV10COpenGLVAO = fixtureArrayVtables;
extern bool CVAOPacketFixedFunction_IsFixedFunction(const CVAOPacket *);
extern bool CVAOPacketProgrammable_IsFixedFunction(const CVAOPacket *);
static void *fixtureFixedPacketVtable[5] = { [4] = CVAOPacketFixedFunction_IsFixedFunction };
static void *fixtureProgramPacketVtable[5] = { [4] = CVAOPacketProgrammable_IsFixedFunction };
void *imp___ZTV23CVAOPacketFixedFunction = fixtureFixedPacketVtable;
void *imp___ZTV22CVAOPacketProgrammable = fixtureProgramPacketVtable;
VAOStatus CVAOPacket_sVAOStatus;
UINT32 CVAOPacket_sCurrentPacket;
static Bool fixtureSpecial;
void *imp_g_special = &fixtureSpecial;
extern GLint g_VAOID;
void *imp_g_VAOID = &g_VAOID;
#else
static void *fixtureArrayVtables[16];
#endif
void *imp___ZTV7CBaseVA = fixtureArrayVtables;
void *imp___ZTV11CColorArray = fixtureArrayVtables;
void *imp___ZTV20CSecondaryColorArray = fixtureArrayVtables;
void *imp___ZTV12CNormalArray = fixtureArrayVtables;
void *imp___ZTV12CVertexArray = fixtureArrayVtables;
#if !defined(COD2_X64)
GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *shader) { assert(shader); return vertexProgram; }
#endif
void MacDisplay_GetVideoMemoryInfo(int *video, int *texture) { *video = *texture = 0; }
int MacPlatform_GetRenderSize(int *w, int *h) { *w = *h = 32; return 1; }
int MacDisplay_GetSupportsAnisotropicFiltering(void) { return 0; }
float MacDisplay_GetMaxSupportedAnisotropy(void) { return 1; }
D3DMATRIX *RB_GetActiveWorldMatrix(void) { assert(!"unexpected view matrix"); return NULL; }
#if defined(COD2_X64)
int MacDisplay_GetCardType(void) { return 0; }
#else
D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *a, const D3DXMATRIX *b, const D3DXMATRIX *c) { assert(!"unexpected matrix multiply"); return a; }
#endif

static void EnablePixel(const void *shader)
{
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragmentProgram);
}

static void Set(DeviceImpl *dev, D3DRENDERSTATETYPE state, DWORD value)
{
    assert(!CDirect3DDevice_SetRenderState((const void *)dev, state, value));
}

#if defined(COD2_X64)
static void *FixtureSurface(void)
{
    static void *surface;
    if (!surface) {
        surface = calloc(1, 128);
        CDirect3DSurface_CDirect3DSurface(surface, 0, 0, 0, 32, 32, D3DFMT_A8R8G8B8, NULL, NULL);
    }
    return surface;
}
#endif

static void Surface(DeviceImpl *dev, int present)
{
#if defined(COD2_X64)
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev,
        present ? FixtureSurface() : NULL));
#else
    static int surface;
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev,
        present ? (IDirect3DSurface9 *)&surface : NULL));
#endif
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
    if (color[0] != red || color[1] || color[2])
        fprintf(stderr, "actual RGB=%u,%u,%u expected=%d,0,0 depth=%g stencil=%u\n",color[0],color[1],color[2],red,z,value);
    assert(color[0] == red && color[1] == 0 && color[2] == 0);
    assert(fabsf(z - depth) < 0.00001f);
    assert(value == stencil);
}

#if defined(COD2_X64)
static void DepthIdentity(DeviceImpl *dev)
{
    IDirect3DSurface9 *receiver, *cookie;
    dev->backBuffer = dev->renderTarget;
    assert(!CDirect3DDevice_CreateDepthStencilSurface((const void *)dev,
        32, 32, D3DFMT_D24S8, 0, 0, 0, &receiver, NULL));
    assert(!CDirect3DDevice_CreateDepthStencilSurface((const void *)dev,
        16, 16, D3DFMT_D24S8, 0, 0, 0, &cookie, NULL));
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev, receiver));
    assert(!CDirect3DDevice_Clear((const void *)dev, 0, NULL, 7, 0xff000000, .25f, 3));
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev, cookie));
    D3DVIEWPORT9 small = {0, 16, 16, 16, 0, 1};
    assert(!CDirect3DDevice_SetViewport((const void *)dev, &small));
    assert(!CDirect3DDevice_Clear((const void *)dev, 0, NULL, 7, 0xff000000, 1, 0));
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev, receiver));
    float depths[32 * 32];
    byte stencils[32 * 32];
    glReadPixels(0, 0, 32, 32, GL_DEPTH_COMPONENT, GL_FLOAT, depths);
    glReadPixels(0, 0, 32, 32, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencils);
    unsigned changed = 0;
    for (unsigned i = 0; i < 32 * 32; ++i)
        changed += fabsf(depths[i] - .25f) > .00001f || stencils[i] != 3;
    fprintf(stderr, "receiver depth/stencil pixels changed by cookie clear: %u\n", changed);
    assert(changed == 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev, NULL));
    CDirect3DSurface_Release((const void *)receiver);
    CDirect3DSurface_Release((const void *)cookie);
}
#endif

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

    /* The 24-byte stream consumes BGRA; use red in that byte order. */
    struct Vertex { float xyz[3]; byte color[4]; float uv[2]; } vertices[3] = {
        {{-.9f, -.9f, .5f}, {0, 0, 255, 255}, {0, 0}},
        {{ .9f, -.9f, .5f}, {0, 0, 255, 255}, {0, 0}},
        {{  0,  .9f, .5f}, {0, 0, 255, 255}, {0, 0}}
    };
    if (!arb)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) vertices[i].xyz[j] *= 4000;
    unsigned short indices[] = {0, 1, 2};
    CDirect3DVertexBufferClean vb = { .data = (byte *)vertices, .lengthBytes = sizeof(vertices) };
#if defined(COD2_X64)
    /* The indexed driver retains stream zero even for borrowed stack data. */
    void *bufferMethods[16] = { [1] = CDirect3DVertexBuffer_AddRef, [2] = CDirect3DVertexBuffer_Release };
    vb.vtable = bufferMethods;
    vb.refCount = 1;
    CDirect3DVertexShaderImpl shader = {0};
    /* Both modes supply the shader owner; D3D_PROG selects ARB or approximation. */
#endif
    CDirect3DIndexBufferClean ib = { .data = (byte *)indices, .lengthBytes = sizeof(indices), .indexSizeBytes = 2 };
    D3DVERTEXELEMENT9 elements[] = {{0, 0, 2, 0, 0, 0}, {0, 12, 4, 0, 10, 0}, {255, 0, 17, 0, 0, 0}};
    CDirect3DVertexDeclarationImpl declaration = { .elementCount = 3, .elements = elements };
    void *pixelMethods[8] = { [7] = EnablePixel };
#if defined(COD2_X64)
    struct { void **vtable; ULONG refCount; UINT32 textureUsageMask; } pixel = {pixelMethods, 1, 0};
#else
    struct { void **vtable; } pixel = {pixelMethods};
#endif
    /* CreateDevice zeroes its storage then calls Init, with implicit GL depth. */
    assert(sizeof(DeviceImpl) <= 4096);
    DeviceImpl dev = {0};
    CDirect3DDevice_Init(&dev);
#if defined(COD2_X64)
    /* Init-only setup supplies COpenGL_Init's initial depth state. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    /* Reset normally owns the 1x1 fallback texture; this Init-only fixture
     * supplies the same widened secondary-base contract for raster validation. */
    fprintf(stderr, "native texture units: fixed=%u image=%u\n", dev.maxTextureUnits, dev.maxTextureImageUnits);
    COpenGLNative *fixtureGL = CDirect3DDevice_GL();
    fixtureGL->texUnitCount = dev.maxTextureUnits > dev.maxTextureImageUnits
        ? dev.maxTextureUnits : dev.maxTextureImageUnits;
    for (UINT i = 0; i < fixtureGL->texUnitCount; ++i)
        fixtureGL->texUnits[i].mIsProgramableOnly = i >= dev.maxTextureUnits;
    GLuint defaultTextureName;
    UINT32 defaultPixel = 0xffffffff;
    glGenTextures(1, &defaultTextureName);
    glBindTexture(GL_TEXTURE_2D, defaultTextureName);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, &defaultPixel);
    struct { void **vtable; COpenGLTextureStateNative gl; } defaultTexture = {
        .gl = { .name = &defaultTextureName, .width = 1, .height = 1, .depth = 1 }
    };
    dev.saveSurface = (void *)&defaultTexture;
    dev.renderTarget = FixtureSurface();
    /* Init retains the original zero cached viewport; the renderer sets it. */
    D3DVIEWPORT9 fixtureViewport = {0, 0, 32, 32, 0.0f, 1.0f};
    (void)CDirect3DDevice_SetViewport((const void *)&dev, &fixtureViewport);
#endif
    dev.streams[0] = (void *)&vb;
    dev.streamStrides[0] = sizeof(vertices[0]);
    dev.indexBuffer = (void *)&ib;
    g_activeVertexDeclaration = (void *)&declaration;
#if defined(COD2_X64)
    g_activeVertexShader = (void *)&shader;
#endif
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
#if defined(COD2_X64)
        memcpy(shader.baseState, &vertexProgram, sizeof(vertexProgram));
#else
        g_activeVertexShader = (void *)&vertexProgram;
#endif
        dev.pixelShader = (void *)&pixel;
    }
    /* Existing stencil operations isolate the surface enable/mask contract. */
    glStencilFunc(GL_ALWAYS, 0xa6, 255);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (!strcmp(test, "identity")) {
#if defined(COD2_X64)
        DepthIdentity(&dev);
#endif
    } else if (!strcmp(test, "query")) {
        QueryStub *query = NULL;
        assert(!CDirect3DDevice_CreateQuery((const void *)&dev, (D3DQUERYTYPE)9, (bool (*)())&query));
        glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
        glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        for (int covered = 0; covered <= 2; ++covered) {
            glDepthMask(GL_TRUE); glClearDepth(1); glClear(GL_DEPTH_BUFFER_BIT);
            if (covered) {
                glEnable(GL_SCISSOR_TEST); glScissor(0, 0, covered * 16, 32);
                glClearDepth(0); glClear(GL_DEPTH_BUFFER_BIT); glDisable(GL_SCISSOR_TEST);
            }
            glDepthMask(GL_FALSE);
            assert(!Q_Issue(query, 2));
            glRectf(-1, -1, 1, 1);
            assert(!Q_Issue(query, 1));
            glFinish();
            DWORD samples = 0xdeadbeef;
            assert(!Q_GetData(query, &samples, sizeof(samples), 1));
            fprintf(stderr, "query covered=%d/2 samples=%u expected=%u\n", covered, samples, (2 - covered) * 512);
            assert(samples == (2 - covered) * 512);
        }
        assert(!Q_Release(query));
    } else if (!strcmp(test, "blend")) {
        glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
        glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glClearColor(.2f, .3f, .4f, .5f); glClear(GL_COLOR_BUFFER_BIT);
        Set(&dev, D3DRS_BLENDOP, 1);
        Set(&dev, D3DRS_SEPARATEALPHABLENDENABLE, 0);
        Set(&dev, D3DRS_BLENDOPALPHA, 2);
        Set(&dev, D3DRS_SEPARATEALPHABLENDENABLE, 1);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
        glColor4f(.6f, .2f, .1f, .8f); glRectf(-1, -1, 1, 1);
        byte pixel[4]; glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        fprintf(stderr, "separate blend pixel=%u,%u,%u,%u expected=204,128,128,77\n", pixel[0],pixel[1],pixel[2],pixel[3]);
        assert(abs(pixel[0]-204)<=1 && abs(pixel[1]-128)<=1 && abs(pixel[2]-128)<=1 && abs(pixel[3]-77)<=1);
        Set(&dev, D3DRS_SEPARATEALPHABLENDENABLE, 0);
        GLint equation; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation);
        assert(equation == GL_FUNC_ADD);
    } else if (!strcmp(test, "two-sided")) {
        glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        Set(&dev, D3DRS_STENCILENABLE, 1);
        Set(&dev, D3DRS_STENCILFUNC, 8);
        Set(&dev, D3DRS_STENCILPASS, 3); /* replace */
        Set(&dev, D3DRS_STENCILREF, 1);
        Set(&dev, D3DRS_STENCILWRITEMASK, 255);
        Set(&dev, D3DRS_CCW_STENCILFUNC, 8);
        Set(&dev, D3DRS_CCW_STENCILPASS, 2); /* zero */
        Set(&dev, D3DRS_TWOSIDEDSTENCILMODE, 1);
        for (int cull = 2; cull <= 3; ++cull) {
            Set(&dev, D3DRS_CULLMODE, cull); glDisable(GL_CULL_FACE);
            glClearStencil(2); glClear(GL_STENCIL_BUFFER_BIT);
            glBegin(GL_TRIANGLES);
            glVertex2f(-.9f,-.8f); glVertex2f(-.1f,-.8f); glVertex2f(-.5f,.8f);
            glVertex2f(.1f,-.8f); glVertex2f(.5f,.8f); glVertex2f(.9f,-.8f);
            glEnd();
            byte ccw, cw;
            glReadPixels(8,12,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&ccw);
            glReadPixels(24,12,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&cw);
            fprintf(stderr,"stencil cull=%d CCW=%u CW=%u expected=0,1\n",cull,ccw,cw);
            assert(ccw == 0 && cw == 1);
        }
        Set(&dev, D3DRS_TWOSIDEDSTENCILMODE, 0);
        GLint op; glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS,&op); assert(op == GL_REPLACE);
    } else if (!strcmp(test, "initial")) {
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
#if defined(COD2_X64)
    CDirect3DDevice_ResetStreamsNative((const void *)&dev);
    glDeleteTextures(1, &defaultTextureName);
#endif
    glDeleteProgramsARB(1, &vertexProgram); glDeleteProgramsARB(1, &fragmentProgram);
    glDeleteRenderbuffersEXT(2, buffers); glDeleteFramebuffersEXT(1, &framebuffer);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    puts("depth surface: synthetic color, depth, stencil and effective state passed");
    return 0;
}
