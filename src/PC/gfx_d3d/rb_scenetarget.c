/* The 3D scene's own frame buffer (COD2_MAC_D3D9 builds, anti-aliasing on).
 *
 * The game draws everything into one multisampled back buffer: the scene, then
 * glow, then the HUD. Glow has to read the scene, so the graphics chip must stop
 * drawing into the back buffer, hand over a copy, and start again. On this kind
 * of chip stopping and starting is not free: every sample of the buffer, colour
 * and depth, is written out to memory and read back in. At 5120x2880 with four
 * samples that is 59 million samples each way, and it costs more than drawing
 * the scene does.
 *
 * So the multisampled buffer is used for the scene alone and never returned to.
 * The scene is drawn into a target made here. Where the game applies its late
 * post effects (the end of the scene) the samples are resolved once, and what
 * follows (glow, sun flare, HUD, menus) is drawn onto the resolved picture in
 * the device's back buffer, which has one sample per pixel and no depth buffer.
 * Frames with no scene (menus, loading) never touch the multisampled target.
 *
 * The scene keeps every sample it had. What changes is that the edges of HUD
 * rectangles are no longer anti-aliased by the buffer; the HUD's shapes come
 * from its textures, which are filtered as before.
 *
 * rb_backend.c decides when the scene starts and ends; this file owns the
 * surfaces and swaps them in and out of the engine's frame buffer record. */
#if COD2_MAC_D3D9
#include <stddef.h>
#include <stdint.h>

typedef int32_t HRESULT;
typedef uint32_t DWORD;

#define VT(obj) (*(void ***)(obj))
enum { DEV_CreateTexture = 23, DEV_CreateRenderTarget = 28, DEV_StretchRect = 34, TEX_GetSurfaceLevel = 18, UNK_Release = 2 };

typedef HRESULT (*CreateRenderTargetFn)(void *, unsigned, unsigned, DWORD, DWORD, DWORD, int, void **, void *);
typedef HRESULT (*StretchRectFn)(void *, void *, const void *, void *, const void *, DWORD);
typedef uint32_t (*ReleaseFn)(void *);
typedef HRESULT (*CreateTextureFn)(void *, unsigned, unsigned, unsigned, DWORD, DWORD, DWORD, void **, void *);
typedef HRESULT (*GetSurfaceFn)(void *, DWORD, void **);

extern int Dvar_GetInt(const char *name);

static void *s_device;
static void *s_sceneColor;              /* made here: the scene's multisampled colour */
static void *s_sceneDepth, *s_backBuffer; /* the engine's: the scene's depth, the device's back buffer */
static void **s_colorSlot, **s_depthSlot; /* the engine's frame buffer record */
static int s_inScene;
typedef struct {
    void *texture, *surface;
    unsigned width, height;
} Scratch;
static Scratch s_scratch[2];

static void ScratchRelease(Scratch *scratch)
{
    if (scratch->surface)
        ((ReleaseFn)VT(scratch->surface)[UNK_Release])(scratch->surface);
    if (scratch->texture)
        ((ReleaseFn)VT(scratch->texture)[UNK_Release])(scratch->texture);
    scratch->surface = scratch->texture = NULL;
}

/* Is the scene drawn into a target of its own? Read when the device is made
 * (the back buffer then has one sample) and when the targets are. */
int RB_SceneTarget_Wanted(void)
{
    return Dvar_GetInt("r_sceneTarget") != 0;
}

/* Put the engine's record back as it was made, before the engine releases it. */
void RB_SceneTarget_Release(void)
{
    if (s_sceneColor) {
        *s_colorSlot = s_backBuffer;
        *s_depthSlot = s_sceneDepth;
        ((ReleaseFn)VT(s_sceneColor)[UNK_Release])(s_sceneColor);
    }
    ScratchRelease(&s_scratch[0]);
    ScratchRelease(&s_scratch[1]);
    s_sceneColor = s_sceneDepth = s_backBuffer = NULL;
    s_colorSlot = s_depthSlot = NULL;
    s_inScene = 0;
}

/* Called once the engine has the back buffer in *colorSlot and the scene's
 * multisampled depth buffer in *depthSlot. Returns 0 if the target is wanted and
 * cannot be made. */
int RB_SceneTarget_Create(void *device, void **colorSlot, void **depthSlot, int width, int height, DWORD format,
                          DWORD samples, DWORD quality)
{
    RB_SceneTarget_Release();
    if (samples <= 1 || !RB_SceneTarget_Wanted())
        return 1;
    if (((CreateRenderTargetFn)VT(device)[DEV_CreateRenderTarget])(device, (unsigned)width, (unsigned)height, format,
                                                                   samples, quality, 0, &s_sceneColor, NULL) < 0) {
        s_sceneColor = NULL;
        return 0;
    }
    s_device = device;
    s_colorSlot = colorSlot;
    s_depthSlot = depthSlot;
    s_backBuffer = *colorSlot;
    s_sceneDepth = *depthSlot;
    *depthSlot = NULL; /* a frame starts outside the scene */
    return 1;
}

int RB_SceneTarget_InScene(void)
{
    return s_inScene;
}

/* Point the engine's frame buffer record at the scene's target (1) or at the back
 * buffer (0). Returns 1 if that changed it; the caller then binds it again. */
int RB_SceneTarget_Select(int scene)
{
    if (!s_sceneColor || !scene == !s_inScene)
        return 0;
    s_inScene = scene;
    *s_colorSlot = scene ? s_sceneColor : s_backBuffer;
    *s_depthSlot = scene ? s_sceneDepth : NULL;
    return 1;
}

/* The scene's samples, resolved into the back buffer. */
void RB_SceneTarget_Resolve(void)
{
    if (s_sceneColor)
        ((StretchRectFn)VT(s_device)[DEV_StretchRect])(s_device, s_sceneColor, NULL, s_backBuffer, NULL, 2);
}

/* A render-target texture of this size and format, kept in `slot` and made again when
 * the size changes. Returns the texture and puts its surface in *surface; NULL if it
 * cannot be made. rb_backend.c uses slot 0 for the scene at half size (blur) and slot
 * 1 for glow's set-up picture, where the reduced targets are an eighth of the screen
 * (r_rendertarget.c: R_ReducedTargetPicmip). */
void *RB_SceneTarget_Scratch(void *device, int slot, unsigned width, unsigned height, DWORD format, void **surface)
{
    Scratch *scratch = &s_scratch[slot];

    if (!scratch->texture || scratch->width != width || scratch->height != height) {
        ScratchRelease(scratch);
        if (((CreateTextureFn)VT(device)[DEV_CreateTexture])(device, width, height, 1, 1 /* render target */, format, 0,
                                                              &scratch->texture, NULL) < 0 ||
            ((GetSurfaceFn)VT(scratch->texture)[TEX_GetSurfaceLevel])(scratch->texture, 0, &scratch->surface) < 0) {
            ScratchRelease(scratch);
            return NULL;
        }
        scratch->width = width;
        scratch->height = height;
    }
    *surface = scratch->surface;
    return scratch->texture;
}

/* All of one surface into another, filtered: at exactly half the size, each pixel is
 * the average of four. */
int RB_SceneTarget_Shrink(void *device, void *from, void *to)
{
    return ((StretchRectFn)VT(device)[DEV_StretchRect])(device, from, NULL, to, NULL, 2 /* linear */) >= 0;
}
#endif /* COD2_MAC_D3D9 */
