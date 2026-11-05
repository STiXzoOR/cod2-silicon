#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>
#if COD2_APPLE_SDK
#include "platform/macos_display.h"
#endif
#if defined(COD2_X64)
#include <math.h>
#include "lp64_buffers.h"
#include "lp64_gl_state.h"
#include "lp64_surface.h"
#endif

float g_scale1 = 1.0f;
float g_scale2 = 16777216.0f;

typedef struct SDL_Window SDL_Window;
extern void SDL_GL_SwapWindowDirect(void);

typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DDevice[];
extern void *vtbl_CDirect3DTexture[];
extern void *vtbl_CDirect3DCubeTexture[];
#if COD2_APPLE_SDK
extern void *vtbl_CDirect3DVolumeTexture[];
#endif
extern void R_CapturePendingScreenshotBeforePresent(void);

#if defined(COD2_X64)
/* C++ bool flags: one byte in the Mac binary and in the typed native data. */
#define CXX_BOOL Bool
#else
#define CXX_BOOL bool
#endif
extern CXX_BOOL g_ShowShadowCookies;
extern UINT32 g_NumBlurShadowPasses;
extern CXX_BOOL g_RenderToShadowCookie;
extern UINT32 g_showtexid;
extern float g_scale1;
extern float g_scale2;
extern CXX_BOOL g_InhibitCopy;
extern CXX_BOOL g_NoTextureID;

#if defined(_MSC_VER)
/* These C names alias the C++ static-member symbols (defined in the data blob /
 * the reconstructed class). cl has no symbol-rename; bind via /alternatename.
 * x86: a C identifier `__ZN..` emits MSVC symbol `___ZN..` (one extra '_'). */
extern bool CDirect3DDevice_mNeedsVertexShaderValidation;
extern bool CDirect3DDevice_mNeedsTransformationValidation;
extern bool CDirect3DDevice_mNeedsRasterizationValidation;
COD2_ALT("CDirect3DDevice_mNeedsVertexShaderValidation", "__ZN15CDirect3DDevice28mNeedsVertexShaderValidationE")
COD2_ALT("CDirect3DDevice_mNeedsTransformationValidation", "__ZN15CDirect3DDevice30mNeedsTransformationValidationE")
COD2_ALT("CDirect3DDevice_mNeedsRasterizationValidation", "__ZN15CDirect3DDevice29mNeedsRasterizationValidationE")
#else
/* Reconstructed __ZN identifiers also receive the Mach-O C prefix. */
#if defined(_WIN32) || (defined(__APPLE__) && defined(COD2_X64))
#define COD2_COFF_ASM_PREFIX "_"
#else
#define COD2_COFF_ASM_PREFIX ""
#endif
extern CXX_BOOL __attribute__((visibility("default"))) CDirect3DDevice_mNeedsVertexShaderValidation __asm__(COD2_COFF_ASM_PREFIX "__ZN15CDirect3DDevice28mNeedsVertexShaderValidationE");
extern CXX_BOOL __attribute__((visibility("default"))) CDirect3DDevice_mNeedsTransformationValidation __asm__(COD2_COFF_ASM_PREFIX "__ZN15CDirect3DDevice30mNeedsTransformationValidationE");
extern CXX_BOOL __attribute__((visibility("default"))) CDirect3DDevice_mNeedsRasterizationValidation __asm__(COD2_COFF_ASM_PREFIX "__ZN15CDirect3DDevice29mNeedsRasterizationValidationE");
#undef COD2_COFF_ASM_PREFIX
#endif

static const float GaussianBlurWeights[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static float sPointScale[3] = { 0.0f, 0.0f, 0.0f };

extern void CDirect3DTexture_CDirect3DTexture(const CDirect3DTexture *_this,
                                              UINT32 Width, UINT32 Height, UINT32 Levels, DWORD Usage, D3DFORMAT Format);
extern void CDirect3DVolumeTexture_CDirect3DVolumeTexture(const CDirect3DVolumeTexture *_this,
                                                          UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 Levels, DWORD Usage, D3DFORMAT Format);
extern void CDirect3DCubeTexture_CDirect3DCubeTexture(const CDirect3DCubeTexture *_this,
                                                      UINT32 EdgeLength, UINT32 Levels, DWORD Usage, D3DFORMAT Format);
extern void CDirect3DVertexBuffer_CDirect3DVertexBuffer(const CDirect3DVertexBuffer *_this,
                                                        UINT32 Length, DWORD Usage, D3DPOOL Pool);
extern void CDirect3DIndexBuffer_CDirect3DIndexBuffer(const CDirect3DIndexBuffer *_this,
                                                      UINT32 Length, D3DFORMAT Format, DWORD Usage, D3DPOOL Pool);
extern int CDirect3DVertexDeclaration_CDirect3DVertexDeclaration(const CDirect3DVertexDeclaration *_this,
                                                                 const D3DVERTEXELEMENT9 *pVertexElements);
extern unsigned char CDirect3DVertexShader_CDirect3DVertexShader(const CDirect3DVertexShader *_this,
                                                                 const char *pSrcData);
extern void CDirect3DSurface_CDirect3DSurface(const CDirect3DSurface *_this,
                                              SurfaceType s, GLenum CubemapID, UINT32 Level, UINT32 Width, UINT32 Height,
                                              D3DFORMAT Format, const void *pSurfaceMemory, COpenGLTexture *pOpenGLTextureInfo);
extern void CDirect3DSurface_UpdateOpenGLSurfaceObject(const CDirect3DSurface *_this, int bRecreateSurface);
extern int CDirect3DSwapChain_CDirect3DSwapChain(const CDirect3DSwapChain *_this);
extern int CDirect3DSwapChain_SetBackBuffer(const CDirect3DSwapChain *_this, const IDirect3DSurface9 *pBackBuffer);

extern UINT32 MacOpenGLUtils_GetImageSizeInBytes(UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 LevelCount, const D3DFORMAT *f);
extern UINT32 MacOpenGLUtils_GetPCPixelShaderVersion(void);
extern int MacDisplay_GetCardType(void);
extern void *SDL_GL_GetProcAddress(const char *proc);

extern unsigned int COpenGL_SetColorMaterialEnable(const COpenGL *_this, int Value);
extern unsigned int COpenGL_SetActiveTexUnit(const COpenGL *_this, UINT32 Unit);
extern unsigned int COpenGL_SetDither(const COpenGL *_this, int Value);
extern unsigned int COpenGL_SetBlendEXT(const COpenGL *_this, int ForceValidation, GLenum SrcFactorRGB, GLenum DstFactorRGB, GLenum SrcFactorAlpha, GLenum DstFactorAlpha);
extern unsigned int COpenGL_SetBlend(const COpenGL *_this, int ForceValidation, GLenum SrcFactorRGB, GLenum DstFactorRGB);
extern unsigned int COpenGL_SetLight(const COpenGL *_this, UINT32 Light, const LightInfoType *Src);
extern unsigned int COpenGL_SetTexMatrix(const COpenGL *_this, UINT32 Unit, const float *m);
extern unsigned int COpenGL_SetTexMatrixIdentity(const COpenGL *_this, UINT32 Unit);
extern unsigned int COpenGL_EnableTexCoordArray(const COpenGL *_this, UINT32 Unit, GLint Size, GLenum Type, GLsizei Stride, const unsigned int *Pointer);
extern unsigned int COpenGL_Shutdown(const COpenGL *_this);
extern unsigned int COpenGL_Init(const COpenGL *_this, UINT32 Buffers, UINT32 MaxTextureUnits, UINT32 MaxTextureImageUnits);
#if defined(COD2_X64)
extern void CMemoryBuffer_Update(void);
extern void CMemoryBuffer_Reset(void);
extern UINT32 COpenGL_sFrameCount;
#endif

extern void *imp___ZN7COpenGL7sOpenGLE;
extern void *imp___ZN13COpenGLMatrix9sIdentityE;

extern void *CDirect3D_GetDirect3DInterface(void);

extern int MacDisplay_GetSupportsSeparateBlendFunc(void);
extern int MacDisplay_GetSupportsAnisotropicFiltering(void);
extern float MacDisplay_GetMaxSupportedAnisotropy(void);
extern int MacDisplay_GetMaxTextureImageUnits(void);
extern int MacDisplay_GetMaxTextureUnits(void);
#if !COD2_APPLE_SDK
extern void MacDisplay_ReleaseContext(void *ctx);
#endif
#if defined(COD2_X64)
extern uint16_t MacDisplay_SetGammaRamp(const unsigned short *);
#else
extern void MacDisplay_SetGammaRamp(const D3DGAMMARAMP *pRamp);
#endif
#if defined(COD2_X64)
extern void MacDisplay_FadeOut(float);
#else
extern void MacDisplay_FadeOut(int val);
#endif
extern void MacDisplay_FadeIn(float val);
#if defined(COD2_X64)
extern uint16_t MacDisplay_GetCurrentDimensions(int *, int *);
#else
extern void MacDisplay_GetCurrentDimensions(int *w, int *h);
#endif
extern void *MacDisplay_CreateScreenContext(int depth, int windowed, int stencil, int multiSample, int fsaa, int *hasAux);
#if defined(COD2_X64)
extern uint16_t MacDisplay_SwapContext(void *);
#else
extern void MacDisplay_SwapContext(void *ctx);
#endif
#if !COD2_APPLE_SDK
extern void MacDisplay_SetMode(int w, int h, int depth, int freq);
#endif
extern void MacDisplay_GetVideoMemoryInfo(int *video, int *texture);
extern const char *MacDisplay_GetGLVendor(void);
extern const char *MacDisplay_GetGLRenderer(void);
extern const char *MacDisplay_GetGLExtensions(void);
extern void game_dprintf(const char *fmt, ...);
extern void COpenGLMatrix_SetIdentity(float *m);
#if defined(COD2_X64)
extern D3DXMATRIX * D3DXMatrixMultiply(D3DXMATRIX *, const D3DXMATRIX *, const D3DXMATRIX *);
#else
extern void D3DXMatrixMultiply(float *out, const float *a, const float *b);
#endif
extern int MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(float *m, float ViewportWidth, float ViewportHeight);
extern D3DMATRIX *RB_GetActiveWorldMatrix(void);

#if defined(COD2_X64)
HRESULT CDirect3DDevice_CreatePixelShaderOpenGL(const CDirect3DDevice *, OpenGLPixelShaderType,
                                               const unsigned long *, IDirect3DPixelShader9 **);
extern IDirect3DPixelShader9 *CDirect3DPixelShader_CreateARBNative(const char *name, const char *code);
#endif

extern void COpenGLARBFragmentProgram_COpenGLARBFragmentProgram(
    const COpenGLARBFragmentProgram *_this, const string *Name, const string *Code);

typedef struct {
    void **vtable;
    ULONG refCount;
} PixelShaderStub;

static HRESULT PS_QueryInterface(const void *t, const IID *i, void **p)
{
    (void)t;
    (void)i;
    *p = (void *)t;
    return 0;
}
static ULONG PS_AddRef(const void *t)
{
    ((PixelShaderStub *)t)->refCount++;
    return ((PixelShaderStub *)t)->refCount;
}
static ULONG PS_Release(const void *t)
{
    PixelShaderStub *ps = (PixelShaderStub *)t;
    ULONG rc = --ps->refCount;
    if (!rc)
        free(ps);
    return rc;
}
static HRESULT PS_GetDevice(const void *t, void **d)
{
    (void)t;
    (void)d;
    return 0;
}
static HRESULT PS_GetFunction(const void *t, void *d, UINT *s)
{
    (void)t;
    (void)d;
    (void)s;
    return 0;
}

static void PS_Enable(const void *t)
{
    (void)t;
}
static void PS_Disable(const void *t)
{
    (void)t;
}
static void PS_SetConstants(const void *t, int s, const float *d, int c, int u)
{
    (void)t;
    (void)s;
    (void)d;
    (void)c;
    (void)u;
}
static void PS_D1(void *t)
{
    (void)t;
}
static void PS_D0(void *t)
{
    free(t);
}

static void *ps_stub_vtbl[] = {
    (void *)PS_QueryInterface, (void *)PS_AddRef, (void *)PS_Release,
    (void *)PS_GetDevice, (void *)PS_GetFunction,
    (void *)PS_D1, (void *)PS_D0,
    (void *)PS_Enable, (void *)PS_Disable, (void *)PS_SetConstants
};

static IDirect3DVertexShader9 *g_activeVertexShader = NULL;
static IDirect3DVertexDeclaration9 *g_activeVertexDeclaration = NULL;

typedef struct {
    void **vtable;
    ULONG refCount;
} QueryStub;

static HRESULT Q_QueryInterface(const void *t, const IID *i, void **p)
{
    (void)t;
    (void)i;
    *p = (void *)t;
    return 0;
}
static ULONG Q_AddRef(const void *t)
{
    ((QueryStub *)t)->refCount++;
    return ((QueryStub *)t)->refCount;
}
static ULONG Q_Release(const void *t)
{
    QueryStub *q = (QueryStub *)t;
    ULONG rc = --q->refCount;
    if (!rc)
        free(q);
    return rc;
}
static HRESULT Q_GetDevice(const void *t, void **d)
{
    (void)t;
    (void)d;
    return 0;
}
static int Q_GetType(const void *t)
{
    (void)t;
    return 0;
}
static UINT Q_GetDataSize(const void *t)
{
    (void)t;
    return 0;
}
static HRESULT Q_Issue(const void *t, DWORD f)
{
    (void)t;
    (void)f;
    return 0;
}
static HRESULT Q_GetData(const void *t, void *d, DWORD s, DWORD f)
{
    (void)t;
    (void)d;
    (void)s;
    (void)f;
    return 0;
}
static void Q_D1(void *t)
{
    (void)t;
}
static void Q_D0(void *t)
{
    free(t);
}

static void *query_stub_vtbl[] = {
    (void *)Q_QueryInterface, (void *)Q_AddRef, (void *)Q_Release,
    (void *)Q_GetDevice, (void *)Q_GetType, (void *)Q_GetDataSize,
    (void *)Q_Issue, (void *)Q_GetData,
    (void *)Q_D1, (void *)Q_D0
};

typedef struct {
    void **vtable;
    ULONG refCount;
    void *context;
    UINT32 cardType;
    UINT32 _pad010;
    IDirect3DSurface9 *renderTarget;
    IDirect3DSurface9 *depthStencil;
    IDirect3DSurface9 *backBuffer;
    IDirect3DSurface9 *depthBuffer;
    IDirect3DVertexBuffer9 *streams[16];
    UINT streamOffsets[16];
    IDirect3DIndexBuffer9 *indexBuffer;
    UINT _pad0A8;
    UINT streamStrides[16];
    IDirect3DSwapChain9 *swapChain;
    IDirect3DSurface9 *saveSurface;
    IDirect3DSurface9 *saveSurface2;
    unsigned char _pad0F8[0x394 - 0x0F8];
    DWORD srcBlend;
    DWORD destBlend;
    DWORD srcBlendAlpha;
    DWORD destBlendAlpha;
    DWORD _pad3A4;
    float alphaRef;
    GLenum alphaFunc;
    float worldMatrix[16];
    float viewMatrix[16];
    float worldViewMatrix[16];
    unsigned char isWorldViewDirty;
    unsigned char _pad471[3];
    float projMatrix[16];
    unsigned char isProjDirty;
    unsigned char _pad4B5[3];
    float texMatrix[16];
    UINT32 videoMemory;
    UINT32 textureMemory;
    UINT32 maxTextureSize;
    UINT32 maxTextureUnits;
    UINT32 maxTextureImageUnits;
    DWORD alphaSrcBlend;
    DWORD alphaRefVal;
    DWORD alphaFuncVal;
    unsigned char alphaBlendEnable;
    unsigned char _pad519[3];
    DWORD zEnable;
    DWORD zWriteEnable;
    DWORD fillMode;
    DWORD shadeMode;
    DWORD cullMode;
    DWORD lighting;
    DWORD zFunc;
    DWORD fogEnable;
    DWORD stencilEnable;
    DWORD stencilFunc;
    DWORD colorWriteEnable;
    DWORD stencilRef;
    DWORD stencilMask;
    DWORD stencilWriteMask;
    DWORD _pad554;
    DWORD sRGBWriteEnable;
    DWORD separateAlphaBlendEnable;
    DWORD colorWriteEnable1;
    DWORD depthBias;
    DWORD stencilZFail;
    DWORD stencilFail;
    DWORD stencilPass;
    DWORD fogColor;
    DWORD fogTableMode;
    DWORD fogStart;
    DWORD fogEnd;
    DWORD fogDensity;
    DWORD tweenFactor;
    DWORD blendOp;
    DWORD colorWriteEnable2;
    DWORD _pad594;
    DWORD _pad598;
    DWORD _pad59C;
    DWORD _pad5A0;
    DWORD _pad5A4;
    DWORD _pad5A8;
#if defined(COD2_X64)
    D3DMATERIAL9 material;
#else
    unsigned char lights[0x44];
#endif
    UINT32 viewportX;
    UINT32 viewportY;
    UINT32 viewportW;
    UINT32 viewportH;
    float viewportMinZ;
    float viewportMaxZ;

    void *texStageData;
    void *texStageEnd;
    void *texStageCap;
#if defined(COD2_X64)
    LightInfoType nativeLights[8];
#else
    unsigned char lightData[0x5A0];
#endif
    UINT32 bVal1;
    IDirect3DPixelShader9 *pixelShader;
    UINT32 bVal3;
    unsigned char _padBC0[8];
    DWORD bc8Val;
    unsigned char bccPad[0x100];
#if defined(COD2_X64)
    unsigned char depthStencilDetached;
    unsigned char hasDiffuseColor;
    DWORD colorVertex;
    float convertedView[16];
    struct DeviceStreamsNative *streamState;
    GLenum glSrcBlend, glDestBlend, glSrcBlendAlpha, glDestBlendAlpha;
    GLenum glStencilFunc;
    DWORD alphaBlendValue, ditherValue;
    DWORD ambientMaterialSource, diffuseMaterialSource;
    unsigned char blendStateChanged;
    GLfloat lightModelAmbient[4];
    GLfloat slopeScaleDepthBias, scaledDepthBias;
#endif
} DeviceImpl;
#if defined(COD2_X64)
_Static_assert(sizeof(DeviceImpl) <= 4096, "native device storage");
static HRESULT CDirect3DDevice_ResetNative(DeviceImpl *dev, const void *presentation);
void CDirect3DDevice_ResetStreamsNative(const CDirect3DDevice *_this);
_Static_assert(sizeof(D3DMATERIAL9) == 68, "native material width");
_Static_assert(sizeof(LightInfoType) == 180, "native light width");
typedef struct {
    uint32_t X, Y, Width, Height;
    float MinZ, MaxZ;
} NativeViewport;
_Static_assert(sizeof(NativeViewport) == 24, "native viewport cache width");
_Static_assert(offsetof(DeviceImpl, viewportMaxZ) - offsetof(DeviceImpl, viewportX) + sizeof(float) == 24,
               "device viewport cache layout");
#endif

#if defined(COD2_X64)
int CDirect3DDevice_IsNativeLive(const void *device)
{
    return ((const DeviceImpl *)device)->context != NULL;
}

void CDirect3DDevice_SetNativeBackBuffer(void *device, void *backBuffer, UINT adapter)
{
    typedef struct { void **vtable; ULONG refCount; IDirect3DSurface9 *backBuffer; } NativeSwapChain;
    DeviceImpl *dev = (DeviceImpl *)device;
    dev->cardType = adapter;
    dev->backBuffer = dev->renderTarget = (IDirect3DSurface9 *)backBuffer;
    if (dev->renderTarget)
        ((ULONG (*)(const void *))(*(void ***)dev->renderTarget)[1])(dev->renderTarget);
    dev->swapChain = (IDirect3DSwapChain9 *)calloc(1, sizeof(NativeSwapChain));
    if (dev->swapChain) {
        CDirect3DSwapChain_CDirect3DSwapChain((const CDirect3DSwapChain *)dev->swapChain);
        CDirect3DSwapChain_SetBackBuffer((const CDirect3DSwapChain *)dev->swapChain, dev->backBuffer);
    }
}
#endif

#if defined(COD2_X64)
/* Native renderer ownership is dx.device. Material_Shutdown releases shaders
 * before R_Shutdown releases the device; R_SafeRelease then nulls dx.device. */
static void CDirect3DDevice_ForgetPixelShaderNative(DeviceImpl *, const void *);

int CDirect3DDevice_DetachPixelShader(const void *shader)
{
    extern struct DxGlobals dx;
    DeviceImpl *device = (DeviceImpl *)dx.device;
    if (device) CDirect3DDevice_ForgetPixelShaderNative(device, shader);
    if (device && device->pixelShader == shader) {
        device->pixelShader = NULL;
        return 1;
    }
    return 0;
}
#endif

static DWORD g_currentFVF = 0;
static float g_vsConst[256 * 4];
#if COD2_APPLE_SDK
static IDirect3DBaseTexture9 *g_boundTextures[16] = { 0 };
#else
static IDirect3DBaseTexture9 *g_boundTextures[8] = { 0 };
#endif
static DWORD g_alphaTestEnable = 0;
static DWORD g_textureFactor = 0xFFFFFFFFu;

static float g_depthBias = 0.0f;
static float g_slopeScaleDepthBias = 0.0f;
static DWORD g_textureStageState[8][33];
static DWORD g_samplerState[16][14];
unsigned int g_prebind_texID = 0;
unsigned int g_prebind_texTarget = 0x0DE1;
static byte *g_colorArrayScratch = NULL;
static UINT g_colorArrayScratchCapacity = 0;
#if COD2_APPLE_SDK
static void MacTrace_Label(const void *shader, const char *source);
#endif

#if defined(COD2_X64)
typedef struct {
    unsigned char used, byteColor;
    UINT32 stream, offset;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const float *pointer;
} DeviceStreamNative;

typedef struct DeviceStreamsNative {
    DeviceStreamNative programmable[16];
    DeviceStreamNative position, normal, color, texCoord[8];
    UINT32 tokens[1024];
    UINT32 low, high;
    COpenGLVAONative *genericPacket;
    IDirect3DPixelShader9 *pixelShader;
    const UINT16 *indices;
    UINT32 elementCount;
    byte *convertedColors[16];
    size_t convertedColorBytes[16];
    /* VAO0 extra-format colors must not realloc storage retained by normal
     * generic packets or the COpenGL array caches. */
    byte *extendedColors[16];
    size_t extendedColorBytes[16];
    unsigned char tokensSet;
} DeviceStreamsNative;

static DeviceStreamsNative *CDirect3DDevice_StreamsNative(const CDirect3DDevice *);
void CDirect3DDevice_ResetStreamsNative(const CDirect3DDevice *);
void CDirect3DDevice_ParseShaderDeclaration(const CDirect3DDevice *, Bool, Bool *);
Bool CDirect3DDevice_GetStreamPtrs(const CDirect3DDevice *, UINT32);
Bool CDirect3DDevice_GetCanUseVAOWithTexCoordStreams(const CDirect3DDevice *, Bool);
void CDirect3DDevice_ValidateTransformation(const CDirect3DDevice *, UINT32, UINT32, const void *, UINT32);
void CDirect3DDevice_ValidateRasterization(const CDirect3DDevice *, UINT32, UINT32);
#endif

typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 elementCount;
    D3DVERTEXELEMENT9 *elements;
} CDirect3DVertexDeclarationImpl;

enum {
    COLOR_BYTES_RGBA,
    COLOR_BYTES_BGRA,
    COLOR_BYTES_ARGB
};
#include "lp64_color_order.h"

#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_REPEAT 0x2901
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_REPLACE 0x1E01
#define GL_ADD 0x0104
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_SOURCE2_ALPHA 0x858A
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859A
#define GL_SUBTRACT 0x84E7
#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#define GL_ALPHA_SCALE 0x0D1C
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_WRAP_R 0x8072

#define D3DTA_SELECTMASK 0x0F
#define D3DTA_COMPLEMENT 0x10
#define D3DTA_ALPHAREPLICATE 0x20

static void CDirect3DDevice_LoadIdentityMatrix(float *m)
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

#if defined(COD2_X64)
static COpenGLNative *CDirect3DDevice_GL(void)
{
    return (COpenGLNative *)imp__ZN7COpenGL7sOpenGLE;
}

/* Temporary pixel/copy overrides must bypass the cache-writing wrappers below.
 * Their matching restoration reads the persistent COpenGL state. */
#include "lp64_device_targets.inc"
#include "lp64_device_draw_scope.inc"

static void CDirect3DDevice_ActiveTexture(GLenum texture)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    UINT unit = texture - 0x84c0;
    if (gl->activeTexUnit != unit) {
        gl->activeTexUnit = unit;
        glActiveTextureARB(texture);
    }
}

static void CDirect3DDevice_SetCapability(GLenum capability, int enabled)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (gl->activeTexUnit < 16 &&
        (capability == 0x0de1 || capability == 0x806f || capability == 0x8513)) {
        UINT index = capability == 0x8513 ? 2 : capability == 0x806f ? 1 : 0;
        gl->texUnits[gl->activeTexUnit].mTargetEnabled[index] = !!enabled;
    }
    if (enabled) glEnable(capability); else glDisable(capability);
}

static void CDirect3DDevice_Enable(GLenum capability)
{
    CDirect3DDevice_SetCapability(capability, 1);
}

static void CDirect3DDevice_Disable(GLenum capability)
{
    CDirect3DDevice_SetCapability(capability, 0);
}

static void CDirect3DDevice_BindTexture(GLenum target, GLuint name)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    UINT index = target == 0x8513 ? 2 : target == 0x806f ? 1 : 0;
    if (gl->activeTexUnit < 16) {
        CTexUnitNative *unit = &gl->texUnits[gl->activeTexUnit];
        if (unit->mTexID[index] && *unit->mTexID[index] != name)
            unit->mTexID[index] = NULL;
    }
    glBindTexture(target, name);
}

static void CDirect3DDevice_MatrixMode(GLenum mode)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (gl->matrixMode != mode) {
        gl->matrixMode = mode;
        glMatrixMode(mode);
    }
}

static void CDirect3DDevice_LoadMatrix(const GLfloat *matrix)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (gl->matrixMode == 0x1700)
        memcpy(gl->modelViewMatrix, matrix, sizeof(gl->modelViewMatrix));
    glLoadMatrixf(matrix);
}

static void CDirect3DDevice_LoadIdentity(void)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (gl->matrixMode == 0x1700)
        CDirect3DDevice_LoadIdentityMatrix(gl->modelViewMatrix);
    glLoadIdentity();
}
#define glEnable CDirect3DDevice_Enable
#define glDisable CDirect3DDevice_Disable
#define glActiveTextureARB CDirect3DDevice_ActiveTexture
#define glBindTexture CDirect3DDevice_BindTexture
#define glMatrixMode CDirect3DDevice_MatrixMode
#define glLoadMatrixf CDirect3DDevice_LoadMatrix
#define glLoadIdentity CDirect3DDevice_LoadIdentity

static CTexStageNative *CDirect3DDevice_Stage(DeviceImpl *dev, UINT stage)
{
    CTexStageNative *begin = dev->texStageData;
    CTexStageNative *end = dev->texStageEnd;
    return begin && stage < (UINT)(end - begin) ? begin + stage : NULL;
}

static DWORD *CDirect3DDevice_StageField(CTexStageNative *stage, UINT type)
{
    switch (type) {
    case 1: return &stage->colorOp;
    case 2: return &stage->colorArg1;
    case 3: return &stage->colorArg2;
    case 4: return &stage->alphaOp;
    case 5: return &stage->alphaArg1;
    case 6: return &stage->alphaArg2;
    case 11: return &stage->texCoordIndex;
    case 24: return &stage->transformFlags;
    case 26: return &stage->colorArg0;
    case 27: return &stage->alphaArg0;
    default: return NULL;
    }
}

static DWORD *CDirect3DDevice_SamplerField(CTexStageNative *stage, UINT type)
{
    switch (type) {
    case 1: return &stage->addressU;
    case 2: return &stage->addressV;
    case 3: return &stage->addressW;
    case 4: return &stage->borderColor;
    case 5: return &stage->magFilter;
    case 6: return &stage->minFilter;
    case 7: return &stage->mipFilter;
    default: return NULL;
    }
}

/* The i386 getter uses truncation to signed int64, then returns its low word.
 * Bound exceptional float inputs instead of invoking an undefined C cast. */
static DWORD CDirect3DDevice_TruncateSampler(float value)
{
    if (!(value >= -0x1p63f && value < 0x1p63f))
        return 0;
    return (DWORD)(uint64_t)(int64_t)value;
}

extern D3DXMATRIX *D3DXMatrixInverse(D3DXMATRIX *, FLOAT *, const D3DXMATRIX *);
extern D3DXVECTOR4 *D3DXVec4Transform(D3DXVECTOR4 *, const D3DXVECTOR4 *, const D3DXMATRIX *);

void CDirect3DDevice_ValidateLighting(const CDirect3DDevice *_this)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (gl->supportsATIArrayRevComps) {
        if (!dev->lighting && !dev->hasDiffuseColor)
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }
    if (!dev->lighting)
        return;
    COpenGL_SetColorMaterialEnable((const COpenGL *)gl,
                                   dev->colorVertex && dev->hasDiffuseColor);
    D3DXMATRIX inverse, transform;
    int haveTransform = 0;
    for (UINT i = 0; i < 8; ++i) {
        const LightInfoType *light = &dev->nativeLights[i];
        if (!light->Enabled)
            continue;
        D3DXVECTOR4 direction, transformed;
        for (UINT j = 0; j < 3; ++j) {
            uint32_t bits;
            memcpy(&bits, (const GLfloat *)&light->Direction + j, sizeof(bits));
            bits ^= 0x80000000u;
            memcpy((GLfloat *)&direction + j, &bits, sizeof(bits));
        }
        direction.w = 0.0f;
        if (!haveTransform) {
            /* Mac ignores this return. The native math provider supplies its
             * defined input-copy fallback if the generic inverse fails. */
            D3DXMatrixInverse(&inverse, NULL, (const D3DXMATRIX *)gl->modelViewMatrix);
            D3DXMatrixMultiply(&transform, (const D3DXMATRIX *)dev->convertedView, &inverse);
            haveTransform = 1;
        }
        D3DXVec4Transform(&transformed, &direction, &transform);
        glLightfv(0x4000 + i, 0x1203, (const GLfloat *)&transformed);
    }
}

