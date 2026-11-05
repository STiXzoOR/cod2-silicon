#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>
#if COD2_APPLE_SDK
#include "platform/macos_display.h"
extern dvar_t *Dvar_RegisterBool(const char *, int, int);
extern dvar_t *r_fullscreen;
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct __attribute__((packed)) _d32_g_CurrentGenericPacket {
    unsigned char f0[12];
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

struct _d32_g_CurrentGenericPacket __attribute__((aligned(4))) g_CurrentGenericPacket = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3D[];
extern void *vtbl_CDirect3DDevice[];

void CDirect3DDevice_Init(void *device);

/* Correct prototype must be in scope: on x64 (LLP64) an implicit int declaration
 * (MSVC C4013) truncates the returned 64-bit context pointer to 32 bits and
 * corrupts it. Matches CDirect3DDevice.c:105. */
extern void *MacDisplay_CreateScreenContext(int depth, int windowed, int stencil,
                                            int multiSample, int fsaa, int *hasAux);

extern void CDirect3DSurface_CDirect3DSurface(const void *_this,
                                              int s, unsigned int CubemapID, unsigned int Level,
                                              unsigned int Width, unsigned int Height, int Format,
                                              const void *pSurfaceMemory, void *pOpenGLTextureInfo);

typedef struct {
    void **vtable;
    ULONG refCount;
} CDirect3DImpl;

__attribute__((used, aligned(4))) static UInt32 D3DToOpenGLPrimitive[13] = {
    0x00000000,
    0x00000000,
    0x00000001,
    0x00000003,
    0x00000004,
    0x00000005,
    0x00000006,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
};
static CDirect3DImpl sDirect3DInterface;
#if defined(COD2_X64)
static int sDirect3DInterfaceActive;
ULONG CDirect3D_AddRef(const void *device);
extern int MacDisplay_GetNumModes(void);
extern uint16_t MacDisplay_GetNthMode(int, int *, int *, int *, int *);
extern uint16_t MacDisplay_GetCurrentMode(int *, int *, int *, int *);
extern int MacDisplay_GetCurrentDepth(void);
extern uint16_t MacDisplay_GetAntiAliasingMultiSampleInfo(int *, int *, unsigned char *, unsigned char *, unsigned char *);
extern int MacDisplay_GetSupportsSeparateBlendFunc(void);
extern int MacDisplay_GetSupportsAnisotropicFiltering(void);
extern float MacDisplay_GetMaxSupportedAnisotropy(void);
extern int MacDisplay_GetMaxTextureImageUnits(void);
extern int MacDisplay_GetMaxTextureUnits(void);
extern UINT32 MacOpenGLUtils_GetPCPixelShaderVersion(void);

void *CDirect3D_GetDirect3DInterface(void)
{
    if (sDirect3DInterfaceActive) {
        CDirect3D_AddRef(&sDirect3DInterface);
    } else {
        sDirect3DInterface.vtable = (void **)vtbl_CDirect3D;
        sDirect3DInterface.refCount = 1;
        sDirect3DInterfaceActive = 1;
    }
    return &sDirect3DInterface;
}
#endif

HRESULT CDirect3D_QueryInterface(const void *_this, const IID *iid, void **ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;
    *ppvObj = (void *)_this;
#if defined(COD2_X64)
    ((ULONG (*)(const void *))(*(void ***)_this)[1])(_this);
#endif
    return 0;
}

ULONG CDirect3D_AddRef(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return ++((CDirect3DImpl *)_this)->refCount;
#else
    (void)_this;
    return ++sDirect3DInterface.refCount;
#endif
}
ULONG CDirect3D_Release(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DImpl *direct3D = (CDirect3DImpl *)_this;
    ULONG count = --direct3D->refCount;
    if (!count)
        ((void (*)(const void *))direct3D->vtable[18])(_this);
    return count;
#else
    (void)_this;
    return --sDirect3DInterface.refCount;
#endif
}

HRESULT CDirect3D_RegisterSoftwareDevice(const void *_this, void *pInitializeFunction)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pInitializeFunction };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_RegisterSoftwareDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pInitializeFunction;
    return 0;
}

UINT CDirect3D_GetAdapterCount(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    UINT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterCount", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 1;
}

