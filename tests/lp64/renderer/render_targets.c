#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_rendertarget.c"

DxGlobals dx;
vidConfig_t vidConfig;
refimport_t ri;
Bool g_NoTextureID;
int alwaysfails;
void *imp_g_NoTextureID = &g_NoTextureID;
static GfxImage images[10];
static void *deviceMethods[30], *surfaceMethods[13];
static struct { void **vtable; } device, surfaces[12];
static int releases, imageReleases;

const char *va(const char *format, ...)
{ (void)format; assert(0); return NULL; }
void Com_Error(int level, const char *format, ...)
{ (void)level; (void)format; assert(0); }
const char *R_ErrorDescription(HRESULT error)
{ (void)error; assert(0); return NULL; }
static void Print(int level, const char *format, ...)
{ (void)level; (void)format; }
static ULONG AddRef(void *object)
{ assert(object); return 1; }
static ULONG Release(void *object)
{ assert(object); ++releases; return 0; }
static HRESULT GetDesc(void *surface, void *description)
{
    assert(surface);
    ((_D3DSURFACE_DESC *)description)->Format = D3DFMT_A8R8G8B8;
    return 0;
}
static HRESULT GetSwapChain(void *object, UINT index, byte *chain)
{ assert(object == &device && index == 0); *(void **)chain = &device; return 0; }
static HRESULT GetBackBuffer(void *object, DWORD chain, DWORD buffer, DWORD type, byte *surface)
{
    assert(object == &device && chain == 0 && buffer == 0 && type == 0);
    *(void **)surface = &surfaces[10];
    return 0;
}
static HRESULT CreateDepth(void *object, UINT width, UINT height, D3DFORMAT format,
                           int multisample, DWORD quality, int discard,
                           IDirect3DSurface9 **surface, void *shared)
{
    assert(object == &device && format == D3DFMT_D24S8 && !multisample && !quality && !discard && !shared);
    assert((width == 1280 && height == 720) || (width == 128 && height == 128));
    *surface = (IDirect3DSurface9 *)&surfaces[width == 128 ? 11 : 10];
    return 0;
}
GfxImage *Image_AllocProg(int id, int category)
{ assert(id >= 0 && id < 10 && category == 6); return &images[id]; }
void Image_SetupRenderTarget(GfxImage *image, int width, int height, D3DFORMAT format)
{
    assert(format == D3DFMT_A8R8G8B8);
    image->width = width;
    image->height = height;
}
void Image_SetupSystem(GfxImage *image, int width, int height, D3DFORMAT format)
{ (void)image; (void)width; (void)height; (void)format; assert(0); }
IDirect3DSurface9 *Image_GetSurface(GfxImage *image)
{ return (IDirect3DSurface9 *)&surfaces[image - images]; }
void Image_Release(GfxImage *image)
{ assert(image >= images && image < images + 10); ++imageReleases; }
void Image_TrackFullscreenTexture(GfxImage *image, int picmip, D3DFORMAT format)
{ (void)image; (void)picmip; (void)format; }
void Image_TrackTexture(GfxImage *image, int flags, D3DFORMAT format, int width, int height, int depth)
{ (void)image; (void)flags; (void)format; (void)width; (void)height; (void)depth; }

static void CheckAlias(int destination, int source)
{
    const GfxRenderTarget *a = &dx.renderTargets[destination], *b = &dx.renderTargets[source];
    assert(a->image == b->image && a->colorSurface == b->colorSurface);
    assert(a->depthStencilSurface == b->depthStencilSurface);
    assert(a->width == 1280 && a->height == 720 && a->width == b->width && a->height == b->height);
}

int main(int argc, char **argv)
{
    _Static_assert(sizeof(GfxRenderTarget) == 32, "native render target size");
    _Static_assert(offsetof(GfxRenderTarget, depthStencilSurface) == 16, "native depth pointer");
    _Static_assert(offsetof(GfxRenderTarget, width) == 24, "native target dimensions");
    device.vtable = deviceMethods;
    deviceMethods[0x38 / 4] = GetSwapChain;
    deviceMethods[0x48 / 4] = GetBackBuffer;
    deviceMethods[0x74 / 4] = CreateDepth;
    surfaceMethods[1] = AddRef;
    surfaceMethods[2] = Release;
    surfaceMethods[0x30 / 4] = GetDesc;
    for (int i = 0; i < 12; ++i) surfaces[i].vtable = surfaceMethods;
    dx.device = (IDirect3DDevice9 *)&device;
    vidConfig.width = 1280;
    vidConfig.height = 720;
    ri.Printf = Print;
    /* Real native pointers have nonzero high halves; aliases must retain them. */
    assert((uintptr_t)&surfaces[10] > UINT32_MAX);
    R_InitRenderTargets();
    assert(argc == 1 || (argc == 2 && !strcmp(argv[1], "--shutdown-only")));
    if (argc == 1) {
        CheckAlias(R_RENDERTARGET_RESOLVED_POST_SUN, R_RENDERTARGET_DYNAMICSHADOWS);
        CheckAlias(R_RENDERTARGET_SAVED_SCREEN, R_RENDERTARGET_DYNAMICSHADOWS);
    }
    for (int i = 0; i < R_RENDERTARGET_COUNT; ++i) {
        dx.renderTargets[i].width = 1000 + i;
        dx.renderTargets[i].height = 2000 + i;
    }
    R_ShutdownRenderTargets();
    for (int i = 0; i < R_RENDERTARGET_COUNT; ++i) {
        const GfxRenderTarget *target = &dx.renderTargets[i];
        assert(!target->image && !target->colorSurface && !target->depthStencilSurface);
        assert(!target->width && !target->height);
    }
    assert((void *)dx.device == &device && !dx.singleSampleDepthStencilSurface);
    assert(imageReleases == 9 && releases == 19);
    puts("native render targets: full pointer aliases, dimensions and shutdown clearing passed");
}