void CDirect3DDevice_ValidateModelViewMatrix(const CDirect3DDevice *_this)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (!dev->isProjDirty)
        return;
    dev->isProjDirty = 0;
    D3DXMatrixMultiply((D3DXMATRIX *)dev->worldViewMatrix,
                      (const D3DXMATRIX *)dev->worldMatrix,
                      (const D3DXMATRIX *)dev->convertedView);
    glMatrixMode(0x1700);
    glLoadMatrixf(dev->worldViewMatrix);
}
#endif

static const D3DVERTEXELEMENT9 *CDirect3DDevice_FindVertexElement(
    UINT stream, BYTE usage, BYTE usageIndex)
{
    CDirect3DVertexDeclarationImpl *declaration;
    UINT32 i;

#if defined(COD2_X64)
    if (g_currentFVF) {
        static D3DVERTEXELEMENT9 element;
        UINT offset = 0;
        if (g_currentFVF & 2) {
            if (!stream && usage == 0 && !usageIndex) {
                element = (D3DVERTEXELEMENT9){ 0, offset, 2, 0, 0, 0 }; return &element;
            }
            offset += 12;
        }
        if (g_currentFVF & 0x10) {
            if (!stream && usage == 3 && !usageIndex) {
                element = (D3DVERTEXELEMENT9){ 0, offset, 2, 0, 3, 0 }; return &element;
            }
            offset += 12;
        }
        if (g_currentFVF & 0x40) {
            if (!stream && usage == 10 && !usageIndex) {
                element = (D3DVERTEXELEMENT9){ 0, offset, 4, 0, 10, 0 }; return &element;
            }
            offset += 4;
        }
        for (UINT i = 0; i < ((g_currentFVF & 0xf00) >> 8); ++i) {
            UINT dimensions = (g_currentFVF >> ((16 + i * 2) & 31)) & 3;
            BYTE type = dimensions == 0 ? 1 : dimensions == 1 ? 2 : dimensions == 2 ? 3 : 0;
            if (!stream && usage == 5 && usageIndex == i) {
                element = (D3DVERTEXELEMENT9){ 0, offset, type, 0, 5, i }; return &element;
            }
            offset += (type + 1) * 4;
        }
        return NULL;
    }
#endif
    if (!g_activeVertexDeclaration)
        return NULL;

    declaration = (CDirect3DVertexDeclarationImpl *)g_activeVertexDeclaration;
    for (i = 0; i + 1 < declaration->elementCount; ++i) {
        const D3DVERTEXELEMENT9 *element = &declaration->elements[i];
        if (element->Stream == stream &&
            element->Usage == usage &&
            element->UsageIndex == usageIndex) {
            return element;
        }
    }

    return NULL;
}

#if defined(COD2_X64)
static const byte *CDirect3DDevice_ConvertColorArray(const byte *vertBase, UINT stride,
                                                     int colorOffset, UINT vertexCount, int byteOrder,
                                                     const unsigned short *indices, UINT indexCount)
#else
static const byte *CDirect3DDevice_ConvertColorArray(const byte *vertBase, UINT stride,
                                                     int colorOffset, UINT vertexCount, int byteOrder)
#endif
{
    UINT i;
    UINT bytesNeeded;

    if (colorOffset < 0 || vertexCount == 0)
        return NULL;

#if defined(COD2_X64)
    /* Callers already accept the original interleaved RGBA array on NULL. */
    if (byteOrder == COLOR_BYTES_RGBA)
        return NULL;
    if (!indexCount)
        return NULL;
    /* GL indexes every enabled client array with the same unsigned shorts.
     * Keep scratch offsets unchanged, but convert only the indexed span. */
    UINT firstVertex = vertexCount;
    UINT lastVertex = 0;
    for (UINT n = 0; n < indexCount; ++n) {
        if (indices[n] < firstVertex) firstVertex = indices[n];
        if (indices[n] > lastVertex) lastVertex = indices[n];
    }
    vertexCount = lastVertex + 1;
#endif

    bytesNeeded = vertexCount * 4;
    if (bytesNeeded > g_colorArrayScratchCapacity) {
        byte *newScratch = (byte *)realloc(g_colorArrayScratch, bytesNeeded);
        if (!newScratch)
            return NULL;
        g_colorArrayScratch = newScratch;
        g_colorArrayScratchCapacity = bytesNeeded;
    }

#if defined(COD2_X64)
    for (i = firstVertex; i < vertexCount; ++i) {
#else
    for (i = 0; i < vertexCount; ++i) {
#endif
        const byte *src = vertBase + i * stride + colorOffset;
        byte *dst = g_colorArrayScratch + i * 4;

        switch (byteOrder) {
        case COLOR_BYTES_RGBA:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            break;
        case COLOR_BYTES_ARGB:
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
            break;
        default:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            break;
        }
    }

    return g_colorArrayScratch;
}

static GLenum CDirect3DDevice_MapCompareFunc(DWORD func)
{
    switch (func) {
    case 1:
        return 0x0200;
    case 2:
        return 0x0201;
    case 3:
        return 0x0202;
    case 4:
        return 0x0203;
    case 5:
        return 0x0204;
    case 6:
        return 0x0205;
    case 7:
        return 0x0206;
    default:
        return 0x0207;
    }
}

static void CDirect3DDevice_DisableExtraTextureUnits(void)
{
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glDisable(0x0DE1);
    glDisable(GL_TEXTURE_CUBE_MAP);
    glClientActiveTextureARB(GL_TEXTURE1_ARB);
    glDisableClientState(0x8078);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);
}

static GLenum CDirect3DDevice_MapBlendFunc(DWORD blend)
{
    switch (blend) {
    case 1:
        return 0;
    case 2:
        return 1;
    case 3:
        return 0x0300;
    case 4:
        return 0x0301;
    case 5:
        return 0x0302;
    case 6:
        return 0x0303;
    case 7:
        return 0x0304;
    case 8:
        return 0x0305;
    case 9:
        return 0x0306;
    case 10:
        return 0x0307;
    case 11:
        return 0x0308;
    case 12:
        return 0x0302;
    case 13:
        return 0x0303;
    default:
        return 1;
    }
}

/* A texture object is identified by its vtable, never by where the allocator happened to
   put it. This used to be a `(uintptr_t)texture <= 0x08000000` range test, which rejected
   perfectly valid textures whenever malloc returned a low address -- every lookup then
   fell back to the prebind texture and the whole scene drew untextured.
   The <64K test only rejects values that cannot be pointers at all (Windows reserves the
   first 64K), so a stale handle is still safe to pass in. */
static int CDirect3DDevice_IsRealTexture(IDirect3DBaseTexture9 *texture)
{
    void **vt;

    if (!texture || (uintptr_t)texture < 0x10000u)
        return 0;
    vt = *(void ***)texture;
#if COD2_APPLE_SDK
    if (vt == vtbl_CDirect3DVolumeTexture) return 1;
#endif
    return vt == (void **)vtbl_CDirect3DTexture || vt == (void **)vtbl_CDirect3DCubeTexture;
}

static unsigned int CDirect3DDevice_GetTextureGLId(IDirect3DBaseTexture9 *texture)
{
    if (!CDirect3DDevice_IsRealTexture(texture))
        return 0;
    /* CDirect3DTexture.texIDStorage: 3 ptrs (2 vtbl + mpTexID) + 16 int fields(64B) +
       2 ptrs (surfaces,pixelData) => 5*sizeof(void*)+64. x86=0x54, x64=0x68.
       CDirect3DCubeTexture has the same layout up to this field. */
#if defined(COD2_X64)
    extern GLuint CDirect3DTexture_GetGLName(const void *texture);
    extern GLuint CDirect3DCubeTexture_GetGLName(const void *texture);
#if COD2_APPLE_SDK
    extern GLuint CDirect3DVolumeTexture_GetGLName(const void *texture);
    if (*(void ***)texture == vtbl_CDirect3DVolumeTexture)
        return CDirect3DVolumeTexture_GetGLName(texture);
#endif
    if (*(void ***)texture == vtbl_CDirect3DCubeTexture)
        return CDirect3DCubeTexture_GetGLName(texture);
    return CDirect3DTexture_GetGLName(texture);
#else
    return *(unsigned int *)((byte *)texture + 5 * sizeof(void *) + 64);
#endif
}

static GLenum CDirect3DDevice_GetTextureTarget(IDirect3DBaseTexture9 *texture)
{
    if (!CDirect3DDevice_IsRealTexture(texture))
        return 0x0DE1;
#if COD2_APPLE_SDK
    if (*(void ***)texture == vtbl_CDirect3DVolumeTexture)
        return GL_TEXTURE_3D;
#endif
    if (*(void ***)texture == vtbl_CDirect3DCubeTexture)
        return GL_TEXTURE_CUBE_MAP;
    return 0x0DE1;
}

static GLenum CDirect3DDevice_GetImageTextureTarget(const GfxImage *image)
{
    if (image && image->mapType == 5)
        return GL_TEXTURE_CUBE_MAP;
    return 0x0DE1;
}

static void CDirect3DDevice_BindTextureTarget(GLenum target, unsigned int texID)
{
    if (target == GL_TEXTURE_CUBE_MAP) {
        glDisable(0x0DE1);
        glEnable(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texID);
    } else {
        glDisable(GL_TEXTURE_CUBE_MAP);
        glEnable(0x0DE1);
        glBindTexture(0x0DE1, texID);
    }
}

static GLenum CDirect3DDevice_MapTextureAddress(DWORD addressMode)
{
    switch (addressMode) {
    case D3DTADDRESS_CLAMP:
        return GL_CLAMP_TO_EDGE;
    case D3DTADDRESS_MIRROR:
        return GL_MIRRORED_REPEAT;
    default:
        return GL_REPEAT;
    }
}

static GLenum CDirect3DDevice_MapTextureMagFilter(DWORD filter)
{
    switch (filter) {
    case D3DTEXF_POINT:
        return GL_NEAREST;
    case D3DTEXF_LINEAR:
    case D3DTEXF_ANISOTROPIC:
    default:
        return GL_LINEAR;
    }
}

static GLenum CDirect3DDevice_MapTextureMinFilter(DWORD minFilter, DWORD mipFilter)
{
    if (minFilter != D3DTEXF_POINT && minFilter != D3DTEXF_LINEAR && minFilter != D3DTEXF_ANISOTROPIC)
        minFilter = D3DTEXF_LINEAR;

    if (mipFilter == D3DTEXF_NONE) {
        return (minFilter == D3DTEXF_POINT) ? GL_NEAREST : GL_LINEAR;
    }

    if (mipFilter == D3DTEXF_POINT) {
        return (minFilter == D3DTEXF_POINT) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
    }

    return (minFilter == D3DTEXF_POINT) ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
}

static GLenum CDirect3DDevice_MapTextureArgSource(DWORD arg)
{
    switch (arg & D3DTA_SELECTMASK) {
    case 0:
        return GL_PRIMARY_COLOR;
    case 1:
        return GL_PREVIOUS;
    case 2:
        return 0x1702;
    case 3:
    case 6:
        return GL_CONSTANT;
    case 4:
        return GL_PRIMARY_COLOR;
    case 5:
    default:
        return GL_PREVIOUS;
    }
}

static GLenum CDirect3DDevice_MapTextureArgOperandRGB(DWORD arg)
{
    if (arg & D3DTA_ALPHAREPLICATE) {
        return (arg & D3DTA_COMPLEMENT) ? 0x0303  : 0x0302 ;
    }
    return (arg & D3DTA_COMPLEMENT) ? 0x0301  : 0x0300 ;
}

static GLenum CDirect3DDevice_MapTextureArgOperandAlpha(DWORD arg)
{
    return (arg & D3DTA_COMPLEMENT) ? 0x0303  : 0x0302 ;
}

static void __attribute__((noinline)) COD2_FORCE_ALIGN_ARG_POINTER CDirect3DDevice_SetTextureFactorColor(void)
{
    GLfloat factor[4];

    factor[0] = ((g_textureFactor >> 16) & 0xFF) / 255.0f;
    factor[1] = ((g_textureFactor >> 8) & 0xFF) / 255.0f;
    factor[2] = (g_textureFactor & 0xFF) / 255.0f;
    factor[3] = ((g_textureFactor >> 24) & 0xFF) / 255.0f;
    glTexEnvfv(GL_TEXTURE_ENV, 0x2201 , factor);
}

static void CDirect3DDevice_SetTextureCombineArgs(GLenum source0Enum, GLenum operand0Enum,
                                                  GLenum source1Enum, GLenum operand1Enum,
                                                  GLenum source2Enum, GLenum operand2Enum,
                                                  DWORD arg0, DWORD arg1, DWORD arg2,
                                                  int alpha)
{
    GLenum (*mapOperand)(DWORD) = alpha ? CDirect3DDevice_MapTextureArgOperandAlpha
                                        : CDirect3DDevice_MapTextureArgOperandRGB;
    GLenum (*sourceEnumBase)(DWORD) = CDirect3DDevice_MapTextureArgSource;

    glTexEnvi(GL_TEXTURE_ENV, source0Enum, sourceEnumBase(arg0));
    glTexEnvi(GL_TEXTURE_ENV, operand0Enum, mapOperand(arg0));
    glTexEnvi(GL_TEXTURE_ENV, source1Enum, sourceEnumBase(arg1));
    glTexEnvi(GL_TEXTURE_ENV, operand1Enum, mapOperand(arg1));
    glTexEnvi(GL_TEXTURE_ENV, source2Enum, sourceEnumBase(arg2));
    glTexEnvi(GL_TEXTURE_ENV, operand2Enum, mapOperand(arg2));
}

static void CDirect3DDevice_ApplyTextureCombineRGB(DWORD op, DWORD arg0, DWORD arg1, DWORD arg2)
{
    GLenum combine = 0x2100;
    GLint scale = 1;
    DWORD src0 = arg1;
    DWORD src1 = arg2;
    DWORD src2 = arg0;

    switch (op) {
    case D3DTOP_DISABLE:
    case D3DTOP_SELECTARG1:
        combine = GL_REPLACE;
        src0 = arg1;
        break;
    case D3DTOP_SELECTARG2:
        combine = GL_REPLACE;
        src0 = arg2;
        break;
    case D3DTOP_MODULATE:
        combine = 0x2100;
        break;
    case D3DTOP_MODULATE2X:
        combine = 0x2100;
        scale = 2;
        break;
    case D3DTOP_MODULATE4X:
        combine = 0x2100;
        scale = 4;
        break;
    case D3DTOP_ADD:
        combine = GL_ADD;
        break;
    case D3DTOP_ADDSIGNED:
    case D3DTOP_ADDSIGNED2X:
        combine = GL_ADD_SIGNED;
        scale = (op == D3DTOP_ADDSIGNED2X) ? 2 : 1;
        break;
    case D3DTOP_SUBTRACT:
        combine = GL_SUBTRACT;
        break;
    case D3DTOP_BLENDDIFFUSEALPHA:
        combine = GL_INTERPOLATE;
        src2 = 0;
        break;
    case D3DTOP_BLENDTEXTUREALPHA:
        combine = GL_INTERPOLATE;
        src2 = 2;
        break;
    case D3DTOP_BLENDFACTORALPHA:
        combine = GL_INTERPOLATE;
        src2 = 3;
        break;
    case D3DTOP_BLENDCURRENTALPHA:
        combine = GL_INTERPOLATE;
        src2 = 1;
        break;
    case D3DTOP_LERP:
        combine = GL_INTERPOLATE;
        break;
    default:
        combine = 0x2100;
        break;
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, combine);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, scale);
    CDirect3DDevice_SetTextureCombineArgs(GL_SOURCE0_RGB, GL_OPERAND0_RGB,
                                          GL_SOURCE1_RGB, GL_OPERAND1_RGB,
                                          GL_SOURCE2_RGB, GL_OPERAND2_RGB,
                                          src0, src1, src2, 0);
}

static void CDirect3DDevice_ApplyTextureCombineAlpha(DWORD op, DWORD arg0, DWORD arg1, DWORD arg2)
{
    GLenum combine = GL_REPLACE;
    GLint scale = 1;
    DWORD src0 = arg1;
    DWORD src1 = arg2;
    DWORD src2 = arg0;

    switch (op) {
    case D3DTOP_DISABLE:
    case D3DTOP_SELECTARG1:
        combine = GL_REPLACE;
        src0 = arg1;
        break;
    case D3DTOP_SELECTARG2:
        combine = GL_REPLACE;
        src0 = arg2;
        break;
    case D3DTOP_MODULATE:
        combine = 0x2100;
        break;
    case D3DTOP_MODULATE2X:
        combine = 0x2100;
        scale = 2;
        break;
    case D3DTOP_MODULATE4X:
        combine = 0x2100;
        scale = 4;
        break;
    case D3DTOP_ADD:
        combine = GL_ADD;
        break;
    case D3DTOP_ADDSIGNED:
    case D3DTOP_ADDSIGNED2X:
        combine = GL_ADD_SIGNED;
        scale = (op == D3DTOP_ADDSIGNED2X) ? 2 : 1;
        break;
    case D3DTOP_SUBTRACT:
        combine = GL_SUBTRACT;
        break;
    case D3DTOP_BLENDDIFFUSEALPHA:
        combine = GL_INTERPOLATE;
        src2 = 0;
        break;
    case D3DTOP_BLENDTEXTUREALPHA:
        combine = GL_INTERPOLATE;
        src2 = 2;
        break;
    case D3DTOP_BLENDFACTORALPHA:
        combine = GL_INTERPOLATE;
        src2 = 3;
        break;
    case D3DTOP_BLENDCURRENTALPHA:
        combine = GL_INTERPOLATE;
        src2 = 1;
        break;
    case D3DTOP_LERP:
        combine = GL_INTERPOLATE;
        break;
    default:
        combine = GL_REPLACE;
        break;
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, combine);
    glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, scale);
    CDirect3DDevice_SetTextureCombineArgs(GL_SOURCE0_ALPHA, GL_OPERAND0_ALPHA,
                                          GL_SOURCE1_ALPHA, GL_OPERAND1_ALPHA,
                                          GL_SOURCE2_ALPHA, GL_OPERAND2_ALPHA,
                                          src0, src1, src2, 1);
}

#include "lp64_shader_options.h"
#if defined(COD2_X64)
/* A renderer feature option, sampled once instead of once per indexed draw. */
static int CDirect3DDevice_UsePrograms(void)
{
    static int enabled = -1;
    if (enabled < 0)
#if COD2_APPLE_SDK
        enabled = MacShader_UseCachePrograms();
#else
        enabled = getenv("D3D_PROG") != NULL;
#endif
    return enabled;
}
#endif

static int CDirect3DDevice_LightmapScale(void)
{
    static int scale = -1;
    if (scale < 0) {
        const char *e = getenv("LMAP_SCALE");
        scale = e ? atoi(e) : 1;
        if (scale < 1)
            scale = 1;
        if (scale > 4)
            scale = 4;
    }
    return scale;
}

static void CDirect3DDevice_ApplyTextureStageState(UINT stage)
{
    DWORD colorOp = g_textureStageState[stage][D3DTSS_COLOROP];
    DWORD alphaOp = g_textureStageState[stage][D3DTSS_ALPHAOP];
    DWORD colorArg0 = g_textureStageState[stage][D3DTSS_COLORARG0];
    DWORD colorArg1 = g_textureStageState[stage][D3DTSS_COLORARG1];
    DWORD colorArg2 = g_textureStageState[stage][D3DTSS_COLORARG2];
    DWORD alphaArg0 = g_textureStageState[stage][D3DTSS_ALPHAARG0];
    DWORD alphaArg1 = g_textureStageState[stage][D3DTSS_ALPHAARG1];
    DWORD alphaArg2 = g_textureStageState[stage][D3DTSS_ALPHAARG2];

#if !defined(COD2_X64)
    if (!colorOp)
        colorOp = D3DTOP_MODULATE;
    if (!alphaOp)
        alphaOp = D3DTOP_MODULATE;
    if (!colorArg1)
        colorArg1 = 2;
    if (!colorArg2)
        colorArg2 = 1;
    if (!alphaArg1)
        alphaArg1 = 2;
    if (!alphaArg2)
        alphaArg2 = 1;

#endif

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    CDirect3DDevice_SetTextureFactorColor();
    CDirect3DDevice_ApplyTextureCombineRGB(colorOp, colorArg0, colorArg1, colorArg2);
    CDirect3DDevice_ApplyTextureCombineAlpha(alphaOp, alphaArg0, alphaArg1, alphaArg2);
}

#if defined(COD2_X64)
void CDirect3DDevice_SynchronizeD3DAndOpenGLTextureState(const CDirect3DDevice *, IDirect3DBaseTexture9 *, UINT);
static void CDirect3DDevice_ApplySamplerState(DeviceImpl *dev, UINT sampler, GLenum target)
{
    if (sampler < sizeof(g_boundTextures) / sizeof(g_boundTextures[0]) &&
        CDirect3DDevice_IsRealTexture(g_boundTextures[sampler]) &&
        CDirect3DDevice_GetTextureTarget(g_boundTextures[sampler]) == target) {
        GLint bound;
        GLenum query = target == 0x8513 ? 0x8514 : target == 0x806f ? 0x806a : 0x8069;
        glGetIntegerv(query, &bound);
        /* The fallback renderer can substitute a lightmap for its D3D stage.
         * Only synchronize the actual selected COM texture. */
        if ((GLuint)bound == CDirect3DDevice_GetTextureGLId(g_boundTextures[sampler])) {
            DeviceDraw_SaveSampler(target, (void *)((void **)g_boundTextures[sampler] + 1));
            CDirect3DDevice_SynchronizeD3DAndOpenGLTextureState((const CDirect3DDevice *)dev,
                                                             g_boundTextures[sampler], sampler);
            return;
        }
    }
    DeviceDraw_SaveSampler(target, NULL);
#else
static void CDirect3DDevice_ApplySamplerState(UINT sampler, GLenum target)
{
#endif
    DWORD addressU = g_samplerState[sampler][D3DSAMP_ADDRESSU];
    DWORD addressV = g_samplerState[sampler][D3DSAMP_ADDRESSV];
    DWORD addressW = g_samplerState[sampler][D3DSAMP_ADDRESSW];
    DWORD minFilter = g_samplerState[sampler][D3DSAMP_MINFILTER];
    DWORD magFilter = g_samplerState[sampler][D3DSAMP_MAGFILTER];
    DWORD mipFilter = g_samplerState[sampler][D3DSAMP_MIPFILTER];

    if (!addressU)
        addressU = D3DTADDRESS_WRAP;
    if (!addressV)
        addressV = D3DTADDRESS_WRAP;
    if (!addressW)
        addressW = D3DTADDRESS_WRAP;
    if (!minFilter)
        minFilter = D3DTEXF_LINEAR;
    if (!magFilter)
        magFilter = D3DTEXF_LINEAR;

    glTexParameteri(target, 0x2802 ,
                    CDirect3DDevice_MapTextureAddress(addressU));
    glTexParameteri(target, 0x2803 ,
                    CDirect3DDevice_MapTextureAddress(addressV));
#if COD2_APPLE_SDK
    if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_3D) {
#else
    if (target == GL_TEXTURE_CUBE_MAP) {
#endif
        glTexParameteri(target, GL_TEXTURE_WRAP_R,
                        CDirect3DDevice_MapTextureAddress(addressW));
    }
    glTexParameteri(target, 0x2801 ,
                    CDirect3DDevice_MapTextureMinFilter(minFilter, mipFilter));
    glTexParameteri(target, 0x2800 ,
                    CDirect3DDevice_MapTextureMagFilter(magFilter));
}

static void CDirect3DDevice_UpdateTextureIfNeeded(IDirect3DBaseTexture9 *texture)
{
#if defined(COD2_X64)
    extern int COpenGLTexture_EnsureNameNative(void *);
    extern void CDirect3DTexture_UpdateOpenGLSurfacesNative(const CDirect3DTexture *, int);
    extern void CDirect3DCubeTexture_UpdateOpenGLSurfacesNative(const CDirect3DCubeTexture *, int);
    extern void CDirect3DVolumeTexture_UpdateOpenGLSurfacesNative(const CDirect3DVolumeTexture *, int);
    if (!CDirect3DDevice_IsRealTexture(texture))
        return;
    GLenum target = GL_TEXTURE_2D, query = GL_TEXTURE_BINDING_2D;
    void **vtable = *(void ***)texture;
    if (vtable == (void **)vtbl_CDirect3DCubeTexture) {
        target = GL_TEXTURE_CUBE_MAP;
        query = GL_TEXTURE_BINDING_CUBE_MAP;
    }
#if COD2_APPLE_SDK
    else if (vtable == vtbl_CDirect3DVolumeTexture) {
        target = GL_TEXTURE_3D;
        query = GL_TEXTURE_BINDING_3D;
    }
#endif
    int recreate = COpenGLTexture_EnsureNameNative((void **)texture + 1);
    GLint previous;
    glGetIntegerv(query, &previous);
    glBindTexture(target, CDirect3DDevice_GetTextureGLId(texture));
    if (vtable == (void **)vtbl_CDirect3DTexture)
        CDirect3DTexture_UpdateOpenGLSurfacesNative((void *)texture, recreate);
    else if (vtable == (void **)vtbl_CDirect3DCubeTexture)
        CDirect3DCubeTexture_UpdateOpenGLSurfacesNative((void *)texture, recreate);
#if COD2_APPLE_SDK
    else
        CDirect3DVolumeTexture_UpdateOpenGLSurfacesNative((void *)texture, recreate);
#endif
    glBindTexture(target, previous);
#else
    extern void CDirect3DTexture_UpdateOpenGLSurfaces(const CDirect3DTexture *_this);

    if (!CDirect3DDevice_IsRealTexture(texture))
        return;
    if (*(void ***)texture == vtbl_CDirect3DTexture)
        CDirect3DTexture_UpdateOpenGLSurfaces((const CDirect3DTexture *)texture);
#if COD2_APPLE_SDK
    else if (*(void ***)texture == vtbl_CDirect3DVolumeTexture) {
        extern void CDirect3DVolumeTexture_UpdateOpenGLSurfaces(const CDirect3DVolumeTexture *);
        CDirect3DVolumeTexture_UpdateOpenGLSurfaces((const void *)texture);
    }
#endif
#endif
}

static GfxImage *CDirect3DDevice_SelectMaterialColorImage(const Material *material)
{
    GfxImage *fallback;
    int textureIndex;

    if (!material || !material->textures || !material->textureCount)
        return NULL;

    fallback = NULL;
    for (textureIndex = 0; textureIndex < material->textureCount; ++textureIndex) {
        const MaterialTextureDef *texture = &material->textures[textureIndex];
        GfxImage *image;

        if (texture->semantic == 5)
            continue;

        image = texture->u.image;
        if (!image)
            continue;

        if (texture->semantic == 2)
            return image;

        if (!fallback)
            fallback = image;
    }

    return fallback;
}

#if defined(COD2_X64)
extern unsigned int COpenGL_SetTex(const COpenGL *, UINT32, GLenum, const COpenGLTexture *);
extern unsigned int COpenGL_SetTexBorderColor(const COpenGL *, UINT32, GLenum, UINT32);

static GLenum CDirect3DDevice_SamplerAddress(DWORD address)
{
    switch (address) {
    case 1: return 0x2901;
    case 3: return 0x812f;
    case 4: return 0x812d;
    default: return 0;
    }
}

void CDirect3DDevice_SynchronizeD3DAndOpenGLTextureState(const CDirect3DDevice *_this,
                                                       IDirect3DBaseTexture9 *texture, UINT stageIndex)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    COpenGLNative *gl = CDirect3DDevice_GL();
    CTexStageNative *stage = CDirect3DDevice_Stage(dev, stageIndex);
    if (stageIndex >= 16 || !stage || !CDirect3DDevice_IsRealTexture(texture))
        return;
    GLenum target = CDirect3DDevice_GetTextureTarget(texture);
    UINT index = target == 0x8513 ? 2 : target == 0x806f ? 1 : 0;
    COpenGLTextureStateNative *info = (void *)((void **)texture + 1);
    CTexUnitNative *unit = &gl->texUnits[stageIndex];
    int hadName = info->name && *info->name;
    CDirect3DDevice_UpdateTextureIfNeeded(texture);
    if (!hadName)
        unit->mTexID[index] = NULL;
    COpenGL_SetTex((const COpenGL *)gl, stageIndex, target, (const COpenGLTexture *)info);
    GLenum wrapS = CDirect3DDevice_SamplerAddress(stage->addressU);
    GLenum wrapT = CDirect3DDevice_SamplerAddress(stage->addressV);
    GLenum wrapR = CDirect3DDevice_SamplerAddress(stage->addressW);
#define DEVICE_TEXTURE_INT(cache, pname, value) \
    do { \
        if (unit->cache[index] != (value)) { \
            COpenGL_SetActiveTexUnit((const COpenGL *)gl, stageIndex); \
            glTexParameteri(target, pname, value); \
            unit->cache[index] = value; \
        } \
    } while (0)
    DEVICE_TEXTURE_INT(mTexWrapS, 0x2802, wrapS); info->wrapS = wrapS;
    DEVICE_TEXTURE_INT(mTexWrapT, 0x2803, wrapT); info->wrapT = wrapT;
    DEVICE_TEXTURE_INT(mTexWrapR, 0x8072, wrapR); info->wrapR = wrapR;
    if (stage->addressU == 4 || stage->addressV == 4 || stage->addressW == 4) {
        COpenGL_SetTexBorderColor((const COpenGL *)gl, stageIndex, target, stage->borderColor);
        info->border = stage->borderColor;
    }
    int base = 0;
    UINT width = info->width, height = info->height;
    if (width && width > 3 && height > 3) {
        do {
            height >>= 1;
            ++base;
            width >>= 1;
        } while (width && width > 3 && height > 3);
    }
    void **methods = *(void ***)texture;
    int levels = ((UINT (*)(const void *))methods[13])(texture);
    int lastLevel = (levels >= base ? levels : base) - 1;
    if (lastLevel < 0) lastLevel = 0;
    GLenum mag = stage->magFilter == 3 || stage->magFilter > 1 ? 0x2601 : 0x2600;
    DEVICE_TEXTURE_INT(mTexMagFilter, 0x2800, mag); info->magFilter = mag;
    UINT min = stage->minFilter == 3 ? 2 : stage->minFilter;
    GLenum minEnum = 0;
    if (!stage->mipFilter || !lastLevel)
        minEnum = min > 1 ? 0x2601 : 0x2600;
    else if (stage->mipFilter == 1)
        minEnum = min > 1 ? 0x2702 : 0x2700;
    else if (stage->mipFilter == 2)
        minEnum = min > 1 ? 0x2703 : 0x2701;
    if (minEnum) {
        DEVICE_TEXTURE_INT(mTexMinFilter, 0x2801, minEnum);
        info->minFilter = minEnum;
    }
    if (gl->supportsLodBias && unit->mLodBias != stage->lodBias) {
        COpenGL_SetActiveTexUnit((const COpenGL *)gl, stageIndex);
        glTexEnvf(0x8500, 0x8501, stage->lodBias);
        unit->mLodBias = stage->lodBias;
    }
    DEVICE_TEXTURE_INT(mTexLastLevel, 0x813d, lastLevel);
    info->lastLevel = lastLevel;
#undef DEVICE_TEXTURE_INT
    if (gl->supportsAnisotropy) {
        if (unit->mTexAnisotropicFilter[index] != stage->anisotropy) {
            COpenGL_SetActiveTexUnit((const COpenGL *)gl, stageIndex);
            glTexParameterf(target, 0x84fe, stage->anisotropy);
            unit->mTexAnisotropicFilter[index] = stage->anisotropy;
        }
        info->anisotropy = stage->anisotropy;
    }
}
#endif

