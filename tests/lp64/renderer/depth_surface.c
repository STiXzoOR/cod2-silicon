/* Synthetic triangles through the production indexed draw, in a private CGL FBO. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
static unsigned alphaDepthCopies;
static int alphaCaptureCopies;
static GLint alphaCopyRect[2][8];
void AlphaAATraceBlit(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
/* The existing fallback uses a core-profile VAO entrypoint in a legacy context.
 * Record its known GL_INVALID_OPERATION separately; every later GL error is fatal.
 * The call itself still reaches the driver, and all pixel assertions stay active. */
#define glBindVertexArray TraceBindVertexArray
#define glBlitFramebufferEXT AlphaAATraceBlit
#include "CDirect3DDevice.c"
#undef glBlitFramebufferEXT
#undef glBindVertexArray
void AlphaAATraceBlit(GLint x0,GLint y0,GLint x1,GLint y1,GLint a0,GLint b0,GLint a1,GLint b1,GLbitfield mask,GLenum filter)
{
    if (alphaCaptureCopies && (mask&GL_DEPTH_BUFFER_BIT)) {
        const GLint rectangle[8]={x0,y0,x1,y1,a0,b0,a1,b1};
        assert(alphaDepthCopies<2); memcpy(alphaCopyRect[alphaDepthCopies++],rectangle,sizeof(rectangle));
    }
    glBlitFramebufferEXT(x0,y0,x1,y1,a0,b0,a1,b1,mask,filter);
}
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
Bool g_InhibitCopy, g_RenderToShadowCookie;
UINT32 g_NumBlurShadowPasses;
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

#if defined(COD2_X64) && COD2_APPLE_SDK
static void AlphaAARead(GLuint source, GLuint resolved, byte pixels[16*16*4])
{
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, source);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, resolved);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBlitFramebufferEXT(0,0,16,16,0,0,16,16,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, resolved);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glReadPixels(0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, source);
}

/* Compare bounded copies with the same geometry through an intentionally
 * unrecognized, mathematically identical vertex program (the full-copy
 * fallback). The farther draw exposes stale depth outside the copy region. */
