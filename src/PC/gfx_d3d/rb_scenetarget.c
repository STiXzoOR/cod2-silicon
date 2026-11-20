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
 * Without anti-aliasing there is nothing to resolve, but glow still has to read
 * the scene, and the game copies the back buffer for it and then goes on drawing
 * into the back buffer. Here the scene is drawn straight into the texture glow
 * reads (the engine's resolved-scene target) in the frames glow runs, and that
 * texture is drawn into the back buffer once, where the HUD follows: no copy,
 * and the back buffer is entered once. Frames without glow draw into the back
 * buffer as the game does.
 *
 * Render scale (r_renderScale below 100): the scene and everything that reads it
 * are made smaller than the display (r_rendertarget.c: R_SceneWidth), and the
 * resolved picture is stretched over the back buffer where it would have been
 * copied, before glow, the HUD and the menus, which stay at the display's size.
 * The scene then always has a target of its own, with or without anti-aliasing,
 * because its depth buffer no longer fits the back buffer.
 *
 * rb_backend.c decides when the scene starts and ends; this file owns the
 * surfaces and swaps them in and out of the engine's frame buffer record. */
#if COD2_MAC_D3D9
#include <stddef.h>
#include <stdint.h>

typedef int32_t HRESULT;
typedef uint32_t DWORD;

#define VT(obj) (*(void ***)(obj))
enum {
    DEV_CreateTexture = 23, DEV_CreateRenderTarget = 28, DEV_StretchRect = 34, TEX_GetSurfaceLevel = 18,
    UNK_AddRef = 1, UNK_Release = 2
};

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
static int *s_widthSlot, *s_heightSlot;
static int s_inScene;
static int s_plain; /* one sample a pixel: the scene's target is the engine's resolved-scene texture */
static void *s_resolved; /* the engine's resolved-scene texture's surface (not owned) */
static int s_sceneWidth, s_sceneHeight, s_backWidth, s_backHeight;
static int s_scaled; /* the scene is smaller than the back buffer */
typedef struct {
    void *texture, *surface;
    unsigned width, height;
} Scratch;
static Scratch s_scratch[3];

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
        if (s_scaled) {
            *s_widthSlot = s_backWidth;
            *s_heightSlot = s_backHeight;
        }
        ((ReleaseFn)VT(s_sceneColor)[UNK_Release])(s_sceneColor);
    }
    ScratchRelease(&s_scratch[0]);
    ScratchRelease(&s_scratch[1]);
    ScratchRelease(&s_scratch[2]);
    s_sceneColor = s_sceneDepth = s_backBuffer = s_resolved = NULL;
    s_colorSlot = s_depthSlot = NULL;
    s_widthSlot = s_heightSlot = NULL;
    s_inScene = s_plain = s_scaled = 0;
}

/* Called once the engine has the back buffer in *colorSlot and the scene's depth
 * buffer in *depthSlot. `resolved` is the surface of the engine's resolved-scene
 * texture, or NULL. *widthSlot by *heightSlot is the back buffer's size as the
 * engine's record has it; width by height is the scene's, which is smaller with
 * a render scale. Returns 0 if the target is wanted and cannot be made. */
int RB_SceneTarget_Create(void *device, void **colorSlot, void **depthSlot, int *widthSlot, int *heightSlot, int width,
                          int height, DWORD format, DWORD samples, DWORD quality, void *resolved)
{
    RB_SceneTarget_Release();
    if (!RB_SceneTarget_Wanted() || (samples <= 1 && !resolved))
        return 1;
    s_scaled = resolved && (width != *widthSlot || height != *heightSlot);
    if (samples <= 1) {
        s_sceneColor = resolved;
        ((ReleaseFn)VT(resolved)[UNK_AddRef])(resolved);
        s_plain = 1;
    } else if (((CreateRenderTargetFn)VT(device)[DEV_CreateRenderTarget])(device, (unsigned)width, (unsigned)height,
                                                                          format, samples, quality, 0, &s_sceneColor,
                                                                          NULL) < 0) {
        s_sceneColor = NULL;
        s_scaled = 0;
        return 0;
    }
    s_device = device;
    s_colorSlot = colorSlot;
    s_depthSlot = depthSlot;
    s_widthSlot = widthSlot;
    s_heightSlot = heightSlot;
    s_backWidth = *widthSlot;
    s_backHeight = *heightSlot;
    s_sceneWidth = width;
    s_sceneHeight = height;
    s_resolved = resolved;
    s_backBuffer = *colorSlot;
    s_sceneDepth = *depthSlot;
    if (!s_plain || s_scaled)
        *depthSlot = NULL; /* a frame starts outside the scene */
    return 1;
}

/* Is the scene smaller than the back buffer (r_renderScale)? */
int RB_SceneTarget_Scaled(void)
{
    return s_scaled;
}

/* Is the scene's target the resolved-scene texture itself (no anti-aliasing)? */
int RB_SceneTarget_Plain(void)
{
    return s_plain;
}

/* Without anti-aliasing the scene's depth buffer fits the back buffer too: a pass drawn
 * there after the scene may bind it to tell the sky from the world. NULL otherwise. */
void *RB_SceneTarget_PlainDepth(void)
{
    return s_plain && !s_scaled ? s_sceneDepth : NULL;
}

/* The frame is over. Without anti-aliasing the next one starts on the back buffer with
 * its depth buffer, as the game has it, and takes the scene's target only if glow runs.
 * Returns 1 if that changed the engine's record. */
int RB_SceneTarget_FrameEnd(void)
{
    if (!s_plain || s_scaled || s_inScene || *s_depthSlot == s_sceneDepth)
        return 0;
    *s_colorSlot = s_backBuffer;
    *s_depthSlot = s_sceneDepth;
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
    if (s_scaled) {
        *s_widthSlot = scene ? s_sceneWidth : s_backWidth;
        *s_heightSlot = scene ? s_sceneHeight : s_backHeight;
    }
    return 1;
}

/* The scene's samples, resolved into the back buffer. */
void RB_SceneTarget_Resolve(void)
{
    if (s_sceneColor)
        ((StretchRectFn)VT(s_device)[DEV_StretchRect])(s_device, s_sceneColor, NULL, s_backBuffer, NULL, 2);
}

/* With a render scale: the scene's samples, resolved into the resolved-scene texture,
 * which is the scene's size, so this is the plain resolve it is at full size. The
 * caller then draws that texture over the back buffer. Without anti-aliasing the scene
 * was drawn into that texture and there is nothing to do. */
void RB_SceneTarget_ResolveToTexture(void)
{
    if (s_sceneColor && !s_plain && s_resolved)
        ((StretchRectFn)VT(s_device)[DEV_StretchRect])(s_device, s_sceneColor, NULL, s_resolved, NULL, 2);
}

/* A render-target texture of this size and format, kept in `slot` and made again when
 * the size changes. Returns the texture and puts its surface in *surface; NULL if it
 * cannot be made. rb_backend.c uses slot 0 for the scene at half size (blur), slot 2
 * for the copy CoD2x's film pass reads, and slot 1 for glow's set-up picture, where the reduced targets are an eighth of the screen
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