HRESULT CDirect3D_GetAdapterIdentifier(const void *_this, UINT Adapter, DWORD Flags, void *pIdentifier)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &Flags, &pIdentifier };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterIdentifier", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Adapter;
    (void)Flags;
#if defined(COD2_X64)
    memset(pIdentifier, 0, sizeof(D3DADAPTER_IDENTIFIER9));
#else
    memset(pIdentifier, 0, 1024);
#endif
    return 0;
}

UINT CDirect3D_GetAdapterModeCount(const void *_this, UINT Adapter, D3DFORMAT Format)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &Format };
    UINT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterModeCount", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Adapter;
#if defined(COD2_X64)
    return Format == 22 ? MacDisplay_GetNumModes() : 0;
#else
    (void)Format;
    return 1;
#endif
}

HRESULT CDirect3D_EnumAdapterModes(const void *_this, UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &Format, &Mode, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_EnumAdapterModes", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif


#if defined(COD2_X64)
    int width = 0, height = 0, depth = 0, refresh = 0;
    int allow32 = Format == 21 || Format == 22 || Format == 35;
    int allow16 = Format == 23 || Format == 24 || Format == 25;
    memset(pMode, 0, sizeof(*pMode));
    if (Mode >= CDirect3D_GetAdapterModeCount(_this, Adapter, Format))
        return (HRESULT)(int32_t)0x8876086cu;
    MacDisplay_GetNthMode((int)Mode, &width, &height, &depth, &refresh);
    if ((depth == 32 && allow32) || (depth == 16 && allow16)) {
        pMode->Width = width;
        pMode->Height = height;
        pMode->RefreshRate = refresh;
        pMode->Format = depth == 32 ? 22 : 25;
    }
    return 0;
#else
    int *p = (int *)pMode;
    (void)_this;
    (void)Adapter;
    (void)Format;
    (void)Mode;
    p[0] = 1024;
    p[1] = 768;
    p[2] = 60;
    p[3] = 22;
    return 0;
#endif
}

HRESULT CDirect3D_GetAdapterDisplayMode(const void *_this, UINT Adapter, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterDisplayMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    int width, height, depth, refresh;
    (void)_this;
    if (Adapter)
        return (HRESULT)(int32_t)0x8876086cu;
    MacDisplay_GetCurrentMode(&width, &height, &depth, &refresh);
    pMode->Width = width;
    pMode->Height = height;
    pMode->RefreshRate = refresh;
    pMode->Format = depth == 16 ? 25 : 21;
    return 0;
#else
    int *p = (int *)pMode;
    (void)_this;
    (void)Adapter;
    p[0] = 1024;
    p[1] = 768;
    p[2] = 60;
    p[3] = 22;
    return 0;
#endif
}

HRESULT CDirect3D_CheckDeviceType(const void *t, UINT a, int dt, D3DFORMAT ab, D3DFORMAT bb, BOOL w)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &ab, &bb, &w };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDeviceType", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)a;
    (void)dt;
    (void)ab;
    (void)bb;
    (void)w;
    return 0;
}
HRESULT CDirect3D_CheckDeviceFormat(const void *t, UINT a, int dt, D3DFORMAT af, DWORD u, int rt, D3DFORMAT cf)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &af, &u, &rt, &cf };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDeviceFormat", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    int depth;
    (void)t; (void)dt; (void)af;
    if (a)
        return (HRESULT)(int32_t)0x8876086cu;
    depth = MacDisplay_GetCurrentDepth();
    if (u == 0 && (cf == 0x31545844 || cf == 0x32545844 || cf == 0x33545844 || cf == 0x35545844))
        return 0;
    if (u == 0 || u == 1)
        return cf == 21 || cf == 22 ? 0 : (HRESULT)(int32_t)0x8876086au;
    if (u == 2) {
        if (rt == 1 && (depth == 16 || depth == 32) &&
            (cf == 80 || cf == 73 || (depth == 32 && (cf == 71 || cf == 75))))
            return 0;
        if (rt == 3 && MacOpenGLUtils_GetPCPixelShaderVersion() > 0xffff01ffu)
            return 0;
    }
    return (HRESULT)(int32_t)0x8876086au;