static unsigned int CDirect3DDevice_GetImageGLId(GfxImage *image)
{
    IDirect3DBaseTexture9 *texture;

    if (!image)
        return 0;

    texture = image->texture.basemap;
    CDirect3DDevice_UpdateTextureIfNeeded(texture);
    return CDirect3DDevice_GetTextureGLId(texture);
}

ULONG CDirect3DDevice_AddRef(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    return ++dev->refCount;
}

HRESULT CDirect3DDevice_QueryInterface(const CDirect3DDevice *_this, const IID *iid, LPVOID *ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;
    *ppvObj = (void *)_this;
    CDirect3DDevice_AddRef(_this);
    return 0;
}

ULONG CDirect3DDevice_Release(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    ULONG rc = --dev->refCount;
    if (!rc) {

        void (*dtor)(void *) = (void (*)(void *))dev->vtable[120];
        dtor((void *)_this);
    }
    return rc;
}

HRESULT CDirect3DDevice_CreateTexture(const CDirect3DDevice *_this, UINT Width, UINT Height,
                                      UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                      IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Levels, &Usage, &Format, &Pool, &ppTexture, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *tex;
    (void)_this;
    (void)Pool;
    (void)pSharedHandle;
    tex = malloc(((0x68) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DTexture_CDirect3DTexture((const CDirect3DTexture *)tex, Width, Height, Levels, Usage, Format);
    *ppTexture = (IDirect3DTexture9 *)tex;
    return 0;
}

HRESULT CDirect3DDevice_CreateVolumeTexture(const CDirect3DDevice *_this, UINT Width, UINT Height,
                                            UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                            IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Depth, &Levels, &Usage, &Format, &Pool, &ppVolumeTexture, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateVolumeTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *tex;
    (void)_this;
    (void)Pool;
    (void)pSharedHandle;
    tex = malloc(((0x6c) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DVolumeTexture_CDirect3DVolumeTexture((const CDirect3DVolumeTexture *)tex, Width, Height, Depth, Levels, Usage, Format);
    *ppVolumeTexture = (IDirect3DVolumeTexture9 *)tex;
    return 0;
}

HRESULT CDirect3DDevice_CreateCubeTexture(const CDirect3DDevice *_this, UINT EdgeLength,
                                          UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                          IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &EdgeLength, &Levels, &Usage, &Format, &Pool, &ppCubeTexture, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateCubeTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *tex;
    (void)_this;
    (void)Pool;
    (void)pSharedHandle;
    tex = malloc(((0x7c) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DCubeTexture_CDirect3DCubeTexture((const CDirect3DCubeTexture *)tex, EdgeLength, Levels, Usage, Format);
    *ppCubeTexture = (IDirect3DCubeTexture9 *)tex;
    return 0;
}

HRESULT CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels,
                            DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                            IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &Width, &Height, &Depth, &Levels, &Usage, &Format, &Pool, &ppVolumeTexture, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CreateVolumeTexture", NULL, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *tex;
    (void)Pool;
    (void)pSharedHandle;
    tex = malloc(((0x6c) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DVolumeTexture_CDirect3DVolumeTexture((const CDirect3DVolumeTexture *)tex, Width, Height, Depth, Levels, Usage, Format);
    *ppVolumeTexture = (IDirect3DVolumeTexture9 *)tex;
    return 0;
}

HRESULT CDirect3DDevice_CreateVertexBuffer(const CDirect3DDevice *_this, UINT Length,
                                           DWORD Usage, DWORD FVF, D3DPOOL Pool,
                                           IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Length, &Usage, &FVF, &Pool, &ppVertexBuffer, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateVertexBuffer", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *vb;
    (void)_this;
    (void)FVF;
    (void)pSharedHandle;
#if defined(COD2_X64)
    vb = malloc(sizeof(CDirect3DVertexBufferClean));
#else
    vb = malloc(((0x3c) + (sizeof(void*)>4 ? 0x100 : 0)));
#endif
    CDirect3DVertexBuffer_CDirect3DVertexBuffer((const CDirect3DVertexBuffer *)vb, Length, Usage, Pool);
    *ppVertexBuffer = (IDirect3DVertexBuffer9 *)vb;
    return 0;
}

HRESULT CDirect3DDevice_CreateIndexBuffer(const CDirect3DDevice *_this, UINT Length,
                                          DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                          IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Length, &Usage, &Format, &Pool, &ppIndexBuffer, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateIndexBuffer", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *ib;
    (void)_this;
    (void)pSharedHandle;
#if defined(COD2_X64)
    ib = malloc(sizeof(CDirect3DIndexBufferClean));
#else
    ib = malloc(((0x34) + (sizeof(void*)>4 ? 0x100 : 0)));
#endif
    CDirect3DIndexBuffer_CDirect3DIndexBuffer((const CDirect3DIndexBuffer *)ib, Length, Format, Usage, Pool);
    *ppIndexBuffer = (IDirect3DIndexBuffer9 *)ib;
    return 0;
}

HRESULT CDirect3DDevice_CreateVertexDeclaration(const CDirect3DDevice *_this,
                                                const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pVertexElements, &ppDecl };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateVertexDeclaration", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *decl;
    (void)_this;
    decl = malloc(((0x10) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DVertexDeclaration_CDirect3DVertexDeclaration((const CDirect3DVertexDeclaration *)decl, pVertexElements);
    *ppDecl = (IDirect3DVertexDeclaration9 *)decl;
    return 0;
}

HRESULT CDirect3DDevice_CreateVertexShader(const CDirect3DDevice *_this,
                                           const DWORD *pFunction, IDirect3DVertexShader9 **ppShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pFunction, &ppShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateVertexShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *shader;
    int errorPos;
    (void)_this;
    shader = malloc(((0x19c) + (sizeof(void*)>4 ? 0x100 : 0)));
    memset(shader, 0, 0x19c);
    CDirect3DVertexShader_CDirect3DVertexShader((const CDirect3DVertexShader *)shader, (const char *)pFunction);
    *ppShader = (IDirect3DVertexShader9 *)shader;
#if COD2_APPLE_SDK
    MacTrace_Label(shader, (const char *)pFunction);
#endif
    glGetIntegerv(0x864b, &errorPos);
    if (errorPos != -1) {
        return 0x8876086c;
    }
    return 0;
}

HRESULT CDirect3DDevice_CreatePixelShader(const CDirect3DDevice *_this,
                                          const DWORD *pFunction, IDirect3DPixelShader9 **ppShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pFunction, &ppShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreatePixelShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    GLint errorPosition = -1;
    HRESULT result = CDirect3DDevice_CreatePixelShaderOpenGL(_this, 2,
        (const unsigned long *)pFunction, ppShader);
    if (result)
        return result;
    glGetIntegerv(0x864b, &errorPosition);
    if (errorPosition != -1) {
        ((ULONG (*)(const void *))(*(void ***)(*ppShader))[2])(*ppShader);
        *ppShader = NULL;
        return (HRESULT)(int32_t)0x8876086cu;
    }
    return 0;
#else
    (void)_this;

    if (pFunction) {
        const char *src = (const char *)pFunction;
        if (src[0] == '!' && src[1] == '!') {

            void *program = calloc(1, 0x80);   /* 0x20 was x86 size; over-allocate for x64 COpenGLARBFragmentProgram */
            const char *codePtr = src;
            COpenGLARBFragmentProgram_COpenGLARBFragmentProgram(
                (const COpenGLARBFragmentProgram *)program, NULL, (const string *)&codePtr);
            *ppShader = (IDirect3DPixelShader9 *)program;
#if COD2_APPLE_SDK
            MacTrace_Label(program, src);
#endif
            return 0;
        }
    }

    {
        PixelShaderStub *ps;
        ps = (PixelShaderStub *)calloc(1, sizeof(PixelShaderStub));
        ps->vtable = ps_stub_vtbl;
        ps->refCount = 1;
        *ppShader = (IDirect3DPixelShader9 *)ps;
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_CreatePixelShaderOpenGL(const CDirect3DDevice *_this,
                                                OpenGLPixelShaderType ShaderType, const long unsigned int *pSrcData,
                                                IDirect3DPixelShader9 **ppShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ShaderType, &pSrcData, &ppShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreatePixelShaderOpenGL", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    const char *source = (const char *)pSrcData;
    const char *separator;
    const char *code;
    size_t nameLength;
    char *name;
    IDirect3DPixelShader9 *program;
    GLint errorPosition = -1;
    (void)_this;
    if (!ppShader)
        return (HRESULT)(int32_t)0x8876086cu;
    if ((unsigned int)ShaderType <= ARBFragmentProgram)
        *ppShader = NULL;
    if (!source || !*source)
        return (HRESULT)(int32_t)0x8876086cu;
    switch (ShaderType) {
    case ATITextFragmentProgram:
    case NVidiaRegisterCombinersProgram:
        /* The selected Apple capability policy uses ARB. The ATI/NV methods
         * do not provide supported native shader activation. */
        return (HRESULT)(int32_t)0x8876086au;
    case ARBFragmentProgram:
        break;
    default:
        /* Mac 0x1b179/0x1b19f leaves the output alone for an unknown type,
         * then returns only the current program-error-position status. */
        glGetIntegerv(0x864b, &errorPosition);
        return errorPosition == -1 ? 0 : (HRESULT)(int32_t)0x8876086cu;
    }
    /* Native shader-cache entries are bare ARB text, without Mac's name
     * prefix. Preserve the entire cached program, including any :: comments. */
    separator = source[0] == '!' && source[1] == '!' ? NULL : strstr(source, "::");
    if (separator) {
        nameLength = (size_t)(separator - source);
        code = separator + 2;
    } else if (source[0] == '!' && source[1] == '!') {
        nameLength = 0;
        code = source;
    } else {
        /* Mac's 32-bit string::npos + 2 wraps to one at 0x1b12a. A
         * nonempty string without a separator therefore loses its first
         * code byte and uses the whole source as its name. */
        nameLength = strlen(source);
        code = source + 1;
    }
    name = (char *)malloc(nameLength + 1);
    if (!name)
        return (HRESULT)(int32_t)0x8007000eu;
    memcpy(name, source, nameLength);
    name[nameLength] = 0;
    program = CDirect3DPixelShader_CreateARBNative(name, code);
    free(name); /* Provider uploads synchronously and retains neither argument. */
    if (!program)
        return (HRESULT)(int32_t)0x8007000eu;
    glGetIntegerv(0x864b, &errorPosition);
    if (errorPosition != -1) {
        /* Material_LoadPassShader discards its header on failure without
         * releasing a published shader. Keep ownership until success. */
        ((ULONG (*)(const void *))(*(void ***)program)[2])(program);
        return (HRESULT)(int32_t)0x8876086cu;
    }
    *ppShader = program;
#if COD2_APPLE_SDK
    MacTrace_Label(program, source);
#endif
    return 0;
#else
    (void)_this;
    (void)ShaderType;

    if (pSrcData) {
        const char *src = (const char *)pSrcData;
        if (src[0] == '!' && src[1] == '!') {
            void *program = calloc(1, 0x80);   /* 0x20 was x86 size; over-allocate for x64 COpenGLARBFragmentProgram */
            const char *codePtr = src;
            COpenGLARBFragmentProgram_COpenGLARBFragmentProgram(
                (const COpenGLARBFragmentProgram *)program, NULL, (const string *)&codePtr);
            *ppShader = (IDirect3DPixelShader9 *)program;
#if COD2_APPLE_SDK
            MacTrace_Label(program, src);
#endif
            return 0;
        }
    }

    {
        PixelShaderStub *ps;
        ps = (PixelShaderStub *)calloc(1, sizeof(PixelShaderStub));
        ps->vtable = ps_stub_vtbl;
        ps->refCount = 1;
        *ppShader = (IDirect3DPixelShader9 *)ps;
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_CreateDepthStencilSurface(const CDirect3DDevice *_this,
                                                  UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
                                                  DWORD MultisampleQuality, BOOL Discard,
                                                  IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Format, &MultiSample, &MultisampleQuality, &Discard, &ppSurface, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateDepthStencilSurface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *surfMem = NULL;
    void *surf;
    int currentTexture = 0;
    (void)_this;
    (void)MultiSample;
    (void)MultisampleQuality;
    (void)Discard;
    (void)pSharedHandle;

    if (!g_NoTextureID) {
        UINT allocW = 1, allocH = 1;
        UINT imageSize;
#if defined(COD2_X64)
        unsigned int remaining;
        for (remaining = 32; allocH < Height && remaining; --remaining)
            allocH <<= 1;
        for (remaining = 32; allocW < Width && remaining; --remaining)
            allocW <<= 1;
#else
        while (allocH < Height)
            allocH <<= 1;
        while (allocW < Width)
            allocW <<= 1;
#endif
        imageSize = MacOpenGLUtils_GetImageSizeInBytes(allocW, allocH, 1, 1, &Format);
        surfMem = malloc(imageSize);
    }

    glGetIntegerv(0x8069, &currentTexture);
    surf = malloc(((0x3c) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DSurface_CDirect3DSurface((const CDirect3DSurface *)surf, 0, 0, 0, Width, Height, Format, surfMem, 0);
    *ppSurface = (IDirect3DSurface9 *)surf;
    glBindTexture(0xde1, currentTexture);
    return 0;
}

HRESULT CDirect3DDevice_CreateRenderTarget(const CDirect3DDevice *_this,
                                           UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
                                           DWORD MultisampleQuality, BOOL Lockable,
                                           IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Format, &MultiSample, &MultisampleQuality, &Lockable, &ppSurface, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateRenderTarget", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    void *surfMem = NULL;
    void *surf;
    int currentTexture = 0;
    (void)_this;
    (void)MultiSample;
    (void)MultisampleQuality;
    (void)Lockable;
    (void)pSharedHandle;

    if (!g_NoTextureID) {
        UINT allocW = 1, allocH = 1;
        UINT imageSize;
#if defined(COD2_X64)
        unsigned int remaining;
        for (remaining = 32; allocH < Height && remaining; --remaining)
            allocH <<= 1;
        for (remaining = 32; allocW < Width && remaining; --remaining)
            allocW <<= 1;
#else
        while (allocH < Height)
            allocH <<= 1;
        while (allocW < Width)
            allocW <<= 1;
#endif
        imageSize = MacOpenGLUtils_GetImageSizeInBytes(allocW, allocH, 1, 1, &Format);
        surfMem = malloc(imageSize);
    }

    glGetIntegerv(0x8069, &currentTexture);
    surf = malloc(((0x3c) + (sizeof(void*)>4 ? 0x100 : 0)));
    CDirect3DSurface_CDirect3DSurface((const CDirect3DSurface *)surf, 0, 0, 0, Width, Height, Format, surfMem, 0);
    *ppSurface = (IDirect3DSurface9 *)surf;
    glBindTexture(0xde1, currentTexture);
    return 0;
}

HRESULT CDirect3DDevice_CreateQuery(const CDirect3DDevice *_this, D3DQUERYTYPE Type, bool (*ppQuery)())
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Type, &ppQuery };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateQuery", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    QueryStub *q;
    (void)_this;
    (void)Type;
    if (ppQuery) {
        q = (QueryStub *)calloc(1, sizeof(QueryStub));
        q->vtable = query_stub_vtbl;
        q->refCount = 1;
        *(void **)ppQuery = (void *)q;
    }
    return 0;
}

HRESULT CDirect3DDevice_CreateOffscreenPlainSurface(const CDirect3DDevice *_this,
                                                    UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                                    IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Format, &Pool, &ppSurface, &pSharedHandle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateOffscreenPlainSurface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSurface *surface;
    void *surfaceMemory;
    int currentTexture;
    UINT allocWidth;
    UINT allocHeight;
    UINT imageSize;

    (void)_this;
    (void)Pool;

    surfaceMemory = NULL;
    currentTexture = 0;

    glGetIntegerv(0x8069, &currentTexture);

    if (!g_NoTextureID) {
        allocWidth = 1;
        while (allocWidth < Width) {
            allocWidth <<= 1;
        }
        allocHeight = 1;
        while (allocHeight < Height) {
            allocHeight <<= 1;
        }
        imageSize = MacOpenGLUtils_GetImageSizeInBytes(allocWidth, allocHeight, 1, 1, &Format);
        surfaceMemory = malloc(imageSize);
    }

    surface = (CDirect3DSurface *)malloc(0x80);   /* 0x3c was x86 sizeof(CDirect3DSurfaceImpl); x64 is 0x50 */
    CDirect3DSurface_CDirect3DSurface(surface, 0, 0, 0, Width, Height, Format, surfaceMemory, 0);

    if (ppSurface != NULL) {
        *ppSurface = (IDirect3DSurface9 *)surface;
    }
    if (pSharedHandle != NULL) {
        *pSharedHandle = 0;
    }

    glBindTexture(0xde1, currentTexture);
    return 0;
}

HRESULT CDirect3DDevice_TestCooperativeLevel(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_TestCooperativeLevel", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

UINT CDirect3DDevice_GetAvailableTextureMem(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    UINT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetAvailableTextureMem", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    return dev->textureMemory;
}

HRESULT CDirect3DDevice_EvictManagedResources(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_EvictManagedResources", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

HRESULT CDirect3DDevice_GetDirect3D(const CDirect3DDevice *_this, IDirect3D9 **ppD3D8)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppD3D8 };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetDirect3D", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    *ppD3D8 = (IDirect3D9 *)CDirect3D_GetDirect3DInterface();
    return 0;
}

HRESULT CDirect3DDevice_GetDeviceCaps(const CDirect3DDevice *_this, D3DCAPS9 *pCaps)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pCaps };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetDeviceCaps", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    int maxTexUnits, maxTexImageUnits, maxStages;
    (void)dev;

    memset(pCaps, 0, sizeof(D3DCAPS9));
    pCaps->DeviceType = 1;
    pCaps->AdapterOrdinal = dev->cardType;
    pCaps->Caps = 0;
    pCaps->Caps2 = 0x30020000;
    pCaps->Caps3 = 0x120;
    pCaps->PresentationIntervals = 0x80000001;
    pCaps->CursorCaps = 1;
    pCaps->DevCaps = 0x19bfc0;
    pCaps->PrimitiveMiscCaps = 0xcaf2;

    if (MacDisplay_GetSupportsSeparateBlendFunc()) {
        pCaps->PrimitiveMiscCaps |= 0x20000;
    }

    pCaps->RasterCaps = 0;
    if (MacDisplay_GetSupportsAnisotropicFiltering()) {
        pCaps->RasterCaps |= 0x20000;
    }
    pCaps->RasterCaps |= 0x5602081;
    pCaps->RasterCaps &= 0xfffeffff;
    pCaps->RasterCaps |= 0x2000000;

    pCaps->ZCmpCaps = 0xff;
    pCaps->SrcBlendCaps = 0x7ff;
    pCaps->DestBlendCaps = 0x7ff;
    pCaps->AlphaCmpCaps = 0xff;
    pCaps->ShadeCaps = 0x84208;
    pCaps->TextureCaps = 0x234d07;
    pCaps->TextureFilterCaps = 0x3030300;
    pCaps->CubeTextureFilterCaps = 0x3030300;

    if (MacDisplay_GetSupportsAnisotropicFiltering()) {
        pCaps->TextureFilterCaps |= 0x4000400;
        pCaps->CubeTextureFilterCaps |= 0x4000400;
    }

    pCaps->VolumeTextureFilterCaps = 0;
    pCaps->TextureAddressCaps = 0xd;
    pCaps->VolumeTextureAddressCaps = 0;
    pCaps->LineCaps = 0;
    pCaps->MaxTextureWidth = dev->maxTextureSize;
    pCaps->MaxTextureHeight = dev->maxTextureSize;
    pCaps->MaxVolumeExtent = 0;
    pCaps->MaxTextureRepeat = 0;
    pCaps->MaxTextureAspectRatio = 0x800;

    if (MacDisplay_GetSupportsAnisotropicFiltering()) {
        pCaps->MaxAnisotropy = (DWORD)MacDisplay_GetMaxSupportedAnisotropy();
    }

    pCaps->MaxVertexW = 0.0f;
    pCaps->GuardBandLeft = 0.0f;
    pCaps->GuardBandTop = 0.0f;
    pCaps->GuardBandRight = 0.0f;
    pCaps->GuardBandBottom = 0.0f;
    pCaps->ExtentsAdjust = 0.0f;
    pCaps->StencilCaps = 0xff;
    pCaps->FVFCaps = 0;
    pCaps->TextureOpCaps = 0x3feffff;
    pCaps->VertexProcessingCaps = 0x3b;
    pCaps->MaxActiveLights = 8;
    pCaps->MaxUserClipPlanes = 0;
    pCaps->MaxVertexBlendMatrices = 0;
    pCaps->MaxVertexBlendMatrixIndex = 0;
    pCaps->MaxPointSize = 1.0f;
    pCaps->MaxPrimitiveCount = 0xffff;
    pCaps->MaxVertexIndex = 0xffff;
    pCaps->MaxStreams = 0x10;
    pCaps->MaxStreamStride = 0x400;
    pCaps->VertexShaderVersion = 0xfffe0200;

    pCaps->PixelShaderVersion = MacOpenGLUtils_GetPCPixelShaderVersion();
    pCaps->MaxVertexShaderConst = 0x100;
    pCaps->PixelShader1xMaxValue = 8.0f;

    maxTexImageUnits = MacDisplay_GetMaxTextureImageUnits();
    maxTexUnits = MacDisplay_GetMaxTextureUnits();
    maxStages = (maxTexUnits < maxTexImageUnits) ? maxTexImageUnits : maxTexUnits;
    pCaps->MaxSimultaneousTextures = maxStages;
    pCaps->MaxTextureBlendStages = maxStages;

    pCaps->DevCaps2 = 1;
    pCaps->MaxNpatchTessellationLevel = 0.0f;
    pCaps->DeclTypes = 0;
    pCaps->NumSimultaneousRTs = 1;
    pCaps->StretchRectFilterCaps = 0x200;
    pCaps->VertexTextureFilterCaps = 0;
    pCaps->MaxVShaderInstructionsExecuted = 0;
    pCaps->MaxPShaderInstructionsExecuted = 0;
    pCaps->MaxVertexShader30InstructionSlots = 0;
    return 0;
}

HRESULT CDirect3DDevice_GetDisplayMode(const CDirect3DDevice *_this, UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetDisplayMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)iSwapChain;
    (void)pMode;
    return 0;
}

HRESULT CDirect3DDevice_GetCreationParameters(const CDirect3DDevice *_this, D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pParameters };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetCreationParameters", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pParameters;
    return 0;
}

HRESULT CDirect3DDevice_GetBackBuffer(const CDirect3DDevice *_this, UINT iSwapChain, UINT BackBuffer,
                                      D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &BackBuffer, &Type, &ppBackBuffer };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetBackBuffer", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    (void)iSwapChain;
    (void)BackBuffer;
    (void)Type;
    *ppBackBuffer = dev->backBuffer;
    if (dev->backBuffer) {
        void **vtbl = *(void ***)dev->backBuffer;
        ((ULONG (*)(const void *))vtbl[1])((const void *)dev->backBuffer);
    }
    return 0;
}

#include "lp64_draw_trace.h"

HRESULT CDirect3DDevice_BeginScene(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_BeginScene", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
#if COD2_APPLE_SDK
    MacTrace_BeginFrame();
#endif
    return 0;
}

HRESULT CDirect3DDevice_EndScene(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_EndScene", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

HRESULT CDirect3DDevice_Clear(const CDirect3DDevice *_this, DWORD Count, const D3DRECT *pRects,
                              DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Count, &pRects, &Flags, &Color, &Z, &Stencil };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_Clear", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)Count; (void)pRects;
    return DeviceTargets_Clear((DeviceImpl *)_this, Flags, Color, Z, Stencil);
#else

    DeviceImpl *dev = (DeviceImpl *)_this;
    GLbitfield glFlags = 0;
#if defined(COD2_X64)
    const DWORD requestedFlags = Flags;
    if (dev->depthStencilDetached)
        Flags &= ~6u;
#endif
    (void)Count;
    (void)pRects;

    if (Flags & 1) {
        float r = ((Color >> 16) & 0xFF) / 255.0f;
        float g = ((Color >> 8) & 0xFF) / 255.0f;
        float b = (Color & 0xFF) / 255.0f;
        float a = ((Color >> 24) & 0xFF) / 255.0f;
        glClearColor(r, g, b, a);
        glFlags |= 0x4000;
    }
    if (Flags & 2) {
        glClearDepth((double)Z);
        glFlags |= 0x100;
    }
    if (Flags & 4) {
        glClearStencil(Stencil);
        glFlags |= 0x400;
    }
    if (glFlags) {

#if defined(COD2_X64)
        if (requestedFlags == 1 && Color == 0x00000000) {
#else
        if (Flags == 1 && Color == 0x00000000) {
#endif
            return 0;
        }
        if (Flags & 1) {
            glColorMask(1, 1, 1, 1);
        }
        if (Flags & 2) {
            glDepthMask(1);
        }
        if (Flags & 4) {
            glStencilMask(0xffffffffu);
        }
        glClear(glFlags);
        if (Flags & 1) {
            glColorMask((dev->colorWriteEnable & 1) != 0,
                        (dev->colorWriteEnable & 2) != 0,
                        (dev->colorWriteEnable & 4) != 0,
                        (dev->colorWriteEnable & 8) != 0);
        }
        if (Flags & 2) {
            glDepthMask(dev->zWriteEnable ? 1 : 0);
        }
        if (Flags & 4) {
            glStencilMask(dev->stencilWriteMask);
        }
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_Present(const CDirect3DDevice *_this, const RECT *pSourceRect,
                                const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSourceRect, &pDestRect, &hDestWindowOverride, &pDirtyRegion };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_Present", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    extern SDL_Window *sdl_gl_window;
    (void)_this;
    (void)pSourceRect;
    (void)pDestRect;
    (void)hDestWindowOverride;
    (void)pDirtyRegion;
#if defined(COD2_X64)
    extern GLuint COpenGL_sShowTextureID;
    extern void ViewSingleTexture(GLuint);
    extern void ShowShadowCookies(void);
#if COD2_APPLE_SDK
    if (!MacDisplay_MakeContextCurrent(((DeviceImpl *)_this)->context))
        return (HRESULT)(int32_t)0x88760869u;
#endif
    COpenGL_sShowTextureID = g_showtexid;
    if (g_showtexid) ViewSingleTexture(g_showtexid);
    if (g_ShowShadowCookies) ShowShadowCookies();
#endif
    R_CapturePendingScreenshotBeforePresent();
#if COD2_APPLE_SDK
    MacTrace_EndFrame();
#endif
    if (sdl_gl_window) {
#if COD2_APPLE_SDK && defined(__aarch64__)
        static const dvar_t *presentMode;
        extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
        extern void MacPlatform_SetPresentMode(int mode);
        if (!presentMode)
            presentMode = Dvar_RegisterInt("r_presentMode", 0, 0, 1, 1);
        MacPlatform_SetPresentMode(presentMode->current.integer);
#endif
        SDL_GL_SwapWindowDirect();
    }
#if defined(COD2_X64)
    CMemoryBuffer_Update();
    ++COpenGL_sFrameCount;
#endif
    return 0;
}

HRESULT CDirect3DDevice_DrawPrimitive(const CDirect3DDevice *_this,
                                      D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PrimitiveType, &StartVertex, &PrimitiveCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawPrimitive", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PrimitiveType;
    (void)StartVertex;
    (void)PrimitiveCount;
    return 0;
}

#if defined(COD2_X64)
static void CDirect3DDevice_SubmitIndexedNative(DeviceImpl *, const byte *, UINT32);
#endif
#include "lp64_shader_draw.h"
#include "lp64_device_draw.inc"

COD2_FORCE_ALIGN_ARG_POINTER
HRESULT CDirect3DDevice_DrawIndexedPrimitive(const CDirect3DDevice *_this,
                                             D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex,
                                             UINT NumVertices, UINT startIndex, UINT primCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PrimitiveType, &BaseVertexIndex, &MinVertexIndex, &NumVertices, &startIndex, &primCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawIndexedPrimitive", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return CDirect3DDevice_DrawIndexedNative(_this, PrimitiveType, BaseVertexIndex,
                                           MinVertexIndex, NumVertices, startIndex, primCount);
#else
    DeviceImpl *dev = (DeviceImpl *)_this;
#if defined(COD2_X64)
    CDirect3DDevice_ValidateModelViewMatrix(_this);
    CDirect3DDevice_ValidateLighting(_this);
#endif
    byte *vbData, *ibData, *vertBase;
    UINT stride, offset;
    int indexCount;
    GLenum glMode;
    GLenum stage0Target;
    r_backEndGlobals_t *backEndState;
    int is2D;
    int usesLightmap;
    int positionOffset;
    int positionComponents;
    int colorOffset;
    int colorByteOrder;
    int texOffset;
    int lightmapOffset;
    int normalOffset;
    const D3DVERTEXELEMENT9 *element;

    (void)PrimitiveType;

#if COD2_APPLE_SDK
    MacTrace_Draw(dev, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
#endif
    if (!dev->streams[0] || !dev->indexBuffer)
        return 0;
#if COD2_APPLE_SDK
    if (MacShader_DrawEnabled() && g_activeVertexShader && dev->pixelShader && g_activeVertexDeclaration)
        return MacShader_DrawIndexed(dev, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
#endif
#if defined(COD2_X64)
    /* The fallback writes client arrays directly. Keep those writes in VAO0,
     * so a later packet hit can still rely on its retained GL array state. */
    glBindVertexArrayAPPLE(0);
    *(GLint *)imp_g_VAOID = -1;
#endif

#if defined(COD2_X64)
    vbData = ((const CDirect3DVertexBufferClean *)dev->streams[0])->data;
#else
    vbData = *(byte **)((byte *)dev->streams[0] + sizeof(void *) + 8);   /* VB.data: vtable+refCount+lengthBytes (x86 was +12) */
#endif
    if (!vbData)
        return 0;
    offset = dev->streamOffsets[0];
    stride = dev->streamStrides[0];
    backEndState = (r_backEndGlobals_t *)imp_backEnd;
    is2D = backEndState && backEndState->projection2D;
    usesLightmap = !is2D && stride == 0x44;
    stage0Target = 0x0DE1;

    positionOffset = 0;
    positionComponents = 3;
    colorOffset = stride == 0x44                       ? 0x18
                  : stride == 0x40                     ? 0x1c
                  : stride == 0x24                     ? 0x18
                  : (stride == 0x18 || stride == 0x20) ? 0x0c
                                                       : -1;
    texOffset = stride == 0x44   ? 0x1c
                : stride == 0x40 ? 0x20
                : stride == 0x24 ? 0x1c
                                 : -1;
    lightmapOffset = stride == 0x44 ? 0x24 : -1;
    normalOffset = stride == 0x44   ? 0x0c
                   : stride == 0x40 ? 0x10
                   : stride == 0x24 ? 0x0c
                                    : -1;

    element = CDirect3DDevice_FindVertexElement(0, 0, 0);
    if (!element)
        element = CDirect3DDevice_FindVertexElement(0, 9, 0);
    if (element) {
        positionOffset = element->Offset;
        positionComponents = element->Type == 3 ? 4 : 3;
    }
    element = CDirect3DDevice_FindVertexElement(0, 10, 0);
    if (element)
        colorOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 5, 0);
    if (element)
        texOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 5, 1);
    if (element)
        lightmapOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 3, 0);
    if (element)
        normalOffset = element->Offset;

#if defined(COD2_X64)
    colorByteOrder = MacShader_ColorByteOrder(stride, colorOffset, positionComponents);
#else
    if (stride == 0x44 || stride == 0x20 || stride == 0x18)
        colorByteOrder = COLOR_BYTES_RGBA;
    else if (stride == 0x40 && positionComponents == 3)
        colorByteOrder = COLOR_BYTES_ARGB;
    else
        colorByteOrder = COLOR_BYTES_BGRA;
#endif

#if defined(COD2_X64)
    ibData = ((const CDirect3DIndexBufferClean *)dev->indexBuffer)->data;
#else
    ibData = *(byte **)((byte *)dev->indexBuffer + sizeof(void *) + 8);   /* IB.data (x86 was +12) */
#endif
    if (!ibData)
        return 0;

    vertBase = vbData + offset + BaseVertexIndex * stride;

    {
        extern void glBindVertexArray(unsigned int);
        glBindVertexArray(0);
    }
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);
    if (!usesLightmap) {
        CDirect3DDevice_DisableExtraTextureUnits();
    }

    {

        {
#if defined(COD2_X64)
            extern GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *shader);
            GLuint vsId = (!is2D && g_activeVertexShader)
                              ? CDirect3DVertexShader_GetProgramId((const CDirect3DVertexShader *)g_activeVertexShader)
                              : 0;
#else
            GLuint vsId = (!is2D && g_activeVertexShader)
                              ? *(GLuint *)((char *)g_activeVertexShader + 4)
                              : 0;
#endif
#if defined(COD2_X64)
            int useProg = vsId && dev->pixelShader && CDirect3DDevice_UsePrograms();
#else
            int useProg = vsId && dev->pixelShader && getenv("D3D_PROG");
#endif
            if (useProg) {
                glEnable(0x8620 );
                glBindProgramARB(0x8620, vsId);
                {
                    void **vt = *(void ***)dev->pixelShader;
                    ((void (*)(const void *))vt[7])(dev->pixelShader);
                }
            } else {

                glDisable(0x8804);
                glBindProgramARB(0x8804, 0);
                glDisable(0x8620);
                glBindProgramARB(0x8620, 0);
            }
        }
#if defined(COD2_X64)
        if (!is2D && dev->zEnable && !dev->depthStencilDetached) {
#else
        if (!is2D && dev->zEnable) {
#endif
            glEnable(0x0B71);
            glDepthFunc(CDirect3DDevice_MapCompareFunc(dev->zFunc));
            glDepthMask(dev->zWriteEnable ? 1 : 0);
        } else {
            glDisable(0x0B71);
            glDepthMask(0);
        }
        glDisable(0x0B44);

        glDisable(0x0B60);
        glDisable(0x0B50);
        if (g_alphaTestEnable) {
            glEnable(0x0BC0);
            glAlphaFunc(CDirect3DDevice_MapCompareFunc(dev->alphaFuncVal), dev->alphaRef);
        } else {
            glDisable(0x0BC0);
        }
        glColorMask(
            (dev->colorWriteEnable & 1) != 0,
            (dev->colorWriteEnable & 2) != 0,
            (dev->colorWriteEnable & 4) != 0,
            (dev->colorWriteEnable & 8) != 0);
        if (is2D) {
            glColorMask(1, 1, 1, 1);
        }
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        if (!is2D) {

            const GfxViewParms *viewParms = backEndState ? backEndState->viewParms : NULL;
            if (viewParms) {
                float world[16];
                float view[16];
                float modelView[16];
                float proj[16];
                GfxCodeMatrices *activeMatrices = NULL;
                float aspect = (dev->viewportW && dev->viewportH)
                                   ? ((float)dev->viewportW / (float)dev->viewportH)
                                   : (640.0f / 480.0f);

                (void)aspect;
                if (backEndState && backEndState->codeMatrixStackLevel >= 0 &&
                    backEndState->codeMatrixStackLevel < 3) {
                    activeMatrices = &backEndState->codeMatrixStack[backEndState->codeMatrixStackLevel];
                }
                if (activeMatrices) {
                    memcpy(world, &activeMatrices->world.matrix[0], sizeof(world));
                    memcpy(view, &activeMatrices->view.matrix[0], sizeof(view));
                    memcpy(proj, &activeMatrices->projection.matrix[0], sizeof(proj));
                } else {
                    D3DMATRIX *activeWorld = RB_GetActiveWorldMatrix();
                    CDirect3DDevice_LoadIdentityMatrix(world);
                    if (activeWorld)
                        memcpy(world, activeWorld, sizeof(world));
                    memcpy(view, &viewParms->viewMatrix, sizeof(view));
                    memcpy(proj, &viewParms->projectionMatrix, sizeof(proj));
                }

                view[2] = -view[2];
                view[6] = -view[6];
                view[10] = -view[10];
                view[14] = -view[14];
                D3DXMatrixMultiply(modelView, world, view);

                MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(
                    proj,
                    dev->viewportW ? (float)dev->viewportW : 640.0f,
                    dev->viewportH ? (float)dev->viewportH : 480.0f);

                glMatrixMode(0x1701);
                glLoadMatrixf(proj);
                glMatrixMode(0x1700);
                glLoadMatrixf(modelView);
            } else {
                glMatrixMode(0x1701);
                glLoadIdentity();
                glScalef(1.0f / 4000.0f, 1.0f / 4000.0f, 1.0f / 4000.0f);
                glMatrixMode(0x1700);
                glLoadIdentity();
            }
        } else {

            float orthoW = dev->viewportW ? (float)dev->viewportW : 640.0f;
            float orthoH = dev->viewportH ? (float)dev->viewportH : 480.0f;
            float ortho[16] = {
                2.0f / orthoW, 0, 0, 0,
                0, -2.0f / orthoH, 0, 0,
                0, 0, -1.0f, 0,
                -1.0f, 1.0f, 0, 1.0f
            };
            glMatrixMode(0x1701);
            glLoadMatrixf(ortho);
            glMatrixMode(0x1700);
            glLoadIdentity();
        }
    }

    if (1)
    {
        extern void glBindTexture(unsigned int, unsigned int);
        {
            extern void glBindTexture(unsigned int, unsigned int);
            DWORD colorOp = g_textureStageState[0][D3DTSS_COLOROP];
            unsigned int glTexID;
            GfxImage *materialImage = NULL;

            if (is2D || stride != 0x44) {
                CDirect3DDevice_UpdateTextureIfNeeded(g_boundTextures[0]);
                glTexID = CDirect3DDevice_GetTextureGLId(g_boundTextures[0]);
                stage0Target = CDirect3DDevice_GetTextureTarget(g_boundTextures[0]);
                if (!glTexID) {
                    glTexID = g_prebind_texID;
                    stage0Target = g_prebind_texTarget ? g_prebind_texTarget : 0x0DE1;
                }
            } else {
                glTexID = 0;
            }

            if (!is2D && stride != 0x44) {
                extern void *imp_tess;
                const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                const Material *mmat = tess->material;
                if (mmat) {
                    GfxImage *mimg = CDirect3DDevice_SelectMaterialColorImage(mmat);
                    unsigned int mtex = CDirect3DDevice_GetImageGLId(mimg);
                    if (mtex) {
                        glTexID = mtex;
                        stage0Target = CDirect3DDevice_GetImageTextureTarget(mimg);
                    }
                }
            }
            if (!glTexID && usesLightmap) {
                extern void *imp_tess;
                const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                const Material *mat = tess->material;
                materialImage = CDirect3DDevice_SelectMaterialColorImage(mat);
                glTexID = CDirect3DDevice_GetImageGLId(materialImage);
                stage0Target = CDirect3DDevice_GetImageTextureTarget(materialImage);
            }
            if (glTexID && colorOp != D3DTOP_DISABLE) {
                CDirect3DDevice_BindTextureTarget(stage0Target, glTexID);
#if defined(COD2_X64)
                CDirect3DDevice_ApplySamplerState(dev, 0, stage0Target);
#else
                CDirect3DDevice_ApplySamplerState(0, stage0Target);
#endif
                if (usesLightmap) {

                    CDirect3DDevice_ApplyTextureStageState(0);

                    {
                        extern void *imp_tess;
                        extern GfxWorld s_world;
                        const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                        int lmapIndex = tess->lmapIndex;
                        int lmapAllowed = !dev->alphaBlendEnable || dev->destBlend == 6;
                        CDirect3DDevice_DisableExtraTextureUnits();
                        if (lmapAllowed && stage0Target != GL_TEXTURE_CUBE_MAP && lmapIndex >= 0 && lmapIndex < 31 && s_world.lightmaps) {
                            GfxImage *lmapImg = s_world.lightmaps[lmapIndex][0];
                            if (lmapImg) {
                                unsigned int lmTexID = CDirect3DDevice_GetImageGLId(lmapImg);
                                if (lmTexID) {
                                    glActiveTextureARB(GL_TEXTURE1_ARB);
                                    glDisable(GL_TEXTURE_CUBE_MAP);
                                    glEnable(0x0DE1);
                                    glBindTexture(0x0DE1, lmTexID);

                                    glMatrixMode(0x1702 );
                                    glLoadIdentity();
                                    glMatrixMode(0x1700 );
#if defined(COD2_X64)
                                    CDirect3DDevice_ApplySamplerState(dev, 1, 0x0DE1);
#else
                                    CDirect3DDevice_ApplySamplerState(1, 0x0DE1);
#endif
                                    if (g_textureStageState[1][D3DTSS_COLOROP] != D3DTOP_DISABLE) {
                                        CDirect3DDevice_ApplyTextureStageState(1);
                                    } else {

                                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, 0x2100);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, CDirect3DDevice_LightmapScale());
                                        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, 0x1702);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, 0x0300);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, 0x0300);
                                    }

                                    glTexParameteri(0x0DE1, 0x2801 , 0x2601 );
                                    glTexParameteri(0x0DE1, 0x2800 , 0x2601 );

                                    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, 0x1E01 );
                                    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                                    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, 0x0302 );
                                    glTexEnvi(GL_TEXTURE_ENV, 0x0D1C , 1);

                                    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE,
                                              dev->alphaBlendEnable ? 1 : CDirect3DDevice_LightmapScale());
                                    glActiveTextureARB(GL_TEXTURE0_ARB);
                                }
                            }
                        }
                    }
                } else {

                    CDirect3DDevice_ApplyTextureStageState(0);
                }
            } else {
                glDisable(0x0DE1);
                glDisable(GL_TEXTURE_CUBE_MAP);
            }
        }
    }

    if (dev->alphaBlendEnable) {
        glEnable(0x0BE2);
        glBlendFunc(CDirect3DDevice_MapBlendFunc(dev->srcBlend),
                    CDirect3DDevice_MapBlendFunc(dev->destBlend));
    } else {
        glDisable(0x0BE2);
    }

    glEnableClientState(0x8074);
    glVertexPointer(positionComponents, 0x1406 , stride,
                    vertBase + positionOffset);

    {
        if (usesLightmap) {
            const byte *colors = CDirect3DDevice_ConvertColorArray(
                vertBase, stride, colorOffset, MinVertexIndex + NumVertices,
#if defined(COD2_X64)
                colorByteOrder, (const unsigned short *)(ibData + startIndex * 2), primCount * 3);
#else
                colorByteOrder);
#endif

            glEnableClientState(0x8076);
            if (colors)
                glColorPointer(4, 0x1401 , 0, colors);
            else
                glColorPointer(4, 0x1401 , stride,
                               vertBase + colorOffset);

            if (lightmapOffset >= 0) {
                glClientActiveTextureARB(0x84C1);
                glEnableClientState(0x8078);
                glTexCoordPointer(2, 0x1406 , stride,
                                  vertBase + lightmapOffset);
                glClientActiveTextureARB(0x84C0);
            }
        } else if (colorOffset >= 0) {
            const byte *colors = CDirect3DDevice_ConvertColorArray(
                vertBase, stride, colorOffset, MinVertexIndex + NumVertices,
#if defined(COD2_X64)
                colorByteOrder, (const unsigned short *)(ibData + startIndex * 2), primCount * 3);
#else
                colorByteOrder);
