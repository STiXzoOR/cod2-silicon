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

    (void)_this;
    return ++sDirect3DInterface.refCount;
}
ULONG CDirect3D_Release(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return --sDirect3DInterface.refCount;
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
    (void)Format;
    return 1;
}

HRESULT CDirect3D_EnumAdapterModes(const void *_this, UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &Format, &Mode, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_EnumAdapterModes", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif


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
}

HRESULT CDirect3D_GetAdapterDisplayMode(const void *_this, UINT Adapter, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Adapter, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_GetAdapterDisplayMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    int *p = (int *)pMode;
    (void)_this;
    (void)Adapter;
    p[0] = 1024;
    p[1] = 768;
    p[2] = 60;
    p[3] = 22;
    return 0;
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

    (void)t;
    (void)a;
    (void)dt;
    (void)af;
    (void)u;
    (void)rt;
    (void)cf;
    return 0;
}
HRESULT CDirect3D_CheckDeviceMultiSampleType(const void *t, UINT a, int dt, D3DFORMAT sf, BOOL w, int ms, DWORD *q)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &a, &dt, &sf, &w, &ms, &q };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3D_CheckDeviceMultiSampleType", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)a;
    (void)dt;
    (void)sf;
    (void)w;
    (void)ms;
    (void)q;
    return 0;
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

    memset(pCaps, 0, sizeof(D3DCAPS9));
    return 0;
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

    static char deviceMem[4096];
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
    int fullscreen = r_fullscreen && r_fullscreen->current.enabled;
    MacPlatform_ConfigureWindow(width, height, fullscreen ? (borderless->current.enabled ? MAC_BORDERLESS : MAC_FULLSCREEN) : MAC_WINDOWED, pp ? pp[12] : 0);
    ctx = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
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
        *(void **)(deviceMem + 48) = bbSurf;     /* backBuffer */
        *(void **)(deviceMem + 32) = bbSurf;     /* renderTarget */
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

    (void)_this;
}
void ZN9CDirect3DD0Ev(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN9CDirect3DD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

void *Direct3DCreate9(int sdkVersion)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &sdkVersion };
    void * macResult = { 0 };
    if (MacShim_Dispatch("Direct3DCreate9", NULL, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)sdkVersion;
    sDirect3DInterface.vtable = vtbl_CDirect3D;
    sDirect3DInterface.refCount = 1;
    /* returns a pointer -- was `int`/`(int)(unsigned long)` which truncated the
     * 8-byte CDirect3D address on x64 (int and Win64 long are both 4 bytes). */
    return (void *)&sDirect3DInterface;
}

fnptr_t vtbl_CDirect3D[] = { (fnptr_t)CDirect3D_QueryInterface, (fnptr_t)CDirect3D_AddRef, (fnptr_t)CDirect3D_Release, (fnptr_t)CDirect3D_RegisterSoftwareDevice, (fnptr_t)CDirect3D_GetAdapterCount, (fnptr_t)CDirect3D_GetAdapterIdentifier, (fnptr_t)CDirect3D_GetAdapterModeCount, (fnptr_t)CDirect3D_EnumAdapterModes, (fnptr_t)CDirect3D_GetAdapterDisplayMode, (fnptr_t)CDirect3D_CheckDeviceType, (fnptr_t)CDirect3D_CheckDeviceFormat, (fnptr_t)CDirect3D_CheckDeviceMultiSampleType, (fnptr_t)CDirect3D_CheckDepthStencilMatch, (fnptr_t)CDirect3D_CheckDeviceFormatConversion, (fnptr_t)CDirect3D_GetDeviceCaps, (fnptr_t)CDirect3D_GetAdapterMonitor, (fnptr_t)CDirect3D_CreateDevice, (fnptr_t)ZN9CDirect3DD1Ev, (fnptr_t)ZN9CDirect3DD0Ev };