static void AlphaAABoundsFixture(DeviceImpl *dev, int samples)
{
    GLuint fbo[2],storage[3],programs[3];
    glGenFramebuffersEXT(2,fbo); glGenRenderbuffersEXT(3,storage); glGenProgramsARB(3,programs);
    for (int target=0;target<2;++target) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[target]); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[target]);
        if (!target) glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_RGBA8,64,64);
        else glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_RGBA8,64,64);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,storage[target]);
        if (!target) {
            glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[2]);
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_DEPTH24_STENCIL8_EXT,64,64);
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[2]);
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[2]);
        }
        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]); glViewport(0,0,64,64);
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB); glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_FOG); glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_COVERAGE);
    glEnable(GL_MULTISAMPLE); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    GLint units; glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
    for (int unit=0;unit<units;++unit) { glActiveTextureARB(GL_TEXTURE0_ARB+unit); glDisable(GL_TEXTURE_2D); glDisable(GL_TEXTURE_3D); glDisable(GL_TEXTURE_CUBE_MAP); }
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_STENCIL_TEST); glStencilMask(255); glClearStencil(0xa0); glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS,0xa7,255); glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE); glColorMask(0,0,0,0);
    glEnable(GL_SAMPLE_COVERAGE); glSampleCoverage(.5f,GL_FALSE); AlphaAA_Quad(1,1); glDisable(GL_SAMPLE_COVERAGE);
    glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glColorMask(1,1,1,1);
    const char *affine="!!ARBvp1.0\nATTRIB p = vertex.attrib[0];\nOUTPUT clip = result.position;\n"
        "PARAM rows[4] = {program.env[40..43]};\nDP4 clip.x,p,rows[0];\nDP4 clip.y,p,rows[1];\nDP4 clip.z,p,rows[2];\nDP4 clip.w,p,rows[3];\nEND\n";
    const char *fallback="!!ARBvp1.0\nTEMP unit,clip;\nPARAM rows[4]={program.env[40..43]};\n"
        "DP4 clip.x,vertex.attrib[0],rows[0];\nDP4 clip.y,vertex.attrib[0],rows[1];\n"
        "DP4 clip.z,vertex.attrib[0],rows[2];\nDP4 clip.w,vertex.attrib[0],rows[3];\n"
        "RCP unit,{1,1,1,1}.x;\nMUL result.position,clip,unit;\nEND\n";
    const char *fragment="!!ARBfp1.0\nTEMP phase;\nMUL phase,fragment.position,{.17,.13,0,0};\nFRC phase,phase;\n"
        "MOV result.color,{1,.2,.1,1};\nMOV result.color.w,phase.y;\nEND\n";
    for (int j=0;j<3;++j) {
        GLenum target=j==2 ? GL_FRAGMENT_PROGRAM_ARB : GL_VERTEX_PROGRAM_ARB;
        const char *source=j==0 ? affine : j==1 ? fallback : fragment;
        glBindProgramARB(target,programs[j]); glProgramStringARB(target,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(source),source);
        GLint error; glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&error);
        if (error!=-1) fprintf(stderr,"bounds program %d error %d: %s\n",j,error,glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        assert(error==-1 && glGetError()==GL_NO_ERROR);
    }
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float perspective[16]={.9f,.2f,0,.15f,-.1f,.8f,0,-.05f,0,0,1,0,.12f,.08f,0,1};
    float vertices[6][4]={{99,99,99,99},{-.75f,-.5f,0,1},{99,99,99,99},{-.25f,-.5f,0,1},{99,99,99,99},{-.75f,0,0,1}};
    const unsigned short indices[3]={1,3,5};
    glBindBufferARB(GL_ARRAY_BUFFER_ARB,0); glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
    glEnableClientState(GL_VERTEX_ARRAY); glVertexPointer(4,GL_FLOAT,sizeof(vertices[0]),vertices);
    glVertexAttribPointerARB(0,4,GL_FLOAT,GL_FALSE,sizeof(vertices[0]),vertices); glEnableVertexAttribArrayARB(0);
    Set(dev,D3DRS_ALPHABLENDENABLE,0); Set(dev,D3DRS_ALPHATESTENABLE,1); Set(dev,D3DRS_ALPHAFUNC,5); Set(dev,D3DRS_ALPHAREF,128);
    Set(dev,D3DRS_ZENABLE,0); Set(dev,D3DRS_ZENABLE,1); Set(dev,D3DRS_ZWRITEENABLE,0); Set(dev,D3DRS_ZWRITEENABLE,1); Set(dev,D3DRS_ZFUNC,2);
    Set(dev,D3DRS_STENCILFAIL,1); Set(dev,D3DRS_STENCILZFAIL,1); Set(dev,D3DRS_STENCILPASS,1); Set(dev,D3DRS_TWOSIDEDSTENCILMODE,0);
    Set(dev,(D3DRENDERSTATETYPE)181,0x41415353);
    byte reference[64*64*4],pixels[64*64*4];
    for (int scenario=0;scenario<9;++scenario) for (int path=0;path<2;++path) {
        int predicate=scenario%3,scissor=scenario>=3;
        CDirect3DDevice_SetVertexShaderConstantF((const void *)dev,40,scenario>=6 ? perspective : identity,4);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]); glDisable(GL_SCISSOR_TEST); glViewport(0,0,64,64);
        glClearColor(0,0,0,0); glClearDepth(.8); glDepthMask(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if (dev->alphaAA) {
            glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,dev->alphaAA->framebuffer);
            glClearDepth(.1); glClear(GL_DEPTH_BUFFER_BIT); glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,fbo[0]);
        }
        if (scissor) { glViewport(7,9,48,44); glScissor(12,20,15,13); glEnable(GL_SCISSOR_TEST); }
        Set(dev,D3DRS_STENCILENABLE,predicate!=0); Set(dev,D3DRS_STENCILFUNC,3); Set(dev,D3DRS_STENCILMASK,15);
        Set(dev,D3DRS_STENCILREF,predicate==1 ? 7 : 0);
        /* The following direct GL background draw deliberately bypasses the
         * device cache, so restore its actual stencil enable before replay. */
        if (predicate) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,programs[2]); glEnable(GL_FRAGMENT_PROGRAM_ARB); glEnable(GL_ALPHA_TEST); glEnable(GL_DEPTH_TEST);
        glEnable(GL_VERTEX_PROGRAM_ARB); glBindProgramARB(GL_VERTEX_PROGRAM_ARB,programs[path==0 ? 1 : 0]);
        if (scenario==0 && path==1) glDisable(GL_VERTEX_PROGRAM_ARB);
        alphaDepthCopies=0; alphaCaptureCopies=1;
        assert(DeviceAlphaAA_Draw(dev,1,5,3,(const byte *)indices)); alphaCaptureCopies=0;
        assert(alphaDepthCopies==2); assert(!memcmp(alphaCopyRect[0],alphaCopyRect[1],sizeof(alphaCopyRect[0])));
        assert(!memcmp(alphaCopyRect[0],alphaCopyRect[0]+4,4*sizeof(GLint)));
#if defined(COD2_ALPHA_AA_BOUNDED_COPIES)
        if (path) assert(alphaCopyRect[0][2]-alphaCopyRect[0][0]<32 && alphaCopyRect[0][3]-alphaCopyRect[0][1]<32);
#endif
        if (!path) assert(alphaCopyRect[0][0]==0 && alphaCopyRect[0][1]==0 && alphaCopyRect[0][2]==64 && alphaCopyRect[0][3]==64);
        assert(glIsEnabled(GL_ALPHA_TEST) && !!glIsEnabled(GL_STENCIL_TEST)==(predicate!=0));
        assert(!!glIsEnabled(GL_SCISSOR_TEST)==scissor);
        /* Expose every original depth sample, including those outside the
         * tight rectangle and samples rejected by alpha or sunpartition. */
        glDisable(GL_SCISSOR_TEST); glViewport(0,0,64,64); glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
        glDisable(GL_ALPHA_TEST); glDisable(GL_STENCIL_TEST); glColor4f(0,0,1,1);
        glBegin(GL_QUADS); glVertex3f(-1,-1,.3f); glVertex3f(1,-1,.3f); glVertex3f(1,1,.3f); glVertex3f(-1,1,.3f); glEnd();
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,fbo[0]); glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,fbo[1]);
        glBlitFramebufferEXT(0,0,64,64,0,0,64,64,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,fbo[1]); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        if (!path) memcpy(reference,pixels,sizeof(pixels)); else assert(!memcmp(reference,pixels,sizeof(pixels)));
        assert(glGetError()==GL_NO_ERROR);
        fprintf(stderr,"alpha bounds %dx scenario%d path%d rect=%d,%d..%d,%d depth/color exact\n",samples,scenario,path,
            alphaCopyRect[0][0],alphaCopyRect[0][1],alphaCopyRect[0][2],alphaCopyRect[0][3]);
    }