#else
    (void)t;
    (void)a;
    (void)dt;
    (void)af;
    (void)u;
    (void)rt;
    (void)cf;
    return 0;
#endif
}
HRESULT CDirect3D_CheckDeviceMultiSampleType(const void *t, UINT a, int dt, D3DFORMAT sf, BOOL w, int ms, DWORD *q)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &sf, &w, &ms, &q };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDeviceMultiSampleType", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    int buffers, samples;
    unsigned char super, multi, alpha;
    (void)t; (void)a; (void)dt; (void)sf;
    MacDisplay_GetAntiAliasingMultiSampleInfo(&buffers, &samples, &super, &multi, &alpha);
    if (!ms || w || !super || (UINT)ms > (UINT)samples)
        return (HRESULT)(int32_t)0x8876086au;
    if (q)
        *q = 1;
    return 0;
#else
    (void)t;
    (void)a;
    (void)dt;
    (void)sf;
    (void)w;
    (void)ms;
    (void)q;
    return 0;
#endif
}
HRESULT CDirect3D_CheckDepthStencilMatch(const void *t, UINT a, int dt, D3DFORMAT af, D3DFORMAT rt, D3DFORMAT ds)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &af, &rt, &ds };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDepthStencilMatch", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)a;
    (void)dt;
    (void)af;
    (void)rt;
    (void)ds;
    return 0;
}
HRESULT CDirect3D_CheckDeviceFormatConversion(const void *t, UINT a, int dt, D3DFORMAT sf, D3DFORMAT tf)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &sf, &tf };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDeviceFormatConversion", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)a;
    (void)dt;
    (void)sf;
    (void)tf;
    return 0;
}

HRESULT CDirect3D_GetDeviceCaps(const void *_this, UINT Adapter, int DeviceType, void *pCaps)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &DeviceType, &pCaps };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetDeviceCaps", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Adapter;
    (void)DeviceType;

#if defined(COD2_X64)
    int maxTexUnits, maxTexImageUnits, maxStages;
    D3DCAPS9 *caps = (D3DCAPS9 *)pCaps;
    memset(caps, 0, sizeof(D3DCAPS9));
    caps->DeviceType = 1;
    caps->AdapterOrdinal = Adapter;
    caps->Caps = 0;
    caps->Caps2 = 0x30020000;
    caps->Caps3 = 0x120;
    caps->PresentationIntervals = 0x80000001;
    caps->CursorCaps = 1;
    caps->DevCaps = 0x19bfc0;
    caps->PrimitiveMiscCaps = 0xcaf2;

    if (MacDisplay_GetSupportsSeparateBlendFunc()) {
        caps->PrimitiveMiscCaps |= 0x20000;
    }

    caps->RasterCaps = 0x7622081;

    caps->ZCmpCaps = 0xff;
    caps->SrcBlendCaps = 0x7ff;
    caps->DestBlendCaps = 0x7ff;
    caps->AlphaCmpCaps = 0xff;
    caps->ShadeCaps = 0x84208;
    caps->TextureCaps = 0x234d07;
    caps->TextureFilterCaps = 0x3030300;
    caps->CubeTextureFilterCaps = 0x3030300;

    if (MacDisplay_GetSupportsAnisotropicFiltering()) {
        caps->TextureFilterCaps |= 0x4000400;
        caps->CubeTextureFilterCaps |= 0x4000400;
    }

    caps->VolumeTextureFilterCaps = 0;
    caps->TextureAddressCaps = 0xd;
    caps->VolumeTextureAddressCaps = 0;
    caps->LineCaps = 0;
    caps->MaxTextureWidth = 0x800;
    caps->MaxTextureHeight = 0x800;
    caps->MaxVolumeExtent = 0;
    caps->MaxTextureRepeat = 0;
    caps->MaxTextureAspectRatio = 0x800;

    if (MacDisplay_GetSupportsAnisotropicFiltering()) {
        caps->MaxAnisotropy = (DWORD)MacDisplay_GetMaxSupportedAnisotropy();
    }

    caps->MaxVertexW = 0.0f;
    caps->GuardBandLeft = 0.0f;
    caps->GuardBandTop = 0.0f;
    caps->GuardBandRight = 0.0f;
    caps->GuardBandBottom = 0.0f;
    caps->ExtentsAdjust = 0.0f;
    caps->StencilCaps = 0xff;
    caps->FVFCaps = 0;
    caps->TextureOpCaps = 0x3feffff;
    caps->VertexProcessingCaps = 0x3b;
    caps->MaxActiveLights = 8;
    caps->MaxUserClipPlanes = 0;
    caps->MaxVertexBlendMatrices = 0;
    caps->MaxVertexBlendMatrixIndex = 0;
    caps->MaxPointSize = 1.0f;
    caps->MaxPrimitiveCount = 0xffff;
    caps->MaxVertexIndex = 0xffff;
    caps->MaxStreams = 0x10;
    caps->MaxStreamStride = 0x400;
    caps->VertexShaderVersion = 0xfffe0200;

    caps->PixelShaderVersion = MacOpenGLUtils_GetPCPixelShaderVersion();
    caps->MaxVertexShaderConst = 0x100;
    caps->PixelShader1xMaxValue = 8.0f;

    maxTexImageUnits = MacDisplay_GetMaxTextureImageUnits();
    maxTexUnits = MacDisplay_GetMaxTextureUnits();
    maxStages = (maxTexUnits < maxTexImageUnits) ? maxTexImageUnits : maxTexUnits;
    caps->MaxSimultaneousTextures = maxStages;
    caps->MaxTextureBlendStages = maxStages;

    caps->DevCaps2 = 1;
    caps->MaxNpatchTessellationLevel = 0.0f;
    caps->DeclTypes = 0;
    caps->NumSimultaneousRTs = 1;
    caps->StretchRectFilterCaps = 0x200;
    caps->VertexTextureFilterCaps = 0;
    caps->MaxVShaderInstructionsExecuted = 0;
    caps->MaxPShaderInstructionsExecuted = 0;
    caps->MaxVertexShader30InstructionSlots = 0;
    return 0;
