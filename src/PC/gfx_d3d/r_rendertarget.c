#include "common_types.h"
#include "PC/gfx_d3d/r_log.h"
extern struct DxGlobals dx;
/* File-scope alias: bound where no local can shadow `dx`, so uses below
   always reach the global even inside functions that declare their own `dx`. */
static struct DxGlobals * const dx_g = &dx;
extern Bool g_NoTextureID;
#include "imports.h"
extern vidConfig_t vidConfig;
extern DxGlobals dx;
#if defined(COD2_X64)
extern const dvar_t *r_zFeather;
extern const dvar_t *r_rendererInUse;
#endif

extern void *imp_dx;
extern void *imp_vidConfig;
extern refimport_t ri;
extern int alwaysfails;
extern void *imp_g_NoTextureID;

extern const char *va(const char *fmt, ...);
extern void Com_Error(int level, const char *fmt, ...);
extern const char *R_ErrorDescription(HRESULT hr);
extern GfxImage *Image_AllocProg(int imageProgType, int category);
extern void Image_SetupRenderTarget(GfxImage *image, int width, int height, D3DFORMAT imageFormat);
extern void Image_SetupSystem(GfxImage *image, int width, int height, D3DFORMAT imageFormat);
extern IDirect3DSurface9 *Image_GetSurface(GfxImage *image);
extern void Image_Release(GfxImage *image);
extern void Image_TrackFullscreenTexture(GfxImage *image, int picmip, D3DFORMAT format);
extern void Image_TrackTexture(GfxImage *image, int imageFlags, D3DFORMAT format, int width, int height, int depth);

#define DX() ((byte *)dx_g)
#define DX_PTR() (&dx)
#define VIDCONFIG() (&vidConfig)

/* offsets must follow the real GfxRenderTarget layout (8-byte pointers on x64), not hardcoded x86 +4/+8. */
#define DX_RT_IMAGE(rt) (*(GfxImage **)((rt) + offsetof(GfxRenderTarget, image)))
#define DX_RT_COLOR_SURFACE(rt) (*(IDirect3DSurface9 **)((rt) + offsetof(GfxRenderTarget, colorSurface)))
#define DX_RT_DEPTH_SURFACE(rt) (*(IDirect3DSurface9 **)((rt) + offsetof(GfxRenderTarget, depthStencilSurface)))
#define DX_RT_WIDTH(rt) (*(int *)((rt) + 12))
#define DX_RT_HEIGHT(rt) (*(int *)((rt) + 16))

#define VTABLE(obj) (*(void ***)obj)

typedef HRESULT (*D3DDevice_GetBackBufferFn)(void *device, UINT swapChain, UINT backBuffer, void *type, IDirect3DSurface9 **surface);
typedef HRESULT (*D3DDevice_GetRenderTargetFn)(void *device, DWORD renderTargetIndex, void *unused, IDirect3DSurface9 **surface);
typedef HRESULT (*D3DDevice_GetDepthStencilFn)(void *device, DWORD unused1, DWORD unused2, DWORD unused3, IDirect3DSurface9 **surface);
typedef HRESULT(D3DVTCC *D3DDevice_CreateDepthStencilFn)(void *device, UINT width, UINT height, D3DFORMAT format, int multiSample, DWORD multiSampleQuality, int discard, IDirect3DSurface9 **surface, void *sharedHandle);
typedef HRESULT(D3DVTCC *D3DSurface_GetDescFn)(void *surface, void *desc);
typedef ULONG(D3DVTCC *IUnknown_AddRefFn)(void *obj);
typedef ULONG(D3DVTCC *IUnknown_ReleaseFn)(void *obj);

const char *R_DescribeFormat(int format)
{
    switch (format) {
    case D3DFMT_A8R8G8B8:
#if defined(COD2_X64)
    case 32:
#endif
        return "24-bit color with 8-bit alpha";
#if defined(COD2_X64)
    case 23:
        return "16-bit color";
#else
    case D3DFMT_X8R8G8B8:
        return "24-bit color";
#endif
    case D3DFMT_A1R5G5B5:
        return "15-bit color with 1-bit alpha";
#if !defined(COD2_X64)
    case D3DFMT_D32:
        return "32-bit depth without stencil";
#endif
    case D3DFMT_D15S1:
        return "15-bit depth with 1-bit stencil";
    case D3DFMT_D24S8:
        return "24-bit depth with 8-bit stencil";
    case D3DFMT_D24X8:
        return "24-bit depth without stencil";
    case D3DFMT_D16:
#if defined(COD2_X64)
    case 70:
#endif
        return "16-bit depth without stencil";
    default:
        return va("unknown format 0x%08x", format);
    }
}