#if defined(COD2_ALPHA_AA_BOUNDED_COPIES)
    CDirect3DDevice_SetVertexShaderConstantF((const void *)dev,40,identity,4);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]); glEnable(GL_VERTEX_PROGRAM_ARB); glBindProgramARB(GL_VERTEX_PROGRAM_ARB,programs[0]);
    GLint bounds[4]; vertices[1][3]=-1;
    AlphaAA_CopyRect(dev->alphaAA,1,5,3,(const byte *)indices,bounds);
    assert(bounds[0]==0 && bounds[1]==0 && bounds[2]==64 && bounds[3]==64); vertices[1][3]=1;
    AlphaAA_CopyRect(dev->alphaAA,1,1,3,(const byte *)indices,bounds);
    assert(bounds[0]==0 && bounds[1]==0 && bounds[2]==64 && bounds[3]==64);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); AlphaAA_CopyRect(dev->alphaAA,1,5,3,(const byte *)indices,bounds);
    assert(bounds[0]==0 && bounds[1]==0 && bounds[2]==64 && bounds[3]==64); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
#endif
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB); glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]); glEnable(GL_STENCIL_TEST); glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    for (int ref=0;ref<2;++ref) {
        glStencilFunc(GL_EQUAL,ref ? 0xa7 : 0xa0,255); glClear(GL_COLOR_BUFFER_BIT); glColor4f(1,1,1,1); AlphaAA_Quad(1,1);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,fbo[0]); glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,fbo[1]);
        glBlitFramebufferEXT(0,0,64,64,0,0,64,64,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,fbo[1]); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        for (unsigned j=0;j<sizeof(pixels);++j) assert(abs((int)pixels[j]-128)<=1);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]);
    }
    DeviceAlphaAA_Destroy(dev); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glDeleteProgramsARB(3,programs); glDeleteFramebuffersEXT(2,fbo); glDeleteRenderbuffersEXT(3,storage);
    assert(glGetError()==GL_NO_ERROR);
}