#else
    memset(pCaps, 0, sizeof(D3DCAPS9));
    return 0;
#endif
}

void *CDirect3D_GetAdapterMonitor(const void *_this, UINT Adapter)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter };
    void * macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterMonitor", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)Adapter;
    return NULL;
}

HRESULT CDirect3D_CreateDevice(const void *_this, UINT Adapter, int DeviceType, void *hFocusWindow, DWORD BehaviorFlags, void *pPresentationParameters, void **ppReturnedDeviceInterface)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &DeviceType, &hFocusWindow, &BehaviorFlags, &pPresentationParameters, &ppReturnedDeviceInterface };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CreateDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    static char deviceMem[4096] COD2_ALIGNED(sizeof(void *));
#else
    static char deviceMem[4096];
#endif
    int *pp = (int *)pPresentationParameters;
    int width = 640, height = 480;
    void *ctx;

    (void)_this;
    (void)Adapter;
    (void)DeviceType;
    (void)hFocusWindow;
    (void)BehaviorFlags;

    if (pp) {
        if (pp[0] > 0)
            width = pp[0];
        if (pp[1] > 0)
            height = pp[1];
    }

#if COD2_APPLE_SDK
    {
        extern int sdl_gl_width, sdl_gl_height;
        sdl_gl_width = width;
        sdl_gl_height = height;
    }
    /* The original Mac build already had a current AGL context here, so
     * CDirect3DDevice_Init could issue GL calls. The SDL display layer creates
     * the context on demand, so create it before Init touches GL state. */
    dvar_t *borderless = Dvar_RegisterBool("r_borderless", 1, 0x1001);
#if defined(COD2_X64)
    int fullscreen = pp ? !pp[7] : (r_fullscreen && r_fullscreen->current.enabled);
#else
    int fullscreen = r_fullscreen && r_fullscreen->current.enabled;
#endif
    MacPlatform_ConfigureWindow(width, height, fullscreen ? (borderless->current.enabled ? MAC_BORDERLESS : MAC_FULLSCREEN) : MAC_WINDOWED, pp ? pp[12] : 0);
#if defined(COD2_X64)
    /* r_init supplies native presentation words, not the imported LP64 struct. */
    ctx = MacDisplay_CreateScreenContext(24, 1, 0, 0, pp ? pp[11] : 0, NULL);
#else
    ctx = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