static IDirect3DSurface9 *R_GetOrCreateDepthStencil(byte *dxPtr, int fullWidth, int fullHeight)
{
    IDirect3DSurface9 *dsSurface;

    dsSurface = ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface ;
    if (dsSurface != NULL) {

        ((IUnknown_AddRefFn)VTABLE(dsSurface)[1])((void *)dsSurface);
        return ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface ;
    }

#if COD2_MAC_D3D9
    {
        /* With the effects layer on, this buffer is a depth texture, so the layer can
         * test against the scene and read it (rb_fxlayer.c). It is then never the
         * frame buffer's own depth, which cannot be read. */
        extern void *RB_FxLayer_CreateSceneDepth(void *device, int width, int height);
        dsSurface = RB_FxLayer_CreateSceneDepth(((DxGlobals *)(dxPtr))->device, fullWidth, fullHeight);
        if (dsSurface != NULL) {
            extern void RB_FxLayer_Prepare(void *device);
            ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface = dsSurface;
            RB_FxLayer_Prepare(((DxGlobals *)(dxPtr))->device);
            return dsSurface;
        }
    }
#endif
    if (((DxGlobals *)(dxPtr))->multiSampleType        == 0) {
        byte *rt0 = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[0]);
        IDirect3DSurface9 *rt0ds = DX_RT_DEPTH_SURFACE(rt0);
        ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface  = rt0ds;
        if (rt0ds != NULL) {
            ((IUnknown_AddRefFn)VTABLE(rt0ds)[1])((void *)rt0ds);
            return ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface ;
        }
    }

    {
        void *device = ((DxGlobals *)(dxPtr))->device;
        void **vtable = VTABLE(device);
        HRESULT hr;
        int w = fullWidth;
        int h = fullHeight;

        hr = ((D3DDevice_CreateDepthStencilFn)vtable[0x74 / 4])(
            device, w, h, D3DFMT_D24S8, 0, 0, 0,
            &((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface , NULL);

        if (hr < 0) {
            const char *desc = R_ErrorDescription(hr);
            Com_Error(0, "Couldn't create a %i x %i depth-stencil surface: %s\n", w, h, desc);
        }
    }

    return ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface ;
}

#if COD2_MAC_D3D9
/* The glow and blur filters work on reduced copies of the screen: a quarter of its
 * size each way. Their cost grows with the square of the resolution (more pixels, and
 * more passes to reach the same width of blur): at 5120x2880 glow took nineteen passes
 * over 1280x720. The picture they make is smooth by design, so above 1600 lines the
 * Metal path gives them an eighth, which at 2160 lines is what 1080 lines has always
 * had. What decides which pixels glow still runs on the full picture at a quarter
 * (rb_backend.c: RB_GlowSetupQuarter); only the blur after it works on fewer pixels
 * (r_glowHalfRes 0 turns this off). */
int R_ReducedTargetPicmip(void)
{
    extern int Dvar_GetInt(const char *name);
    return VIDCONFIG()->height > 1600 && Dvar_GetInt("r_glowHalfRes") ? 3 : 2;
}
#define R_REDUCED_PICMIP R_ReducedTargetPicmip()
#else
#define R_REDUCED_PICMIP 2
#endif

static void R_InitFullscreenRenderTargetImage(int imageProgId, int picmip, D3DFORMAT format, RenderTargetUsage usage, GfxRenderTarget *renderTarget)
{
    vidConfig_t *vidCfg = VIDCONFIG();
    int fullWidth = vidCfg->width;
    int fullHeight = vidCfg->height;
    int width, height;

    width = fullWidth >> (byte)picmip;
    if (width <= 0)
        width = 1;
    height = fullHeight >> (byte)picmip;
    if (height <= 0)
        height = 1;

    renderTarget->image = Image_AllocProg(imageProgId, 6);

    if (usage == RENDERTARGET_USAGE_READBACK) {
        Image_SetupSystem(renderTarget->image, (unsigned short)width, (unsigned short)height, format);
    } else {
        Image_SetupRenderTarget(renderTarget->image, (unsigned short)width, (unsigned short)height, format);
    }

    renderTarget->colorSurface = Image_GetSurface(renderTarget->image);
    renderTarget->width = (unsigned short)width;
    renderTarget->height = (unsigned short)height;

    if (usage == RENDERTARGET_USAGE_RENDER) {
        byte *dxPtr = DX();
        IDirect3DSurface9 *dsSurface = R_GetOrCreateDepthStencil(dxPtr, fullWidth, fullHeight);
        renderTarget->depthStencilSurface = dsSurface;
    }

    Image_TrackFullscreenTexture(renderTarget->image, picmip, format);
}

static IDirect3DSurface9 *R_CreateSmallDepthStencil(byte *dxPtr)
{
    DxGlobals *dx = (DxGlobals *)dxPtr;
    IDirect3DSurface9 *existing = dx->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].depthStencilSurface;
    if (existing != NULL) {

        ((IUnknown_AddRefFn)VTABLE(existing)[1])((void *)existing);
        return dx->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].depthStencilSurface;
    }

    {
        void *device = ((DxGlobals *)(dxPtr))->device;
        void **vtable = VTABLE(device);
        IDirect3DSurface9 *newSurface = NULL;
        HRESULT hr;

        g_NoTextureID = 1;
        hr = ((D3DDevice_CreateDepthStencilFn)vtable[0x74 / 4])(
            device, 128, 128, D3DFMT_D24S8, 0, 0, 0,
            &newSurface, NULL);
        g_NoTextureID = 0;

        if (hr < 0) {
            const char *desc = R_ErrorDescription(hr);
            Com_Error(0, "Couldn't create a %i x %i depth-stencil surface: %s\n", 128, 128, desc);
        }

        return newSurface;
    }
}