static double AlphaAAClock(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec*1e-9;
}
static void AlphaAABenchmark(DeviceImpl *dev, int samples, const char *shaderPath)
{
    const int width=5120,height=2880;
    GLuint fbo,storage[2],fp,vp;
    glGenFramebuffersEXT(1,&fbo); glGenRenderbuffersEXT(2,storage);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[0]);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_RGBA8,width,height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,storage[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[1]);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_DEPTH24_STENCIL8_EXT,width,height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[1]);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[1]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0,0,width,height);
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_CULL_FACE); glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_COVERAGE); glEnable(GL_MULTISAMPLE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    GLint units; glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
    for(int unit=0;unit<units;++unit){glActiveTextureARB(GL_TEXTURE0_ARB+unit);glDisable(GL_TEXTURE_2D);glDisable(GL_TEXTURE_3D);glDisable(GL_TEXTURE_CUBE_MAP);}
    glActiveTextureARB(GL_TEXTURE0_ARB);
    const char source[]="!!ARBfp1.0\nTEMP phase;\nMUL phase,fragment.texcoord[0],{47,31,0,0};\nFRC phase,phase;\nMOV result.color,{1,.5,.25,1};\nMOV result.color.w,phase.y;\nEND\n";
    glGenProgramsARB(1,&fp);glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(source)-1,source);glEnable(GL_FRAGMENT_PROGRAM_ARB);
    const char vertexSource[]="!!ARBvp1.0\nPARAM rows[4]={program.env[40..43]};\n"
        "DP4 result.position.x,vertex.position,rows[0];\nDP4 result.position.y,vertex.position,rows[1];\n"
        "DP4 result.position.z,vertex.position,rows[2];\nDP4 result.position.w,vertex.position,rows[3];\n"
        "MOV result.texcoord[0],vertex.texcoord[0];\nEND\n";
    glGenProgramsARB(1,&vp); glBindProgramARB(GL_VERTEX_PROGRAM_ARB,vp);
    if (shaderPath) {
        FILE *file=fopen(shaderPath,"rb"); assert(file); fseek(file,0,SEEK_END); long length=ftell(file); rewind(file);
        char *text=calloc(1,(size_t)length+1); assert(text); assert(fread(text,1,length,file)==(size_t)length); fclose(file);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)length,text); free(text);
        fprintf(stderr,"5K4x vertex program source=%s\n",shaderPath);
        for (unsigned row=0;row<4;++row) { float values[4]={0}; values[row]=1; glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB,23+row,values); }
    } else glProgramStringARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(vertexSource)-1,vertexSource);
    GLint programError; glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&programError); assert(programError==-1);
    for (unsigned row=0;row<4;++row) { float values[4]={0}; values[row]=1; glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB,40+row,values); }
    float vertex[3][6]={{-1,-1,0,1,0,0},{3,-1,0,1,2,0},{-1,3,0,1,0,2}};
    const unsigned short indices[3]={0,1,2};
    glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
    glEnableClientState(GL_VERTEX_ARRAY);glVertexPointer(4,GL_FLOAT,sizeof(vertex[0]),vertex);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glTexCoordPointer(2,GL_FLOAT,sizeof(vertex[0]),vertex[0]+4);
    Set(dev,D3DRS_ALPHABLENDENABLE,0);Set(dev,D3DRS_ALPHATESTENABLE,1);Set(dev,D3DRS_ALPHAFUNC,5);Set(dev,D3DRS_ALPHAREF,128);
    Set(dev,D3DRS_ZENABLE,0);Set(dev,D3DRS_ZENABLE,1);Set(dev,D3DRS_ZWRITEENABLE,0);Set(dev,D3DRS_ZWRITEENABLE,1);Set(dev,D3DRS_ZFUNC,4);
    Set(dev,D3DRS_STENCILENABLE,0);Set(dev,D3DRS_TWOSIDEDSTENCILMODE,0);
    glColorMask(1,1,1,1);glClearDepth(1);glClearColor(0,0,0,0);
    assert(CDirect3DDevice_SupportsTransparencyAA()); assert(glGetError()==GL_NO_ERROR);
    fprintf(stderr,"5K4x workload renderer=%s version=%s dimensions=%dx%d samples=%d depthWrite=1 repetitions=3 drawsPerBatch=4\n",glGetString(GL_RENDERER),glGetString(GL_VERSION),width,height,samples);
    DWORD modes[]={0,0x434f5441u,0x41415353u};
    for(int programmable=0;programmable<2;++programmable) for(int small=0;small<2;++small){
        if(programmable)glEnable(GL_VERTEX_PROGRAM_ARB);else glDisable(GL_VERTEX_PROGRAM_ARB);
        vertex[1][0]=small ? -1+256.0f/width : 3; vertex[2][1]=small ? -1+256.0f/height : 3;
        for(int mode=0;mode<3;++mode){
            Set(dev,(D3DRENDERSTATETYPE)181,modes[mode]);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            alphaDepthCopies=0; alphaCaptureCopies=mode==2;
            if(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte*)indices))glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,indices);
            alphaCaptureCopies=0;
            if(mode==2){
                assert(alphaDepthCopies==2 && !memcmp(alphaCopyRect[0],alphaCopyRect[1],sizeof(alphaCopyRect[0])));
#if defined(COD2_ALPHA_AA_BOUNDED_COPIES)
                if(small)assert(alphaCopyRect[0][2]-alphaCopyRect[0][0]<256 && alphaCopyRect[0][3]-alphaCopyRect[0][1]<256);
#endif
                fprintf(stderr,"5K4x vertex=%s coverage=%s depth-copy rect=%d,%d..%d,%d\n",programmable?"affine-program":"fixed",small?"128x128-triangle":"fullscreen",alphaCopyRect[0][0],alphaCopyRect[0][1],alphaCopyRect[0][2],alphaCopyRect[0][3]);
            }
            glFinish();assert(glGetError()==GL_NO_ERROR);
            for(int rep=0;rep<3;++rep){
                double begin=AlphaAAClock();
                for(int draw=0;draw<4;++draw)
                    if(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte*)indices))glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,indices);
                glFinish();double end=AlphaAAClock();
                GLint bound;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&bound);assert(bound==(GLint)fbo);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&bound);assert(bound==(GLint)fbo);
                assert(glGetError()==GL_NO_ERROR);assert(glIsEnabled(GL_ALPHA_TEST));assert(!glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
                fprintf(stderr,"5K4x vertex=%s coverage=%s mode=%d repeat=%d millisecondsPerDraw=%.6f glError=0\n",programmable?"affine-program":"fixed",small?"128x128-triangle":"fullscreen",mode,rep,1000*(end-begin)/4);
            }
        }
    }
    DeviceAlphaAA_Destroy(dev);glDeleteProgramsARB(1,&vp);glDeleteProgramsARB(1,&fp);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);glDeleteFramebuffersEXT(1,&fbo);glDeleteRenderbuffersEXT(2,storage);
    assert(glGetError()==GL_NO_ERROR);
}