#endif
    if (!ctx)
        return (HRESULT)(int32_t)0x8876086cu;

    memset(deviceMem, 0, sizeof(deviceMem));
    *(void ***)deviceMem = vtbl_CDirect3DDevice;
    CDirect3DDevice_Init(deviceMem);
#else
    memset(deviceMem, 0, sizeof(deviceMem));
    *(void ***)deviceMem = vtbl_CDirect3DDevice;
    CDirect3DDevice_Init(deviceMem);

    {
        extern int sdl_gl_width, sdl_gl_height;
        sdl_gl_width = width;
        sdl_gl_height = height;
    }
    ctx = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
#endif
    /* DeviceImpl field offsets differ on x64 (8-byte ptrs): context@16 renderTarget@32
     * backBuffer@48 (x86: 0x08/0x14/0x1C). GetBackBuffer reads the typed backBuffer
     * field, so these must land at the right offset or it returns NULL. */
#if defined(COD2_X64)
    *(void **)(deviceMem + 16) = ctx;            /* context */
#else
    *(void **)(deviceMem + 0x008) = ctx;
#endif

    {
        void *bbSurf = calloc(1, 0x80);   /* 0x3c was x86 sizeof(CDirect3DSurfaceImpl); x64 is 0x50 */

        CDirect3DSurface_CDirect3DSurface(bbSurf, 0, 0, 0, width, height, 0x16, NULL, NULL);
#if defined(COD2_X64)
        extern void CDirect3DDevice_SetNativeBackBuffer(void *, void *, UINT);
        CDirect3DDevice_SetNativeBackBuffer(deviceMem, bbSurf, Adapter);
#else
        *(void **)(deviceMem + 0x01C) = bbSurf;
        *(void **)(deviceMem + 0x014) = bbSurf;
#endif
    }

    *ppReturnedDeviceInterface = deviceMem;
    return 0;
}

void ZN9CDirect3DD1Ev(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN9CDirect3DD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ((CDirect3DImpl *)_this)->vtable = (void **)vtbl_CDirect3D;
    sDirect3DInterfaceActive = 0;
#else
    (void)_this;
#endif
}
void ZN9CDirect3DD0Ev(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN9CDirect3DD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ((CDirect3DImpl *)_this)->vtable = (void **)vtbl_CDirect3D;
    sDirect3DInterfaceActive = 0;
#else
    (void)_this;
#endif
}

void *Direct3DCreate9(int sdkVersion)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &sdkVersion };
    void * macResult = { 0 };
    if (MacShim_Dispatch("Direct3DCreate9", NULL, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)sdkVersion;
    return CDirect3D_GetDirect3DInterface();
#else
    (void)sdkVersion;
    sDirect3DInterface.vtable = vtbl_CDirect3D;
    sDirect3DInterface.refCount = 1;
    /* returns a pointer -- was `int`/`(int)(unsigned long)` which truncated the
     * 8-byte CDirect3D address on x64 (int and Win64 long are both 4 bytes). */
    return (void *)&sDirect3DInterface;
#endif
}

fnptr_t vtbl_CDirect3D[] = { (fnptr_t)CDirect3D_QueryInterface, (fnptr_t)CDirect3D_AddRef, (fnptr_t)CDirect3D_Release, (fnptr_t)CDirect3D_RegisterSoftwareDevice, (fnptr_t)CDirect3D_GetAdapterCount, (fnptr_t)CDirect3D_GetAdapterIdentifier, (fnptr_t)CDirect3D_GetAdapterModeCount, (fnptr_t)CDirect3D_EnumAdapterModes, (fnptr_t)CDirect3D_GetAdapterDisplayMode, (fnptr_t)CDirect3D_CheckDeviceType, (fnptr_t)CDirect3D_CheckDeviceFormat, (fnptr_t)CDirect3D_CheckDeviceMultiSampleType, (fnptr_t)CDirect3D_CheckDepthStencilMatch, (fnptr_t)CDirect3D_CheckDeviceFormatConversion, (fnptr_t)CDirect3D_GetDeviceCaps, (fnptr_t)CDirect3D_GetAdapterMonitor, (fnptr_t)CDirect3D_CreateDevice, (fnptr_t)ZN9CDirect3DD1Ev, (fnptr_t)ZN9CDirect3DD0Ev };