void R_ShutdownRenderTargets(void)
{
    int renderTargetId;
    byte *dxPtr;

    for (renderTargetId = 0; renderTargetId <= 12; renderTargetId++) {

#if !defined(COD2_X64)
        if (renderTargetId == 1 || renderTargetId == 7) {
            continue;
        }
#endif

        dxPtr = DX();
        byte *rt = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[renderTargetId]);

        {
            IDirect3DSurface9 *colorSurf = DX_RT_COLOR_SURFACE(rt);
            if (colorSurf != NULL) {
                ((IUnknown_ReleaseFn)VTABLE(colorSurf)[2])((void *)colorSurf);
            }
        }

        {
            IDirect3DSurface9 *dsSurf = DX_RT_DEPTH_SURFACE(rt);
            if (dsSurf != NULL) {
                ((IUnknown_ReleaseFn)VTABLE(dsSurf)[2])((void *)dsSurf);
            }
        }

        {
            GfxImage *img = DX_RT_IMAGE(rt);
            if (img != NULL) {
                Image_Release(img);
            }
        }
    }

    dxPtr = DX();
#if defined(COD2_X64)
    memset(((DxGlobals *)dxPtr)->renderTargets, 0, sizeof(((DxGlobals *)dxPtr)->renderTargets));
#else
    memset(((char *)dxPtr + offsetof(DxGlobals, renderTargets[0].image)), 0, 0x104);
#endif

    ((DxGlobals *)(dxPtr))->singleSampleDepthStencilSurface  = 0;
}

void R_InitRenderTargets(void)
{
    byte *dxPtr;
    vidConfig_t *vidCfg;
    int fullWidth, fullHeight;
    void *device;
    void **vtable;
    HRESULT hr;
    D3DFORMAT backBufferFormat;
    D3DFORMAT surfaceFormat;
    _D3DSURFACE_DESC desc;

    dxPtr = DX();
    vidCfg = VIDCONFIG();
    fullWidth = vidCfg->width;
    DX_PTR()->renderTargets[0].width = fullWidth;
    fullHeight = vidCfg->height;
    DX_PTR()->renderTargets[0].height = fullHeight;

    device = ((DxGlobals *)(dxPtr))->device;
    vtable = VTABLE(device);
    hr = ((HRESULT(D3DVTCC *)(void *, UINT, byte *))vtable[0x38 / 4])(
#if defined(COD2_X64)
        device, 0, (byte *)&DX_PTR()->windows[0].swapChain);
#else
        device, 0, (byte *)&DX_PTR()->windows[0]);