static void AlphaAAFixture(DeviceImpl *dev, int samples)
{
    GLuint fbo[2], storage[3], fp;
    glGenFramebuffersEXT(2,fbo); glGenRenderbuffersEXT(3,storage);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[0]);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_RGBA8,16,16);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,storage[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[1]);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_DEPTH24_STENCIL8_EXT,16,16);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[1]);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,storage[1]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[1]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,storage[2]); glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_RGBA8,16,16);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,storage[2]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo[0]); glViewport(0,0,16,16);
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glEnable(GL_MULTISAMPLE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    /* Original stencil differs between samples in every pixel. Preserve its
     * full eight bits, including bits outside the sunpartition read mask. */
    glEnable(GL_STENCIL_TEST); glStencilMask(255); glClearStencil(0xa0); glClear(GL_STENCIL_BUFFER_BIT);
    glColorMask(0,0,0,0); glStencilFunc(GL_ALWAYS,0xa7,255); glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
    glEnable(GL_SAMPLE_COVERAGE); glSampleCoverage(.5f,GL_FALSE); AlphaAA_Quad(1,1); glDisable(GL_SAMPLE_COVERAGE);
    glColorMask(1,1,1,1); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glStencilMask(0); glDisable(GL_STENCIL_TEST);
    const char source[] = "!!ARBfp1.0\nPARAM freq = {47,31,0,0};\nATTRIB tc = fragment.texcoord[ 0 ];\n"
        "TEMP phase;\nMUL phase.xy,tc,freq;\nFRC phase.xy,phase;\n"
        "MOV result.color.xy,tc;\nMOV result.color.z,phase.x;\nMOV result.color.w,phase.y;\nEND\n";
    glGenProgramsARB(1,&fp); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(source)-1,source);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    float vertex[3][6]={{-3,-3,0,1,0,0},{10,-6,0,2,1,0},{-12,20,0,4,0,1}};
    const unsigned short index[3]={0,1,2};
    glBindBufferARB(GL_ARRAY_BUFFER_ARB,0); glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
    glEnableClientState(GL_VERTEX_ARRAY); glVertexPointer(4,GL_FLOAT,sizeof(vertex[0]),vertex);
    glClientActiveTextureARB(GL_TEXTURE0_ARB); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2,GL_FLOAT,sizeof(vertex[0]),vertex[0]+4);
    Set(dev,D3DRS_ALPHABLENDENABLE,0); Set(dev,D3DRS_ALPHATESTENABLE,1);
    Set(dev,D3DRS_ALPHAFUNC,5); Set(dev,D3DRS_ALPHAREF,128);
    Set(dev,D3DRS_ZENABLE,0); Set(dev,D3DRS_ZENABLE,1); Set(dev,D3DRS_ZWRITEENABLE,0); Set(dev,D3DRS_ZWRITEENABLE,1);
    Set(dev,D3DRS_ZFUNC,2); glClearDepth(1);
    Set(dev,D3DRS_STENCILFAIL,1); Set(dev,D3DRS_STENCILZFAIL,1); Set(dev,D3DRS_STENCILPASS,1);
    Set(dev,D3DRS_TWOSIDEDSTENCILMODE,0); Set(dev,D3DRS_STENCILWRITEMASK,255);
    Set(dev,(D3DRENDERSTATETYPE)181,0x41415353);
    AlphaAASamplePattern *pattern=AlphaAA_Pattern(samples); assert(pattern);
    for (int k=0;k<samples;++k) fprintf(stderr,"alpha AA %dx tag%d position %.6f %.6f\n",samples,k+1,pattern->position[k][0],pattern->position[k][1]);
    for (int predicate=0;predicate<3;++predicate) {
        Set(dev,D3DRS_STENCILENABLE,predicate!=0); Set(dev,D3DRS_STENCILFUNC,3);
        Set(dev,D3DRS_STENCILMASK,15); Set(dev,D3DRS_STENCILREF,predicate==1 ? 7 : 0);
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        assert(DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index));
        assert(glIsEnabled(GL_ALPHA_TEST) && !glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
        assert(!!glIsEnabled(GL_STENCIL_TEST)==(predicate!=0));
        GLint bound; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&bound); assert((GLuint)bound==fbo[0]);
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&bound); assert((GLuint)bound==fp);
        byte pixels[16*16*4]; AlphaAARead(fbo[0],fbo[1],pixels);
        double total=0; int maximum=0;
        byte depthExpected[16*16*4];
        for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
            double expected[4]={0}; int passed=0;
            for (int sample=0;sample<samples;++sample) {
                if (predicate && ((sample<samples/2)!=(predicate==1))) continue;
                double b1=(x+pattern->position[sample][0]+16)/64.0, b2=(y+pattern->position[sample][1]+16)/64.0;
                double d=1-b1-b2+b1/2+b2/4,u=(b1/2)/d,v=(b2/4)/d;
                double blue=u*47-floor(u*47),alpha=v*31-floor(v*31);
                if (alpha<=128.0/255) continue;
                ++passed;
                double value[4]={u,v,blue,alpha};
                for (int c=0;c<4;++c) expected[c]+=round(value[c]*255)/samples;
            }
            for (int c=0;c<4;++c) {
                int error=abs((int)pixels[(y*16+x)*4+c]-(int)lround(expected[c]));
                total+=error; if (error>maximum) maximum=error;
                depthExpected[(y*16+x)*4+c]=(byte)lround(expected[c]+(c>=2 ? 255.0*(samples-passed)/samples : 0));
            }
        }
        fprintf(stderr,"alpha AA %dx predicate%d: max=%d mean=%.6f\n",samples,predicate,maximum,total/(16*16*4));
        assert(maximum<=1); assert(glGetError()==GL_NO_ERROR);
        /* A farther blue surface reaches only samples that failed the alpha
         * or stencil test. This checks the written original MSAA depth. */
        GLuint background; glGenProgramsARB(1,&background); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,background);
        const char blue[]="!!ARBfp1.0\nMOV result.color,{0,0,1,1};\nEND\n";
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(blue)-1,blue);
        glDisable(GL_ALPHA_TEST); glDisable(GL_STENCIL_TEST);
        for (int j=0;j<3;++j) vertex[j][2]=vertex[j][3]*.5f;
        glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,index);
        AlphaAARead(fbo[0],fbo[1],pixels);
        for (int j=0;j<16*16*4;++j) assert(abs((int)pixels[j]-depthExpected[j])<=1);
        for (int j=0;j<3;++j) vertex[j][2]=0;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp); glDeleteProgramsARB(1,&background);
        glEnable(GL_ALPHA_TEST);
        if (predicate) glEnable(GL_STENCIL_TEST);
    }
    /* The DX7 texture-combiner path uses the same sample evaluation. */
    Set(dev,D3DRS_STENCILENABLE,0); glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glActiveTextureARB(GL_TEXTURE0_ARB); glClientActiveTextureARB(GL_TEXTURE0_ARB);
    GLint units; glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
    for (int unit=0;unit<units;++unit) {
        glActiveTextureARB(GL_TEXTURE0_ARB+unit); glDisable(GL_TEXTURE_2D); glDisable(GL_TEXTURE_3D); glDisable(GL_TEXTURE_CUBE_MAP);
    }
    glActiveTextureARB(GL_TEXTURE0_ARB); GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    byte texels[16*16*4];
    for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
        byte *p=texels+(y*16+x)*4; p[0]=255; p[1]=128; p[2]=64; p[3]=((x+y)&1) ? 255 : 0;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_MODULATE); glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA,GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR); glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA); glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_ALPHA,GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_RGB_SCALE,1); glTexEnvi(GL_TEXTURE_ENV,GL_ALPHA_SCALE,1);
    glDisableClientState(GL_COLOR_ARRAY); glColor4f(.5f,1,.75f,.75f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    assert(DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index)); assert(!glIsEnabled(GL_FRAGMENT_PROGRAM_ARB));
    byte fixedPixels[16*16*4]; AlphaAARead(fbo[0],fbo[1],fixedPixels); int fixedMaximum=0;
    for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
        int passed=0;
        for (int sample=0;sample<samples;++sample) {
            double b1=(x+pattern->position[sample][0]+16)/64.0,b2=(y+pattern->position[sample][1]+16)/64.0;
            double d=1-b1-b2+b1/2+b2/4,u=(b1/2)/d,v=(b2/4)/d;
            passed+=((int)floor(u*16)+(int)floor(v*16))&1;
        }
        const int rgba[4]={128,128,48,191};
        for (int c=0;c<4;++c) {
            int error=abs((int)fixedPixels[(y*16+x)*4+c]-(int)lround((double)rgba[c]*passed/samples));
            if (error>fixedMaximum) fixedMaximum=error;
        }
    }
    fprintf(stderr,"alpha AA %dx fixed-function checker: max=%d\n",samples,fixedMaximum); assert(fixedMaximum<=1);
    glDisable(GL_TEXTURE_2D); glDeleteTextures(1,&texture); glEnable(GL_FRAGMENT_PROGRAM_ARB); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    /* ATOC replaces the threshold; ordinary alpha and blending are restored. */
    const char constant[]="!!ARBfp1.0\nMOV result.color,{1,1,1,.5};\nEND\n";
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(constant)-1,constant);
    Set(dev,D3DRS_STENCILENABLE,0); Set(dev,(D3DRENDERSTATETYPE)181,0x434f5441);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); assert(DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index));
    byte pixels[16*16*4]; AlphaAARead(fbo[0],fbo[1],pixels);
    assert(abs((int)pixels[0]-128)<=1 && abs((int)pixels[3]-64)<=1);
    Set(dev,D3DRS_ALPHABLENDENABLE,1);
    assert(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index)); assert(glIsEnabled(GL_BLEND));
    Set(dev,D3DRS_ALPHABLENDENABLE,0); Set(dev,D3DRS_ALPHATESTENABLE,0);
    assert(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index)); assert(!glIsEnabled(GL_ALPHA_TEST));
    Set(dev,(D3DRENDERSTATETYPE)181,0); DWORD state=1;
    assert(!CDirect3DDevice_GetRenderState((const void *)dev,(D3DRENDERSTATETYPE)181,&state) && state==0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB); glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST); glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glStencilFunc(GL_EQUAL,0xa7,255); glClear(GL_COLOR_BUFFER_BIT); glColor4f(1,1,1,1); AlphaAA_Quad(1,1);
    AlphaAARead(fbo[0],fbo[1],pixels); assert(abs((int)pixels[0]-128)<=1);
    glStencilFunc(GL_EQUAL,0xa0,255); glClear(GL_COLOR_BUFFER_BIT); AlphaAA_Quad(1,1);
    AlphaAARead(fbo[0],fbo[1],pixels); assert(abs((int)pixels[0]-128)<=1);
    /* A mod's stencil-writing material keeps ordinary alpha/stencil behavior.
     * Its sample predicate cannot be replaced by private immutable tags. */
    Set(dev,D3DRS_STENCILENABLE,1); Set(dev,D3DRS_STENCILFUNC,8);
    Set(dev,D3DRS_STENCILPASS,3); Set(dev,D3DRS_STENCILREF,0x33);
    Set(dev,D3DRS_STENCILMASK,255); Set(dev,D3DRS_STENCILWRITEMASK,255);
    Set(dev,D3DRS_ALPHATESTENABLE,1); Set(dev,(D3DRENDERSTATETYPE)181,0x41415353);
    glEnable(GL_FRAGMENT_PROGRAM_ARB); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    assert(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index));
    assert(glIsEnabled(GL_ALPHA_TEST) && glIsEnabled(GL_STENCIL_TEST));
    glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,index);
    glDisable(GL_FRAGMENT_PROGRAM_ARB); glDisable(GL_ALPHA_TEST);
    glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glStencilFunc(GL_EQUAL,0x33,255);
    glClear(GL_COLOR_BUFFER_BIT); AlphaAA_Quad(1,1); AlphaAARead(fbo[0],fbo[1],pixels); assert(!pixels[0]);
    Set(dev,D3DRS_ALPHAREF,0); glEnable(GL_ALPHA_TEST); glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glStencilMask(255); glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE); glStencilFunc(GL_ALWAYS,0x33,255);
    assert(!DeviceAlphaAA_Draw(dev,0,2,3,(const byte *)index));
    glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,index);
    glDisable(GL_FRAGMENT_PROGRAM_ARB); glDisable(GL_ALPHA_TEST);
    glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glStencilFunc(GL_EQUAL,0x33,255);
    glClear(GL_COLOR_BUFFER_BIT); AlphaAA_Quad(1,1); AlphaAARead(fbo[0],fbo[1],pixels); assert(pixels[0]==255);
    assert(glGetError()==GL_NO_ERROR);
    GLuint privateFramebuffer=dev->alphaAA->framebuffer,privateStorage=dev->alphaAA->stencil;
    GLuint privatePrograms[16]; unsigned privateCount=0;
    for (DeviceAlphaAAProgram *entry=dev->alphaAA->programs;entry;entry=entry->next) {
        assert(privateCount<16); privatePrograms[privateCount++]=entry->program;
    }
    DeviceAlphaAA_Destroy(dev); assert(!dev->alphaAA);
    assert(!glIsFramebufferEXT(privateFramebuffer) && !glIsRenderbufferEXT(privateStorage));
    for (unsigned j=0;j<privateCount;++j) assert(!glIsProgramARB(privatePrograms[j]));
    glDeleteProgramsARB(1,&fp); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glDeleteFramebuffersEXT(2,fbo); glDeleteRenderbuffersEXT(3,storage);
    fprintf(stderr,"alpha AA %dx: original stencil, alpha, blend, program and FBO restored; private resources deleted\n",samples);
}