#endif

            glEnableClientState(0x8076);
            if (colors)
                glColorPointer(4, 0x1401 , 0, colors);
            else
                glColorPointer(4, 0x1401 , stride, vertBase + colorOffset);
        }
        if (texOffset >= 0) {
            glEnableClientState(0x8078);
            if (stage0Target == GL_TEXTURE_CUBE_MAP) {
                if (normalOffset >= 0)
                    glTexCoordPointer(3, 0x1406 , stride,
                                      vertBase + normalOffset);
                else
                    glTexCoordPointer(2, 0x1406 , stride, vertBase + texOffset);
            } else {
                glTexCoordPointer(2, 0x1406 , stride, vertBase + texOffset);
            }
        }
    }

    glActiveTextureARB(GL_TEXTURE0_ARB);
    glMatrixMode(0x1702);
    if (g_textureStageState[0][D3DTSS_TEXTURETRANSFORMFLAGS] != D3DTTFF_DISABLE)
        glLoadMatrixf(dev->texMatrix);
    else
        glLoadIdentity();
    glMatrixMode(0x1700);

    {
        extern void glDrawElements(unsigned int, int, unsigned int, const void *);
        indexCount = primCount * 3;

        glDrawElements(0x0004 , indexCount,
                       0x1403 ,
                       ibData + startIndex * 2);
    }

    glDisableClientState(0x8074);
    glDisableClientState(0x8076);
    glDisableClientState(0x8078);
    CDirect3DDevice_DisableExtraTextureUnits();

    return 0;
#endif
}

HRESULT CDirect3DDevice_DrawPrimitiveUP(const CDirect3DDevice *_this,
                                        D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount,
                                        const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PrimitiveType, &PrimitiveCount, &pVertexStreamZeroData, &VertexStreamZeroStride };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawPrimitiveUP", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PrimitiveType;
    (void)PrimitiveCount;
    (void)pVertexStreamZeroData;
    (void)VertexStreamZeroStride;
    return 0;
}

HRESULT CDirect3DDevice_DrawIndexedPrimitiveUP(const CDirect3DDevice *_this,
                                               D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices,
                                               UINT PrimitiveCount, const long unsigned int *pIndexData,
                                               D3DFORMAT IndexDataFormat, const long unsigned int *pVertexStreamZeroData,
                                               UINT VertexStreamZeroStride)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PrimitiveType, &MinVertexIndex, &NumVertexIndices, &PrimitiveCount, &pIndexData, &IndexDataFormat, &pVertexStreamZeroData, &VertexStreamZeroStride };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawIndexedPrimitiveUP", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PrimitiveType;
    (void)MinVertexIndex;
    (void)NumVertexIndices;
    (void)PrimitiveCount;
    (void)pIndexData;
    (void)IndexDataFormat;
    (void)pVertexStreamZeroData;
    (void)VertexStreamZeroStride;
    return 0;
}

#if defined(COD2_X64)
#include "lp64_device_render_state.inc"
#endif

static void CDirect3DDevice_ApplyDepthBias(void)
{
    if (g_depthBias != 0.0f || g_slopeScaleDepthBias != 0.0f) {
        glEnable(0x8037 );

        ((void (*)(float, float))glPolygonOffset)(g_slopeScaleDepthBias, g_depthBias * 16777216.0f);
    } else {
        glDisable(0x8037 );
    }
}

HRESULT COD2_FORCE_ALIGN_ARG_POINTER CDirect3DDevice_SetRenderState(const CDirect3DDevice *_this, D3DRENDERSTATETYPE State, DWORD Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &State, &Value };
    HRESULT COD2_FORCE_ALIGN_ARG_POINTER macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetRenderState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
#if defined(COD2_X64)
    return DeviceRenderState_Set(dev, State, Value);
#else
    switch (State) {
#if defined(COD2_X64)
    case D3DRS_DITHERENABLE:
        COpenGL_SetDither((const COpenGL *)CDirect3DDevice_GL(), !!Value);
        break;
    case D3DRS_SHADEMODE:
        if (dev->shadeMode != Value) {
            dev->shadeMode = Value;
            glShadeModel(Value == 1 ? 0x1d00 : Value == 2 ? 0x1d01 : 0);
        }
        break;
    case D3DRS_LIGHTING:
        if (dev->lighting != Value) {
            dev->lighting = Value;
            if (!CDirect3DDevice_GL()->supportsATIArrayRevComps) {
                if (Value) glEnable(0xb50); else glDisable(0xb50);
            }
        }
        break;
    case D3DRS_COLORVERTEX:
        dev->colorVertex = Value;
        break;
#endif
    case D3DRS_ZENABLE:
        dev->zEnable = Value;
#if defined(COD2_X64)
        if (Value && !dev->depthStencilDetached)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
#endif
        break;
    case D3DRS_ZWRITEENABLE:
        dev->zWriteEnable = Value;
#if defined(COD2_X64)
        glDepthMask(Value && !dev->depthStencilDetached);
#endif
        break;
#if defined(COD2_X64)
    case D3DRS_STENCILENABLE:
        if (dev->stencilEnable != Value) {
            dev->stencilEnable = Value;
            /* Mac SetRenderState applies this even without a depth surface. */
            if (Value)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
        }
        break;
    case D3DRS_STENCILWRITEMASK:
        dev->stencilWriteMask = Value & 0xff;
        glStencilMask(dev->depthStencilDetached ? 0 : dev->stencilWriteMask);
        break;
#endif
    case D3DRS_ZFUNC:
        dev->zFunc = Value;
        break;
    case D3DRS_DEPTHBIAS:
        g_depthBias = *(const float *)&Value;
        CDirect3DDevice_ApplyDepthBias();
        break;
    case D3DRS_SLOPESCALEDEPTHBIAS:
        g_slopeScaleDepthBias = *(const float *)&Value;
        CDirect3DDevice_ApplyDepthBias();
        break;
    case D3DRS_ALPHATESTENABLE:
        g_alphaTestEnable = Value;
        break;
    case D3DRS_ALPHAREF:
        dev->alphaRefVal = Value;
        dev->alphaRef = (float)Value / 255.0f;
        break;
    case D3DRS_ALPHAFUNC:
        dev->alphaFuncVal = Value;
        dev->alphaFunc = CDirect3DDevice_MapCompareFunc(Value);
        break;
    case D3DRS_SRCBLEND:
        dev->srcBlend = Value;
        break;
    case D3DRS_DESTBLEND:
        dev->destBlend = Value;
        break;
    case D3DRS_CULLMODE:
        dev->cullMode = Value;
        break;
    case D3DRS_ALPHABLENDENABLE:
        dev->alphaBlendEnable = (unsigned char)Value;
        break;
    case D3DRS_FOGENABLE:
        dev->fogEnable = Value;
        break;
    case D3DRS_TEXTUREFACTOR:
        g_textureFactor = Value;
        break;
    case 0x22: {
        float fc[4];
        dev->fogColor = Value;
        fc[0] = ((Value >> 16) & 0xFF) / 255.0f;
        fc[1] = ((Value >> 8) & 0xFF) / 255.0f;
        fc[2] = (Value & 0xFF) / 255.0f;
        fc[3] = ((Value >> 24) & 0xFF) / 255.0f;
        glFogfv(0x0B66 , fc);
        break;
    }
    case D3DRS_COLORWRITEENABLE:
        dev->colorWriteEnable = Value;
        break;
    case D3DRS_BLENDOP:
        dev->blendOp = Value;
        break;
    case D3DRS_SEPARATEALPHABLENDENABLE:
        dev->separateAlphaBlendEnable = Value;
        break;
    case D3DRS_SRCBLENDALPHA:
        dev->srcBlendAlpha = Value;
        break;
    case D3DRS_DESTBLENDALPHA:
        dev->destBlendAlpha = Value;
        break;
    case D3DRS_BLENDOPALPHA:
        dev->alphaSrcBlend = Value;
        break;
    default:
        break;
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetRenderState(const CDirect3DDevice *_this, D3DRENDERSTATETYPE State, DWORD *pValue)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &State, &pValue };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetRenderState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    if (!pValue)
        return 0;
    switch (State) {
    case D3DRS_ZENABLE:
        *pValue = dev->zEnable;
        break;
    case D3DRS_ZWRITEENABLE:
        *pValue = dev->zWriteEnable;
        break;
    case D3DRS_ZFUNC:
        *pValue = dev->zFunc;
        break;
#if defined(COD2_X64)
    case D3DRS_STENCILENABLE:
        *pValue = dev->stencilEnable;
        break;
    case D3DRS_STENCILWRITEMASK:
        *pValue = dev->stencilWriteMask;
        break;
#endif
    case D3DRS_ALPHATESTENABLE:
        *pValue = g_alphaTestEnable;
        break;
    case D3DRS_ALPHAREF:
        *pValue = dev->alphaRefVal;
        break;
    case D3DRS_ALPHAFUNC:
        *pValue = dev->alphaFuncVal;
        break;
    case D3DRS_SRCBLEND:
        *pValue = dev->srcBlend;
        break;
    case D3DRS_DESTBLEND:
        *pValue = dev->destBlend;
        break;
    case D3DRS_CULLMODE:
        *pValue = dev->cullMode;
        break;
    case D3DRS_ALPHABLENDENABLE:
#if defined(COD2_X64)
        *pValue = dev->alphaBlendValue;
#else
        *pValue = dev->alphaBlendEnable;
#endif
        break;
    case D3DRS_FOGENABLE:
        *pValue = dev->fogEnable;
        break;
    case D3DRS_TEXTUREFACTOR:
        *pValue = g_textureFactor;
        break;
    case D3DRS_FOGCOLOR:
        *pValue = dev->fogColor;
        break;
    case D3DRS_COLORWRITEENABLE:
        *pValue = dev->colorWriteEnable;
        break;
    case D3DRS_BLENDOP:
        *pValue = dev->blendOp;
        break;
    case D3DRS_SEPARATEALPHABLENDENABLE:
#if defined(COD2_X64)
        /* Mac 0x13c53 falls through to the CULLMODE store at 0x13bcf. */
        *pValue = dev->cullMode;
#else
        *pValue = dev->separateAlphaBlendEnable;
#endif
        break;
    case D3DRS_SRCBLENDALPHA:
        *pValue = dev->srcBlendAlpha;
        break;
    case D3DRS_DESTBLENDALPHA:
        *pValue = dev->destBlendAlpha;
        break;
    case D3DRS_BLENDOPALPHA:
        *pValue = dev->alphaSrcBlend;
        break;
    default:
        *pValue = 0;
        break;
    }
    return 0;
}

HRESULT CDirect3DDevice_SetTexture(const CDirect3DDevice *_this, DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Stage, &pTexture };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    UINT limit = dev->maxTextureUnits > dev->maxTextureImageUnits ? dev->maxTextureUnits : dev->maxTextureImageUnits;
    if (Stage < limit && Stage < sizeof(g_boundTextures) / sizeof(g_boundTextures[0]) && g_boundTextures[Stage] != pTexture) {
        IDirect3DBaseTexture9 *previous = g_boundTextures[Stage];
        if (CDirect3DDevice_IsRealTexture(previous))
            ((ULONG (*)(const void *))(*(void ***)previous)[2])(previous);
        g_boundTextures[Stage] = pTexture;
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, Stage);
        if (stage) stage->texture = pTexture;
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
        if (CDirect3DDevice_IsRealTexture(pTexture))
            ((ULONG (*)(const void *))(*(void ***)pTexture)[1])(pTexture);
        if (Stage == 0 && !pTexture) {
            g_prebind_texID = 0;
            g_prebind_texTarget = 0x0DE1;
        }
    }
    return 0;
#else
    (void)_this;
#if COD2_APPLE_SDK
    if (Stage < 16) {
#else
    if (Stage < 8) {
#endif
        g_boundTextures[Stage] = pTexture;
        if (Stage == 0 && !pTexture) {
            g_prebind_texID = 0;
            g_prebind_texTarget = 0x0DE1;
        }
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetTexture(const CDirect3DDevice *_this, DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Stage, &ppTexture };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    IDirect3DBaseTexture9 *texture = NULL;
    if (Stage < dev->maxTextureUnits && Stage < sizeof(g_boundTextures) / sizeof(g_boundTextures[0]))
        texture = g_boundTextures[Stage];
    if (CDirect3DDevice_IsRealTexture(texture))
        ((ULONG (*)(const void *))(*(void ***)texture)[1])(texture);
    if (ppTexture)
        *ppTexture = texture;
    return 0;
#else
    (void)_this;
    if (ppTexture)
#if COD2_APPLE_SDK
        *ppTexture = (Stage < 16) ? g_boundTextures[Stage] : NULL;
#else
        *ppTexture = (Stage < 8) ? g_boundTextures[Stage] : NULL;
#endif
    return 0;
#endif
}

HRESULT CDirect3DDevice_SetTextureStageState(const CDirect3DDevice *_this, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Stage, &Type, &Value };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetTextureStageState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    CTexStageNative *stage = CDirect3DDevice_Stage(dev, Stage);
    if (Stage >= dev->maxTextureUnits || !stage)
        return 0;
    DWORD *field = CDirect3DDevice_StageField(stage, Type);
    if (field && *field != Value) {
        *field = Value;
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
        if (Type == 11 || Type == 24)
            CDirect3DDevice_mNeedsTransformationValidation = 1;
        if (Stage < 8)
            g_textureStageState[Stage][Type] = Value;
    }
    return 0;
#else

    (void)_this;
    if (Stage < 8 && (unsigned int)Type < 33)
        g_textureStageState[Stage][Type] = Value;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetTextureStageState(const CDirect3DDevice *_this, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Stage, &Type, &pValue };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetTextureStageState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    CTexStageNative *stage = CDirect3DDevice_Stage(dev, Stage);
    /* The original getter accepts the sentinel stage at the fixed-unit limit. */
    if (Stage <= dev->maxTextureUnits && stage && (UINT)Type <= 24) {
        DWORD *field = CDirect3DDevice_StageField(stage, Type);
        if (field && pValue)
            *pValue = *field;
    }
    return 0;
#else

    (void)_this;
    if (pValue)
        *pValue = (Stage < 8 && (unsigned int)Type < 33) ? g_textureStageState[Stage][Type] : 0;
    return 0;
#endif
}

HRESULT CDirect3DDevice_SetSamplerState(const CDirect3DDevice *_this, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Sampler, &Type, &Value };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetSamplerState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    UINT limit = dev->maxTextureUnits > dev->maxTextureImageUnits ? dev->maxTextureUnits : dev->maxTextureImageUnits;
    CTexStageNative *stage = CDirect3DDevice_Stage(dev, Sampler);
    if (Sampler >= limit || !stage)
        return 0;
    DWORD previous = 0;
    DWORD *field = CDirect3DDevice_SamplerField(stage, Type);
    if (field)
        previous = *field;
    else if (Type == 8)
        previous = CDirect3DDevice_TruncateSampler(stage->lodBias * 4.0f);
    else if (Type == 10)
        previous = CDirect3DDevice_TruncateSampler(stage->anisotropy);
    else
        return 0;
    if (previous == Value)
        return 0;
    CDirect3DDevice_mNeedsRasterizationValidation = 1;
    if (field)
        *field = Value;
    else if (Type == 8) {
        float value;
        memcpy(&value, &Value, sizeof(value));
        stage->lodBias = 0.25f * value;
    } else {
        if (MacDisplay_GetSupportsAnisotropicFiltering())
            memcpy(&stage->anisotropy, &Value, sizeof(stage->anisotropy));
        else
            stage->anisotropy = 1.0f;
    }
    if (Sampler < 16)
        g_samplerState[Sampler][Type] = Value;
    return 0;
#else

    (void)_this;
    if (Sampler < 16 && (unsigned int)Type < 14)
        g_samplerState[Sampler][Type] = Value;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetSamplerState(const CDirect3DDevice *_this, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Sampler, &Type, &pValue };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetSamplerState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    UINT limit = dev->maxTextureUnits > dev->maxTextureImageUnits ? dev->maxTextureUnits : dev->maxTextureImageUnits;
    CTexStageNative *stage = CDirect3DDevice_Stage(dev, Sampler);
    if (Sampler < limit && stage && pValue) {
        DWORD *field = CDirect3DDevice_SamplerField(stage, Type);
        if (field)
            *pValue = *field;
        else if (Type == 8)
            *pValue = CDirect3DDevice_TruncateSampler(4.0f * stage->lodBias);
        else if (Type == 10)
            *pValue = CDirect3DDevice_TruncateSampler(stage->anisotropy);
    }
    return 0;
#else

    (void)_this;
    if (pValue)
        *pValue = (Sampler < 16 && (unsigned int)Type < 14) ? g_samplerState[Sampler][Type] : 0;
    return 0;
#endif
}