#endif

    if (hr < 0) {
        const char *errDesc = R_ErrorDescription(hr);
        Com_Error(0, "Couldn't get an interface to the swap chain: %s\n", errDesc);
    }

    do {
        dxPtr = DX();
        device = ((DxGlobals *)(dxPtr))->device;
        vtable = VTABLE(device);
        R_LOG("dx.device->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &renderTarget->colorSurface )\n");
        hr = ((HRESULT(D3DVTCC *)(void *, DWORD, DWORD, DWORD, byte *))vtable[0x48 / 4])(
            device, 0, 0, 0, (byte *)&DX_PTR()->renderTargets[0].colorSurface);

    } while (*(int *)&alwaysfails != 0);

    vidCfg = VIDCONFIG();
    fullWidth = vidCfg->width;
    fullHeight = vidCfg->height;

    {
        byte *creatingTex = (byte *)imp_g_NoTextureID;
        *creatingTex = 1;

        dxPtr = DX();
        device = ((DxGlobals *)(dxPtr))->device;
        vtable = VTABLE(device);

        hr = ((D3DDevice_CreateDepthStencilFn)vtable[0x74 / 4])(
            device, fullWidth, fullHeight, D3DFMT_D24S8,
            ((DxGlobals *)(dxPtr))->multiSampleType       , ((DxGlobals *)(dxPtr))->multiSampleQuality ,
            0, &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_FRAME_BUFFER].depthStencilSurface, NULL);

        *creatingTex = 0;

        if (hr < 0) {
            const char *errDesc = R_ErrorDescription(hr);
            Com_Error(0, "Couldn't create a %i x %i depth-stencil surface: %s\n", fullWidth, fullHeight, errDesc);
        }
    }

    {
        void (*riPrintf)(int, const char *, ...) = (void (*)(int, const char *, ...)) * (void (**)())&ri;
        riPrintf(0, "Requested frame buffer to be %s\n", "24-bit color with 8-bit alpha");

        dxPtr = DX();
        {
            IDirect3DSurface9 *colorSurf = ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_FRAME_BUFFER].colorSurface;
            void **surfVtable = VTABLE(colorSurf);
            ((D3DSurface_GetDescFn)surfVtable[0x30 / 4])((void *)colorSurf, &desc);
        }

        surfaceFormat = desc.Format;
        ((DxGlobals *)dxPtr)->backBufferFormat = surfaceFormat;

        riPrintf = (void (__cdecl *)(int,const char *,...))(*(void (**)())&ri);
        riPrintf(0, "DirectX returned a frame buffer that is %s\n", R_DescribeFormat(surfaceFormat));
    }

    dxPtr = DX();
    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(0, 0, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_DYNAMICSHADOWS]);

#if defined(COD2_X64)
    R_InitFullscreenRenderTargetImage(8, 0, backBufferFormat, RENDERTARGET_USAGE_TEXTURE,
                                      &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_RESOLVED_POST_SUN]);
#else
    {
        byte *rt3 = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[3]);
        byte *rt1 = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[1]);
        *(void **)(rt1 + 0) = *(void **)(rt3 + 0);
        *(void **)(rt1 + 4) = *(void **)(rt3 + 4);
        *(void **)(rt1 + 8) = *(void **)(rt3 + 8);
        *(int *)(rt1 + 12) = *(int *)(rt3 + 12);
        *(int *)(rt1 + 16) = *(int *)(rt3 + 16);
    }
#endif

    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(9, 0, backBufferFormat, RENDERTARGET_USAGE_TEXTURE,
                                      &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_RESOLVED_SCENE]);

#if defined(COD2_X64)
    R_InitFullscreenRenderTargetImage(10, 0, backBufferFormat, RENDERTARGET_USAGE_TEXTURE,
                                      &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SAVED_SCREEN]);
#else
    {
        byte *rt1 = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[1]);
        byte *rt7 = ((byte *)&((DxGlobals *)(dxPtr))->renderTargets[7]);
        *(void **)(rt7 + 0) = *(void **)(rt1 + 0);
        *(void **)(rt7 + 4) = *(void **)(rt1 + 4);
        *(void **)(rt7 + 8) = *(void **)(rt1 + 8);
        *(int *)(rt7 + 12) = *(int *)(rt1 + 12);
        *(int *)(rt7 + 16) = *(int *)(rt1 + 16);
    }
