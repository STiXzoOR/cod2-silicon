#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>

typedef struct {
    void **vtable;
    ULONG refCount;
    IDirect3DSurface9 *backBuffer;
} CDirect3DSwapChainImpl;

typedef struct {
    void *QueryInterface;
    ULONG (*AddRef)(IDirect3DSurface9 *);
    ULONG (*Release)(IDirect3DSurface9 *);
} IDirect3DSurface9VTable;
typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DSwapChain[];

static IDirect3DSurface9VTable *CDirect3DSwapChain_GetSurfaceVTable(IDirect3DSurface9 *surface)
{
    return *(IDirect3DSurface9VTable **)surface;
}

static ULONG CDirect3DSwapChain_SurfaceAddRef(IDirect3DSurface9 *surface)
{
    return CDirect3DSwapChain_GetSurfaceVTable(surface)->AddRef(surface);
}

static ULONG CDirect3DSwapChain_SurfaceRelease(IDirect3DSurface9 *surface)
{
    return CDirect3DSwapChain_GetSurfaceVTable(surface)->Release(surface);
}

static void CDirect3DSwapChain_Destroy(CDirect3DSwapChainImpl *swapChain)
{
    swapChain->vtable = vtbl_CDirect3DSwapChain;
    if (swapChain->backBuffer) {
        CDirect3DSwapChain_SurfaceRelease(swapChain->backBuffer);
    }
}

ULONG CDirect3DSwapChain_AddRef(const CDirect3DSwapChain *_this);
void ZN18CDirect3DSwapChainD1Ev(const CDirect3DSwapChain *_this);
void ZN18CDirect3DSwapChainD0Ev(const CDirect3DSwapChain *_this);
HRESULT CDirect3DSwapChain_QueryInterface(const CDirect3DSwapChain *_this, const IID *iid, LPVOID *ppvObj);
ULONG CDirect3DSwapChain_Release(const CDirect3DSwapChain *_this);
HRESULT CDirect3DSwapChain_GetBackBuffer(const CDirect3DSwapChain *_this, UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer);
int CDirect3DSwapChain_SetBackBuffer(const CDirect3DSwapChain *_this, const IDirect3DSurface9 *pBackBuffer);
int CDirect3DSwapChain_CDirect3DSwapChain(const CDirect3DSwapChain *_this);
HRESULT CDirect3DSwapChain_Present(const CDirect3DSwapChain *_this, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags);
HRESULT CDirect3DSwapChain_GetFrontBufferData(const CDirect3DSwapChain *_this, IDirect3DSurface9 *pDestSurface);
HRESULT CDirect3DSwapChain_GetRasterStatus(const CDirect3DSwapChain *_this, D3DRASTER_STATUS *pRasterStatus);
HRESULT CDirect3DSwapChain_GetDisplayMode(const CDirect3DSwapChain *_this, D3DDISPLAYMODE *pMode);
HRESULT CDirect3DSwapChain_GetDevice(const CDirect3DSwapChain *_this, D3DDEVICE_CREATION_PARAMETERS *(*ppDevice)());
HRESULT CDirect3DSwapChain_GetPresentParameters(const CDirect3DSwapChain *_this, D3DPRESENT_PARAMETERS *pPresentationParameters);

ULONG CDirect3DSwapChain_AddRef(const CDirect3DSwapChain *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSwapChainImpl *swapChain;

    swapChain = (CDirect3DSwapChainImpl *)_this;
    ++swapChain->refCount;
    return swapChain->refCount;
}

HRESULT CDirect3DSwapChain_QueryInterface(const CDirect3DSwapChain *_this, const IID *iid, LPVOID *ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;

    *ppvObj = (LPVOID)_this;
    CDirect3DSwapChain_AddRef(_this);
    return 0;
}

ULONG CDirect3DSwapChain_Release(const CDirect3DSwapChain *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSwapChainImpl *swapChain;
    ULONG refCount;

    swapChain = (CDirect3DSwapChainImpl *)_this;
    refCount = --swapChain->refCount;
    if (!refCount) {
        ZN18CDirect3DSwapChainD0Ev(_this);
    }

    return refCount;
}