HRESULT CDirect3DDevice_SetTransform(const CDirect3DDevice *_this, D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &State, &pMatrix };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetTransform", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    const float *matrix = (const float *)pMatrix;
    if (!matrix)
        return 0;
    if (State == 0x100 || State == 2) {
        float *raw = State == 0x100 ? dev->worldMatrix : dev->viewMatrix;
        if (memcmp(raw, matrix, 64)) {
            memcpy(raw, matrix, 64);
            if (State == 2) {
                memcpy(dev->convertedView, matrix, 64);
                for (UINT i = 2; i < 16; i += 4) {
                    uint32_t bits;
                    memcpy(&bits, &dev->convertedView[i], 4);
                    bits ^= 0x80000000u;
                    memcpy(&dev->convertedView[i], &bits, 4);
                }
            }
            dev->isWorldViewDirty = dev->isProjDirty = 1;
        }
    } else if (State == 3) {
        if (memcmp(dev->projMatrix, matrix, 64)) {
            float converted[16];
            glMatrixMode(0x1701);
            memcpy(dev->projMatrix, matrix, 64);
            memcpy(converted, matrix, 64);
            MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(converted,
                (float)dev->viewportW, (float)dev->viewportH);
            glLoadMatrixf(converted);
        }
    } else if ((UINT)State >= 16 && (UINT)State - 16 < dev->maxTextureUnits) {
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, (UINT)State - 16);
        if (stage) {
            memcpy(stage->matrix, matrix, 64);
            if (State == 16)
                memcpy(dev->texMatrix, matrix, 64);
            CDirect3DDevice_mNeedsTransformationValidation = 1;
        }
    }
    return 0;
#else

    DeviceImpl *dev = (DeviceImpl *)_this;

    if (!pMatrix)
        return 0;

    if (State == 0x100) {
        memcpy(dev->worldMatrix, pMatrix, 64);
        dev->isWorldViewDirty = 1;
    } else if (State == 2) {
        memcpy(dev->viewMatrix, pMatrix, 64);
        dev->isWorldViewDirty = 1;
    } else if (State == 3) {
        memcpy(dev->projMatrix, pMatrix, 64);
        dev->isProjDirty = 1;
    } else if (State >= D3DTS_TEXTURE0 && State < D3DTS_TEXTURE0 + 8) {
        memcpy(dev->texMatrix, pMatrix, 64);
    }
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetTransform(const CDirect3DDevice *_this, D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &State, &pMatrix };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetTransform", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    const float *src = NULL;
    if (State == 0x100) {
        src = dev->worldMatrix;
    } else if (State == 2) {
        src = dev->viewMatrix;
    } else if (State == 3) {
        src = dev->projMatrix;
#if defined(COD2_X64)
    } else if ((UINT)State >= 16 && (UINT)State - 16 < dev->maxTextureUnits) {
#else
    } else if (State >= D3DTS_TEXTURE0 && State < D3DTS_TEXTURE0 + 8) {
#endif
#if defined(COD2_X64)
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, (UINT)State - 16);
        if ((UINT)State - 16 < dev->maxTextureUnits && stage)
            src = stage->matrix;
#else
        src = dev->texMatrix;
#endif
    }
    if (src && pMatrix) {
        memcpy(pMatrix, src, 64);
    }
    return 0;
}

HRESULT CDirect3DDevice_SetViewport(const CDirect3DDevice *_this, const D3DVIEWPORT9 *pViewport)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pViewport };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetViewport", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    D3DSURFACE_DESC target;
    NativeViewport viewport;
    viewport.X = (uint32_t)pViewport->X;
    viewport.Y = (uint32_t)pViewport->Y;
    viewport.Width = (uint32_t)pViewport->Width;
    viewport.Height = (uint32_t)pViewport->Height;
    memcpy(&viewport.MinZ, &pViewport->MinZ, sizeof(float));
    memcpy(&viewport.MaxZ, &pViewport->MaxZ, sizeof(float));
    if (memcmp(&dev->viewportX, &viewport, sizeof(viewport)) == 0)
        return 0;
    memcpy(&dev->viewportX, &viewport, sizeof(viewport));
    if (dev->renderTarget) {
        void **methods = *(void ***)dev->renderTarget;
        ((HRESULT (*)(const void *, D3DSURFACE_DESC *))methods[12])(dev->renderTarget, &target);
        glViewport(viewport.X, target.Height - (viewport.Y + viewport.Height),
                   viewport.Width, viewport.Height);
    }
    glDepthRange((double)viewport.MinZ, (double)viewport.MaxZ);
    return 0;
#else
    DeviceImpl *dev = (DeviceImpl *)_this;

    dev->viewportX = pViewport->X;
    dev->viewportY = pViewport->Y;
    dev->viewportW = pViewport->Width;
    dev->viewportH = pViewport->Height;
    dev->viewportMinZ = pViewport->MinZ;
    dev->viewportMaxZ = pViewport->MaxZ;

    glViewport(pViewport->X, pViewport->Y, pViewport->Width, pViewport->Height);
    glDepthRange((double)pViewport->MinZ, (double)pViewport->MaxZ);
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetViewport(const CDirect3DDevice *_this, IDirect3DIndexBuffer9 *(*pViewport)[10])
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pViewport };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetViewport", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    D3DVIEWPORT9 *viewport = (D3DVIEWPORT9 *)pViewport;
    viewport->X = dev->viewportX;
    viewport->Y = dev->viewportY;
    viewport->Width = dev->viewportW;
    viewport->Height = dev->viewportH;
    memcpy(&viewport->MinZ, &dev->viewportMinZ, sizeof(float));
    memcpy(&viewport->MaxZ, &dev->viewportMaxZ, sizeof(float));
    return 0;
#else
    DeviceImpl *dev = (DeviceImpl *)_this;
    UINT32 *dst = (UINT32 *)pViewport;
    dst[0] = dev->viewportX;
    dst[1] = dev->viewportY;
    dst[2] = dev->viewportW;
    dst[3] = dev->viewportH;
    *(float *)&dst[4] = dev->viewportMinZ;
    *(float *)&dst[5] = dev->viewportMaxZ;
    return 0;
#endif
}

HRESULT CDirect3DDevice_SetMaterial(const CDirect3DDevice *_this, const D3DMATERIAL9 *pMaterial)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pMaterial };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetMaterial", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (CDirect3DDevice_GL()->supportsATIArrayRevComps) {
        memcpy(&dev->material, pMaterial, sizeof(dev->material));
    } else if (memcmp(&dev->material, pMaterial, sizeof(dev->material))) {
        memcpy(&dev->material, pMaterial, sizeof(dev->material));
        glMaterialfv(0x408, 0x1201, (const GLfloat *)&dev->material.Diffuse);
        glMaterialfv(0x408, 0x1200, (const GLfloat *)&dev->material.Ambient);
        glMaterialfv(0x408, 0x1202, (const GLfloat *)&dev->material.Specular);
        glMaterialfv(0x408, 0x1600, (const GLfloat *)&dev->material.Emissive);
        glMaterialf(0x408, 0x1601, dev->material.Power);
    }
    return 0;
#else

    (void)_this;
    (void)pMaterial;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetMaterial(const CDirect3DDevice *_this, char (*pMaterial)[4])
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pMaterial };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetMaterial", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pMaterial;
    return 0;
}

HRESULT CDirect3DDevice_SetLight(const CDirect3DDevice *_this, DWORD Index, const D3DLIGHT9 *pLight)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index, &pLight };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetLight", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (Index >= 8)
        return 0;
    LightInfoType *light = &dev->nativeLights[Index];
    int enabled = light->Enabled;
    memset(light, 0, sizeof(*light));
    light->Enabled = enabled;
    light->Type = pLight->Type;
    light->Range = pLight->Range;
    memcpy(&light->Direction, &pLight->Direction, sizeof(light->Direction));
    memcpy(&light->Position, &pLight->Position, sizeof(light->Position));
    memcpy(&light->Diffuse, &pLight->Diffuse, sizeof(light->Diffuse));
    memcpy(&light->Specular, &pLight->Specular, sizeof(light->Specular));
    memcpy(&light->Ambient, &pLight->Ambient, sizeof(light->Ambient));
    light->Theta = pLight->Theta;
    light->Phi = pLight->Phi;
    light->Falloff = pLight->Falloff;
    int ati = CDirect3DDevice_GL()->supportsATIArrayRevComps;
    if (pLight->Type == 2) {
        if (ati) {
            light->CosHalfTheta = cosf(light->Theta * 0.5f);
            light->CosHalfPhi = cosf(0.5f * light->Phi);
            light->C1 = 1.0f / (light->CosHalfTheta - light->CosHalfPhi);
        } else {
            float angle;
            if (pLight->Falloff == 0.0f) {
                angle = 3.14f;
            } else {
                angle = pLight->Phi - pLight->Theta;
                float divisor = pLight->Falloff + pLight->Falloff;
                angle = angle / divisor;
                angle = pLight->Theta + angle;
                angle = angle < 0.0001f ? 0.00005f : angle * 0.5f;
            }
            light->Exponent = (float)(log(cos((double)angle)) * -0.30000001192092896);
            float cutoff = 90.0f * pLight->Phi;
            cutoff = cutoff / 0x1.921fb4p+1f;
            light->Cutoff = cutoff / 1.5f;
        }
    } else if (!ati) {
        light->Exponent = 0.0f;
        light->Cutoff = 180.0f;
    }
    light->Attenuation0 = pLight->Attenuation0;
    light->Attenuation1 = pLight->Attenuation1;
    light->Attenuation2 = pLight->Attenuation2;
    if (!ati)
        COpenGL_SetLight((const COpenGL *)CDirect3DDevice_GL(), Index, light);
    return 0;
#else

    (void)_this;
    (void)Index;
    (void)pLight;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetLight(const CDirect3DDevice *_this, DWORD Index)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetLight", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Index;
    return 0;
}

HRESULT CDirect3DDevice_LightEnable(const CDirect3DDevice *_this, DWORD Index, BOOL Enable)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index, &Enable };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_LightEnable", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    /* The original assumes an eight-light caller; bound the native object. */
    if (Index < 8 && dev->nativeLights[Index].Enabled != !!Enable) {
        dev->nativeLights[Index].Enabled = !!Enable;
        if (!CDirect3DDevice_GL()->supportsATIArrayRevComps) {
            if (Enable)
                glEnable(0x4000 + Index);
            else
                glDisable(0x4000 + Index);
        }
    }
    return 0;
#else

    (void)_this;
    (void)Index;
    (void)Enable;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetLightEnable(const CDirect3DDevice *_this, DWORD Index, BOOL *pEnable)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index, &pEnable };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetLightEnable", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Index;
    (void)pEnable;
    return 0;
}

HRESULT CDirect3DDevice_SetClipPlane(const CDirect3DDevice *_this, DWORD Index, const float *pPlane)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index, &pPlane };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetClipPlane", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Index;
    (void)pPlane;
    return 0;
}

HRESULT CDirect3DDevice_GetClipPlane(const CDirect3DDevice *_this, DWORD Index, float *pPlane)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Index, &pPlane };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetClipPlane", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Index;
    (void)pPlane;
    return 0;
}

HRESULT CDirect3DDevice_SetStreamSource(const CDirect3DDevice *_this, UINT StreamNumber,
                                        IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StreamNumber, &pStreamData, &OffsetInBytes, &Stride };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetStreamSource", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
#if defined(COD2_X64)
    CDirect3DVertexBufferClean *buffer = (void *)pStreamData;
    unsigned char wasLocked = buffer ? buffer->isLocked : 1;
    if (buffer)
        buffer->isLocked = 0;
    if (dev->streams[StreamNumber] == pStreamData &&
        dev->streamOffsets[StreamNumber] == OffsetInBytes && !wasLocked)
        return 0;
#endif
    dev->streams[StreamNumber] = (IDirect3DVertexBuffer9 *)pStreamData;
    dev->streamOffsets[StreamNumber] = OffsetInBytes;
    dev->streamStrides[StreamNumber] = Stride;
    CDirect3DDevice_mNeedsTransformationValidation = 1;
    CDirect3DDevice_mNeedsRasterizationValidation = 1;
    return 0;
}

HRESULT CDirect3DDevice_GetStreamSource(const CDirect3DDevice *_this, UINT StreamNumber,
                                        IDirect3DVertexBuffer9 **ppStreamData, UINT *pOffsetInBytes, UINT *pStride)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StreamNumber, &ppStreamData, &pOffsetInBytes, &pStride };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetStreamSource", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    *ppStreamData = dev->streams[StreamNumber];
    if (dev->streams[StreamNumber]) {
        void **vtbl = *(void ***)dev->streams[StreamNumber];
        ((ULONG (*)(const void *))vtbl[1])((const void *)dev->streams[StreamNumber]);
    }
    *pStride = dev->streamStrides[StreamNumber];
    *pOffsetInBytes = dev->streamOffsets[StreamNumber];
    return 0;
}

HRESULT CDirect3DDevice_SetIndices(const CDirect3DDevice *_this, IDirect3DIndexBuffer9 *pIndexData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pIndexData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetIndices", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    dev->indexBuffer = pIndexData;
    return 0;
}

HRESULT CDirect3DDevice_GetIndices(const CDirect3DDevice *_this, IDirect3DIndexBuffer9 **ppIndexData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppIndexData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetIndices", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    *ppIndexData = dev->indexBuffer;
    return 0;
}

HRESULT CDirect3DDevice_SetVertexDeclaration(const CDirect3DDevice *_this, IDirect3DVertexDeclaration9 *pDecl)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDecl };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetVertexDeclaration", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (g_activeVertexDeclaration != pDecl) {
        g_activeVertexDeclaration = pDecl;
        CDirect3DDevice_StreamsNative(_this)->tokensSet = 0;
        g_currentFVF = 0;
        dev->hasDiffuseColor = 0;
        const CDirect3DVertexDeclarationImpl *declaration = (const void *)pDecl;
        if (declaration) {
            for (UINT i = 0; i + 1 < declaration->elementCount; ++i) {
                const D3DVERTEXELEMENT9 *element = &declaration->elements[i];
                if (element->Usage == 10 && element->UsageIndex == 0)
                    dev->hasDiffuseColor = 1;
            }
        }
        /* A same-pointer call retains the current tokens even after SetFVF,
         * as the comparison at Mac 0x14032 does. */
        CDirect3DDevice_mNeedsTransformationValidation = 1;
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
    }
    return 0;
#else
    (void)_this;
    g_activeVertexDeclaration = pDecl;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetVertexDeclaration(const CDirect3DDevice *_this, IDirect3DVertexDeclaration9 **ppDecl)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDecl };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetVertexDeclaration", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    *ppDecl = g_activeVertexDeclaration;
    return 0;
}

HRESULT CDirect3DDevice_SetFVF(const CDirect3DDevice *_this, DWORD FVF)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &FVF };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetFVF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    UINT32 *token = state->tokens;
    if (FVF) {
        *token++ = 0x20000000;
        if (FVF & 2) *token++ = 0x40020000;
        if (FVF & 0x10) *token++ = 0x40020003;
        if (FVF & 0x40) *token++ = 0x40040005;
        UINT32 count = (FVF & 0xf00) >> 8;
        for (UINT32 i = 0; i < count; ++i) {
            UINT32 dimensions = (FVF >> ((16 + i * 2) & 31)) & 3;
            UINT32 type = dimensions == 0 ? 1 : dimensions == 1 ? 2 : dimensions == 2 ? 3 : 0;
            *token++ = 0x40000000 | (type << 16) | (8 + i);
        }
    }
    *token = 0xffffffff;
    state->tokensSet = 1;
    ((DeviceImpl *)_this)->hasDiffuseColor = !!(FVF & 0x40);
    CDirect3DDevice_mNeedsTransformationValidation = 1;
    CDirect3DDevice_mNeedsRasterizationValidation = 1;
#else
    (void)_this;
#endif
    g_currentFVF = FVF;
    return 0;
}

HRESULT CDirect3DDevice_GetFVF(const CDirect3DDevice *_this, DWORD *pFVF)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pFVF };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetFVF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pFVF;
    return 0;
}

HRESULT CDirect3DDevice_SetVertexShader(const CDirect3DDevice *_this, IDirect3DVertexShader9 *pShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetVertexShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)_this;
    if (g_activeVertexShader != pShader) {
        g_activeVertexShader = pShader;
        CDirect3DDevice_mNeedsVertexShaderValidation = 1;
        CDirect3DDevice_mNeedsTransformationValidation = 1;
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
    }
    return 0;
#else
    (void)_this;
    g_activeVertexShader = pShader;
    CDirect3DDevice_mNeedsVertexShaderValidation = 1;

    return 0;
#endif
}

HRESULT CDirect3DDevice_GetVertexShader(const CDirect3DDevice *_this, IDirect3DVertexShader9 **ppShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetVertexShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppShader;
    return 0;
}

HRESULT CDirect3DDevice_SetPixelShader(const CDirect3DDevice *_this, IDirect3DPixelShader9 *pShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetPixelShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    if (dev->pixelShader != pShader) {
        dev->pixelShader = pShader;
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
    }
    return 0;
}

HRESULT CDirect3DDevice_GetPixelShader(const CDirect3DDevice *_this, IDirect3DPixelShader9 **ppShader)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppShader };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetPixelShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppShader;
    return 0;
}

HRESULT CDirect3DDevice_SetVertexShaderConstantF(const CDirect3DDevice *_this, UINT StartRegister,
                                                 const float *pConstantData, UINT Vector4fCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4fCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetVertexShaderConstantF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    UINT i;
    const float *pf = pConstantData;
    (void)_this;
    for (i = StartRegister; i < StartRegister + Vector4fCount; i++) {
        glProgramEnvParameter4fvARB(0x8620, i, pf);
        if (i < 256)
            memcpy(g_vsConst + i * 4, pf, 16);
        pf += 4;
    }
    return 0;
}

HRESULT CDirect3DDevice_GetVertexShaderConstantF(const CDirect3DDevice *_this, UINT StartRegister,
                                                 float *pConstantData, UINT Vector4fCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4fCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetVertexShaderConstantF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4fCount;
    return 0;
}

HRESULT CDirect3DDevice_SetPixelShaderConstantF(const CDirect3DDevice *_this, UINT StartRegister,
                                                const float *pConstantData, UINT Vector4fCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4fCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetPixelShaderConstantF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
#if COD2_APPLE_SDK
    if (StartRegister <= 256 && Vector4fCount <= 256 - StartRegister)
        memcpy(macPixelConstants[StartRegister], pConstantData, Vector4fCount * 16);
#endif
    if (dev->pixelShader) {
        void **vtbl = *(void ***)dev->pixelShader;

        ((void (*)(const void *, int, const float *, int, int))vtbl[9])(
            (const void *)dev->pixelShader, StartRegister, pConstantData, Vector4fCount, 0);
    }
    return 0;
}

HRESULT CDirect3DDevice_GetPixelShaderConstantF(const CDirect3DDevice *_this, UINT StartRegister,
                                                float *pConstantData, UINT Vector4fCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4fCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetPixelShaderConstantF", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4fCount;
    return 0;
}

HRESULT CDirect3DDevice_SetVertexShaderConstantI(const CDirect3DDevice *_this, UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4iCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetVertexShaderConstantI", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4iCount;
    return 0;
}
HRESULT CDirect3DDevice_GetVertexShaderConstantI(const CDirect3DDevice *_this, UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4iCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetVertexShaderConstantI", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4iCount;
    return 0;
}
HRESULT CDirect3DDevice_SetVertexShaderConstantB(const CDirect3DDevice *_this, UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &BoolCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetVertexShaderConstantB", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)BoolCount;
    return 0;
}
HRESULT CDirect3DDevice_GetVertexShaderConstantB(const CDirect3DDevice *_this, UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &BoolCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetVertexShaderConstantB", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)BoolCount;
    return 0;
}
HRESULT CDirect3DDevice_SetPixelShaderConstantI(const CDirect3DDevice *_this, UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4iCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetPixelShaderConstantI", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4iCount;
    return 0;
}
HRESULT CDirect3DDevice_GetPixelShaderConstantI(const CDirect3DDevice *_this, UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &Vector4iCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetPixelShaderConstantI", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)Vector4iCount;
    return 0;
}
HRESULT CDirect3DDevice_SetPixelShaderConstantB(const CDirect3DDevice *_this, UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &BoolCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetPixelShaderConstantB", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)BoolCount;
    return 0;
}
HRESULT CDirect3DDevice_GetPixelShaderConstantB(const CDirect3DDevice *_this, UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StartRegister, &pConstantData, &BoolCount };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetPixelShaderConstantB", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StartRegister;
    (void)pConstantData;
    (void)BoolCount;
    return 0;
}

HRESULT CDirect3DDevice_SetRenderTarget(const CDirect3DDevice *_this, DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &RenderTargetIndex, &pRenderTarget };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetRenderTarget", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)RenderTargetIndex;
    return DeviceTargets_SetTarget((DeviceImpl *)_this, pRenderTarget);
#else

    DeviceImpl *dev = (DeviceImpl *)_this;
    (void)RenderTargetIndex;
#if defined(COD2_X64)
    /* Mac 1.3 resolves the previous auxiliary buffer before selecting the
     * next target. Gaussian passes then sample the texture just written. */
    if (pRenderTarget && dev->renderTarget && dev->renderTarget != dev->backBuffer && !g_InhibitCopy) {
        extern int CDirect3DSurface_GetGLBlitInfo(const void *, unsigned int *, unsigned int *, unsigned int *);
        unsigned int texture, width, height;
        if (CDirect3DSurface_GetGLBlitInfo(dev->renderTarget, &texture, &width, &height)) {
            GLint activeTexture, binding;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glActiveTextureARB(GL_TEXTURE0_ARB);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
            glBindTexture(GL_TEXTURE_2D, texture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
            glBindTexture(GL_TEXTURE_2D, binding);
            glActiveTextureARB(activeTexture);
        }
    }
#endif
    dev->renderTarget = pRenderTarget;
#if defined(COD2_X64)
    /* The engine updates its viewport cache during this switch; the device
     * must apply the new extent here, as the Mac setter does. */
    if (pRenderTarget) {
        D3DSURFACE_DESC desc;
        void **methods = *(void ***)pRenderTarget;
        ((HRESULT (*)(const void *, D3DSURFACE_DESC *))methods[12])(pRenderTarget, &desc);
        D3DVIEWPORT9 viewport = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };
        CDirect3DDevice_SetViewport(_this, &viewport);
    }
#endif
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetRenderTarget(const CDirect3DDevice *_this, DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &RenderTargetIndex, &ppRenderTarget };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetRenderTarget", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    (void)RenderTargetIndex;
    *ppRenderTarget = dev->renderTarget;
    if (dev->renderTarget) {
        void **vtbl = *(void ***)dev->renderTarget;
        ((ULONG (*)(const void *))vtbl[1])((const void *)dev->renderTarget);
    }
    return 0;
}

HRESULT CDirect3DDevice_SetDepthStencilSurface(const CDirect3DDevice *_this, IDirect3DSurface9 *pNewZStencil)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pNewZStencil };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetDepthStencilSurface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return DeviceTargets_SetDepth((DeviceImpl *)_this, pNewZStencil);
#else

    DeviceImpl *dev = (DeviceImpl *)_this;
#if defined(COD2_X64)
    if (dev->depthStencilDetached != (pNewZStencil == NULL)) {
        dev->depthStencilDetached = pNewZStencil == NULL;
        glDepthMask(pNewZStencil && dev->zWriteEnable);
        if (pNewZStencil && dev->zEnable)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glStencilMask(pNewZStencil ? dev->stencilWriteMask & 0xff : 0);
        if (pNewZStencil && dev->stencilEnable)
            glEnable(GL_STENCIL_TEST);
        else
            glDisable(GL_STENCIL_TEST);
    }
#endif
    dev->depthStencil = pNewZStencil;
    return 0;
#endif
}

HRESULT CDirect3DDevice_GetDepthStencilSurface(const CDirect3DDevice *_this, IDirect3DSurface9 **ppZStencilSurface)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppZStencilSurface };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetDepthStencilSurface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    DeviceImpl *dev = (DeviceImpl *)_this;
    *ppZStencilSurface = dev->depthBuffer;
    if (dev->depthBuffer) {
        void **vtbl = *(void ***)dev->depthBuffer;
        ((ULONG (*)(const void *))vtbl[1])((const void *)dev->depthBuffer);
    }
    return 0;
}

HRESULT CDirect3DDevice_SetStreamSourceFreq(const CDirect3DDevice *_this, UINT StreamNumber, UINT Setting)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StreamNumber, &Setting };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetStreamSourceFreq", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StreamNumber;
    (void)Setting;
    return 0;
}
HRESULT CDirect3DDevice_GetStreamSourceFreq(const CDirect3DDevice *_this, UINT StreamNumber, UINT *pSetting)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &StreamNumber, &pSetting };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetStreamSourceFreq", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)StreamNumber;
    (void)pSetting;
    return 0;
}

HRESULT CDirect3DDevice_ValidateDevice(const CDirect3DDevice *_this, DWORD *pNumPasses)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pNumPasses };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_ValidateDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    if (pNumPasses)
        *pNumPasses = 1;
    return 0;
}

HRESULT CDirect3DDevice_Reset(const CDirect3DDevice *_this, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pPresentationParameters };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_Reset", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return CDirect3DDevice_ResetNative((DeviceImpl *)_this, pPresentationParameters);
#else
    (void)_this;
#if COD2_APPLE_SDK
    if (!pPresentationParameters)
        return (HRESULT)(int32_t)0x8876086cu;
    /* r_init.c constructs this retained D3D9 presentation word array. */
    const int *parameters = (const int *)pPresentationParameters;
    extern dvar_t *Dvar_RegisterBool(const char *, int, int);
    dvar_t *borderless = Dvar_RegisterBool("r_borderless", 1, 0x1001);
    MacPlatform_ConfigureWindow(parameters[0], parameters[1],
        parameters[7] ? MAC_WINDOWED : (borderless->current.enabled ? MAC_BORDERLESS : MAC_FULLSCREEN), parameters[12]);
    if (MacDisplay_SetMode(parameters[0], parameters[1], 32, parameters[12]))
        return (HRESULT)(int32_t)0x8876086cu;
#if defined(COD2_X64)
    MacDisplay_SetPresentationInterval(((DeviceImpl *)_this)->context, (uint32_t)parameters[11]);
#endif
    extern void CDirect3DSurface_SetNativeDimensions(void *, UINT32, UINT32);
    CDirect3DSurface_SetNativeDimensions(((DeviceImpl *)_this)->backBuffer, parameters[0], parameters[1]);
    return 0;
#else
    (void)pPresentationParameters;
    return 0;
#endif
#endif
}

long unsigned int CDirect3DDevice_SetGammaRamp(const CDirect3DDevice *_this, UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &Flags, &pRamp };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetGammaRamp", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)iSwapChain;
    (void)Flags;
    MacDisplay_SetGammaRamp(pRamp);
    return 0;
}

long unsigned int CDirect3DDevice_GetGammaRamp(const CDirect3DDevice *_this, UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &pRamp };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetGammaRamp", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)iSwapChain;
    (void)pRamp;
    return 0;
}

/* Blit a source surface onto the current framebuffer as a textured quad.
   This was an empty stub, which is why ROQ cinematics never appeared: the frame was
   decoded and uploaded to a texture, then RB_StretchRawCmd asked StretchRect to put it
   on the backbuffer and nothing happened -- leaving only the letterbox bars, i.e. a
   black screen. Only the backbuffer destination used by the cinematic path is handled;
   surface-to-surface copies still no-op. */
HRESULT CDirect3DDevice_StretchRect(const CDirect3DDevice *_this, IDirect3DSurface9 *pSourceSurface,
                                    const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSourceSurface, &pSourceRect, &pDestSurface, &pDestRect, &Filter };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_StretchRect", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return DeviceTargets_Stretch((DeviceImpl *)_this, pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
#else

#if defined(COD2_X64)
    /* Mac 1.3 copies the backbuffer into the destination texture without
     * drawing a quad. The saved-screen blend needs this copy every frame. */
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (pSourceSurface == dev->backBuffer && !pSourceRect && !pDestRect) {
        extern int CDirect3DSurface_GetGLBlitInfo(const void *, unsigned int *, unsigned int *, unsigned int *);
        unsigned int texture, width, height;
        D3DSURFACE_DESC source;
        void **methods = *(void ***)pSourceSurface;
        if (((HRESULT (*)(const void *, D3DSURFACE_DESC *))methods[12])(pSourceSurface, &source) == 0 &&
            CDirect3DSurface_GetGLBlitInfo(pDestSurface, &texture, &width, &height) &&
            source.Width == width && source.Height == height) {
            GLint activeTexture, binding;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glActiveTextureARB(GL_TEXTURE0_ARB);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
            glBindTexture(GL_TEXTURE_2D, texture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
            glBindTexture(GL_TEXTURE_2D, binding);
            glActiveTextureARB(activeTexture);
            return 0;
        }
    }
#endif
    extern int CDirect3DSurface_GetGLBlitInfo(const void *surf, unsigned int *texId,
                                              unsigned int *width, unsigned int *height);
    extern void glDrawElements(unsigned int, int, unsigned int, const void *);
    static const unsigned short quadIdx[6] = { 0, 1, 2, 0, 2, 3 };
    unsigned int srcTex = 0, srcW = 0, srcH = 0;
    int vp[4] = { 0, 0, 640, 480 };
    float x0, y0, x1, y1, u0, v0, u1, v1;
    float pos[8], uv[8], ortho[16];

    (void)_this;
    (void)pDestSurface;
    (void)Filter;

    if (!CDirect3DSurface_GetGLBlitInfo(pSourceSurface, &srcTex, &srcW, &srcH))
        return 0;

    glGetIntegerv(0x0BA2 /*GL_VIEWPORT*/, vp);

    if (pDestRect) {
        x0 = (float)pDestRect->left;  y0 = (float)pDestRect->top;
        x1 = (float)pDestRect->right; y1 = (float)pDestRect->bottom;
    } else {
        x0 = 0.0f; y0 = 0.0f; x1 = (float)vp[2]; y1 = (float)vp[3];
    }

    u0 = v0 = 0.0f;
    u1 = v1 = 1.0f;
    if (pSourceRect && srcW && srcH) {
        u0 = (float)pSourceRect->left  / (float)srcW;
        v0 = (float)pSourceRect->top   / (float)srcH;
        u1 = (float)pSourceRect->right / (float)srcW;
        v1 = (float)pSourceRect->bottom / (float)srcH;
    }

    /* same top-left-origin ortho the 2D path builds, so pDestRect is in screen pixels */
    {
        float w = vp[2] ? (float)vp[2] : 640.0f;
        float h = vp[3] ? (float)vp[3] : 480.0f;
        float m[16] = { 2.0f / w, 0, 0, 0,
                        0, -2.0f / h, 0, 0,
                        0, 0, -1.0f, 0,
                        -1.0f, 1.0f, 0, 1.0f };
        memcpy(ortho, m, sizeof(ortho));
    }

    pos[0] = x0; pos[1] = y0;   uv[0] = u0; uv[1] = v0;
    pos[2] = x1; pos[3] = y0;   uv[2] = u1; uv[3] = v0;
    pos[4] = x1; pos[5] = y1;   uv[4] = u1; uv[5] = v1;
    pos[6] = x0; pos[7] = y1;   uv[6] = u0; uv[7] = v1;

    glMatrixMode(0x1701 /*PROJECTION*/);
    glLoadMatrixf(ortho);
    glMatrixMode(0x1700 /*MODELVIEW*/);
    glLoadIdentity();

    glDisable(0x0B71 /*DEPTH_TEST*/);
    glDepthMask(0);
    glDisable(0x0BE2 /*BLEND*/);
    glDisable(0x0BC0 /*ALPHA_TEST*/);
    glDisable(0x0B44 /*CULL_FACE*/);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glActiveTextureARB(GL_TEXTURE0_ARB);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);
    CDirect3DDevice_DisableExtraTextureUnits();
    glEnable(0x0DE1 /*TEXTURE_2D*/);
    glBindTexture(0x0DE1, srcTex);
    glTexEnvi(0x2300 /*TEXTURE_ENV*/, 0x2200 /*TEXTURE_ENV_MODE*/, 0x1E01 /*REPLACE*/);

    glDisableClientState(0x8076 /*COLOR_ARRAY*/);
    glEnableClientState(0x8074 /*VERTEX_ARRAY*/);
    glVertexPointer(2, 0x1406 /*FLOAT*/, 0, pos);
    glEnableClientState(0x8078 /*TEXTURE_COORD_ARRAY*/);
    glTexCoordPointer(2, 0x1406, 0, uv);

    glDrawElements(0x0004 /*TRIANGLES*/, 6, 0x1403 /*UNSIGNED_SHORT*/, quadIdx);

    glDisableClientState(0x8074);
    glDisableClientState(0x8078);
    return 0;