#endif

    {
        GfxImage *image;
        IDirect3DSurface9 *depthSurface;

        image = Image_AllocProg(1, 6);
        ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].image = image;

        Image_SetupRenderTarget(image, 128, 128, D3DFMT_A8R8G8B8);

        image = ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].image;
        ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].colorSurface = Image_GetSurface(image);
        ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].width = 128;
        ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].height = 128;

        Image_TrackTexture(((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].image, 3, D3DFMT_A8R8G8B8, 128, 128, 1);

        depthSurface = R_CreateSmallDepthStencil(dxPtr);
        dxPtr = DX();
        ((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_SHADOWCOOKIE].depthStencilSurface = depthSurface;
    }

    {
        GfxImage *image;
        IDirect3DSurface9 *depthSurface;

        image = Image_AllocProg(2, 6);
        ((DxGlobals *)dxPtr)->renderTargets[5].image = image;

        Image_SetupRenderTarget(image, 128, 128, D3DFMT_A8R8G8B8);

        image = ((DxGlobals *)dxPtr)->renderTargets[5].image;
        ((DxGlobals *)dxPtr)->renderTargets[5].colorSurface = Image_GetSurface(image);
        ((DxGlobals *)dxPtr)->renderTargets[5].width = 128;
        ((DxGlobals *)dxPtr)->renderTargets[5].height = 128;

        Image_TrackTexture(((DxGlobals *)dxPtr)->renderTargets[5].image, 3, D3DFMT_A8R8G8B8, 128, 128, 1);

        depthSurface = R_CreateSmallDepthStencil(dxPtr);
        dxPtr = DX();
        ((DxGlobals *)dxPtr)->renderTargets[5].depthStencilSurface = depthSurface;
    }

    dxPtr = DX();
    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
#if defined(COD2_X64)
    /* Windows 1.3 10023163: full-size R32F, sharing single-sample depth. */
    if (r_rendererInUse->current.integer != 2 && r_zFeather->current.enabled)
        R_InitFullscreenRenderTargetImage(12, 0, D3DFMT_R32F, RENDERTARGET_USAGE_RENDER,
                                          &((DxGlobals *)dxPtr)->renderTargets[R_RENDERTARGET_FLOAT_Z]);
#endif
    R_InitFullscreenRenderTargetImage(3, R_REDUCED_PICMIP, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      (GfxRenderTarget *)&((DxGlobals *)dxPtr)->renderTargets[8].image);

    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(4, R_REDUCED_PICMIP, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      (GfxRenderTarget *)&((DxGlobals *)dxPtr)->renderTargets[9].image);

    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(5, R_REDUCED_PICMIP, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      (GfxRenderTarget *)&((DxGlobals *)dxPtr)->renderTargets[10].image);

    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(6, R_REDUCED_PICMIP, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      (GfxRenderTarget *)&((DxGlobals *)dxPtr)->renderTargets[11].image);

    backBufferFormat = ((DxGlobals *)(dxPtr))->backBufferFormat;
    R_InitFullscreenRenderTargetImage(7, R_REDUCED_PICMIP, backBufferFormat, RENDERTARGET_USAGE_RENDER,
                                      (GfxRenderTarget *)&((DxGlobals *)dxPtr)->renderTargets[12].image);

#if COD2_MAC_D3D9
    /* With anti-aliasing the scene gets a multisampled target of its own
     * (rb_scenetarget.c); the frame buffer set up above is then the resolved picture. */
    {
        extern int RB_SceneTarget_Create(void *device, void **colorSlot, void **depthSlot, int width, int height,
                                         unsigned format, unsigned samples, unsigned quality, void *resolved);
        DxGlobals *globals = (DxGlobals *)DX();
        GfxRenderTarget *frameBuffer = &globals->renderTargets[R_RENDERTARGET_FRAME_BUFFER];
        if (!RB_SceneTarget_Create(globals->device, (void **)&frameBuffer->colorSurface,
                                   (void **)&frameBuffer->depthStencilSurface, frameBuffer->width, frameBuffer->height,
                                   (unsigned)globals->backBufferFormat, (unsigned)globals->multiSampleType,
                                   (unsigned)globals->multiSampleQuality, globals->renderTargets[2].colorSurface))
            Com_Error(0, "Couldn't create a %i x %i multisampled render target\n", frameBuffer->width,
                      frameBuffer->height);
    }
#endif

    return;
}