static void AlphaAACache(const char *path)
{
    DIR *directory=opendir(path); assert(directory);
    struct dirent *entry; int count=0,failed=0;
    while ((entry=readdir(directory))) {
        size_t n=strlen(entry->d_name); if (n<4 || strcmp(entry->d_name+n-4,".pse")) continue;
        char filename[4096]; snprintf(filename,sizeof(filename),"%s/%s",path,entry->d_name);
        FILE *file=fopen(filename,"rb"); assert(file); fseek(file,0,SEEK_END); long length=ftell(file); rewind(file);
        char *source=calloc(1,(size_t)length+1); assert(source); assert(fread(source,1,length,file)==(size_t)length); fclose(file);
        char *variant=AlphaAA_SampleProgram(source); assert(variant); GLuint program;
        glGenProgramsARB(1,&program); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,program);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(variant),variant);
        GLint error; glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&error);
        if (error!=-1) { ++failed; fprintf(stderr,"%s sample variant error %d: %s\n",entry->d_name,error,(const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB)); glGetError(); }
        glDeleteProgramsARB(1,&program); free(variant); free(source); ++count;
    }
    closedir(directory); fprintf(stderr,"alpha AA shader variants: %d compiled, %d failed\n",count,failed);
    assert(count && !failed && glGetError()==GL_NO_ERROR);