#endif
}

HRESULT CDirect3DDevice_UpdateTexture(const CDirect3DDevice *_this, IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSourceTexture, &pDestinationTexture };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_UpdateTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pSourceTexture;
    (void)pDestinationTexture;
    return 0;
}

HRESULT CDirect3DDevice_MultiplyTransform(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_MultiplyTransform", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

HRESULT CDirect3DDevice_SetCursorProperties(const CDirect3DDevice *_this, UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &XHotSpot, &YHotSpot, &pCursorBitmap };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetCursorProperties", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)XHotSpot;
    (void)YHotSpot;
    (void)pCursorBitmap;
    return 0;
}
long unsigned int CDirect3DDevice_SetCursorPosition(const CDirect3DDevice *_this, int XScreenSpace, int YScreenSpace, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &XScreenSpace, &YScreenSpace, &Flags };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetCursorPosition", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)XScreenSpace;
    (void)YScreenSpace;
    (void)Flags;
    return 0;
}
BOOL CDirect3DDevice_ShowCursor(const CDirect3DDevice *_this, BOOL bShow)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &bShow };
    BOOL macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_ShowCursor", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)_this;
    (void)bShow;
    return 1;
#else
    (void)_this;
    (void)bShow;
    return 0;
#endif
}

HRESULT CDirect3DDevice_CreateAdditionalSwapChain(const CDirect3DDevice *_this, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **pSwapChain)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pPresentationParameters, &pSwapChain };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateAdditionalSwapChain", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pPresentationParameters;
    (void)pSwapChain;
    return 0;
}
HRESULT CDirect3DDevice_GetSwapChain(const CDirect3DDevice *_this, UINT iSwapChain, IDirect3DSwapChain9 **pSwapChain)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &pSwapChain };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetSwapChain", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    (void)iSwapChain;
    *pSwapChain = dev->swapChain;
    if (dev->swapChain)
        ((ULONG (*)(const void *))(*(void ***)dev->swapChain)[1])(dev->swapChain);
    return 0;
#else
    (void)_this;
    (void)iSwapChain;
    (void)pSwapChain;
    return 0;
#endif
}
UINT CDirect3DDevice_GetNumberOfSwapChains(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    UINT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetNumberOfSwapChains", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)_this;
    return 0;
#else
    (void)_this;
    return 1;
#endif
}
HRESULT CDirect3DDevice_GetRasterStatus(const CDirect3DDevice *_this, UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &pRasterStatus };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetRasterStatus", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)iSwapChain;
    (void)pRasterStatus;
    return 0;
}
HRESULT CDirect3DDevice_SetDialogBoxMode(const CDirect3DDevice *_this, BOOL bEnableDialogs)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &bEnableDialogs };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetDialogBoxMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)bEnableDialogs;
    return 0;
}

HRESULT CDirect3DDevice_UpdateSurface(const CDirect3DDevice *_this, IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSourceSurface, &pSourceRect, &pDestinationSurface, &pDestPoint };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_UpdateSurface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pSourceSurface;
    (void)pSourceRect;
    (void)pDestinationSurface;
    (void)pDestPoint;
    return 0;
}
HRESULT CDirect3DDevice_GetRenderTargetData(const CDirect3DDevice *_this, IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pRenderTarget, &pDestSurface };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetRenderTargetData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pRenderTarget;
    (void)pDestSurface;
    return 0;
}
HRESULT CDirect3DDevice_GetFrontBufferData(const CDirect3DDevice *_this, UINT iSwapChain, IDirect3DSurface9 *pDestSurface)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iSwapChain, &pDestSurface };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetFrontBufferData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)iSwapChain;
    (void)pDestSurface;
    return 0;
}
HRESULT CDirect3DDevice_ColorFill(const CDirect3DDevice *_this, IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSurface, &pRect, &color };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_ColorFill", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pSurface;
    (void)pRect;
    (void)color;
    return 0;
}

HRESULT CDirect3DDevice_CreateStateBlock(const CDirect3DDevice *_this, D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Type, &ppSB };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CreateStateBlock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Type;
    (void)ppSB;
    return 0;
}
HRESULT CDirect3DDevice_BeginStateBlock(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_BeginStateBlock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}
HRESULT CDirect3DDevice_EndStateBlock(const CDirect3DDevice *_this, IDirect3DStateBlock9 **ppSB)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppSB };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_EndStateBlock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppSB;
    return 0;
}
HRESULT CDirect3DDevice_SetClipStatus(const CDirect3DDevice *_this, const D3DCLIPSTATUS9 *pClipStatus)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pClipStatus };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetClipStatus", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pClipStatus;
    return 0;
}
HRESULT CDirect3DDevice_GetClipStatus(const CDirect3DDevice *_this, D3DCLIPSTATUS9 *pClipStatus)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pClipStatus };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetClipStatus", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pClipStatus;
    return 0;
}
HRESULT CDirect3DDevice_SetPaletteEntries(const CDirect3DDevice *_this, UINT PaletteNumber, const PALETTEENTRY *pEntries)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PaletteNumber, &pEntries };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetPaletteEntries", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PaletteNumber;
    (void)pEntries;
    return 0;
}
HRESULT CDirect3DDevice_GetPaletteEntries(const CDirect3DDevice *_this, UINT PaletteNumber, _D3DFORMAT (*pEntries)[4][4])
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PaletteNumber, &pEntries };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetPaletteEntries", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PaletteNumber;
    (void)pEntries;
    return 0;
}
HRESULT CDirect3DDevice_SetCurrentTexturePalette(const CDirect3DDevice *_this, UINT PaletteNumber)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PaletteNumber };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetCurrentTexturePalette", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PaletteNumber;
    return 0;
}
HRESULT CDirect3DDevice_GetCurrentTexturePalette(const CDirect3DDevice *_this, UINT *PaletteNumber)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PaletteNumber };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetCurrentTexturePalette", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PaletteNumber;
    return 0;
}
HRESULT CDirect3DDevice_SetScissorRect(const CDirect3DDevice *_this, const RECT *pRect)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pRect };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetScissorRect", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pRect;
    return 0;
}
HRESULT CDirect3DDevice_GetScissorRect(const CDirect3DDevice *_this, RECT *pRect)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pRect };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetScissorRect", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pRect;
    return 0;
}
HRESULT CDirect3DDevice_SetSoftwareVertexProcessing(const CDirect3DDevice *_this, BOOL bSoftware)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &bSoftware };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetSoftwareVertexProcessing", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)bSoftware;
    return 0;
}
BOOL CDirect3DDevice_GetSoftwareVertexProcessing(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    BOOL macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetSoftwareVertexProcessing", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}
HRESULT CDirect3DDevice_SetNPatchMode(const CDirect3DDevice *_this, float nSegments)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &nSegments };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetNPatchMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)nSegments;
    return 0;
}
float CDirect3DDevice_GetNPatchMode(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    float macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetNPatchMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0.0f;
}
HRESULT CDirect3DDevice_ProcessVertices(const CDirect3DDevice *_this, UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &SrcStartIndex, &DestIndex, &VertexCount, &pDestBuffer, &pVertexDecl, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_ProcessVertices", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)SrcStartIndex;
    (void)DestIndex;
    (void)VertexCount;
    (void)pDestBuffer;
    (void)pVertexDecl;
    (void)Flags;
    return 0;
}
HRESULT CDirect3DDevice_DrawRectPatch(const CDirect3DDevice *_this, UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Handle, &pNumSegs, &pRectPatchInfo };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawRectPatch", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Handle;
    (void)pNumSegs;
    (void)pRectPatchInfo;
    return 0;
}
HRESULT CDirect3DDevice_DrawTriPatch(const CDirect3DDevice *_this, UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Handle, &pNumSegs, &pTriPatchInfo };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DrawTriPatch", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Handle;
    (void)pNumSegs;
    (void)pTriPatchInfo;
    return 0;
}
HRESULT CDirect3DDevice_DeletePatch(const CDirect3DDevice *_this, UINT Handle)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Handle };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_DeletePatch", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Handle;
    return 0;
}

long unsigned int CDirect3DDevice_StartPixelOneToOneState(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_StartPixelOneToOneState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceTargets_StartPixel((DeviceImpl *)_this);
    return 0;
#else

    (void)_this;
    return 0;
#endif
}

long unsigned int CDirect3DDevice_EndPixelOneToOneState(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_EndPixelOneToOneState", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceTargets_EndPixel((DeviceImpl *)_this);
    return 0;
#else

    (void)_this;
    return 0;
#endif
}

long unsigned int CDirect3DDevice_SetAlphaFuncAndRef(const CDirect3DDevice *_this, DWORD AlphaFuncValue, DWORD AlphaRefValue)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &AlphaFuncValue, &AlphaRefValue };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetAlphaFuncAndRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    int changed = 0;
    if (dev->alphaRefVal != AlphaRefValue) {
        dev->alphaRefVal = AlphaRefValue;
        dev->alphaRef = (float)AlphaRefValue / 255.0f;
        changed = 1;
    }
    if (dev->alphaFuncVal != AlphaFuncValue) {
        dev->alphaFuncVal = AlphaFuncValue;
        if (AlphaFuncValue >= 1 && AlphaFuncValue <= 8)
            dev->alphaFunc = 0x1ff + AlphaFuncValue;
        changed = 1;
    }
    if (changed)
        glAlphaFunc(dev->alphaFunc, dev->alphaRef);
    return 0;
#else
    (void)_this;
    (void)AlphaFuncValue;
    (void)AlphaRefValue;
    return 0;
#endif
}

GLenum CDirect3DDevice_GetStencilOpFunc(const CDirect3DDevice *_this, UINT32 Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Value };
    GLenum macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_GetStencilOpFunc", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    switch (Value) {
    case 1:
        return 0x1e00;
    case 3:
        return 0x1e01;
    case 4:
        return 0x1e02;
    case 5:
        return 0x1e03;
    case 6:
        return 0x150a;
    case 7:
        return 0x8507;
    case 8:
        return 0x8508;
    default:
        return 0;
    }
}

long unsigned int CDirect3DDevice_CopyColorToRenderTargetTexture(const CDirect3DDevice *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CopyColorToRenderTargetTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceTargets_CopyColor((DeviceImpl *)_this);
    return 0;
#else

    (void)_this;
    return 0;
#endif
}

#if defined(COD2_X64)
bool CDirect3DDevice_CTexStage_GetUsesTexture(const CTexStage *_this, CXX_BOOL *UsesTexFactor)
#else
bool CDirect3DDevice_CTexStage_GetUsesTexture(const CTexStage *_this, bool *UsesTexFactor)
#endif
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &UsesTexFactor };
    bool macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CTexStage_GetUsesTexture", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    const CTexStageNative *stage = (const CTexStageNative *)_this;
    UINT c0 = stage->colorArg0 & 15, c1 = stage->colorArg1 & 15, c2 = stage->colorArg2 & 15;
    UINT a0 = stage->alphaArg0 & 15, a1 = stage->alphaArg1 & 15, a2 = stage->alphaArg2 & 15;
    int texture = 0, factor = 0;
    switch (stage->colorOp) {
    case 2: texture = c1 == 2; factor = c1 == 3; break;
    case 3: texture = c2 == 2; factor = c2 == 3; break;
    case 4: case 5: case 6: case 7: case 10: case 12: case 14: case 16: case 18: case 24:
        texture = c1 == 2 || c2 == 2; factor = c1 == 3 || c2 == 3; break;
    case 13:
        texture = 1; factor = c1 == 3 || c2 == 3 || c0 == 3; break;
    case 25: case 26:
        texture = c1 == 2 || c2 == 2 || c0 == 2;
        factor = c1 == 3 || c2 == 3 || c0 == 3; break;
    default: break;
    }
    if (!texture) {
        switch (stage->alphaOp) {
        case 1: factor = 0; break;
        case 2: texture = a1 == 2; break;
        case 3: texture = a2 == 2; break;
        case 4: case 5: case 7: case 12: texture = a1 == 2 || a2 == 2; break;
        case 24: texture = a1 == 2 || a2 == 2 || a0 == 2; break;
        default: break;
        }
    }
    if (!factor) {
        switch (stage->alphaOp) {
        case 1: factor = 0; break;
        case 2: factor = a1 == 3; break;
        case 3: factor = a2 == 3; break;
        case 4: case 5: case 7: case 12: factor = a1 == 3 || a2 == 3; break;
        /* Mac compares texture selectors here, including this deterministic quirk. */
        case 24: factor = a1 == 2 || a2 == 2 || a0 == 2; break;
        default: break;
        }
    }
    *UsesTexFactor = !!factor;
    return !!texture;
#else

    (void)_this;
    (void)UsesTexFactor;
    return 0;
#endif
}

#if defined(COD2_X64)
extern VAOStatus CVAOPacket_sVAOStatus;
extern void CTexCoordArray_SetGuardFull(const CTexCoordArray *, int, GLenum, GLsizei, const void *, GLint);
#endif

long unsigned int CDirect3DDevice_SetTextureCoordStream(const CDirect3DDevice *_this,
                                                        UINT32 TexUnit, UINT32 TransformFlags, const D3DMATRIX *OriginalTexTransform,
                                                        GLint VSize, UINT32 Tci, UINT32 StartVertex, UINT32 EndVertex,
                                                        const float *pSrcCoords, UINT32 SrcStride, const float *pSrc2Coords, UINT32 Src2Stride)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &TexUnit, &TransformFlags, &OriginalTexTransform, &VSize, &Tci, &StartVertex, &EndVertex, &pSrcCoords, &SrcStride, &pSrc2Coords, &Src2Stride };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_SetTextureCoordStream", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    COpenGL *gl = (COpenGL *)CDirect3DDevice_GL();
    float matrix[16], modelView[16], combined[16];
    const float *original = (const float *)OriginalTexTransform;
    Bool projected = (TransformFlags & 0x100) != 0;
    UINT32 count = TransformFlags & ~0x100u;
    Bool setMatrix = 0, bindArray = 0;
    (void)StartVertex; (void)EndVertex; (void)pSrc2Coords; (void)Src2Stride;
    if (Tci == 0) {
        if (count) {
            memcpy(matrix, original, sizeof(matrix));
            if (count == 2) {
                matrix[12] = matrix[8]; matrix[13] = matrix[9];
                matrix[8] = matrix[9] = matrix[2] = matrix[6] = matrix[10] = matrix[14] = 0.0f;
            }
            setMatrix = 1;
        }
        bindArray = 1;
    } else if (Tci == 0x20000) {
        if (projected || count == 3 || count == 2) {
            memcpy(matrix, original, sizeof(matrix));
            if (!projected && count == 2) {
                matrix[2] = matrix[6] = matrix[10] = matrix[14] = 0.0f;
                matrix[3] = matrix[7] = matrix[11] = 0.0f;
                matrix[15] = 1.0f;
            }
            D3DXMatrixMultiply((D3DXMATRIX *)modelView, (const D3DXMATRIX *)dev->worldMatrix,
                              (const D3DXMATRIX *)dev->viewMatrix);
            D3DXMatrixMultiply((D3DXMATRIX *)combined, (const D3DXMATRIX *)modelView,
                              (const D3DXMATRIX *)matrix);
            memcpy(matrix, combined, sizeof(matrix));
            setMatrix = 1;
        }
        bindArray = 1;
    } else if (Tci == 0x10000) {
        if (count != 2) {
            *(Bool *)imp__ZN7COpenGL9sDrawFlagE = 0;
        } else if (MacDisplay_GetCardType() != 2) {
            memcpy(matrix, original, sizeof(matrix));
            matrix[2] = matrix[6] = matrix[10] = matrix[14] = 0.0f;
            matrix[3] = matrix[7] = matrix[11] = 0.0f;
            matrix[15] = 1.0f;
            D3DXMatrixMultiply((D3DXMATRIX *)modelView, (const D3DXMATRIX *)dev->worldMatrix,
                              (const D3DXMATRIX *)dev->viewMatrix);
            COpenGL_SetActiveTexUnit(gl, TexUnit);
            /* The first plane's first component is loaded from the owning
             * model-view cache in Mac; the remaining plane values share it. */
            for (UINT32 i = 0; i < 3; ++i) {
                float plane[4] = { modelView[i], modelView[i + 4], modelView[i + 8], modelView[i + 12] };
                glTexGenfv(0x2000 + i, 0x2501, plane);
            }
            setMatrix = 1;
        }
    } else if (Tci == 0x30000) {
        if (count == 2) {
            memcpy(matrix, original, sizeof(matrix));
            matrix[2] = matrix[6] = matrix[10] = matrix[14] = 0.0f;
            matrix[3] = matrix[7] = matrix[11] = 0.0f;
            matrix[15] = 1.0f;
            setMatrix = 1;
        } else if (count == 3) {
            memcpy(combined, dev->convertedView, sizeof(combined));
            if (projected) combined[2] = combined[6] = combined[10] = combined[14] = 0.0f;
            for (UINT32 row = 0; row < 4; ++row)
                for (UINT32 column = 0; column < 4; ++column)
                    matrix[row * 4 + column] = combined[column * 4 + row];
            matrix[3] = matrix[7] = matrix[11] = 0.0f;
            matrix[12] = matrix[13] = matrix[14] = 0.0f;
            matrix[15] = 1.0f;
            setMatrix = 1;
        } else if (count != 0) {
            *(Bool *)imp__ZN7COpenGL9sDrawFlagE = 0;
        }
    } else {
        *(Bool *)imp__ZN7COpenGL9sDrawFlagE = 0;
    }
    if (bindArray && CVAOPacket_sVAOStatus == USING_GENERIC_VAO) {
        COpenGLVAONative *packet = state->genericPacket;
        if (packet)
            CTexCoordArray_SetGuardFull((const void *)&packet->mTexCoordArrays[TexUnit],
                                        1, 0x1406, SrcStride, pSrcCoords, VSize);
        COpenGL_EnableTexCoordArray(gl, TexUnit, VSize, 0x1406, SrcStride, (const void *)pSrcCoords);
    }
    if (setMatrix) return COpenGL_SetTexMatrix(gl, TexUnit, matrix);
    return COpenGL_SetTexMatrixIdentity(gl, TexUnit);
#else
    (void)_this;
    (void)TexUnit;
    (void)TransformFlags;
    (void)OriginalTexTransform;
    (void)VSize;
    (void)Tci;
    (void)StartVertex;
    (void)EndVertex;
    (void)pSrcCoords;
    (void)SrcStride;
    (void)pSrc2Coords;
    (void)Src2Stride;
    return 0;
#endif
}

extern unsigned char *COpenGL_TexUnitBase(const COpenGL *_this, UINT32 Unit);

#define GL_TEXTURE_ENV_ 0x2300

#define CMB_COMBINE_RGB 0x8571
#define CMB_RGB_SCALE 0x8573
#define CMB_SRC0_RGB 0x8580
#define CMB_SRC1_RGB 0x8581
#define CMB_SRC2_RGB 0x8582
#define CMB_OP0_RGB 0x8590
#define CMB_OP1_RGB 0x8591
#define CMB_OP2_RGB 0x8592

#define CMB_COMBINE_ALPHA 0x8572
#define CMB_ALPHA_SCALE 0x0D1C
#define CMB_SRC0_ALPHA 0x8588
#define CMB_SRC1_ALPHA 0x8589
#define CMB_SRC2_ALPHA 0x858A
#define CMB_OP0_ALPHA 0x8598
#define CMB_OP1_ALPHA 0x8599
#define CMB_OP2_ALPHA 0x859A

#if defined(COD2_X64)
#define CMB_OFFSET(field, legacyOffset) offsetof(CTexUnitNative, field)
#else
#define CMB_OFFSET(field, legacyOffset) legacyOffset
#endif

static inline void Cmb_SetI(const COpenGL *gl, UINT32 Unit, unsigned char *u,
                            int off, unsigned int glEnum, int val)
{
    if (*(int *)(u + off) != val) {
        COpenGL_SetActiveTexUnit(gl, Unit);
        glTexEnvi(GL_TEXTURE_ENV_, glEnum, val);
        *(int *)(u + off) = val;
    }
}
static inline void Cmb_SetScale(const COpenGL *gl, UINT32 Unit, unsigned char *u,
                                int off, unsigned int glEnum, float scale)
{
#if defined(COD2_X64)
    GLfloat *cachedScale = (GLfloat *)(u + off);
    if (scale != *cachedScale) {
        COpenGL_SetActiveTexUnit(gl, Unit);
        glTexEnvf(GL_TEXTURE_ENV_, glEnum, scale);
        *cachedScale = scale;
    }
#else
    if (scale != (float)*(int *)(u + off)) {
        COpenGL_SetActiveTexUnit(gl, Unit);
        glTexEnvf(GL_TEXTURE_ENV_, glEnum, scale);
        *(int *)(u + off) = (int)scale;
    }
#endif
}

long unsigned int COpenGL_SetTexCombinerRGB1(const COpenGL *_this, UINT32 Unit,
                                             GLenum ColorOp, GLenum Source0, GLenum Operand0, GLfloat Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &ColorOp, &Source0, &Operand0, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerRGB1", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOp, 112), CMB_COMBINE_RGB, ColorOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerRGBScale, 168), CMB_RGB_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource0, 116), CMB_SRC0_RGB, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand0, 120), CMB_OP0_RGB, Operand0);
    return 0;
}

long unsigned int COpenGL_SetTexCombinerRGB2(const COpenGL *_this, UINT32 Unit,
                                             GLenum ColorOp, GLenum Source0, GLenum Operand0, GLenum Source1, GLenum Operand1, GLfloat Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &ColorOp, &Source0, &Operand0, &Source1, &Operand1, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerRGB2", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOp, 112), CMB_COMBINE_RGB, ColorOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerRGBScale, 168), CMB_RGB_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource1, 124), CMB_SRC1_RGB, Source1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand1, 128), CMB_OP1_RGB, Operand1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource0, 116), CMB_SRC0_RGB, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand0, 120), CMB_OP0_RGB, Operand0);
    return 0;
}

long unsigned int COpenGL_SetTexCombinerRGB3(const COpenGL *_this, UINT32 Unit,
                                             GLenum ColorOp, GLenum Source0, GLenum Operand0, GLenum Source1, GLenum Operand1,
                                             GLenum Source2, GLenum Operand2, GLfloat Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &ColorOp, &Source0, &Operand0, &Source1, &Operand1, &Source2, &Operand2, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerRGB3", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOp, 112), CMB_COMBINE_RGB, ColorOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerRGBScale, 168), CMB_RGB_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource2, 132), CMB_SRC2_RGB, Source2);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand2, 136), CMB_OP2_RGB, Operand2);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource1, 124), CMB_SRC1_RGB, Source1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand1, 128), CMB_OP1_RGB, Operand1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorSource0, 116), CMB_SRC0_RGB, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerColorOperand0, 120), CMB_OP0_RGB, Operand0);
    return 0;
}

long unsigned int COpenGL_SetTexCombinerAlpha1(const COpenGL *_this, UINT32 Unit,
                                               GLenum AlphaOp, GLenum Source0, GLenum Operand0, float Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &AlphaOp, &Source0, &Operand0, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerAlpha1", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOp, 140), CMB_COMBINE_ALPHA, AlphaOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerAlphaScale, 172), CMB_ALPHA_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource0, 144), CMB_SRC0_ALPHA, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand0, 148), CMB_OP0_ALPHA, Operand0);
    return 0;
}

long unsigned int COpenGL_SetTexCombinerAlpha2(const COpenGL *_this, UINT32 Unit,
                                               GLenum AlphaOp, GLenum Source0, GLenum Operand0, GLenum Source1, GLenum Operand1, float Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &AlphaOp, &Source0, &Operand0, &Source1, &Operand1, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerAlpha2", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOp, 140), CMB_COMBINE_ALPHA, AlphaOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerAlphaScale, 172), CMB_ALPHA_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource1, 152), CMB_SRC1_ALPHA, Source1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand1, 156), CMB_OP1_ALPHA, Operand1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource0, 144), CMB_SRC0_ALPHA, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand0, 148), CMB_OP0_ALPHA, Operand0);
    return 0;
}

long unsigned int COpenGL_SetTexCombinerAlpha3(const COpenGL *_this, UINT32 Unit,
                                               GLenum AlphaOp, GLenum Source0, GLenum Operand0, GLenum Source1, GLenum Operand1,
                                               GLenum Source2, GLenum Operand2, float Scale)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &AlphaOp, &Source0, &Operand0, &Source1, &Operand1, &Source2, &Operand2, &Scale };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexCombinerAlpha3", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *u = COpenGL_TexUnitBase(_this, Unit);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOp, 140), CMB_COMBINE_ALPHA, AlphaOp);
    Cmb_SetScale(_this, Unit, u, CMB_OFFSET(mCombinerAlphaScale, 172), CMB_ALPHA_SCALE, Scale);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource2, 160), CMB_SRC2_ALPHA, Source2);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand2, 164), CMB_OP2_ALPHA, Operand2);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource1, 152), CMB_SRC1_ALPHA, Source1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand1, 156), CMB_OP1_ALPHA, Operand1);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaSource0, 144), CMB_SRC0_ALPHA, Source0);
    Cmb_SetI(_this, Unit, u, CMB_OFFSET(mCombinerAlphaOperand0, 148), CMB_OP0_ALPHA, Operand0);
    return 0;
}

long unsigned int CDirect3DDevice_CTexStage_Init(const CTexStage *_this, UINT32 Stage, int IsProgrammableStage)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Stage, &IsProgrammableStage };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CTexStage_Init", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexStageNative *stage = (CTexStageNative *)_this;
    stage->programmableOnly = !!IsProgrammableStage;
    stage->texture = NULL;
    if (!IsProgrammableStage) {
        stage->colorOp = Stage == 0 ? 4 : 1;
        stage->colorArg0 = 3; stage->colorArg1 = 2; stage->colorArg2 = 1;
        stage->alphaOp = Stage == 0 ? 2 : 1;
        stage->alphaArg0 = 3; stage->alphaArg1 = 0; stage->alphaArg2 = 1;
        stage->transformFlags = 0; stage->texCoordIndex = Stage;
    }
    stage->addressU = stage->addressV = stage->addressW = 1;
    stage->borderColor = 0;
    stage->minFilter = stage->magFilter = 1;
    stage->mipFilter = 0; stage->lodBias = 0.0f;
    stage->anisotropy = 1.0f;
    for (UINT i = 0; i < 4; ++i)
        stage->factor[i] = 1.0f;
    CDirect3DDevice_LoadIdentityMatrix(stage->matrix);
    return 0;
#else

    (void)_this;
    (void)Stage;
    (void)IsProgrammableStage;
    return 0;
#endif
}

#if defined(COD2_X64)
Bool ZNSt11char_traitsIcE2eqERKcS2_(const char *left, const char *right)
{
    return *left == *right;
}
#else
void ZNSt11char_traitsIcE2eqERKcS2_(void) {}
#endif

void ZNSt6vectorIN15CDirect3DDevice9CTexStageESaIS1_EE5eraseEN9__gnu_cxx17__normal_iteratorIPS1_S3_EES7_(void) {}

void ZNSt6vectorIN15CDirect3DDevice9CTexStageESaIS1_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS1_S3_EERKS1_(void) {}

long unsigned int CDirect3DDevice_CDirect3DDevice(const CDirect3DDevice *_this,
                                                  UINT Adapter, D3DDEVTYPE DeviceType, ContextRef Context,
                                                  D3DPRESENT_PARAMETERS *pPresentationParameters)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &DeviceType, &Context, &pPresentationParameters };
    long unsigned int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DDevice_CDirect3DDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    memset(dev, 0, sizeof(*dev));
    dev->vtable = vtbl_CDirect3DDevice;
    dev->context = Context;
    dev->cardType = Adapter;
    dev->_pad010 = DeviceType;
    HRESULT result = CDirect3DDevice_Reset(_this, pPresentationParameters);
    dev->refCount = 1;
    return (unsigned long)result;
#else
    DeviceImpl *dev = (DeviceImpl *)_this;
    (void)Adapter;
    (void)DeviceType;
    (void)Context;
    (void)pPresentationParameters;

    dev->vtable = vtbl_CDirect3DDevice;
    dev->refCount = 1;

    return 0;
#endif
}

void CDirect3DDevice_Init(void *device)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &device };
    if (MacShim_Dispatch("CDirect3DDevice_Init", device, macArguments, NULL, 0))
        return;