void ZN18CDirect3DSwapChainD1Ev(const CDirect3DSwapChain *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN18CDirect3DSwapChainD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DSwapChain_Destroy((CDirect3DSwapChainImpl *)_this);
}

void ZN18CDirect3DSwapChainD0Ev(const CDirect3DSwapChain *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN18CDirect3DSwapChainD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DSwapChain_Destroy((CDirect3DSwapChainImpl *)_this);
    free((void *)_this);
}

HRESULT CDirect3DSwapChain_GetBackBuffer(const CDirect3DSwapChain *_this, UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &BackBuffer, &Type, &ppBackBuffer };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetBackBuffer", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSwapChainImpl *swapChain;

    (void)BackBuffer;
    (void)Type;

    swapChain = (CDirect3DSwapChainImpl *)_this;
    *ppBackBuffer = swapChain->backBuffer;
    CDirect3DSwapChain_SurfaceAddRef(swapChain->backBuffer);
    return 0;
}

int CDirect3DSwapChain_SetBackBuffer(const CDirect3DSwapChain *_this, const IDirect3DSurface9 *pBackBuffer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pBackBuffer };
    int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_SetBackBuffer", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSwapChainImpl *swapChain;

    swapChain = (CDirect3DSwapChainImpl *)_this;
    swapChain->backBuffer = (IDirect3DSurface9 *)pBackBuffer;
    return (int)CDirect3DSwapChain_SurfaceAddRef(swapChain->backBuffer);
}

int CDirect3DSwapChain_CDirect3DSwapChain(const CDirect3DSwapChain *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    int macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_CDirect3DSwapChain", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DSwapChainImpl *swapChain;

    swapChain = (CDirect3DSwapChainImpl *)_this;
    swapChain->vtable = vtbl_CDirect3DSwapChain;
    swapChain->backBuffer = NULL;
    swapChain->refCount = 1;
    return (int)_this;
}

HRESULT CDirect3DSwapChain_Present(const CDirect3DSwapChain *_this, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSourceRect, &pDestRect, &hDestWindowOverride, &pDirtyRegion, &dwFlags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_Present", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pSourceRect;
    (void)pDestRect;
    (void)hDestWindowOverride;
    (void)pDirtyRegion;
    (void)dwFlags;
    return 0;
}

HRESULT CDirect3DSwapChain_GetFrontBufferData(const CDirect3DSwapChain *_this, IDirect3DSurface9 *pDestSurface)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDestSurface };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetFrontBufferData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pDestSurface;
    return 0;
}

HRESULT CDirect3DSwapChain_GetRasterStatus(const CDirect3DSwapChain *_this, D3DRASTER_STATUS *pRasterStatus)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pRasterStatus };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetRasterStatus", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pRasterStatus;
    return 0;
}

HRESULT CDirect3DSwapChain_GetDisplayMode(const CDirect3DSwapChain *_this, D3DDISPLAYMODE *pMode)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pMode };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetDisplayMode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pMode;
    return 0;
}

HRESULT CDirect3DSwapChain_GetDevice(const CDirect3DSwapChain *_this, D3DDEVICE_CREATION_PARAMETERS *(*ppDevice)())
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDevice };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppDevice;
    return 0;
}

HRESULT CDirect3DSwapChain_GetPresentParameters(const CDirect3DSwapChain *_this, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pPresentationParameters };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DSwapChain_GetPresentParameters", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pPresentationParameters;
    return 0;
}

fnptr_t vtbl_CDirect3DSwapChain[] = { (fnptr_t)CDirect3DSwapChain_QueryInterface, (fnptr_t)CDirect3DSwapChain_AddRef, (fnptr_t)CDirect3DSwapChain_Release, (fnptr_t)CDirect3DSwapChain_Present, (fnptr_t)CDirect3DSwapChain_GetFrontBufferData, (fnptr_t)CDirect3DSwapChain_GetBackBuffer, (fnptr_t)CDirect3DSwapChain_GetRasterStatus, (fnptr_t)CDirect3DSwapChain_GetDisplayMode, (fnptr_t)CDirect3DSwapChain_GetDevice, (fnptr_t)CDirect3DSwapChain_GetPresentParameters, (fnptr_t)ZN18CDirect3DSwapChainD1Ev, (fnptr_t)ZN18CDirect3DSwapChainD0Ev };