#if defined(COD2_ALPHA_AA_BOUNDED_COPIES)
    /* Exercise the position recognizer on the shipped translation output;
     * shader names are assertions here, never production eligibility rules. */
    directory=opendir(path); assert(directory); int bounded=0,full=0;
    DeviceAlphaAA aa={0}; AlphaAARange input[4],clip[4];
    for (unsigned j=0;j<4;++j) { input[j]=AlphaAA_Number(j==3 ? 1 : .25); input[j].varying=1; }
    for (unsigned base=0;base<3;++base) for (unsigned row=0;row<4;++row) {
        float values[4]={0}; values[row]=1;
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB,(unsigned[]){0,23,27}[base]+row,values);
    }
    while ((entry=readdir(directory))) {
        size_t n=strlen(entry->d_name); if (n<4 || strcmp(entry->d_name+n-4,".vsa")) continue;
        char filename[4096]; snprintf(filename,sizeof(filename),"%s/%s",path,entry->d_name);
        FILE *file=fopen(filename,"rb"); assert(file); fseek(file,0,SEEK_END); long length=ftell(file); rewind(file);
        char *source=calloc(1,(size_t)length+1); assert(source); assert(fread(source,1,length,file)==(size_t)length); fclose(file);
        GLuint program; glGenProgramsARB(1,&program); glBindProgramARB(GL_VERTEX_PROGRAM_ARB,program);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)length,source);
        GLint error; glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&error); assert(error==-1);
        int recognized=AlphaAA_PositionProgram(&aa,input,clip);
        if (recognized) ++bounded; else ++full;
        if (!strcmp(entry->d_name,"lmap_s_dtl.vsa") || !strcmp(entry->d_name,"lprobe_sm_s_dtl.vsa")) assert(recognized);
        if (strstr(entry->d_name,"flag")) assert(!recognized);
        fprintf(stderr,"alpha AA position %s: %s\n",entry->d_name,recognized ? "proved affine" : "full-copy fallback");
        glDeleteProgramsARB(1,&program); free(source);
    }
    closedir(directory); free(aa.boundsSource);
    fprintf(stderr,"alpha AA position programs: %d affine, %d full-copy fallback\n",bounded,full);
    assert(bounded && full && glGetError()==GL_NO_ERROR);
#endif
}
#endif