#endif

    unsigned int stage;
    int videoMemory = 0;
    int textureMemory = 0;
    DeviceImpl *dev = (DeviceImpl *)device;
    dev->vtable = vtbl_CDirect3DDevice;
    dev->refCount = 1;
    CDirect3DDevice_LoadIdentityMatrix(dev->worldMatrix);
    CDirect3DDevice_LoadIdentityMatrix(dev->viewMatrix);
    CDirect3DDevice_LoadIdentityMatrix(dev->worldViewMatrix);
    CDirect3DDevice_LoadIdentityMatrix(dev->projMatrix);
    CDirect3DDevice_LoadIdentityMatrix(dev->texMatrix);
    MacDisplay_GetVideoMemoryInfo(&videoMemory, &textureMemory);
    dev->videoMemory = (UINT32)videoMemory;
    dev->textureMemory = (UINT32)textureMemory;
#if defined(COD2_X64)
    memset(dev->streams, 0, sizeof(dev->streams));
    memset(dev->streamOffsets, 0, sizeof(dev->streamOffsets));
    memset(dev->streamStrides, 0, sizeof(dev->streamStrides));
    dev->indexBuffer = NULL; dev->_pad0A8 = 0xffffffffu;
    dev->pixelShader = NULL; dev->bVal1 = dev->bVal3 = dev->bc8Val = 0;
    dev->fillMode = 3; dev->shadeMode = 2;
    dev->fogEnable = dev->stencilEnable = dev->stencilRef = 0;
    dev->stencilFunc = 8; dev->stencilMask = 0xffffffffu;
    dev->stencilZFail = dev->stencilFail = dev->stencilPass = 1;
    dev->sRGBWriteEnable = dev->depthBias = dev->colorWriteEnable1 = dev->colorWriteEnable2 = 0;
    dev->fogTableMode = dev->fogStart = dev->tweenFactor = 0;
    dev->fogEnd = dev->fogDensity = 0x3f800000u;
    memset(g_vsConst, 0, sizeof(g_vsConst));
#if COD2_APPLE_SDK
    memset(macPixelConstants, 0, sizeof(macPixelConstants));
#endif
    g_depthBias = g_slopeScaleDepthBias = 0.0f;
    g_prebind_texID = 0;
    dev->srcBlend = 2;
    dev->destBlend = 1;
    dev->srcBlendAlpha = 2;
    dev->destBlendAlpha = 1;
    dev->alphaBlendEnable = 0;
    dev->separateAlphaBlendEnable = 0;
    dev->alphaSrcBlend = 1;
    dev->isWorldViewDirty = 1;
    dev->isProjDirty = 1;
    CDirect3DDevice_LoadIdentityMatrix(dev->convertedView);
    memset(&dev->material, 0, sizeof(dev->material));
    memset(dev->nativeLights, 0, sizeof(dev->nativeLights));
    D3DLIGHT9 defaultLight = {0};
    defaultLight.Type = 3;
    for (UINT i = 0; i < 8; ++i)
        CDirect3DDevice_SetLight((const CDirect3DDevice *)device, i, &defaultLight);
    dev->lighting = 1; dev->colorVertex = 1; dev->hasDiffuseColor = 0;
    glGetIntegerv(0x0d33, (GLint *)&dev->maxTextureSize);
    glGetIntegerv(0x84e2, (GLint *)&dev->maxTextureUnits);
    glGetIntegerv(0x8872, (GLint *)&dev->maxTextureImageUnits);
    if (dev->maxTextureUnits > 16 || dev->maxTextureImageUnits > 16)
        abort();
    DeviceRenderState_Init(dev);
    free(dev->texStageData);
    CTexStageNative *stages = calloc(17, sizeof(*stages));
    if (!stages)
        abort();
    dev->texStageData = stages;
    UINT stageCount = dev->maxTextureUnits > dev->maxTextureImageUnits ? dev->maxTextureUnits : dev->maxTextureImageUnits;
    dev->texStageEnd = stages + stageCount;
    dev->texStageCap = stages + 17;
    for (UINT i = 0; i < 17; ++i)
        CDirect3DDevice_CTexStage_Init((const CTexStage *)&stages[i], i, i >= dev->maxTextureUnits);
    g_activeVertexShader = NULL;
    g_activeVertexDeclaration = NULL;
    g_currentFVF = 0;
    CDirect3DDevice_ResetStreamsNative((const CDirect3DDevice *)device);
    CDirect3DDevice_mNeedsVertexShaderValidation = 1;
    CDirect3DDevice_mNeedsTransformationValidation = 1;
    CDirect3DDevice_mNeedsRasterizationValidation = 1;
#else
    dev->srcBlend = 5;
    dev->destBlend = 6;
    dev->srcBlendAlpha = 5;
    dev->destBlendAlpha = 6;
#endif
    dev->alphaRef = 0.0f;
    dev->alphaFunc = 0x0207;
    dev->alphaRefVal = 0;
    dev->alphaFuncVal = 8;
    dev->viewportX = 0;
    dev->viewportY = 0;
    dev->viewportW = 640;
    dev->viewportH = 480;
    dev->viewportMinZ = 0.0f;
    dev->viewportMaxZ = 1.0f;
#if COD2_APPLE_SDK
    int renderWidth, renderHeight;
    MacPlatform_GetRenderSize(&renderWidth, &renderHeight);
    dev->viewportW = renderWidth;
    dev->viewportH = renderHeight;
    glViewport(0, 0, renderWidth, renderHeight);
#else
    glViewport(0, 0, 640, 480);
#endif
    glDepthRange(0.0, 1.0);
#if defined(COD2_X64)
    memset(&dev->viewportX, 0, sizeof(NativeViewport));
    dev->depthStencilDetached = 0;
    dev->stencilWriteMask = 0xffffffffu;
#endif
    dev->zEnable = 1;
    dev->zWriteEnable = 1;
    dev->cullMode = 1;
    dev->zFunc = 4;
    dev->colorWriteEnable = 0xF;
#if defined(COD2_X64)
    dev->fogColor = 0;
#else
    dev->fogColor = 0xFF000000u;
#endif
    dev->blendOp = 1;
    g_alphaTestEnable = 0;
    g_textureFactor = 0xFFFFFFFFu;
    g_prebind_texTarget = 0x0DE1;
    memset(g_textureStageState, 0, sizeof(g_textureStageState));
    memset(g_samplerState, 0, sizeof(g_samplerState));
    for (stage = 0; stage < 8; ++stage) {
        g_textureStageState[stage][D3DTSS_COLOROP] = (stage == 0) ? D3DTOP_MODULATE : D3DTOP_DISABLE;
        g_textureStageState[stage][D3DTSS_COLORARG1] = 2;
        g_textureStageState[stage][D3DTSS_COLORARG2] = 1;
#if defined(COD2_X64)
        g_textureStageState[stage][D3DTSS_COLORARG0] = 3;
        g_textureStageState[stage][D3DTSS_ALPHAOP] = (stage == 0) ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
        g_textureStageState[stage][D3DTSS_ALPHAARG0] = 3;
        g_textureStageState[stage][D3DTSS_ALPHAARG1] = 0;
#else
        g_textureStageState[stage][D3DTSS_ALPHAOP] = (stage == 0) ? D3DTOP_MODULATE : D3DTOP_DISABLE;
        g_textureStageState[stage][D3DTSS_ALPHAARG1] = 2;
#endif
        g_textureStageState[stage][D3DTSS_ALPHAARG2] = 1;
        g_textureStageState[stage][D3DTSS_TEXCOORDINDEX] = stage;
        g_textureStageState[stage][D3DTSS_TEXTURETRANSFORMFLAGS] = D3DTTFF_DISABLE;
        g_samplerState[stage][D3DSAMP_ADDRESSU] = D3DTADDRESS_WRAP;
        g_samplerState[stage][D3DSAMP_ADDRESSV] = D3DTADDRESS_WRAP;
        g_samplerState[stage][D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
#if defined(COD2_X64)
        g_samplerState[stage][D3DSAMP_MINFILTER] = D3DTEXF_POINT;
        g_samplerState[stage][D3DSAMP_MAGFILTER] = D3DTEXF_POINT;
        g_samplerState[stage][D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
#else
        g_samplerState[stage][D3DSAMP_MINFILTER] = D3DTEXF_LINEAR;
        g_samplerState[stage][D3DSAMP_MAGFILTER] = D3DTEXF_LINEAR;
        g_samplerState[stage][D3DSAMP_MIPFILTER] = D3DTEXF_LINEAR;
#endif
        g_samplerState[stage][D3DSAMP_MAXANISOTROPY] = 1;
    }
}

#if defined(COD2_X64)
static void CDirect3DDevice_ReleaseNativeObject(void *object)
{
    if (object) ((ULONG (*)(const void *))(*(void ***)object)[2])(object);
}

static void CDirect3DDevice_ReleaseNativeContents(DeviceImpl *dev)
{
    CDirect3DDevice_ReleaseNativeObject(dev->swapChain); dev->swapChain = NULL;
    CDirect3DDevice_ReleaseNativeObject(dev->backBuffer); dev->backBuffer = NULL;
    CDirect3DDevice_ReleaseNativeObject(dev->renderTarget); dev->renderTarget = NULL;
    CDirect3DDevice_ReleaseNativeObject(dev->depthStencil); dev->depthStencil = NULL;
    /* Default depth storage belongs to the display context. COM overrides and
     * the two backbuffer aliases each retain their own native reference. */
    CDirect3DDevice_ReleaseNativeObject(dev->depthBuffer); dev->depthBuffer = NULL;
    CDirect3DDevice_ReleaseNativeObject(dev->saveSurface); dev->saveSurface = NULL;
    CDirect3DDevice_ReleaseNativeObject(dev->saveSurface2); dev->saveSurface2 = NULL;
    dev->renderTarget = dev->depthStencil = NULL;
    for (UINT stage = 0; stage < sizeof(g_boundTextures) / sizeof(g_boundTextures[0]); ++stage) {
        if (CDirect3DDevice_IsRealTexture(g_boundTextures[stage]))
            CDirect3DDevice_ReleaseNativeObject(g_boundTextures[stage]);
        g_boundTextures[stage] = NULL;
    }
}

static HRESULT CDirect3DDevice_ResetNative(DeviceImpl *dev, const void *presentation)
{
    if (!presentation) return (HRESULT)(int32_t)0x8876086cu;
    NativePresentationParameters14 parameters;
    memcpy(&parameters, presentation, sizeof(parameters));
    /* The selected display advertises no multisample buffers. Reject callers
     * that bypass capability negotiation instead of silently dropping FSAA. */
    if (parameters.multiSampleType || parameters.multiSampleQuality)
        return (HRESULT)(int32_t)0x8876086au;
    int depthBits = NativePresentation_DepthBits(parameters.depthFormat);
    if (!depthBits) return (HRESULT)(int32_t)0x88760869u;
    if (!parameters.backBufferFormat) {
#if COD2_APPLE_SDK
        int width, height;
        MacPlatform_GetRenderSize(&width, &height);
        parameters.width = width; parameters.height = height;
#endif
        parameters.backBufferFormat = 0x16;
        memcpy((void *)presentation, &parameters, sizeof(parameters));
    }
#if COD2_APPLE_SDK
    if (!MacDisplay_MakeContextCurrent(dev->context)) return (HRESULT)(int32_t)0x88760869u;
#endif
    if (CDirect3DDevice_GL()->texUnitCount)
        COpenGL_Shutdown((const COpenGL *)CDirect3DDevice_GL());
    CDirect3DDevice_ReleaseNativeContents(dev);
#if COD2_APPLE_SDK
    extern dvar_t *Dvar_RegisterBool(const char *, int, int);
    dvar_t *borderless = Dvar_RegisterBool("r_borderless", 1, 0x1001);
    MacPlatform_ConfigureWindow(parameters.width, parameters.height,
        parameters.windowed ? MAC_WINDOWED : (borderless->current.enabled ? MAC_BORDERLESS : MAC_FULLSCREEN), parameters.refreshRate);
    if (MacDisplay_SetMode(parameters.width, parameters.height,
            parameters.backBufferFormat == 21 || parameters.backBufferFormat == 22 ? 32 : 16, parameters.refreshRate) ||
        MacDisplay_SetDepthStencilFormat(dev->context, depthBits, parameters.depthFormat == 75))
        return (HRESULT)(int32_t)0x88760869u;
    MacDisplay_SetPresentationInterval(dev->context, parameters.presentationInterval);
#endif
    UINT fixedUnits = MacDisplay_GetMaxTextureUnits(), imageUnits = MacDisplay_GetMaxTextureImageUnits();
    COpenGL_Init((const COpenGL *)CDirect3DDevice_GL(), 2, fixedUnits, imageUnits);
#if COD2_APPLE_SDK
    SDL_GL_SwapWindowDirect();
#endif
    ULONG referenceCount = dev->refCount;
    CDirect3DDevice_Init(dev);
    dev->refCount = referenceCount;
    void *backBuffer = calloc(1, 0x80);
    if (!backBuffer) return (HRESULT)(int32_t)0x8007000eu;
    CXX_BOOL previousNoTextureID = g_NoTextureID;
    /* Mac's default surface has no owner. SDL's FBO backing needs the native
     * surface's owned staging texture for scaled framebuffer copies. */
    g_NoTextureID = 0;
    CDirect3DSurface_CDirect3DSurface((const CDirect3DSurface *)backBuffer, 0, 0, 0,
        parameters.width, parameters.height, parameters.backBufferFormat, NULL, NULL);
    g_NoTextureID = previousNoTextureID;
    CDirect3DDevice_SetNativeBackBuffer(dev, backBuffer, dev->cardType);
    if (!dev->swapChain) return (HRESULT)(int32_t)0x8007000eu;
    /* Mac 0x19fc1 creates the default 1x1 texture, writes the exact 0x000000ff
     * word at 0x1a03b and binds it to every fixed/image unit. */
    IDirect3DTexture9 *fallback;
    CDirect3DDevice_CreateTexture((const CDirect3DDevice *)dev, 1, 1, 1, 0, 21, 1, &fallback, NULL);
    dev->saveSurface = (IDirect3DSurface9 *)fallback;
    D3DLOCKED_RECT locked;
    void **methods = *(void ***)fallback;
    ((HRESULT (*)(const void *, UINT, D3DLOCKED_RECT *, const RECT *, DWORD))methods[19])(fallback, 0, &locked, NULL, 0);
    *(uint32_t *)locked.pBits = 0xff;
    ((HRESULT (*)(const void *, UINT))methods[20])(fallback, 0);
    CDirect3DDevice_UpdateTextureIfNeeded((IDirect3DBaseTexture9 *)fallback);
    UINT limit = fixedUnits > imageUnits ? fixedUnits : imageUnits;
    for (UINT unit = 0; unit < limit; ++unit)
        COpenGL_SetTex((const COpenGL *)CDirect3DDevice_GL(), unit, 0xde1, (const COpenGLTexture *)((void **)fallback + 1));
    return 0;
}
#endif

void ZN15CDirect3DDeviceD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN15CDirect3DDeviceD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    DeviceImpl *dev = (DeviceImpl *)_this;
    dev->vtable = vtbl_CDirect3DDevice;
#if COD2_APPLE_SDK
    if (dev->context) MacDisplay_MakeContextCurrent(dev->context);
#endif
    g_activeVertexShader = NULL;
    g_activeVertexDeclaration = NULL;
    memset(dev->streams, 0, sizeof(dev->streams));
    dev->indexBuffer = NULL;
    CDirect3DDevice_ResetStreamsNative((const CDirect3DDevice *)dev);
    CDirect3DDevice_ReleaseNativeContents(dev);
    if (dev->context) {
        COpenGL_Shutdown((const COpenGL *)CDirect3DDevice_GL());
        MacDisplay_ReleaseContext(&dev->context);
    }
    free(dev->texStageData);
    dev->texStageData = dev->texStageEnd = dev->texStageCap = NULL;
    g_prebind_texID = 0;
    g_activeVertexShader = NULL;
    g_activeVertexDeclaration = NULL;
#else
    DeviceImpl *dev = (DeviceImpl *)_this;
    dev->vtable = vtbl_CDirect3DDevice;

#if defined(COD2_X64)
    /* Mac 0x1b003 releases the swapchain before the device's backbuffer
     * reference, while their GL context is still current. */
    if (dev->swapChain) {
        ((ULONG (*)(const void *))(*(void ***)dev->swapChain)[2])(dev->swapChain);
        dev->swapChain = NULL;
    }
    if (dev->backBuffer) {
        ((ULONG (*)(const void *))(*(void ***)dev->backBuffer)[2])(dev->backBuffer);
        dev->backBuffer = NULL;
    }
    dev->renderTarget = NULL;
    /* The native texture cache is global rather than an instance vector.
     * Reclaim its owned COM references before SDL destroys this context. */
    for (UINT stage = 0; stage < sizeof(g_boundTextures) / sizeof(g_boundTextures[0]); ++stage) {
        IDirect3DBaseTexture9 *texture = g_boundTextures[stage];
        if (CDirect3DDevice_IsRealTexture(texture))
            ((ULONG (*)(const void *))(*(void ***)texture)[2])(texture);
        g_boundTextures[stage] = NULL;
    }
    free(dev->texStageData);
    dev->texStageData = dev->texStageEnd = dev->texStageCap = NULL;
    g_prebind_texID = 0;
    g_activeVertexShader = NULL;
    g_activeVertexDeclaration = NULL;
#endif

    if (dev->context) {
#if defined(COD2_X64) && COD2_APPLE_SDK
        /* Mac 0x1b05f drains delayed storage before releasing this context. */
        if (MacDisplay_MakeContextCurrent(dev->context))
            CMemoryBuffer_Reset();
#endif
        MacDisplay_ReleaseContext(&dev->context);
    }
#endif
}

void ZN15CDirect3DDeviceD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN15CDirect3DDeviceD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN15CDirect3DDeviceD1Ev(_this);

}

void ZN15CDirect3DDeviceD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN15CDirect3DDeviceD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN15CDirect3DDeviceD1Ev(_this);
}

#if defined(COD2_X64)
extern int MacOpenGLUtils_GetDeclarationInfo(GLint *, GLenum *, UINT32 *, GLboolean *, UINT32);
extern int CDirect3DVertexDeclaration_GetShaderDeclaration(const CDirect3DVertexDeclaration *, const UINT32 *);

static void CDirect3DDevice_ForgetPixelShaderNative(DeviceImpl *dev, const void *shader)
{
    if (dev->streamState && dev->streamState->pixelShader == shader)
        dev->streamState->pixelShader = NULL;
}

static DeviceStreamsNative *CDirect3DDevice_StreamsNative(const CDirect3DDevice *_this)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (!dev->streamState) {
        dev->streamState = calloc(1, sizeof(*dev->streamState));
        if (!dev->streamState) abort();
        dev->streamState->tokens[0] = 0xffffffff;
    }
    return dev->streamState;
}

/* This owns only widened stream metadata. COM buffers/declarations remain
 * borrowed, as in the Mac setters; packet bindings belong to CVAOPacket. */
void CDirect3DDevice_ResetStreamsNative(const CDirect3DDevice *_this)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    if (dev->streamState)
        for (UINT32 i = 0; i < 16; ++i) {
            free(dev->streamState->convertedColors[i]);
            free(dev->streamState->extendedColors[i]);
        }
    free(dev->streamState);
    dev->streamState = NULL;
}

void CDirect3DDevice_SetStream(const CDirect3DDevice *_this, Bool *other,
                               UINT32 stream, UINT32 reg, UINT32 offset,
                               UINT32 stride, GLint size, GLenum type)
{
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    DeviceStreamNative *value;
    if (reg == 0) value = &state->position;
    else if (reg == 3) value = &state->normal;
    else if (reg == 5) value = &state->color;
    else {
        if (reg < 8 || reg >= 16) { *other = 1; return; }
        value = &state->texCoord[reg - 8];
        *other = 1;
    }
    value->used = 1;
    value->stream = stream;
    value->offset = offset;
    value->stride = stride;
    if (reg != 3) value->size = size;
    if (reg < 8) value->type = type;
}

void CDirect3DDevice_ParseShaderDeclaration(const CDirect3DDevice *_this,
                                            Bool fixedFunction, Bool *canUseVAO)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    UINT32 stream = 0xffffffff, offset = 0, stride = 0;
    if (!state->tokensSet && g_activeVertexDeclaration)
        CDirect3DVertexDeclaration_GetShaderDeclaration((const void *)g_activeVertexDeclaration, state->tokens);
    for (UINT32 i = 0; i < 1024; ++i) {
        UINT32 token = state->tokens[i];
        UINT32 operation = token >> 29;
        if (operation == 7) break;
        if (operation == 1) {
            stream = token & 15;
            offset = 0;
            stride = dev->streamStrides[stream];
        } else if (operation == 2) {
            if (token & 0x10000000) {
                offset += (token >> 16) & 15;
                continue;
            }
            UINT32 reg = token & 31, type = (token >> 16) & 15, componentSize = 0;
            GLint size = 0;
            GLenum glType = 0;
            GLboolean normalized = 0;
            Bool other = 0;
            /* The original declaration table has eight entries. Retain defined
             * native handling for malformed tokens instead of reading past it. */
            if (stream >= 16 || type >= 8 || reg >= 16) { *canUseVAO = 0; continue; }
            MacOpenGLUtils_GetDeclarationInfo(&size, &glType, &componentSize, &normalized, type);
            if (fixedFunction) {
                CDirect3DDevice_SetStream(_this, &other, stream, reg,
                                         dev->streamOffsets[stream] + offset, stride, size, glType);
                if (reg == 5) state->color.byteColor = type == 4;
            } else {
                DeviceStreamNative *value = &state->programmable[reg];
                value->used = 1;
                value->stream = stream;
                value->offset = dev->streamOffsets[stream] + offset;
                value->size = size;
                value->type = glType;
                value->normalized = normalized;
                value->stride = stride;
                value->byteColor = type == 4;
            }
            const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[stream];
            if (!other && buffer && (buffer->usage & 0x200)) *canUseVAO = 0;
            offset += size * componentSize;
        }
    }
}

static const float *CDirect3DDevice_StreamPointerNative(DeviceImpl *dev,
                                                       DeviceStreamNative *stream, UINT32 base)
{
    const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[stream->stream];
    return (const void *)(buffer->data + stream->offset + (intptr_t)(int32_t)base * stream->stride);
}

Bool CDirect3DDevice_GetStreamPtrs(const CDirect3DDevice *_this, UINT32 base)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    state->position.pointer = CDirect3DDevice_StreamPointerNative(dev, &state->position, base);
    Bool normals = 0;
    if (state->normal.used) {
        normals = dev->lighting != 0;
        if (!normals) {
            for (UINT32 i = 0; i < dev->maxTextureUnits; ++i) {
                CTexStageNative *stage = CDirect3DDevice_Stage(dev, i);
                if (!stage) break;
                UINT32 generation = stage->texCoordIndex & 0xffff0000;
                if (generation == 0x10000 || generation == 0x30000) { normals = 1; break; }
                if (stage->colorOp == 1) break;
            }
        }
        state->normal.pointer = normals ? CDirect3DDevice_StreamPointerNative(dev, &state->normal, base) : NULL;
    }
    state->color.pointer = state->color.used ? CDirect3DDevice_StreamPointerNative(dev, &state->color, base) : NULL;
    return normals;
}

Bool CDirect3DDevice_GetCanUseVAOWithTexCoordStreams(const CDirect3DDevice *_this, Bool canUseVAO)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    for (UINT32 i = 0; i < dev->maxTextureUnits; ++i) {
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, i);
        if (!stage || stage->colorOp == 1) break;
        Bool factor = 0;
        if (CDirect3DDevice_CTexStage_GetUsesTexture((const CTexStage *)stage, &factor)) {
            UINT32 tci = stage->texCoordIndex;
            UINT32 generation = tci & 0xffff0000;
            if (generation == 0x10000 || generation == 0x30000 || generation == 0x40000 ||
                (tci & 0xffff) >= 8 || !state->texCoord[tci & 0xffff].used)
                return 0;
        }
    }
    return canUseVAO;
}
#endif

#if defined(COD2_X64)
extern void CVAOPacket_CVAOPacket(const CVAOPacket *);
extern void CVAOPacket_SetVAO(const CVAOPacket *, int);
extern void CVAOPacket_SetGenericVAO(int, int);
extern void CVAOPacket_InitializeGenericVAO(void);
extern bool CVAOPacket_IsCached(CVAOPacket *);
extern void CVAOPacket_Cache(CVAOPacket *);
extern VAOStatus CVAOPacket_sVAOStatus;
extern void COpenGLVAO_CreateNewBinding(const COpenGLVAO *);
extern void ZN10CVAOPacketD2Ev(const CVAOPacket *);
extern void *imp___ZTV23CVAOPacketFixedFunction, *imp___ZTV22CVAOPacketProgrammable;
extern void CVertexArray_SetGuard(const CVertexArray *, int, GLenum, GLsizei, const void *, GLint);
extern void CNormalArray_SetGuard(const CNormalArray *, int, GLenum, GLsizei, const void *);
extern void CColorArray_SetGuard(const CColorArray *, int, GLenum, GLsizei, const void *, GLint);
extern void CTexCoordArray_SetGuardFull(const CTexCoordArray *, int, GLenum, GLsizei, const void *, GLint);
extern void CTexCoordArray_SetGuard(const CTexCoordArray *, int);
extern unsigned int COpenGL_SetVARInfo(const COpenGL *, const unsigned int *, UINT32);
extern unsigned int COpenGL_EnableVertexArray(const COpenGL *, GLint, GLenum, GLsizei, const unsigned int *);
extern unsigned int COpenGL_DisableNormalArray(const COpenGL *);
extern unsigned int COpenGL_EnableNormalArray(const COpenGL *, GLenum, GLsizei, const unsigned int *);
extern unsigned int COpenGL_DisableColorArray(const COpenGL *);
extern unsigned int COpenGL_EnableColorArray(const COpenGL *, GLint, GLenum, GLsizei, const unsigned int *);
extern unsigned int COpenGL_DisableTexCoordArray(const COpenGL *, UINT32);
extern unsigned int COpenGL_SetVertexProgramStreamInfo(const COpenGL *, UINT32, GLint, GLenum, int, GLsizei, const unsigned int *);
extern unsigned int COpenGL_DisableVertexProgramStream(const COpenGL *, UINT32);
extern void CStaticCacheInfo_Flush(const CStaticCacheInfo *, void (*)(), void (*)());

static int CDirect3DDevice_StreamColorOrderNative(DeviceImpl *dev, DeviceStreamNative *stream, Bool programmable)
{
    DeviceStreamsNative *state = dev->streamState;
    GLint positionSize = programmable ? state->programmable[0].size : state->position.size;
    return MacShader_ColorByteOrder(stream->stride, stream->offset - dev->streamOffsets[stream->stream], positionSize);
}

static const void *CDirect3DDevice_StreamColorNative(DeviceImpl *dev, DeviceStreamNative *stream,
                                                    const void *pointer, UINT32 reg, Bool programmable)
{
    DeviceStreamsNative *state = dev->streamState;
    if (!stream->byteColor || !state->elementCount) return pointer;
    int order = CDirect3DDevice_StreamColorOrderNative(dev, stream, programmable);
    if (order == COLOR_BYTES_RGBA) return pointer;
    UINT32 first = 0xffffffff, last = 0;
    for (UINT32 i = 0; i < state->elementCount; ++i) {
        UINT32 index = state->indices[i];
        if (index < first) first = index;
        if (index > last) last = index;
    }
    size_t bytes = ((size_t)last + 1) * 4;
    if (bytes > state->convertedColorBytes[reg]) {
        byte *allocation = realloc(state->convertedColors[reg], bytes);
        if (!allocation) abort();
        state->convertedColors[reg] = allocation;
        state->convertedColorBytes[reg] = bytes;
    }
    for (UINT32 i = first; i <= last; ++i) {
        const byte *src = (const byte *)pointer + i * stream->stride;
        byte *dst = state->convertedColors[reg] + i * 4;
        if (order == COLOR_BYTES_ARGB) {
            dst[0] = src[1]; dst[1] = src[2]; dst[2] = src[3]; dst[3] = src[0];
        } else {
            dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
        }
    }
    return state->convertedColors[reg];
}

static void CDirect3DDevice_PacketRangeNative(DeviceImpl *dev, COpenGLVAONative *packet,
                                              const CDirect3DVertexBufferClean *buffer, UINT32 count)
{
    DeviceStreamsNative *state = dev->streamState;
    packet->mpVAStart = buffer->data + dev->streamOffsets[0] + state->low * dev->streamStrides[0];
    packet->mSize = count * dev->streamStrides[0];
}

static void CDirect3DDevice_FlushRangeNative(DeviceImpl *dev, COpenGLVAONative *packet,
                                             const CDirect3DVertexBufferClean *buffer, Bool generic)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    COpenGL_SetVARInfo((const COpenGL *)gl, packet->mpVAStart, packet->mSize);
    if (gl->supportsVAR) {
        if (generic && *(const Bool *)imp_g_special) {
            CStaticCacheInfo_Flush((const void *)buffer->staticCache,
                (void (*)())packet->mpVAStart, (void (*)())((const byte *)packet->mpVAStart + packet->mSize));
        } else {
            glFlushVertexArrayRangeAPPLE(packet->mSize, packet->mpVAStart);
        }
    }
}

static void CDirect3DDevice_ProgramArraysNative(DeviceImpl *dev, COpenGLVAONative *packet,
                                                UINT32 base, Bool disableUnused)
{
    DeviceStreamsNative *state = dev->streamState;
    COpenGL *gl = (COpenGL *)CDirect3DDevice_GL();
    for (UINT32 i = 0; i < 16; ++i) {
        DeviceStreamNative *stream = &state->programmable[i];
        VertexProgramStreamStateNative *array = &packet->mGenericArrays[i];
        if (stream->used || disableUnused) {
            array->mEnabled = stream->used;
            array->mVSize = stream->size;
            array->mVType = stream->type;
            array->mNormalized = stream->normalized;
            array->mStride = stream->stride;
            array->mpStream = stream->used ? CDirect3DDevice_StreamPointerNative(dev, stream, base) : NULL;
        }
        if (stream->used) {
            const void *pointer = CDirect3DDevice_StreamColorNative(dev, stream, array->mpStream, i, 1);
            if (pointer != array->mpStream) { array->mpStream = pointer; array->mStride = 0; }
            COpenGL_SetVertexProgramStreamInfo(gl, i, stream->size, stream->type,
                stream->normalized, array->mStride, array->mpStream);
        } else if (disableUnused) {
            COpenGL_DisableVertexProgramStream(gl, i);
        }
    }
}

static void CDirect3DDevice_FixedArraysNative(DeviceImpl *dev, COpenGLVAONative *packet,
                                              Bool normals, Bool setGuards, Bool setGL, Bool unusedGuards, Bool disableUnused)
{
    DeviceStreamsNative *state = dev->streamState;
    COpenGL *gl = (COpenGL *)CDirect3DDevice_GL();
    DeviceStreamNative *position = &state->position, *normal = &state->normal, *color = &state->color;
    const void *colorPointer = color->used ? CDirect3DDevice_StreamColorNative(dev, color, color->pointer, 5, 0) : color->pointer;
    GLsizei colorStride = colorPointer == color->pointer ? color->stride : 0;
    if (setGuards) {
        CVertexArray_SetGuard((const void *)&packet->mVertexArray, 1,
            position->type, position->stride, position->pointer, position->size);
        if (normals || unusedGuards)
            CNormalArray_SetGuard((const void *)&packet->mNormalArray, normals,
                normal->type, normal->stride, normal->pointer);
        if (color->used || unusedGuards)
            CColorArray_SetGuard((const void *)&packet->mColorArray, color->used,
                color->type, colorStride, colorPointer, color->size);
    }
    if (setGL) {
        COpenGL_EnableVertexArray(gl, position->size, position->type, position->stride, (const void *)position->pointer);
        if (normals)
            COpenGL_EnableNormalArray(gl, normal->type, normal->stride, (const void *)normal->pointer);
        else if (disableUnused)
            COpenGL_DisableNormalArray(gl);
        if (color->used)
            COpenGL_EnableColorArray(gl, color->size, color->type, colorStride, colorPointer);
        else if (disableUnused)
            COpenGL_DisableColorArray(gl);
    }
}

void CDirect3DDevice_CreateAndSetGenericProgrammableVAO(const CDirect3DDevice *_this,
                                                       UINT32 base, UINT32 count, const void *indices, UINT32 elements)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    (void)indices; (void)elements;
    if (CVAOPacket_sVAOStatus == UNINITIALIZED_VAO_STATUS) CVAOPacket_InitializeGenericVAO();
    COpenGLVAONative *packet = CVAOPacket_GetCurrentGenericNative();
    state->genericPacket = packet;
    CVAOPacket_SetGenericVAO(0, 0);
    const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[0];
    CDirect3DDevice_PacketRangeNative(dev, packet, buffer, count);
    CDirect3DDevice_FlushRangeNative(dev, packet, buffer, 1);
    CDirect3DDevice_ProgramArraysNative(dev, packet, base, 1);
}

void CDirect3DDevice_CreateAndSetGenericFixedFunctionVAO(const CDirect3DDevice *_this,
                                                        UINT32 base, UINT32 count, const void *indices,
                                                        UINT32 elements, Bool normals)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    (void)base; (void)indices; (void)elements;
    if (CVAOPacket_sVAOStatus == UNINITIALIZED_VAO_STATUS) CVAOPacket_InitializeGenericVAO();
    COpenGLVAONative *packet = CVAOPacket_GetCurrentGenericNative();
    state->genericPacket = packet;
    const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[0];
    CVAOPacket_SetGenericVAO(1, 0);
    CDirect3DDevice_PacketRangeNative(dev, packet, buffer, count);
    CDirect3DDevice_FlushRangeNative(dev, packet, buffer, 1);
    CDirect3DDevice_FixedArraysNative(dev, packet, normals, 1, 1, 1, 1);
}

void CDirect3DDevice_CreateAndSetProgrammableVAO(const CDirect3DDevice *_this,
                                                UINT32 base, UINT32 count, const void *indices, UINT32 elements)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    COpenGLVAONative packet, virgin;
    const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[0];
    (void)indices; (void)elements;
    CVAOPacket_CVAOPacket((const void *)&packet);
    packet.vtable = (void **)((byte *)imp___ZTV22CVAOPacketProgrammable + 2 * sizeof(void *));
    CDirect3DDevice_PacketRangeNative(dev, &packet, buffer, count);
    if (CVAOPacket_IsCached((void *)&packet)) {
        CVAOPacket_SetVAO((const void *)&packet, 1);
        CDirect3DDevice_ProgramArraysNative(dev, &packet, base, 1);
        CVAOPacket_Cache((void *)&packet);
    } else {
        CVAOPacket_CVAOPacket((const void *)&virgin);
        virgin.vtable = packet.vtable;
        COpenGLVAO_CreateNewBinding((const void *)&virgin);
        CVAOPacket_SetVAO((const void *)&virgin, 0);
        virgin.mpVAStart = packet.mpVAStart;
        virgin.mSize = packet.mSize;
        CDirect3DDevice_FlushRangeNative(dev, &virgin, buffer, 0);
        CDirect3DDevice_ProgramArraysNative(dev, &virgin, base, 0);
        CVAOPacket_Cache((void *)&virgin);
        ZN10CVAOPacketD2Ev((const void *)&virgin);
    }
    ZN10CVAOPacketD2Ev((const void *)&packet);
}

static void CDirect3DDevice_FixedTexArraysNative(DeviceImpl *dev, COpenGLVAONative *packet,
                                                UINT32 base, Bool setGuards, Bool setGL)
{
    DeviceStreamsNative *state = dev->streamState;
    COpenGL *gl = (COpenGL *)CDirect3DDevice_GL();
    for (UINT32 i = 0; i < dev->maxTextureUnits; ++i) {
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, i);
        if (!stage || stage->colorOp == 1) break;
        Bool factor = 0;
        if (CDirect3DDevice_CTexStage_GetUsesTexture((const void *)stage, &factor)) {
            DeviceStreamNative *stream = (stage->texCoordIndex & 0xffff0000) == 0x20000
                ? &state->position : &state->texCoord[stage->texCoordIndex & 7];
            UINT32 stride = dev->streamStrides[stream->stream];
            const float *pointer = CDirect3DDevice_StreamPointerNative(dev, stream, base);
            GLint size = stream == &state->position ? 3 : stream->size;
            if (setGuards)
                CTexCoordArray_SetGuardFull((const void *)&packet->mTexCoordArrays[i], 1, 0x1406, stride, pointer, size);
            if (setGL)
                COpenGL_EnableTexCoordArray(gl, i, size, 0x1406, stride, (const void *)pointer);
        } else if (setGL) {
            COpenGL_DisableTexCoordArray(gl, i);
        }
    }
}

void CDirect3DDevice_CreateAndSetFixedFunctionVAO(const CDirect3DDevice *_this, Bool *canUseVAO,
                                                 UINT32 base, UINT32 count, const void *indices,
                                                 UINT32 elements, Bool normals)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = dev->streamState;
    COpenGLVAONative packet, virgin;
    const CDirect3DVertexBufferClean *buffer = (const void *)dev->streams[0];
    (void)indices; (void)elements;
    CVAOPacket_CVAOPacket((const void *)&packet);
    packet.vtable = (void **)((byte *)imp___ZTV23CVAOPacketFixedFunction + 2 * sizeof(void *));
    for (UINT32 i = 0; i < dev->maxTextureUnits; ++i) {
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, i);
        if (!stage || stage->colorOp == 1) break;
        Bool factor = 0;
        if (CDirect3DDevice_CTexStage_GetUsesTexture((const void *)stage, &factor) &&
            (stage->texCoordIndex & 0xffff0000) != 0x20000) {
            buffer = (const void *)dev->streams[state->texCoord[stage->texCoordIndex & 7].stream];
            if (buffer->usage & 0x200) {
                *canUseVAO = 0;
                ZN10CVAOPacketD2Ev((const void *)&packet);
                return;
            }
        }
    }
    CDirect3DDevice_PacketRangeNative(dev, &packet, buffer, count);
    if (CVAOPacket_IsCached((void *)&packet)) {
        CVAOPacket_SetVAO((const void *)&packet, 1);
        CDirect3DDevice_FixedArraysNative(dev, &packet, normals, 1, 1, 0, 1);
        CDirect3DDevice_FixedTexArraysNative(dev, &packet, base, 1, 1);
        CVAOPacket_Cache((void *)&packet);
    } else {
        CVAOPacket_CVAOPacket((const void *)&virgin);
        virgin.vtable = packet.vtable;
        COpenGLVAO_CreateNewBinding((const void *)&virgin);
        CVAOPacket_SetVAO((const void *)&virgin, 0);
        virgin.mpVAStart = packet.mpVAStart;
        virgin.mSize = packet.mSize;
        CDirect3DDevice_FixedArraysNative(dev, &virgin, normals, 1, 0, 0, 0);
        CDirect3DDevice_FixedTexArraysNative(dev, &virgin, base, 1, 0);
        CDirect3DDevice_FlushRangeNative(dev, &virgin, buffer, 0);
        CDirect3DDevice_FixedArraysNative(dev, &virgin, normals, 0, 1, 0, 0);
        CDirect3DDevice_FixedTexArraysNative(dev, &virgin, base, 0, 1);
        CVAOPacket_Cache((void *)&virgin);
        ZN10CVAOPacketD2Ev((const void *)&virgin);
    }
    ZN10CVAOPacketD2Ev((const void *)&packet);
}

void CDirect3DDevice_ValidateTransformation(const CDirect3DDevice *_this,
                                          UINT32 base, UINT32 count, const void *indices, UINT32 elements)
{
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    Bool canUseVAO = 1;
    state->indices = indices;
    state->elementCount = elements;
    if (g_activeVertexShader) {
        for (UINT32 i = 0; i < 16; ++i) state->programmable[i].used = 0;
        CDirect3DDevice_ParseShaderDeclaration(_this, 0, &canUseVAO);
        for (UINT32 i = 0; i < 16; ++i)
            if (state->programmable[i].used && state->programmable[i].byteColor &&
                CDirect3DDevice_StreamColorOrderNative((DeviceImpl *)_this, &state->programmable[i], 1) != COLOR_BYTES_RGBA)
                canUseVAO = 0;
        if (canUseVAO)
            CDirect3DDevice_CreateAndSetProgrammableVAO(_this, base, count, indices, elements);
        else
            CDirect3DDevice_CreateAndSetGenericProgrammableVAO(_this, base, count, indices, elements);
    } else {
        state->position.used = state->normal.used = state->color.used = 0;
        for (UINT32 i = 0; i < 8; ++i) state->texCoord[i].used = 0;
        CDirect3DDevice_ParseShaderDeclaration(_this, 1, &canUseVAO);
        Bool normals = CDirect3DDevice_GetStreamPtrs(_this, base);
        if (state->color.used && state->color.byteColor &&
            CDirect3DDevice_StreamColorOrderNative((DeviceImpl *)_this, &state->color, 0) != COLOR_BYTES_RGBA)
            canUseVAO = 0;
        if (canUseVAO) canUseVAO = CDirect3DDevice_GetCanUseVAOWithTexCoordStreams(_this, 1);
        if (canUseVAO)
            CDirect3DDevice_CreateAndSetFixedFunctionVAO(_this, &canUseVAO, base, count, indices, elements, normals);
        if (!canUseVAO)
            CDirect3DDevice_CreateAndSetGenericFixedFunctionVAO(_this, base, count, indices, elements, normals);
    }
}
#endif

#if defined(COD2_X64)
extern unsigned int COpenGL_SetTexGenEnable(const COpenGL *, UINT32, int, int, int);
extern unsigned int COpenGL_DisableTexUnit(const COpenGL *, UINT32);
extern int MacOpenGLUtils_GetOpenGLTextureOp(GLenum *, GLenum *, UINT32);
extern int MacOpenGLUtils_GetOpenGLTextureAlphaOp(GLenum *, GLenum *, UINT32);

static void CDirect3DDevice_CombinerNative(COpenGL *gl, UINT32 unit, CTexStageNative *stage)
{
    GLenum source[3], operand[3];
    MacOpenGLUtils_GetOpenGLTextureOp(&source[0], &operand[0], stage->colorArg1);
    MacOpenGLUtils_GetOpenGLTextureOp(&source[1], &operand[1], stage->colorArg2);
    switch (stage->colorOp) {
    case 2: COpenGL_SetTexCombinerRGB1(gl, unit, 0x1e01, source[0], operand[0], 1.0f); break;
    case 3: COpenGL_SetTexCombinerRGB1(gl, unit, 0x1e01, source[1], operand[1], 1.0f); break;
    case 4: case 5: case 6:
        COpenGL_SetTexCombinerRGB2(gl, unit, 0x2100, source[0], operand[0], source[1], operand[1],
                                  stage->colorOp == 4 ? 1.0f : stage->colorOp == 5 ? 2.0f : 4.0f); break;
    case 7: case 10:
        COpenGL_SetTexCombinerRGB2(gl, unit, stage->colorOp == 7 ? 0x104 : 0x84e7,
                                  source[0], operand[0], source[1], operand[1], 1.0f); break;
    case 12: case 13: case 14: case 16:
        COpenGL_SetTexCombinerRGB3(gl, unit, 0x8575, source[0], operand[0], source[1], operand[1],
            stage->colorOp == 12 ? 0x8577 : stage->colorOp == 13 ? 0x1702 : stage->colorOp == 14 ? 0x8576 : 0x8578,
            0x302, 1.0f); break;
    case 18:
        COpenGL_SetTexCombinerRGB3(gl, unit, 0x8744, source[0], (stage->colorArg1 & 0x10) ? 0x303 : 0x302,
            source[0], (stage->colorArg1 & 0x10) ? 0x301 : 0x300,
            source[1], (stage->colorArg2 & 0x10) ? 0x301 : 0x300, 1.0f); break;
    case 24:
        COpenGL_SetTexCombinerRGB2(gl, unit, 0x86af, source[0], operand[0], source[1], operand[1], 1.0f);
        return;
    case 25: case 26:
        MacOpenGLUtils_GetOpenGLTextureOp(&source[2], &operand[2], stage->colorArg0);
        COpenGL_SetTexCombinerRGB3(gl, unit, stage->colorOp == 25 ? 0x8744 : 0x8575,
            source[0], operand[0], source[1], operand[1], source[2], operand[2], 1.0f); break;
    default: break;
    }
    MacOpenGLUtils_GetOpenGLTextureAlphaOp(&source[0], &operand[0], stage->alphaArg1);
    MacOpenGLUtils_GetOpenGLTextureAlphaOp(&source[1], &operand[1], stage->alphaArg2);
    switch (stage->alphaOp) {
    case 1: COpenGL_SetTexCombinerAlpha1(gl, unit, 0x1e01, 0x8578, 0x302, 1.0f); break;
    case 2: COpenGL_SetTexCombinerAlpha1(gl, unit, 0x1e01, source[0], operand[0], 1.0f); break;
    case 3: COpenGL_SetTexCombinerAlpha1(gl, unit, 0x1e01, source[1], operand[1], 1.0f); break;
    case 4: case 5: case 7:
        COpenGL_SetTexCombinerAlpha2(gl, unit, stage->alphaOp == 7 ? 0x104 : 0x2100,
            source[0], operand[0], source[1], operand[1], stage->alphaOp == 5 ? 2.0f : 1.0f); break;
    case 12:
        COpenGL_SetTexCombinerAlpha3(gl, unit, 0x8575, source[0], operand[0], source[1], operand[1], 0x8577, 0x302, 1.0f); break;
    default: break;
    }
}

void CDirect3DDevice_ValidateRasterization(const CDirect3DDevice *_this, UINT32 base, UINT32 end)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    COpenGLNative *gl = CDirect3DDevice_GL();
    /* The real native shader providers also bind outside this cache; their
     * Enable methods query the GL context. Keep that ownership contract. */
    if (dev->pixelShader) {
        void **vtable = *(void ***)dev->pixelShader;
        ((void (*)(const void *))vtable[7])(dev->pixelShader);
    } else if (state->pixelShader) {
        void **vtable = *(void ***)state->pixelShader;
        ((void (*)(const void *))vtable[8])(state->pixelShader);
    }
    state->pixelShader = dev->pixelShader;
    typedef struct { void **vtable; ULONG references; UINT32 textureMask; } NativePixelPrefix;
    UINT32 textureMask = dev->pixelShader && *(void ***)dev->pixelShader != ps_stub_vtbl
        ? ((const NativePixelPrefix *)dev->pixelShader)->textureMask : 0;
    UINT32 units = dev->maxTextureUnits > dev->maxTextureImageUnits ? dev->maxTextureUnits : dev->maxTextureImageUnits;
    UINT32 i;
    for (i = 0; i < units; ++i) {
        CTexStageNative *stage = CDirect3DDevice_Stage(dev, i);
        if (!stage || (!dev->pixelShader && stage->colorOp == 1)) break;
        Bool usesFactor = 0;
        Bool usesTexture = dev->pixelShader ? !!(textureMask & (1u << i))
            : CDirect3DDevice_CTexStage_GetUsesTexture((const void *)stage, &usesFactor);
        if (!g_activeVertexShader) {
            Bool enableGen = 0, reflection = 0, normalMap = 0, arraySet = 0;
            if (usesTexture) {
                UINT32 tci = stage->texCoordIndex & 0xffff0000;
                const float *source = NULL, *source2 = NULL;
                UINT32 stride = 0, stride2 = 0;
                GLint size = 0;
                if (tci == 0) {
                    UINT32 index = stage->texCoordIndex & 0xffff;
                    if (index < 8 && state->texCoord[index].used) {
                        DeviceStreamNative *stream = &state->texCoord[index];
                        source = CDirect3DDevice_StreamPointerNative(dev, stream, base);
                        stride = dev->streamStrides[stream->stream];
                        size = stream->size;
                        arraySet = 1;
                    } else {
                        COpenGL_SetActiveTexUnit((const void *)gl, i);
                        if (gl->matrixMode != 0x1702) { gl->matrixMode = 0x1702; glMatrixMode(0x1702); }
                        glPushMatrix(); glLoadIdentity(); glTexCoord2f(0.0f, 0.0f); glPopMatrix();
                    }
                } else if (tci == 0x20000) {
                    source = state->position.pointer; stride = state->position.stride; arraySet = 1;
                } else if (tci == 0x30000) {
                    source = state->position.pointer; stride = state->position.stride;
                    source2 = state->normal.pointer; stride2 = state->normal.stride;
                    enableGen = reflection = 1;
                } else if (tci == 0x10000) {
                    source = state->normal.pointer; stride = state->normal.stride;
                    if (MacDisplay_GetCardType() == 2) arraySet = 1;
                    else enableGen = normalMap = 1;
                }
                if (tci || arraySet)
                    CDirect3DDevice_SetTextureCoordStream(_this, i, stage->transformFlags,
                        (const D3DMATRIX *)stage->matrix, size, tci, base, end, source, stride, source2, stride2);
            }
            if (!arraySet && CVAOPacket_sVAOStatus != USING_CACHED_VAO) {
                if (state->genericPacket)
                    CTexCoordArray_SetGuard((const void *)&state->genericPacket->mTexCoordArrays[i], 0);
                COpenGL_DisableTexCoordArray((const void *)gl, i);
            }
            COpenGL_SetTexGenEnable((const void *)gl, i, enableGen, reflection, normalMap);
        }
        if (!stage->programmableOnly && !dev->pixelShader) {
            CDirect3DDevice_CombinerNative((void *)gl, i, stage);
            CTexUnitNative *unit = &gl->texUnits[i];
            if (usesFactor && memcmp(unit->mTexFactor, stage->factor, sizeof(stage->factor))) {
                COpenGL_SetActiveTexUnit((const void *)gl, i);
                glTexEnvfv(0x2300, 0x2201, stage->factor);
                memcpy(unit->mTexFactor, stage->factor, sizeof(stage->factor));
            }
        }
        if (stage->texture) {
            if (usesTexture)
                CDirect3DDevice_SynchronizeD3DAndOpenGLTextureState(_this, stage->texture, i);
        } else {
            const void *defaultTexture = dev->saveSurface ? (const void *)((void **)dev->saveSurface + 1) : NULL;
            COpenGL_SetTex((const void *)gl, i, 0xde1, defaultTexture);
            if (!dev->pixelShader && usesTexture && !stage->programmableOnly) {
                GLenum source, operand;
                MacOpenGLUtils_GetOpenGLTextureOp(&source, &operand, stage->colorArg1);
                if (source == 0x1702) {
                    COpenGL_SetTex((const void *)gl, i, 0xde1, defaultTexture);
                    COpenGL_SetTexCombinerRGB1((const void *)gl, i, 0x1e01, 0x8578, 0x300, 1.0f);
                    COpenGL_SetTexCombinerAlpha1((const void *)gl, i, 0x1e01, 0x8578, 0x302, 1.0f);
                }
            }
        }
    }
    if (dev->pixelShader) return;
    for (; i < dev->maxTextureUnits; ++i) {
        if (CVAOPacket_sVAOStatus != USING_CACHED_VAO) {
            if (CVAOPacket_sVAOStatus == USING_GENERIC_VAO && state->genericPacket)
                CTexCoordArray_SetGuard((const void *)&state->genericPacket->mTexCoordArrays[i], 0);
            COpenGL_DisableTexCoordArray((const void *)gl, i);
        }
        COpenGL_DisableTexUnit((const void *)gl, i);
    }
}
#endif

fnptr_t vtbl_CDirect3DDevice[] = { (fnptr_t)CDirect3DDevice_QueryInterface, (fnptr_t)CDirect3DDevice_AddRef, (fnptr_t)CDirect3DDevice_Release, (fnptr_t)CDirect3DDevice_TestCooperativeLevel, (fnptr_t)CDirect3DDevice_GetAvailableTextureMem, (fnptr_t)CDirect3DDevice_EvictManagedResources, (fnptr_t)CDirect3DDevice_GetDirect3D, (fnptr_t)CDirect3DDevice_GetDeviceCaps, (fnptr_t)CDirect3DDevice_GetDisplayMode, (fnptr_t)CDirect3DDevice_GetCreationParameters, (fnptr_t)CDirect3DDevice_SetCursorProperties, (fnptr_t)CDirect3DDevice_SetCursorPosition, (fnptr_t)CDirect3DDevice_ShowCursor, (fnptr_t)CDirect3DDevice_CreateAdditionalSwapChain, (fnptr_t)CDirect3DDevice_GetSwapChain, (fnptr_t)CDirect3DDevice_GetNumberOfSwapChains, (fnptr_t)CDirect3DDevice_Reset, (fnptr_t)CDirect3DDevice_Present, (fnptr_t)CDirect3DDevice_GetBackBuffer, (fnptr_t)CDirect3DDevice_GetRasterStatus, (fnptr_t)CDirect3DDevice_SetDialogBoxMode, (fnptr_t)CDirect3DDevice_SetGammaRamp, (fnptr_t)CDirect3DDevice_GetGammaRamp, (fnptr_t)CDirect3DDevice_CreateTexture, (fnptr_t)CDirect3DDevice_CreateVolumeTexture, (fnptr_t)CDirect3DDevice_CreateCubeTexture, (fnptr_t)CDirect3DDevice_CreateVertexBuffer, (fnptr_t)CDirect3DDevice_CreateIndexBuffer, (fnptr_t)CDirect3DDevice_CreateRenderTarget, (fnptr_t)CDirect3DDevice_CreateDepthStencilSurface, (fnptr_t)CDirect3DDevice_UpdateSurface, (fnptr_t)CDirect3DDevice_UpdateTexture, (fnptr_t)CDirect3DDevice_GetRenderTargetData, (fnptr_t)CDirect3DDevice_GetFrontBufferData, (fnptr_t)CDirect3DDevice_StretchRect, (fnptr_t)CDirect3DDevice_ColorFill, (fnptr_t)CDirect3DDevice_CreateOffscreenPlainSurface, (fnptr_t)CDirect3DDevice_SetRenderTarget, (fnptr_t)CDirect3DDevice_GetRenderTarget, (fnptr_t)CDirect3DDevice_SetDepthStencilSurface, (fnptr_t)CDirect3DDevice_GetDepthStencilSurface, (fnptr_t)CDirect3DDevice_BeginScene, (fnptr_t)CDirect3DDevice_EndScene, (fnptr_t)CDirect3DDevice_Clear, (fnptr_t)CDirect3DDevice_SetTransform, (fnptr_t)CDirect3DDevice_GetTransform, (fnptr_t)CDirect3DDevice_MultiplyTransform, (fnptr_t)CDirect3DDevice_SetViewport, (fnptr_t)CDirect3DDevice_GetViewport, (fnptr_t)CDirect3DDevice_SetMaterial, (fnptr_t)CDirect3DDevice_GetMaterial, (fnptr_t)CDirect3DDevice_SetLight, (fnptr_t)CDirect3DDevice_GetLight, (fnptr_t)CDirect3DDevice_LightEnable, (fnptr_t)CDirect3DDevice_GetLightEnable, (fnptr_t)CDirect3DDevice_SetClipPlane, (fnptr_t)CDirect3DDevice_GetClipPlane, (fnptr_t)CDirect3DDevice_SetRenderState, (fnptr_t)CDirect3DDevice_GetRenderState, (fnptr_t)CDirect3DDevice_CreateStateBlock, (fnptr_t)CDirect3DDevice_BeginStateBlock, (fnptr_t)CDirect3DDevice_EndStateBlock, (fnptr_t)CDirect3DDevice_SetClipStatus, (fnptr_t)CDirect3DDevice_GetClipStatus, (fnptr_t)CDirect3DDevice_GetTexture, (fnptr_t)CDirect3DDevice_SetTexture, (fnptr_t)CDirect3DDevice_GetTextureStageState, (fnptr_t)CDirect3DDevice_SetTextureStageState, (fnptr_t)CDirect3DDevice_GetSamplerState, (fnptr_t)CDirect3DDevice_SetSamplerState, (fnptr_t)CDirect3DDevice_ValidateDevice, (fnptr_t)CDirect3DDevice_SetPaletteEntries, (fnptr_t)CDirect3DDevice_GetPaletteEntries, (fnptr_t)CDirect3DDevice_SetCurrentTexturePalette, (fnptr_t)CDirect3DDevice_GetCurrentTexturePalette, (fnptr_t)CDirect3DDevice_SetScissorRect, (fnptr_t)CDirect3DDevice_GetScissorRect, (fnptr_t)CDirect3DDevice_SetSoftwareVertexProcessing, (fnptr_t)CDirect3DDevice_GetSoftwareVertexProcessing, (fnptr_t)CDirect3DDevice_SetNPatchMode, (fnptr_t)CDirect3DDevice_GetNPatchMode, (fnptr_t)CDirect3DDevice_DrawPrimitive, (fnptr_t)CDirect3DDevice_DrawIndexedPrimitive, (fnptr_t)CDirect3DDevice_DrawPrimitiveUP, (fnptr_t)CDirect3DDevice_DrawIndexedPrimitiveUP, (fnptr_t)CDirect3DDevice_ProcessVertices, (fnptr_t)CDirect3DDevice_CreateVertexDeclaration, (fnptr_t)CDirect3DDevice_SetVertexDeclaration, (fnptr_t)CDirect3DDevice_GetVertexDeclaration, (fnptr_t)CDirect3DDevice_SetFVF, (fnptr_t)CDirect3DDevice_GetFVF, (fnptr_t)CDirect3DDevice_CreateVertexShader, (fnptr_t)CDirect3DDevice_SetVertexShader, (fnptr_t)CDirect3DDevice_GetVertexShader, (fnptr_t)CDirect3DDevice_SetVertexShaderConstantF, (fnptr_t)CDirect3DDevice_GetVertexShaderConstantF, (fnptr_t)CDirect3DDevice_SetVertexShaderConstantI, (fnptr_t)CDirect3DDevice_GetVertexShaderConstantI, (fnptr_t)CDirect3DDevice_SetVertexShaderConstantB, (fnptr_t)CDirect3DDevice_GetVertexShaderConstantB, (fnptr_t)CDirect3DDevice_SetStreamSource, (fnptr_t)CDirect3DDevice_GetStreamSource, (fnptr_t)CDirect3DDevice_SetStreamSourceFreq, (fnptr_t)CDirect3DDevice_GetStreamSourceFreq, (fnptr_t)CDirect3DDevice_SetIndices, (fnptr_t)CDirect3DDevice_GetIndices, (fnptr_t)CDirect3DDevice_CreatePixelShader, (fnptr_t)CDirect3DDevice_SetPixelShader, (fnptr_t)CDirect3DDevice_GetPixelShader, (fnptr_t)CDirect3DDevice_SetPixelShaderConstantF, (fnptr_t)CDirect3DDevice_GetPixelShaderConstantF, (fnptr_t)CDirect3DDevice_SetPixelShaderConstantI, (fnptr_t)CDirect3DDevice_GetPixelShaderConstantI, (fnptr_t)CDirect3DDevice_SetPixelShaderConstantB, (fnptr_t)CDirect3DDevice_GetPixelShaderConstantB, (fnptr_t)CDirect3DDevice_DrawRectPatch, (fnptr_t)CDirect3DDevice_DrawTriPatch, (fnptr_t)CDirect3DDevice_DeletePatch, (fnptr_t)CDirect3DDevice_CreateQuery, (fnptr_t)ZN15CDirect3DDeviceD1Ev, (fnptr_t)ZN15CDirect3DDeviceD0Ev, (fnptr_t)0 };

const unsigned char __ZZN15CDirect3DDeviceC4Ej11_D3DDEVTYPEP16OpaqueContextRefP23_D3DPRESENT_PARAMETERS_E5C_232[4320] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x77, 0x00, 0x02, 0x00, 0xea, 0x00, 0x02, 0x00, 0xf4, 0x00, 0x02, 0x00, 0x01, 0x01, 0x02, 0x00,
    0x0e, 0x01, 0x02, 0x00, 0x1b, 0x01, 0x02, 0x00, 0x28, 0x01, 0x02, 0x00, 0x35, 0x01, 0x02, 0x00,
    0x94, 0x00, 0x02, 0x00, 0x42, 0x01, 0x02, 0x00, 0x4c, 0x01, 0x02, 0x00, 0x53, 0x01, 0x02, 0x00,
    0x5d, 0x01, 0x02, 0x00, 0x67, 0x01, 0x02, 0x00, 0x73, 0x01, 0x02, 0x00, 0x8f, 0x00, 0x02, 0x00,
    0x8f, 0x00, 0x02, 0x00, 0x94, 0x00, 0x02, 0x00, 0x7d, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#if defined(COD2_X64)
#undef glEnable
#undef glDisable
#undef glActiveTextureARB
#undef glBindTexture
#undef glMatrixMode
#undef glLoadMatrixf
#undef glLoadIdentity
#endif