#if defined(COD2_X64)
static void FloatTarget(DeviceImpl *dev)
{
    IDirect3DTexture9 *texture;
    IDirect3DSurface9 *surface, *depth;
    IDirect3DSurface9 *back = dev->renderTarget;
    dev->backBuffer = back;
    DeviceTargets_AddRef(back);
    GLint previousDraw, previousRead;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &previousDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &previousRead);
    assert(!CDirect3DDevice_CreateTexture((const void *)dev, 32, 32, 1, 1,
        D3DFMT_R32F, 0, &texture, NULL));
    assert(!CDirect3DTexture_GetSurfaceLevel((const void *)texture, 0, &surface));
    assert(!CDirect3DDevice_CreateDepthStencilSurface((const void *)dev,
        32, 32, D3DFMT_D24S8, 0, 0, 0, &depth, NULL));
    assert(!CDirect3DDevice_SetRenderTarget((const void *)dev, 0, surface));
    assert(!CDirect3DDevice_SetDepthStencilSurface((const void *)dev, depth));
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    /* The feather-depth R32F target is single-sample. Transparency AA must
     * decline it before touching its framebuffer, even after a mode request. */
    Set(dev,D3DRS_ALPHABLENDENABLE,0); Set(dev,D3DRS_ALPHATESTENABLE,1);
    GLint floatDraw,floatRead,samples;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&floatDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&floatRead);
    glGetIntegerv(GL_SAMPLES,&samples); assert(samples<2);
    const DWORD alphaModes[]={0x434f5441,0x41415353};
    for (unsigned j=0;j<2;++j) {
        Set(dev,(D3DRENDERSTATETYPE)181,alphaModes[j]);
        assert(!DeviceAlphaAA_Draw(dev,0,0,0,NULL));
        GLint binding; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&binding); assert(binding==floatDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&binding); assert(binding==floatRead);
        assert(!dev->alphaAA && glIsEnabled(GL_ALPHA_TEST));
    }
    Set(dev,(D3DRENDERSTATETYPE)181,0); Set(dev,D3DRS_ALPHATESTENABLE,0);
    const char program[] = "!!ARBfp1.0\nMOV result.color, {908066.8125,0,0,1};\nEND";
    GLuint fp;
    glGenProgramsARB(1, &fp);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(program)-1, program);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glColorMask(1,1,1,1);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glBegin(GL_QUADS);
    glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1);
    glEnd();
    float value = 0;
    glReadPixels(16,16,1,1,GL_RED,GL_FLOAT,&value);
    fprintf(stderr,"float target shader depth: %.4f expected 908066.8125\n",value);
    assert(value == 908066.8125f);
    assert(!CDirect3DDevice_SetRenderTarget((const void *)dev,0,back));
    GLint draw,read;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&read);
    assert(draw==previousDraw && read==previousRead);
    CDirect3DSurfaceNativeInfo info;
    assert(CDirect3DSurface_GetNativeInfo(surface,&info));
    glBindTexture(GL_TEXTURE_2D,info.name);
    float pixels[32*32];
    glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,pixels);
    assert(pixels[16*32+16]==value);
    assert(glGetError()==GL_NO_ERROR);
    glDeleteProgramsARB(1,&fp);
    DeviceTargets_Release(surface);
    DeviceTargets_Release(texture);
    DeviceTargets_Release(depth);
    DeviceTargets_Release(back);
}

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
    assert(argc == 3 || argc == 4);
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
    if (!strcmp(test, "alpha-cache")) {
        assert(argc==4); AlphaAACache(argv[3]);
    } else if (!strcmp(test, "alpha-benchmark")) {
        AlphaAABenchmark(&dev,4,argc==4 ? argv[3] : NULL);
    } else if (!strcmp(test, "alpha-bounds2") || !strcmp(test, "alpha-bounds4")) {
        AlphaAABoundsFixture(&dev,test[12]-'0');
    } else if (!strcmp(test, "alpha2") || !strcmp(test, "alpha4")) {
        AlphaAAFixture(&dev, test[5]-'0');
    } else if (!strcmp(test, "wire-bias")) {
        Surface(&dev, 1);
        Set(&dev, D3DRS_STENCILENABLE, 0);
        Set(&dev, D3DRS_ZENABLE, 1);
        Set(&dev, D3DRS_ZWRITEENABLE, 1);
        Set(&dev, D3DRS_ZFUNC, 2); /* Strict less rejects the coplanar overlay. */
        Set(&dev, D3DRS_FILLMODE, 2);
        glClearDepth(.75);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Draw(&dev);
        byte pixels[32*32*4];
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        int before = 0, after = 0;
        for (int i=0;i<32*32;i++) before += pixels[i*4] != 0;
        float bias = -1.0f / 65536.0f;
        DWORD bits; memcpy(&bits, &bias, sizeof(bits));
        Set(&dev, D3DRS_DEPTHBIAS, bits);
        Draw(&dev);
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (int i=0;i<32*32;i++) after += pixels[i*4] != 0;
        fprintf(stderr, "wireframe depth bias: coplanar pixels %d -> %d\n", before, after);
        assert(before == 0 && after > 50);
        Set(&dev, D3DRS_DEPTHBIAS, 0);
        assert(!glIsEnabled(GL_POLYGON_OFFSET_LINE));
    } else if (!strcmp(test, "float")) {
        FloatTarget(&dev);
    } else if (!strcmp(test, "identity")) {
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
