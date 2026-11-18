/* The effects layer (COD2_MAC_D3D9 builds).
 *
 * Smoke, dust and fire are the tail of the emissive pass: dozens of blended
 * layers that each cover much of the screen. Drawn straight onto a multisampled
 * frame buffer, every one of them is blended into every sample of every pixel,
 * and standing in smoke that is nearly all a frame costs.
 *
 * The layer is a target of their own: one sample per pixel, at the picture's
 * size, a half or a quarter of it. It keeps colour already multiplied by
 * coverage, and in its alpha channel how much of the scene behind still shows
 * through, so one blend (ONE, SRCALPHA) puts the whole stack over the picture.
 *
 * It is used only when it is needed. An occlusion query counts the pixels the
 * effects cover; while that is little enough to draw at the frame rate asked
 * for (com_maxfps), they are drawn straight onto the frame buffer as they
 * always were. Past that the layer takes them, and past what the layer can do
 * at the picture's size it becomes a half, then a quarter. That much overdraw
 * only comes from large, soft sprites stacked up, which a smaller layer shows
 * the same. (One thing does differ, and is why the layer waits to be needed:
 * a faint sprite blended straight onto an 8-bit frame buffer can fall below
 * one step and vanish, while the layer adds such sprites up. Distant mist
 * comes out a few steps stronger through the layer than the game draws it.)
 *
 * Where the layer is used, it gives what drawing onto the frame buffer gives:
 *
 * - Effects are hidden by the scene as before. The float-Z prepass that soft
 *   particles need has already drawn the scene into a single-sample depth
 *   buffer; here that buffer is a texture as well (INTZ), and while r_fxLayer
 *   is on, models are drawn into it too (afterwards and without colour, so the
 *   float-Z picture keeps what it had). That texture is the layer's depth
 *   buffer. A smaller layer tests against a smaller copy that keeps the
 *   farthest depth of each block. Copying the frame buffer's own depth instead
 *   costs more than the smoke does when it is multisampled.
 *
 * - Under anti-aliasing a pixel on an edge has samples on two surfaces, and the
 *   layer has one answer per pixel. The samples that do not lie at the prepass's
 *   depth for their pixel are marked in the stencil buffer, nearer or farther,
 *   and the layer is blended over the others first. A marked sample then takes
 *   the layer's value for the surface next door: at the layer's full size, from
 *   the neighbouring texel nearest or farthest in depth. (Drawing the effects a
 *   second time into the marked samples alone would be exact, and was tried: the
 *   graphics chip takes as long to turn the unmarked samples away as it does to
 *   draw them.)
 *
 * - A smaller layer is not stretched over the picture like an image, or smoke
 *   would leak across every edge. Each pixel takes only the texels that were
 *   tested against its own surface, and the view weapon takes none
 *   (BlendShader).
 *
 * - The layer is drawn straight after the prepass, ahead of the scene, and put
 *   over the frame buffer where the effects stand in the emissive pass
 *   (RB_FxLayer_BeginEarly), so the scene's buffer is not interrupted for it.
 *
 * - Only blends the target can hold go into it: source ZERO, ONE or SRCALPHA
 *   against destination ZERO, ONE or INVSRCALPHA. A material that writes depth
 *   or uses the stencil buffer is drawn as it always was.
 *
 * - With soft particles off (r_zFeather 0) there is no float-Z prepass. The scene's
 *   depth then comes from the dynamic shadows' depth prepass, with the models added,
 *   or, with those off as well, from a depth-only drawing of the scene made in the
 *   frames that use the layer (rb_backend.c, r_scene.c). The Direct3D 7 renderer
 *   gets it the second way too (RB_FxLayer_DepthPassBegin).
 *
 * - Some smoke also blends into the frame buffer's alpha, which glow reads to
 *   tell sky from everything else. Each such sprite leaves alpha as
 *   alpha * b + a, with a and b set by its blend. Those sprites are drawn a
 *   second time into a target that keeps the two running totals (the product
 *   of the b's in its colour, the sum in its alpha), and the frame buffer's
 *   alpha is brought up to date from it in two blends. While glow takes the
 *   world from the depth buffer (r_glowSkyFromDepth) only sky samples need
 *   that, so only they get it.
 *
 * One more thing is done for speed alone. Smoke is drawn with an alpha test
 * that only skips pixels of no opacity at all (alpha greater than zero, under
 * a blend that multiplies by alpha). Such a pixel changes nothing either way,
 * and the test makes the shader discard, which costs on this kind of graphics
 * chip, so in the layer that test is left off. */
#if COD2_MAC_D3D9
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int32_t HRESULT;
typedef uint32_t DWORD;
typedef struct { DWORD X, Y, Width, Height; float MinZ, MaxZ; } Viewport;
typedef struct { DWORD Format, Type, Usage, Pool, MultiSampleType, MultiSampleQuality, Width, Height; } SurfaceDesc;
typedef struct { float x, y, z, rhw, u, v; } QuadVertex;

#define VT(obj) (*(void ***)(obj))
#define CALL(type, obj, slot) ((type)VT(obj)[slot])
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef HRESULT (*CreateTextureFn)(void *, unsigned, unsigned, unsigned, DWORD, DWORD, DWORD, void **, void *);
typedef HRESULT (*CreateQueryFn)(void *, int, void **);
typedef HRESULT (*IssueFn)(void *, DWORD);
typedef HRESULT (*GetDataFn)(void *, void *, DWORD, DWORD);
typedef HRESULT (*GetSurfaceFn)(void *, DWORD, void **);
typedef HRESULT (*SetSurfaceFn)(void *, DWORD, void *);
typedef HRESULT (*GetDepthFn)(void *, void **);
typedef HRESULT (*SetDepthFn)(void *, void *);
typedef HRESULT (*ClearFn)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD);
typedef HRESULT (*ViewportFn)(void *, Viewport *);
typedef HRESULT (*SetStateFn)(void *, DWORD, DWORD);
typedef HRESULT (*GetStateFn)(void *, DWORD, DWORD *);
typedef HRESULT (*SetStageFn)(void *, DWORD, DWORD, DWORD);
typedef HRESULT (*GetStageFn)(void *, DWORD, DWORD, DWORD *);
typedef HRESULT (*SetObjectFn)(void *, void *);
typedef HRESULT (*GetObjectFn)(void *, void **);
typedef HRESULT (*SetTextureFn)(void *, DWORD, void *);
typedef HRESULT (*GetTextureFn)(void *, DWORD, void **);
typedef HRESULT (*SetFvfFn)(void *, DWORD);
typedef HRESULT (*DrawUpFn)(void *, DWORD, unsigned, const void *, unsigned);
typedef HRESULT (*GetDescFn)(void *, SurfaceDesc *);
typedef HRESULT (*CreateShaderFn)(void *, const void *, void **);
typedef uint32_t (*ReleaseFn)(void *);
typedef uint32_t (*AddRefFn)(void *);
typedef void *(*BlobPointerFn)(void *);

enum {
    DEV_CreateTexture = 23, DEV_SetRenderTarget = 37, DEV_GetRenderTarget = 38, DEV_SetDepthStencilSurface = 39,
    DEV_GetDepthStencilSurface = 40, DEV_Clear = 43, DEV_SetViewport = 47, DEV_GetViewport = 48, DEV_SetRenderState = 57,
    DEV_GetRenderState = 58, DEV_GetTexture = 64, DEV_SetTexture = 65, DEV_GetTextureStageState = 66,
    DEV_SetTextureStageState = 67, DEV_GetSamplerState = 68, DEV_SetSamplerState = 69, DEV_DrawPrimitiveUP = 83,
    DEV_SetVertexDeclaration = 87, DEV_GetVertexDeclaration = 88, DEV_SetFVF = 89, DEV_SetVertexShader = 92,
    DEV_GetVertexShader = 93, DEV_CreatePixelShader = 106, DEV_SetPixelShader = 107, DEV_GetPixelShader = 108,
    DEV_CreateQuery = 118, QUERY_Issue = 6, QUERY_GetData = 7,
    TEX_GetSurfaceLevel = 18, SURF_GetDesc = 12, UNK_AddRef = 1, UNK_Release = 2, BLOB_GetBufferPointer = 3,
};
enum {
    RS_ZENABLE = 7, RS_ZWRITEENABLE = 14, RS_ALPHATESTENABLE = 15, RS_SRCBLEND = 19, RS_DESTBLEND = 20, RS_CULLMODE = 22,
    RS_ZFUNC = 23, RS_ALPHAREF = 24, RS_ALPHAFUNC = 25, RS_ALPHABLENDENABLE = 27, RS_FOGENABLE = 28, RS_STENCILENABLE = 52,
    RS_STENCILFAIL = 53, RS_STENCILZFAIL = 54, RS_STENCILPASS = 55, RS_STENCILFUNC = 56, RS_STENCILREF = 57,
    RS_STENCILMASK = 58, RS_STENCILWRITEMASK = 59, RS_LIGHTING = 137, RS_COLORWRITEENABLE = 168, RS_BLENDOP = 171,
    RS_SCISSORTESTENABLE = 174, RS_TWOSIDEDSTENCILMODE = 185, RS_SLOPESCALEDEPTHBIAS = 175, RS_DEPTHBIAS = 195, RS_SEPARATEALPHABLENDENABLE = 206, RS_SRCBLENDALPHA = 207,
    RS_DESTBLENDALPHA = 208, RS_BLENDOPALPHA = 209,
};
enum { BLEND_ZERO = 1, BLEND_ONE = 2, BLEND_SRCALPHA = 5, BLEND_INVSRCALPHA = 6 };
enum { CMP_LESS = 2, CMP_EQUAL = 3, CMP_GREATER = 5, CMP_ALWAYS = 8, OP_KEEP = 1, OP_ZERO = 2, OP_REPLACE = 3 };
enum { FORMAT_A8R8G8B8 = 21, FORMAT_A16B16G16R16 = 36, FORMAT_INTZ = 0x5A544E49 };
enum { USAGE_RENDERTARGET = 1, USAGE_DEPTHSTENCIL = 2, QUERY_OCCLUSION = 9, ISSUE_END = 1, ISSUE_BEGIN = 2 };
/* Stencil bits of a sample the layer cannot stand for: it lies beyond, or in front of,
 * the depth the layer was tested against. */
enum { MARK_FAR = 0x40, MARK_NEAR = 0x80, MARK_ANY = 0xc0 };
/* The engine draws the view weapon into depths 0 to 0.2 (rb_backend.c: RB_SetDepthRange);
 * the world's nearest surface lies beyond that. */
#define WEAPON_DEPTH 0.2
#define NEAR_EVEN "1.03" /* a block's depths count as one surface within this ratio */
enum { LEVELS = 3, DIRECT = -1 }; /* the picture's size (0), a half (1) and a quarter (2); or no layer */

extern void RB_ClearAllStreamSources(void);
extern int Dvar_GetInt(const char *name);
extern HRESULT Win_D3D9_CompileShader(const char *source, int length, const char *name, const char *entry,
                                      const char *target, void **shader, void **messages);

/* A layer of one size and what it is drawn and blended with. Each is made the first
 * time it is used and kept until the renderer's targets go. */
typedef struct {
    void *colorTexture, *colorSurface;
    void *depthTexture, *depthSurface; /* the scene's depth at this size; level 0 uses the scene's own */
    void *reduceShader;                /* fills that depth from the scene's */
    void *alphaTexture, *alphaSurface; /* what its sprites do to the frame buffer's alpha */
    void *markShader[2];               /* samples farther, and nearer, than the layer's depth */
    void *fixShader[2];                /* the layer's value for each of the two */
    unsigned width, height;
    void *blendShader[2][3]; /* a smaller layer: BlendShader, by one picture or two and by `pick` */
    void *farTexture, *farSurface; /* a smaller layer: the farthest depth its effects reach in each texel */
    /* A smaller layer, a second time: tested against the nearest depth of each block. */
    void *nearTexture, *nearSurface, *nearColorTexture, *nearColorSurface, *reduceNearShader;
    /* A smaller layer: the scene's float-Z at this size, for the effects that feather (CopyFloatZ). */
    void *floatZTexture, *floatZSurface, *floatZShader;
    int floatZFailed;
    int failed;
} Layer;

static void *s_device;
static int s_active;    /* effects are being drawn into the layer */
static int s_depthOnly; /* the prepass is drawing models and sky: depth, no colour */
static int s_depthPass; /* the Direct3D 7 depth pass is bound (RB_FxLayer_DepthPassBegin) */
static void *s_depthPassColor, *s_depthPassDepth; /* what was bound before it */
static void *s_sceneDepthTexture, *s_sceneDepthSurface; /* the prepass's depth, at the picture's size */
static void *s_sceneFloatZ; /* the engine's float-Z texture, when this frame's effects feather against it */
static int s_floatZReady;   /* the layer in use holds this frame's copy of it */
static unsigned s_sceneWidth, s_sceneHeight;
static Layer s_layers[LEVELS], *s_layer; /* s_layer: the one in use */
static Layer *GetLayer(int level);
static int s_level;
static unsigned s_samples;              /* samples per pixel of the frame buffer */
static void *s_savedColor, *s_savedDepth;
static Viewport s_viewport;             /* what the engine last asked for, at full size */
/* The count of pixels the effects cover. */
static void *s_opaqueShader; /* writes 1 to every channel it may */
static void *s_filmShader[3]; /* RB_ScreenPass_Film: everything, sky alone, sky told apart by the prepass's depth */
static float s_filmValues[11]; /* the settings those were written for */
static int s_filmValued;
static int s_sceneDepthDrawn; /* the prepass has put this frame's scene, models included, into its depth */
static void *s_redShader;    /* a texture's red channel in every channel */
static int s_alphaPass;      /* the sprites that blend into alpha are being drawn a second time */
static int s_alphaDrawn;     /* ...and the frame buffer's alpha is owed the result */
static int s_skyOnlyAlpha;   /* this frame only sky samples need it */
static DWORD s_destBlend;    /* DESTBLEND as the engine wants it */
static void *s_query;
static int s_queryPending, s_queryMode, s_measuring;
static double s_fill;                   /* frame buffer pixels, as last counted */
static int s_farPass;        /* the run is being drawn a last time, for the depth its effects reach */
static int s_nearPass;       /* the run is being drawn into the layer's second picture */
static int s_nearFrame = 1;  /* this frame's smaller layer has a second picture */
static int s_nearOn = 1;     /* the frame has time for it (NearGovern) */
static int s_nearDrawn;      /* it was drawn in the frame before this */
static double s_nearOffFill; /* the fill at which it stopped being affordable */
static double s_frameMs;     /* the time between frames, smoothed */
static uint64_t s_frameAt;
static DWORD s_zFuncWanted, s_zEnableWanted; /* the engine's, while that pass has its own */
static int s_early;          /* the layer is being drawn ahead of the scene (RB_FxLayer_BeginEarly) */
static int s_earlyPending;   /* and waits to be put over the frame buffer */
static int s_mode = DIRECT;             /* how the effects are drawn this frame: DIRECT or a level */
/* States the layer sets for itself while the engine's own values wait. */
static const DWORD layerStates[] = { RS_COLORWRITEENABLE, RS_SEPARATEALPHABLENDENABLE, RS_SRCBLENDALPHA,
    RS_DESTBLENDALPHA, RS_BLENDOPALPHA, RS_ZWRITEENABLE };
static DWORD s_layerWanted[COUNT(layerStates)], s_colorWriteWanted;
/* The alpha test as the engine wants it, and whether it is left off for now. */
static DWORD s_alphaTest, s_alphaFunc, s_alphaRef, s_sourceBlend, s_alphaTestApplied;

static void SetState(DWORD state, DWORD value)
{
    CALL(SetStateFn, s_device, DEV_SetRenderState)(s_device, state, value);
}

static DWORD GetState(DWORD state)
{
    DWORD value = 0;
    CALL(GetStateFn, s_device, DEV_GetRenderState)(s_device, state, &value);
    return value;
}

static void Release(void **object)
{
    if (*object) {
        CALL(ReleaseFn, *object, UNK_Release)(*object);
        *object = NULL;
    }
}

/* 0 is off, 1 as needed, 2 always at the picture's own size, 3 a half, 4 a quarter. */
static int Setting(void)
{
    return Dvar_GetInt("r_fxLayer");
}

/* How far, as a share of its distance, a sample may lie from the depth the layer was
 * tested against and still take the layer. A smaller layer blends texels that were
 * tested a few pixels apart, so a floor seen at a low angle needs more room. */
static double Tolerance(int level)
{
    return level ? 0.06 * level : 0.02;
}

static void ReleaseLayers(void)
{
    int i, k;
    for (i = 0; i < LEVELS; i++) {
        Layer *layer = &s_layers[i];
        Release(&layer->colorSurface);
        Release(&layer->colorTexture);
        Release(&layer->depthSurface);
        Release(&layer->depthTexture);
        Release(&layer->alphaSurface);
        Release(&layer->alphaTexture);
        Release(&layer->reduceShader);
        Release(&layer->markShader[0]);
        Release(&layer->markShader[1]);
        Release(&layer->fixShader[0]);
        Release(&layer->fixShader[1]);
        for (k = 0; k < 6; k++)
            Release(&layer->blendShader[k / 3][k % 3]);
        Release(&layer->farSurface);
        Release(&layer->farTexture);
        Release(&layer->nearSurface);
        Release(&layer->nearTexture);
        Release(&layer->nearColorSurface);
        Release(&layer->nearColorTexture);
        Release(&layer->reduceNearShader);
        Release(&layer->floatZSurface);
        Release(&layer->floatZTexture);
        Release(&layer->floatZShader);
        memset(layer, 0, sizeof(*layer));
    }
}

/* Everything made here goes before the device is reset or released. */
void RB_FxLayer_Release(void)
{
    ReleaseLayers();
    Release(&s_sceneDepthSurface);
    Release(&s_sceneDepthTexture);
    Release(&s_query);
    Release(&s_opaqueShader);
    Release(&s_filmShader[0]);
    Release(&s_filmShader[1]);
    Release(&s_filmShader[2]);
    s_filmValued = s_sceneDepthDrawn = 0;
    Release(&s_redShader);
    s_alphaPass = s_alphaDrawn = 0;
    s_active = s_depthOnly = s_queryPending = s_measuring = s_farPass = s_nearPass = 0;
    Release(&s_depthPassColor);
    Release(&s_depthPassDepth);
    s_depthPass = 0;
    s_fill = 0.0;
    s_mode = DIRECT;
}

static int CreateDepth(void *device, unsigned width, unsigned height, void **texture, void **surface)
{
    return CALL(CreateTextureFn, device, DEV_CreateTexture)(device, width, height, 1, USAGE_DEPTHSTENCIL, FORMAT_INTZ, 0,
                                                            texture, NULL) >= 0 &&
           CALL(GetSurfaceFn, *texture, TEX_GetSurfaceLevel)(*texture, 0, surface) >= 0;
}

/* The prepass's single-sample depth buffer as a texture, for r_rendertarget.c to use
 * in place of a plain depth surface. NULL when the layer is off (r_fxLayer is read
 * when the renderer starts) or the format is refused. */
void *RB_FxLayer_CreateSceneDepth(void *device, int width, int height)
{
    RB_FxLayer_Release();
    if (!Setting())
        return NULL;
    if (!CreateDepth(device, (unsigned)width, (unsigned)height, &s_sceneDepthTexture, &s_sceneDepthSurface)) {
        RB_FxLayer_Release();
        return NULL;
    }
    s_sceneWidth = (unsigned)width;
    s_sceneHeight = (unsigned)height;
    /* The caller owns the reference it is handed; this module keeps its own. */
    CALL(AddRefFn, s_sceneDepthSurface, UNK_AddRef)(s_sceneDepthSurface);
    return s_sceneDepthSurface;
}

/* Make the layers now, while the renderer starts, not in the frame smoke first fills. */
void RB_FxLayer_Prepare(void *device)
{
    int level;
    if (!s_sceneDepthTexture)
        return;
    s_device = device;
    for (level = 0; level < LEVELS; level++)
        GetLayer(level);
}

static void *CompileShaderAs(const char *name, const char *source, const char *profile)
{
    void *blob = NULL, *messages = NULL, *shader = NULL;
    if (Win_D3D9_CompileShader(source, (int)strlen(source), name, "main", profile, &blob, &messages) < 0 || !blob) {
        fprintf(stderr, "r_fxLayer: %s did not compile%s%s\n", name, messages ? ": " : "",
                messages ? (const char *)CALL(BlobPointerFn, messages, BLOB_GetBufferPointer)(messages) : "");
        Release(&messages);
        return NULL;
    }
    CALL(CreateShaderFn, s_device, DEV_CreatePixelShader)(s_device, CALL(BlobPointerFn, blob, BLOB_GetBufferPointer)(blob),
                                                          &shader);
    Release(&blob);
    Release(&messages);
    return shader;
}

static void *CompileShader(const char *name, const char *source)
{
    return CompileShaderAs(name, source, "ps_2_0");
}

/* Depth of a block of the scene's texels (2 by 2 for a half, 4 by 4 for a quarter):
 * the farthest, so an effect is drawn wherever any pixel of the block would show it,
 * or the nearest, for the layer's second picture (RB_FxLayer_NearBegin). That picture
 * is only wanted where a block holds two surfaces. Where its depths are all but equal
 * the nearest is written as 0, in front of everything, and the depth test turns every
 * effect away there before it costs anything: drawn everywhere, the second picture
 * cost as much as the first.
 * Sizes are written into the source, so the shaders here need no constants of the
 * engine's. */
static void *ReduceShader(int level, int nearest)
{
    char source[8192];
    int count = 1 << level, x, y, used;
    used = snprintf(source, sizeof source,
                    "sampler depthMap : register(s0);\n"
                    "struct Out { float4 color : COLOR0; float depth : DEPTH; };\n"
                    "Out main(float2 uv : TEXCOORD0)\n"
                    "{\n"
                    "    Out o;\n"
                    "    float t;\n"
                    "    float z = 0.0;\n"
                    "    float n = 1.0;\n");
    for (y = 0; y < count; y++)
        for (x = 0; x < count; x++)
            used += snprintf(source + used, sizeof source - (size_t)used,
                             "    t = tex2D(depthMap, uv + float2(%.9g, %.9g)).r;\n"
                             "    z = max(z, t);\n"
                             "%s",
                             (x + 0.5 - count / 2.0) / s_sceneWidth, (y + 0.5 - count / 2.0) / s_sceneHeight,
                             nearest ? "    n = min(n, t);\n" : "");
    snprintf(source + used, sizeof source - (size_t)used,
             "    o.depth = %s;\n"
             "    o.color = 0;\n"
             "    return o;\n"
             "}\n",
             nearest ? "lerp(n, 0.0, step(1.0 - n, (1.0 - z) * " NEAR_EVEN "))" : "z");
    return CompileShader(nearest ? "fxlayer_reduce_near" : "fxlayer_reduce", source);
}

/* The marking shaders write a depth for the frame buffer's own depth test to compare
 * each sample with. `nearer` 0: the far limit of the depths the layer was tested
 * against here (a sample beyond it passes LESS). `nearer` 1: the near limit (a sample
 * in front of it passes GREATER). Distance goes as 1 / (1 - depth), so a share of the
 * distance is a share of 1 - depth. A layer of the picture's size has one depth per
 * pixel; a smaller one is blended from four texels, and all four must agree. */
static void *MarkShader(const Layer *layer, int nearer, int level)
{
    char source[2048], taps[1024];
    double tolerance = Tolerance(level);
    if (level)
        snprintf(taps, sizeof taps,
                 "    float2 size = float2(%u.0, %u.0);\n"
                 "    float2 t = uv * float2(%.9g, %.9g) * size - 0.5;\n"
                 "    float2 origin = (t - frac(t) + 0.5) / size;\n"
                 "    float2 texel = 1.0 / size;\n"
                 "    float a = tex2D(depthMap, origin).r;\n"
                 "    float b = tex2D(depthMap, origin + float2(texel.x, 0.0)).r;\n"
                 "    float c = tex2D(depthMap, origin + float2(0.0, texel.y)).r;\n"
                 "    float e = tex2D(depthMap, origin + texel).r;\n"
                 "    float z = %s(%s(a, b), %s(c, e));\n",
                 layer->width, layer->height, (double)s_sceneWidth / (layer->width << level),
                 (double)s_sceneHeight / (layer->height << level), nearer ? "max" : "min", nearer ? "max" : "min",
                 nearer ? "max" : "min");
    else
        snprintf(taps, sizeof taps, "    float z = tex2D(depthMap, uv).r;\n");
    snprintf(source, sizeof source,
             "sampler depthMap : register(s0);\n"
             "struct Out { float4 color : COLOR0; float depth : DEPTH; };\n"
             "Out main(float2 uv : TEXCOORD0)\n"
             "{\n"
             "    Out o;\n"
             "%s"
             "    o.depth = saturate(1.0 - (1.0 - z) * %.9g);\n"
             "    o.color = 0;\n"
             "    return o;\n"
             "}\n",
             taps, nearer ? 1.0 + tolerance : 1.0 / (1.0 + tolerance));
    return CompileShader(nearer ? "fxlayer_mark_near" : "fxlayer_mark_far", source);
}

/* The layer's value for a marked sample: that of the texel, among the nearest one and
 * the eight around it, whose depth is the smallest (`nearer` 1) or the greatest
 * (`nearer` 0). Sampler 0 is the depth the layer was tested against, sampler 1 the
 * layer. */
static void *FixShader(const Layer *layer, int nearer, int level)
{
    char source[4096];
    int x, y, used;
    used = snprintf(source, sizeof source,
                    "sampler depthMap : register(s0);\n"
                    "sampler layerMap : register(s1);\n"
                    "float4 main(float2 uv : TEXCOORD0) : COLOR0\n"
                    "{\n"
                    "    float2 size = float2(%u.0, %u.0);\n"
                    "    float2 pick = (floor(uv * float2(%.9g, %.9g) * size) + 0.5) / size;\n"
                    "    float2 centre = pick;\n"
                    "    float best = tex2D(depthMap, centre).r;\n"
                    "    float2 c;\n"
                    "    float d;\n"
                    "    float m;\n",
                    layer->width, layer->height, (double)s_sceneWidth / (layer->width << level),
                    (double)s_sceneHeight / (layer->height << level));
    for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++) {
            if (!x && !y)
                continue;
            used += snprintf(source + used, sizeof source - (size_t)used,
                             "    c = centre + float2(%.9g, %.9g);\n"
                             "    d = tex2D(depthMap, c).r;\n"
                             "    m = step(%s);\n"
                             "    pick = lerp(pick, c, m);\n"
                             "    best = lerp(best, d, m);\n",
                             (double)x / layer->width, (double)y / layer->height, nearer ? "d, best" : "best, d");
        }
    snprintf(source + used, sizeof source - (size_t)used, "    return tex2D(layerMap, pick);\n}\n");
    return CompileShader(nearer ? "fxlayer_fix_near" : "fxlayer_fix_far", source);
}

/* What a smaller layer gives one pixel of the frame buffer.
 *
 * A texel of a smaller layer stands for a block of pixels. Blending the four texels
 * around a pixel as a picture is blended would let smoke leak across every edge:
 * beside the hand that holds the weapon, a pixel of the street would take part of its
 * smoke from a texel that lies on the hand and has none. And one answer per block is
 * not enough where a block holds two surfaces: a bush in thick smoke, drawn with the
 * smoke of the wall behind it, was gone.
 *
 * So a smaller layer is two pictures of the same effects, one tested against the
 * farthest depth of each block and one against the nearest, and it keeps the farthest
 * depth its effects reach in each texel (RB_FxLayer_NearBegin, RB_FxLayer_FarBegin).
 * A texel counts for a pixel in two cases:
 *
 * - The pixel's depth lies between the block's nearest and farthest (within the
 *   tolerance). It takes the near picture at the nearest depth, the far picture at
 *   the farthest, and a mix by its place between them. A block of one surface has
 *   no near picture (ReduceShader) and gives the far one.
 * - All of the texel's smoke lies in front of both the pixel and the block (the far
 *   wall and the sky behind a cloud that hangs in the street). It takes the far
 *   picture, which then holds all of it. Refusing these drew every roofline behind
 *   the smoke into it as a row of steps.
 *
 * The texels that count are blended by their usual weights. If there is none, all four
 * are blended, each counting for more the nearer its depth is to the pixel's.
 *
 * `pick` chooses the pixel's depth: the prepass's depth of the pixel itself (0), or
 * the nearest (1) or farthest (2) of the pixels around it, for the samples that
 * anti-aliasing puts on the surface next door. Nothing is given to the view weapon,
 * which is drawn in a depth range of its own (WEAPON_DEPTH).
 *
 * Sampler 0 is the layer's depth, 1 the layer, 2 the prepass's depth, 3 the depth the
 * layer's effects reach, 4 and 5 the layer's second depth and picture. */
static void *BlendShader(const Layer *layer, int level, int pick, int twice)
{
    static const char *const names[] = { "fxlayer_blend", "fxlayer_blend_near", "fxlayer_blend_far" };
    char source[8192], counted[2048];
    double tolerance = Tolerance(level);
    int x, y, used;

    if (twice)
        snprintf(counted, sizeof counted,
                 "    float4 zn = float4(tex2D(nearDepth, o).r, tex2D(nearDepth, o + float2(e.x, 0.0)).r,\n"
                 "                       tex2D(nearDepth, o + float2(0.0, e.y)).r, tex2D(nearDepth, o + e).r);\n"
                 "    float4 even = step(zn, 0.0);\n"
                 "    float4 qn = lerp(1.0 - zn, q, even);\n"
                 "    float4 inside = step(q, r * %.9g) * step(r, qn * %.9g);\n"
                 "    float4 front = step(max(q, r) * %.9g, reach);\n"
                 "    float4 place = max(max(saturate((qn - r) / max(qn - q, 0.0000001)), front), even);\n"
                 "    c0 = lerp(tex2D(nearMap, o), c0, place.x);\n"
                 "    c1 = lerp(tex2D(nearMap, o + float2(e.x, 0.0)), c1, place.y);\n"
                 "    c2 = lerp(tex2D(nearMap, o + float2(0.0, e.y)), c2, place.z);\n"
                 "    c3 = lerp(tex2D(nearMap, o + e), c3, place.w);\n"
                 "    float4 w = b * max(inside, front);\n",
                 1.0 + tolerance, 1.0 + tolerance, 1.0 + tolerance);
    else
        snprintf(counted, sizeof counted,
                 "    float4 same = step(q, r * %.9g) * step(r * %.9g, q);\n"
                 "    float4 w = b * max(same, step(max(q, r) * %.9g, reach));\n",
                 1.0 + tolerance, 1.0 / (1.0 + tolerance), 1.0 + tolerance);
    used = snprintf(source, sizeof source,
                    "sampler layerDepth : register(s0);\n"
                    "sampler layerMap : register(s1);\n"
                    "sampler sceneDepth : register(s2);\n"
                    "sampler layerFar : register(s3);\n"
                    "sampler nearDepth : register(s4);\n"
                    "sampler nearMap : register(s5);\n"
                    "float4 main(float2 uv : TEXCOORD0) : COLOR0\n"
                    "{\n"
                    "    float d = tex2D(sceneDepth, uv).r;\n");
    for (y = -1; pick && y <= 1; y++)
        for (x = -1; x <= 1; x++)
            if (x || y)
                used += snprintf(source + used, sizeof source - (size_t)used,
                                 "    d = %s(d, tex2D(sceneDepth, uv + float2(%.9g, %.9g)).r);\n", pick == 1 ? "min" : "max",
                                 (double)x / s_sceneWidth, (double)y / s_sceneHeight);
    snprintf(source + used, sizeof source - (size_t)used,
             "    float2 size = float2(%u.0, %u.0);\n"
             "    float2 t = uv * float2(%.9g, %.9g) * size - 0.5;\n"
             "    float2 f = frac(t);\n"
             "    float2 o = (t - f + 0.5) / size;\n"
             "    float2 e = 1.0 / size;\n"
             "    float4 q = 1.0 - float4(tex2D(layerDepth, o).r, tex2D(layerDepth, o + float2(e.x, 0.0)).r,\n"
             "                            tex2D(layerDepth, o + float2(0.0, e.y)).r, tex2D(layerDepth, o + e).r);\n"
             "    float4 c0 = tex2D(layerMap, o);\n"
             "    float4 c1 = tex2D(layerMap, o + float2(e.x, 0.0));\n"
             "    float4 c2 = tex2D(layerMap, o + float2(0.0, e.y));\n"
             "    float4 c3 = tex2D(layerMap, o + e);\n"
             "    float r = 1.0 - d;\n"
             "    float4 b = float4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);\n"
             "    float4 reach = 1.0 - float4(tex2D(layerFar, o).r, tex2D(layerFar, o + float2(e.x, 0.0)).r,\n"
             "                                tex2D(layerFar, o + float2(0.0, e.y)).r, tex2D(layerFar, o + e).r);\n"
             "%s"
             "    float sum = w.x + w.y + w.z + w.w;\n"
             "    float4 mixed = (c0 * w.x + c1 * w.y + c2 * w.z + c3 * w.w) / max(sum, 0.00001);\n"
             "    float4 s = b / (abs(q - r) + r * 0.02 + 0.000001);\n"
             "    float4 soft = (c0 * s.x + c1 * s.y + c2 * s.z + c3 * s.w) / (s.x + s.y + s.z + s.w);\n"
             "    float4 result = lerp(soft, mixed, step(0.0001, sum));\n"
             "    return lerp(result, float4(0.0, 0.0, 0.0, 1.0), step(d, %.9g));\n"
             "}\n",
             layer->width, layer->height, (double)s_sceneWidth / (layer->width << level),
             (double)s_sceneHeight / (layer->height << level), counted, WEAPON_DEPTH);
    return CompileShaderAs(names[pick], source, "ps_3_0");
}

/* The layer of this size, made on first use. Sixteen bits a channel where the device
 * has them: thick smoke is fifty blends deep, and eight bits would round each one. */
static Layer *GetLayer(int level)
{
    Layer *layer = &s_layers[level];
    void *device = s_device;

    if (layer->colorTexture || layer->failed)
        return layer->failed ? NULL : layer;
    layer->width = (s_sceneWidth + (1u << level) - 1) >> level;
    layer->height = (s_sceneHeight + (1u << level) - 1) >> level;
    if (CALL(CreateTextureFn, device, DEV_CreateTexture)(device, layer->width, layer->height, 1, USAGE_RENDERTARGET,
                                                         FORMAT_A16B16G16R16, 0, &layer->colorTexture, NULL) < 0)
        CALL(CreateTextureFn, device, DEV_CreateTexture)(device, layer->width, layer->height, 1, USAGE_RENDERTARGET,
                                                         FORMAT_A8R8G8B8, 0, &layer->colorTexture, NULL);
    if (layer->colorTexture) {
        SurfaceDesc desc;
        void *surface = NULL;
        memset(&desc, 0, sizeof(desc));
        if (CALL(GetSurfaceFn, layer->colorTexture, TEX_GetSurfaceLevel)(layer->colorTexture, 0, &surface) >= 0) {
            CALL(GetDescFn, surface, SURF_GetDesc)(surface, &desc);
            Release(&surface);
        }
        if (CALL(CreateTextureFn, device, DEV_CreateTexture)(device, layer->width, layer->height, 1, USAGE_RENDERTARGET,
                                                             desc.Format, 0, &layer->alphaTexture, NULL) >= 0)
            CALL(GetSurfaceFn, layer->alphaTexture, TEX_GetSurfaceLevel)(layer->alphaTexture, 0, &layer->alphaSurface);
        if (level && CALL(CreateTextureFn, device, DEV_CreateTexture)(device, layer->width, layer->height, 1,
                                                                      USAGE_RENDERTARGET, desc.Format, 0,
                                                                      &layer->nearColorTexture, NULL) >= 0)
            CALL(GetSurfaceFn, layer->nearColorTexture, TEX_GetSurfaceLevel)(layer->nearColorTexture, 0,
                                                                             &layer->nearColorSurface);
    }
    if (!layer->colorTexture || !layer->alphaSurface ||
        CALL(GetSurfaceFn, layer->colorTexture, TEX_GetSurfaceLevel)(layer->colorTexture, 0, &layer->colorSurface) < 0 ||
        (level && (!CreateDepth(device, layer->width, layer->height, &layer->depthTexture, &layer->depthSurface) ||
                   !(layer->reduceShader = ReduceShader(level, 0)))) ||
        !(layer->markShader[0] = MarkShader(layer, 0, 0)) || !(layer->markShader[1] = MarkShader(layer, 1, 0)) ||
        (!level && (!(layer->fixShader[0] = FixShader(layer, 0, level)) || !(layer->fixShader[1] = FixShader(layer, 1, level)))) ||
        (level && !CreateDepth(device, layer->width, layer->height, &layer->farTexture, &layer->farSurface)) ||
        (level && (!layer->nearColorSurface || !(layer->reduceNearShader = ReduceShader(level, 1)) ||
                   !CreateDepth(device, layer->width, layer->height, &layer->nearTexture, &layer->nearSurface))) ||
        (level && (!(layer->blendShader[0][0] = BlendShader(layer, level, 0, 0)) ||
                   !(layer->blendShader[0][1] = BlendShader(layer, level, 1, 0)) ||
                   !(layer->blendShader[0][2] = BlendShader(layer, level, 2, 0)) ||
                   !(layer->blendShader[1][0] = BlendShader(layer, level, 0, 1)) ||
                   !(layer->blendShader[1][1] = BlendShader(layer, level, 1, 1)) ||
                   !(layer->blendShader[1][2] = BlendShader(layer, level, 2, 1))))) {
        layer->failed = 1;
        fprintf(stderr, "r_fxLayer: no layer of %ux%u; effects are drawn without it\n", layer->width, layer->height);
        return NULL;
    }
    return layer;
}

static DWORD AlphaDestFor(DWORD destBlend)
{
    /* Additive layers leave what shows through unchanged; a layer that covers reduces it. */
    if (destBlend == BLEND_ONE)
        return BLEND_ONE;
    if (destBlend == BLEND_ZERO)
        return BLEND_ZERO;
    return BLEND_INVSRCALPHA;
}

static void ScaleViewport(const Viewport *full, Viewport *out)
{
    DWORD divisor = 1u << s_level;
    *out = *full;
    out->X = full->X / divisor;
    out->Y = full->Y / divisor;
    out->Width = (full->Width + divisor - 1) / divisor;
    out->Height = (full->Height + divisor - 1) / divisor;
    if (out->X + out->Width > s_layer->width)
        out->Width = s_layer->width - out->X;
    if (out->Y + out->Height > s_layer->height)
        out->Height = s_layer->height - out->Y;
}

/* A screen-filling pass of this module's own: every state it touches is put back. */
typedef struct {
    DWORD states[32], stages[6], samplers[6][5];
    void *textures[6], *decl, *vs, *ps;
} Saved;

static const DWORD passStates[] = { RS_ZENABLE, RS_ZWRITEENABLE, RS_ZFUNC, RS_ALPHATESTENABLE, RS_SRCBLEND, RS_DESTBLEND,
    RS_CULLMODE, RS_ALPHABLENDENABLE, RS_FOGENABLE, RS_LIGHTING, RS_COLORWRITEENABLE, RS_BLENDOP,
    RS_SEPARATEALPHABLENDENABLE, RS_SCISSORTESTENABLE, RS_STENCILENABLE, RS_STENCILFAIL, RS_STENCILZFAIL, RS_STENCILPASS,
    RS_STENCILFUNC, RS_STENCILREF, RS_STENCILMASK, RS_STENCILWRITEMASK, RS_TWOSIDEDSTENCILMODE,
    RS_SLOPESCALEDEPTHBIAS, RS_DEPTHBIAS };
static const DWORD passStages[][2] = { { 0, 1 }, { 0, 2 }, { 0, 4 }, { 0, 5 }, { 1, 1 }, { 1, 4 } };
static const DWORD passSamplers[] = { 1, 2, 5, 6, 7 }; /* address u, v, mag, min, mip */

static void PassBegin(Saved *saved)
{
    void *device = s_device;
    unsigned i, unit;

    for (i = 0; i < COUNT(passStates); i++)
        saved->states[i] = GetState(passStates[i]);
    for (i = 0; i < COUNT(passStages); i++)
        CALL(GetStageFn, device, DEV_GetTextureStageState)(device, passStages[i][0], passStages[i][1], &saved->stages[i]);
    for (unit = 0; unit < 6; unit++) {
        saved->textures[unit] = NULL;
        CALL(GetTextureFn, device, DEV_GetTexture)(device, unit, &saved->textures[unit]);
        for (i = 0; i < COUNT(passSamplers); i++)
            CALL(GetStageFn, device, DEV_GetSamplerState)(device, unit, passSamplers[i], &saved->samplers[unit][i]);
    }
    saved->decl = saved->vs = saved->ps = NULL;
    CALL(GetObjectFn, device, DEV_GetVertexDeclaration)(device, &saved->decl);
    CALL(GetObjectFn, device, DEV_GetVertexShader)(device, &saved->vs);
    CALL(GetObjectFn, device, DEV_GetPixelShader)(device, &saved->ps);
    CALL(SetObjectFn, device, DEV_SetVertexShader)(device, NULL);
    CALL(SetFvfFn, device, DEV_SetFVF)(device, 0x104); /* XYZRHW | TEX1 */
    SetState(RS_ALPHATESTENABLE, 0);
    SetState(RS_CULLMODE, 1);
    SetState(RS_FOGENABLE, 0);
    SetState(RS_LIGHTING, 0);
    SetState(RS_SCISSORTESTENABLE, 0);
    SetState(RS_SEPARATEALPHABLENDENABLE, 0);
    SetState(RS_BLENDOP, 1);
    SetState(RS_ZWRITEENABLE, 0);
    SetState(RS_STENCILENABLE, 0);
    SetState(RS_TWOSIDEDSTENCILMODE, 0);
    /* These passes compare depths they set themselves: a decal's bias must not move them. */
    SetState(RS_SLOPESCALEDEPTHBIAS, 0);
    SetState(RS_DEPTHBIAS, 0);
    SetState(RS_STENCILFAIL, OP_KEEP);
    SetState(RS_STENCILZFAIL, OP_KEEP);
    SetState(RS_STENCILPASS, OP_KEEP);
}

/* Sampler `unit` reads `texture`, clamped, with this filter (1 point, 2 linear). */
static void PassTexture(DWORD unit, void *texture, DWORD filter)
{
    static const DWORD values[] = { 3, 3, 0, 0, 0 };
    unsigned i;
    CALL(SetTextureFn, s_device, DEV_SetTexture)(s_device, unit, texture);
    for (i = 0; i < COUNT(passSamplers); i++)
        CALL(SetStageFn, s_device, DEV_SetSamplerState)(s_device, unit, passSamplers[i],
                                                        i == 2 || i == 3 ? filter : values[i]);
}

/* The rectangle x, y, w, h of the bound target at depth z, reading u0, v0 to u1, v1. */
static void PassDrawAt(float z, float x, float y, float w, float h, float u0, float v0, float u1, float v1)
{
    QuadVertex quad[4];
    float x0 = x - 0.5f, y0 = y - 0.5f, x1 = x0 + w, y1 = y0 + h;
    quad[0] = (QuadVertex){ x0, y0, z, 1.0f, u0, v0 };
    quad[1] = (QuadVertex){ x1, y0, z, 1.0f, u1, v0 };
    quad[2] = (QuadVertex){ x0, y1, z, 1.0f, u0, v1 };
    quad[3] = (QuadVertex){ x1, y1, z, 1.0f, u1, v1 };
    CALL(DrawUpFn, s_device, DEV_DrawPrimitiveUP)(s_device, 5, 2, quad, sizeof(QuadVertex)); /* triangle strip */
}

static void PassDraw(float x, float y, float w, float h, float u0, float v0, float u1, float v1)
{
    PassDrawAt(0.0f, x, y, w, h, u0, v0, u1, v1);
}

/* The engine's viewport on the frame buffer, reading the same part of a texture whose
 * whole is `w` by `h` frame buffer pixels. */
static void PassDrawViewport(float w, float h)
{
    PassDraw((float)s_viewport.X, (float)s_viewport.Y, (float)s_viewport.Width, (float)s_viewport.Height,
             (float)s_viewport.X / w, (float)s_viewport.Y / h, (float)(s_viewport.X + s_viewport.Width) / w,
             (float)(s_viewport.Y + s_viewport.Height) / h);
}

static void PassEnd(Saved *saved)
{
    void *device = s_device;
    unsigned i, unit;

    for (unit = 0; unit < 6; unit++) {
        CALL(SetTextureFn, device, DEV_SetTexture)(device, unit, saved->textures[unit]);
        Release(&saved->textures[unit]);
        for (i = 0; i < COUNT(passSamplers); i++)
            CALL(SetStageFn, device, DEV_SetSamplerState)(device, unit, passSamplers[i], saved->samplers[unit][i]);
    }
    CALL(SetObjectFn, device, DEV_SetVertexDeclaration)(device, saved->decl);
    CALL(SetObjectFn, device, DEV_SetVertexShader)(device, saved->vs);
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, saved->ps);
    Release(&saved->decl);
    Release(&saved->vs);
    Release(&saved->ps);
    for (i = 0; i < COUNT(passStages); i++)
        CALL(SetStageFn, device, DEV_SetTextureStageState)(device, passStages[i][0], passStages[i][1], saved->stages[i]);
    for (i = 0; i < COUNT(passStates); i++)
        SetState(passStates[i], saved->states[i]);
    /* DrawPrimitiveUP leaves stream 0 unset; make the engine's record agree. */
    RB_ClearAllStreamSources();
}

/* Glow tells sky from everything else by the frame buffer's alpha: the sky leaves it
 * at nothing, the world writes it. Effects drawn later can lower it again, so the game
 * keeps a copy of the scene from before them and takes the greater of the two alphas.
 * Making that copy means leaving the frame buffer in mid-scene, which on this kind of
 * graphics chip writes every sample of it out to memory and reads it back in.
 *
 * The same answer is in the depth buffer, which only the world writes: this pass, run
 * when the scene is complete, sets alpha to 1 wherever anything nearer than the far
 * plane was drawn, sample by sample, without leaving the frame buffer. Glow then reads
 * one picture for both. (rb_backend.c: r_glowSkyFromDepth.) */
void RB_ScreenPass_NonSkyAlpha(void *device, unsigned width, unsigned height)
{
    Saved saved;

    s_device = device;
    if (!s_opaqueShader)
        s_opaqueShader = CompileShader("screenpass_opaque", "float4 main() : COLOR0 { return float4(1.0, 1.0, 1.0, 1.0); }\n");
    if (!s_opaqueShader)
        return;
    PassBegin(&saved);
    SetState(RS_ZENABLE, 1);
    SetState(RS_ZFUNC, CMP_GREATER); /* the far plane lies beyond whatever was drawn here */
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 8); /* alpha alone */
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_opaqueShader);
    PassDrawAt(1.0f, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 1.0f, 1.0f);
    PassEnd(&saved);
}

/* All of `texture` onto the bound target of the same size, as it is. Drawn as the
 * first thing on the back buffer in a frame, this costs less than a copy between the
 * two: the graphics chip then has no old contents to read in before drawing on. */
void RB_ScreenPass_Copy(void *device, void *texture, unsigned width, unsigned height)
{
    static const DWORD values[] = { 2, 2, 2, 2, 1, 1 }; /* select the texture; second stage off */
    Saved saved;
    unsigned i;

    s_device = device;
    PassBegin(&saved);
    SetState(RS_ZENABLE, 0);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0xf);
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, NULL);
    for (i = 0; i < COUNT(passStages); i++)
        CALL(SetStageFn, device, DEV_SetTextureStageState)(device, passStages[i][0], passStages[i][1], values[i]);
    PassTexture(0, texture, 1);
    PassDraw(0.0f, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 1.0f, 1.0f);
    PassEnd(&saved);
}

/* The engine's float-Z texture, if the effects about to be drawn feather against it. */
void RB_FxLayer_SceneFloatZ(void *texture)
{
    s_sceneFloatZ = texture;
}

/* What a feathering effect reads in place of the scene's float-Z while it is drawn into
 * a smaller layer, or NULL. `slot` tells the copies of the layers' sizes apart. */
void *RB_FxLayer_FloatZ(int *slot)
{
    if (!s_active || !s_level || !s_floatZReady)
        return NULL;
    *slot = s_level;
    return s_layer->floatZTexture;
}

/* The float-Z prepass has drawn the scene, models included, into the depth texture. */
void RB_FxLayer_SceneDepthDrawn(void)
{
    s_sceneDepthDrawn = 1;
}

/* The film pass's shader. `values` are contrast, bias, exponent, the gain at black
 * (three) and what full brightness adds to it (three). The numbers are written into
 * the source, like every shader here, so none of the engine's constants are touched;
 * players change these settings by hand and rarely. */
static void *FilmShader(const float *values, const char *saturation)
{
    char source[2048];
    snprintf(source, sizeof source,
             "sampler image : register(s0);\n"
             "sampler depthMap : register(s1);\n"
             "float4 main(float2 uv : TEXCOORD0) : COLOR0\n"
             "{\n"
             "    float4 pixel = tex2D(image, uv);\n"
             "    float3 rgb = pixel.rgb * %.9g + %.9g;\n"
             "    float grey = dot(rgb, float3(0.299, 0.587, 0.114));\n"
             "    rgb = grey + (rgb - grey) * %s;\n"
             "    rgb *= float3(%.9g, %.9g, %.9g) + float3(%.9g, %.9g, %.9g) * saturate(grey);\n"
             "    return float4(pow(saturate(rgb), %.9g), pixel.a);\n"
             "}\n",
             values[0], values[1], saturation, values[3], values[4], values[5], values[6], values[7], values[8], values[2]);
    return CompileShader("screenpass_film", source);
}

/* CoD2x's film colour controls: the rectangle x, y, w, h of the bound target, taken
 * from `source` (a copy of that target, `width` by `height`) and graded. The sky has a
 * saturation of its own. Where that differs it is told apart by depth: with a depth
 * buffer bound (or `depthIfNone` to bind), by drawing at the far plane twice, as the
 * OpenGL pass does; without one, from the prepass's depth texture. With neither, the
 * sky takes the scene's saturation. Returns NULL, or what went wrong. */
const char *RB_ScreenPass_Film(void *device, void *source, void *depthIfNone, unsigned width, unsigned height, float x,
                               float y, float w, float h, const float *values, float saturation, float skySaturation)
{
    enum { PLAIN, DEPTH_TEST, DEPTH_TEXTURE };
    static int told;
    float key[11], u0 = x / width, v0 = y / height, u1 = (x + w) / width, v1 = (y + h) / height;
    Viewport full = { 0, 0, width, height, 0.0f, 1.0f }, before;
    void *depth = NULL;
    char text[128];
    int how = PLAIN, boundDepth = 0, i;
    Saved saved;

    s_device = device;
    memcpy(key, values, 9 * sizeof(float));
    key[9] = saturation;
    key[10] = skySaturation;
    if (!s_filmValued || memcmp(key, s_filmValues, sizeof key)) {
        for (i = 0; i < 3; i++)
            Release(&s_filmShader[i]);
        memcpy(s_filmValues, key, sizeof key);
        s_filmValued = 1;
    }
    if (skySaturation != saturation) {
        CALL(GetDepthFn, device, DEV_GetDepthStencilSurface)(device, &depth);
        if (!depth && depthIfNone) {
            CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, depthIfNone);
            boundDepth = 1;
        }
        if (depth || boundDepth) {
            how = DEPTH_TEST;
        } else if (s_sceneDepthTexture && s_sceneDepthDrawn) {
            how = DEPTH_TEXTURE;
        } else if (!told) {
            told = 1;
            fprintf(stderr, "r_filmTweakSkySaturation: no scene depth to tell the sky by; it takes r_filmTweakSaturation\n");
        }
        Release(&depth);
    }
    s_sceneDepthDrawn = 0;

    if (how != DEPTH_TEXTURE && !s_filmShader[0]) {
        snprintf(text, sizeof text, "%.9g", saturation);
        s_filmShader[0] = FilmShader(values, text);
    }
    if (how == DEPTH_TEST && !s_filmShader[1]) {
        snprintf(text, sizeof text, "%.9g", skySaturation);
        s_filmShader[1] = FilmShader(values, text);
    }
    if (how == DEPTH_TEXTURE && !s_filmShader[2]) {
        snprintf(text, sizeof text, "lerp(%.9g, %.9g, step(1.0, tex2D(depthMap, uv).r))", saturation, skySaturation);
        s_filmShader[2] = FilmShader(values, text);
    }
    if (!s_filmShader[how == DEPTH_TEXTURE ? 2 : 0] || (how == DEPTH_TEST && !s_filmShader[1])) {
        if (boundDepth)
            CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, NULL);
        return "the film shader could not be compiled";
    }

    CALL(ViewportFn, device, DEV_GetViewport)(device, &before);
    PassBegin(&saved);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &full);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0xf); /* alpha is written as it was read */
    PassTexture(0, source, 1);
    if (how == DEPTH_TEST) {
        SetState(RS_ZENABLE, 1);
        SetState(RS_ZFUNC, 4); /* less or equal: the far plane shows only where nothing was drawn */
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_filmShader[1]);
        PassDrawAt(1.0f, x, y, w, h, u0, v0, u1, v1);
        SetState(RS_ZFUNC, CMP_GREATER);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_filmShader[0]);
        PassDrawAt(1.0f, x, y, w, h, u0, v0, u1, v1);
    } else {
        SetState(RS_ZENABLE, 0);
        if (how == DEPTH_TEXTURE)
            PassTexture(1, s_sceneDepthTexture, 1);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_filmShader[how == DEPTH_TEXTURE ? 2 : 0]);
        PassDraw(x, y, w, h, u0, v0, u1, v1);
    }
    PassEnd(&saved);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &before);
    if (boundDepth)
        CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, NULL);
    return NULL;
}

/* The scene's float-Z at a smaller layer's size, one texel for each of the layer's
 * pixels. A feathering effect reads the float-Z once for every pixel it covers, and
 * thick smoke covers each many times over. Read from the scene's own texture, a quarter
 * layer's pixels lie four texels apart in each direction, so every read lands on memory
 * the graphics chip does not have at hand: at 3840x2160 that took two thirds of the
 * time the smoke cost. The copy holds the texel the effect would have read, the one
 * above and left of the centre of the layer pixel's block.
 * Returns 1 when the copy is there. Leaves the layer's colour target bound. */
static int CopyFloatZ(void)
{
    void *device = s_device;
    Layer *layer = s_layer;
    Viewport view = { 0, 0, layer->width, layer->height, 0.0f, 1.0f };

    if (!s_sceneFloatZ || layer->floatZFailed)
        return 0;
    if (!layer->floatZSurface) {
        char source[512];
        snprintf(source, sizeof source,
                 "sampler map : register(s0);\n"
                 "float4 main(float2 uv : TEXCOORD0) : COLOR0 { return tex2D(map, uv - float2(%.9g, %.9g)); }\n",
                 0.25 / s_sceneWidth, 0.25 / s_sceneHeight);
        if (CALL(CreateTextureFn, device, DEV_CreateTexture)(device, layer->width, layer->height, 1, USAGE_RENDERTARGET,
                                                             114 /* R32F */, 0, &layer->floatZTexture, NULL) < 0 ||
            CALL(GetSurfaceFn, layer->floatZTexture, TEX_GetSurfaceLevel)(layer->floatZTexture, 0, &layer->floatZSurface) < 0 ||
            !(layer->floatZShader = CompileShader("fxlayer_floatz", source))) {
            Release(&layer->floatZSurface);
            Release(&layer->floatZTexture);
            layer->floatZFailed = 1;
            return 0;
        }
    }
    SetState(RS_ZENABLE, 0);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0xf);
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, layer->floatZSurface);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, NULL);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &view);
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, layer->floatZShader);
    PassTexture(0, s_sceneFloatZ, 1);
    PassDraw(0.0f, 0.0f, (float)view.Width, (float)view.Height, 0.0f, 0.0f, 1.0f, 1.0f);
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, layer->colorSurface);
    return 1;
}

/* Fill the layer's smaller depth buffer from the scene's. The layer's colour target
 * is bound only because a depth buffer cannot be drawn into alone. */
static void ReduceDepth(void *surface, void *shader)
{
    void *device = s_device;
    Viewport view = { 0, 0, s_layer->width, s_layer->height, 0.0f, 1.0f };

    SetState(RS_ZENABLE, 1);
    SetState(RS_ZWRITEENABLE, 1);
    SetState(RS_ZFUNC, CMP_ALWAYS);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, surface);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &view);
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, shader);
    PassTexture(0, s_sceneDepthTexture, 1);
    /* A layer of odd size reaches past the scene's edge; read as far as it reaches. */
    PassDraw(0.0f, 0.0f, (float)view.Width, (float)view.Height, 0.0f, 0.0f,
             (float)(view.Width << s_level) / (float)s_sceneWidth, (float)(view.Height << s_level) / (float)s_sceneHeight);
    SetState(RS_ZWRITEENABLE, 0);
}

/* Mark the samples of the frame buffer that do not lie at the depth the prepass has for
 * their pixel: nearer, or farther. */
static void Mark(void)
{
    void *device = s_device;
    int pass;

    SetState(RS_ZENABLE, 1);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0);
    SetState(RS_STENCILENABLE, 1);
    SetState(RS_STENCILFUNC, CMP_ALWAYS);
    SetState(RS_STENCILPASS, OP_REPLACE);
    PassTexture(0, s_sceneDepthTexture, 1);
    for (pass = 0; pass < 2; pass++) {
        SetState(RS_STENCILREF, pass ? MARK_NEAR : MARK_FAR);
        SetState(RS_STENCILWRITEMASK, pass ? MARK_NEAR : MARK_FAR);
        SetState(RS_ZFUNC, pass ? CMP_GREATER : CMP_LESS);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_layer->markShader[pass]);
        PassDrawViewport((float)s_sceneWidth, (float)s_sceneHeight);
    }
    SetState(RS_STENCILPASS, OP_KEEP);
    SetState(RS_STENCILWRITEMASK, 0);
}

/* The layer over the samples of the frame buffer that are not marked, then the marked
 * ones, whose marks come off as they are drawn. */
static void Composite(void)
{
    static const DWORD values[] = { 2, 2, 2, 2, 1, 1 }; /* select the texture; second stage off */
    void *device = s_device;
    unsigned i;
    int pass;

    SetState(RS_ZENABLE, 0);
    SetState(RS_ALPHABLENDENABLE, 1);
    SetState(RS_SRCBLEND, BLEND_ONE);
    SetState(RS_DESTBLEND, BLEND_SRCALPHA);
    SetState(RS_COLORWRITEENABLE, 7);
    SetState(RS_STENCILENABLE, 1);
    SetState(RS_STENCILFUNC, CMP_EQUAL);
    SetState(RS_STENCILREF, 0);
    SetState(RS_STENCILMASK, MARK_ANY);
    if (s_level) {
        PassTexture(0, s_layer->depthTexture, 1);
        PassTexture(1, s_layer->colorTexture, 1);
        PassTexture(2, s_sceneDepthTexture, 1);
        PassTexture(3, s_layer->farTexture, 1);
        PassTexture(4, s_layer->nearTexture, 1);
        PassTexture(5, s_layer->nearColorTexture, 1);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_layer->blendShader[s_nearFrame][0]);
        PassDrawViewport((float)s_sceneWidth, (float)s_sceneHeight);
    } else {
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, NULL);
        for (i = 0; i < COUNT(passStages); i++)
            CALL(SetStageFn, device, DEV_SetTextureStageState)(device, passStages[i][0], passStages[i][1], values[i]);
        PassTexture(0, s_layer->colorTexture, 1);
        PassDrawViewport((float)s_layer->width, (float)s_layer->height);
        PassTexture(0, s_sceneDepthTexture, 1);
        PassTexture(1, s_layer->colorTexture, 1);
    }

    /* A sample marked both ways lies between two surfaces; it goes with the nearer. */
    SetState(RS_STENCILWRITEMASK, MARK_ANY);
    SetState(RS_STENCILPASS, OP_ZERO);
    for (pass = 1; pass >= 0; pass--) {
        SetState(RS_STENCILREF, pass ? MARK_NEAR : MARK_FAR);
        SetState(RS_STENCILMASK, pass ? MARK_NEAR : MARK_ANY);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device,
                                                      s_level ? s_layer->blendShader[s_nearFrame][pass ? 1 : 2]
                                                              : s_layer->fixShader[pass]);
        PassDrawViewport((float)s_sceneWidth, (float)s_sceneHeight);
    }
    SetState(RS_STENCILPASS, OP_KEEP);
    SetState(RS_STENCILWRITEMASK, 0);
}

/* Bring the frame buffer's alpha up to date: times the product, plus the sum. */
static void CompositeAlpha(void)
{
    static const DWORD values[] = { 2, 2, 2, 2, 1, 1 }; /* select the texture; second stage off */
    void *device = s_device;
    float w = (float)(s_layer->width << s_level), h = (float)(s_layer->height << s_level);
    unsigned i;
    int pass;

    if (!s_redShader)
        s_redShader = CompileShader("fxlayer_red", "sampler map : register(s0);\n"
                                                   "float4 main(float2 uv : TEXCOORD0) : COLOR0 { return tex2D(map, uv).rrrr; }\n");
    if (!s_redShader)
        return;
    /* Sky is what lies at the far plane, where nothing wrote depth. */
    SetState(RS_ZENABLE, s_skyOnlyAlpha);
    SetState(RS_ZFUNC, CMP_EQUAL);
    SetState(RS_STENCILENABLE, 0);
    SetState(RS_ALPHABLENDENABLE, 1);
    SetState(RS_COLORWRITEENABLE, 8); /* alpha alone */
    for (i = 0; i < COUNT(passStages); i++)
        CALL(SetStageFn, device, DEV_SetTextureStageState)(device, passStages[i][0], passStages[i][1], values[i]);
    PassTexture(0, s_layer->alphaTexture, s_level ? 2 : 1);
    for (pass = 0; pass < 2; pass++) {
        SetState(RS_SRCBLEND, pass ? BLEND_ONE : BLEND_ZERO);
        SetState(RS_DESTBLEND, pass ? BLEND_ONE : BLEND_SRCALPHA);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, pass ? NULL : s_redShader);
        PassDrawAt(1.0f, (float)s_viewport.X, (float)s_viewport.Y, (float)s_viewport.Width, (float)s_viewport.Height,
                   (float)s_viewport.X / w, (float)s_viewport.Y / h, (float)(s_viewport.X + s_viewport.Width) / w,
                   (float)(s_viewport.Y + s_viewport.Height) / h);
    }
}

/* Start counting the pixels drawn from here on, if the last count has been read. */
static void CountBegin(int mode)
{
    if (s_measuring || s_queryPending)
        return;
    if (!s_query && CALL(CreateQueryFn, s_device, DEV_CreateQuery)(s_device, QUERY_OCCLUSION, &s_query) < 0)
        s_query = NULL;
    if (!s_query || CALL(IssueFn, s_query, QUERY_Issue)(s_query, ISSUE_BEGIN) < 0)
        return;
    s_measuring = 1;
    s_queryMode = mode;
}

static void CountEnd(void)
{
    if (!s_measuring)
        return;
    s_measuring = 0;
    CALL(IssueFn, s_query, QUERY_Issue)(s_query, ISSUE_END);
    s_queryPending = 1;
}

/* What drawing the effects takes, in pixels of blending one sample each: straight
 * onto the frame buffer every sample is blended (measured: four samples take about
 * three times one); a smaller layer has fewer pixels. */
static double Cost(int mode)
{
    if (mode == DIRECT)
        return s_fill * (s_samples >= 4 ? 3.0 : s_samples >= 2 ? 1.8 : 1.0);
    return s_fill / (double)(1u << (2 * mode));
}

/* A smaller layer's second picture restores what stands inside the smoke (a bush, long
 * grass), and in a field of grass it costs as much again as the first. It is drawn
 * while the frame rate asked for is held. When frames come late with it, it is left
 * out, and it comes back once the effects cover clearly less than they did then, so it
 * is not tried again in the same smoke. With vertical sync on, a frame is paced by the
 * display and says nothing about what it cost; the picture is kept. */
static void NearGovern(int cap)
{
    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    double target = 1000.0 / cap, took = s_frameAt ? (double)(now - s_frameAt) / 1.0e6 : target;

    s_frameAt = now;
    if (took > 3.0 * target)
        took = 3.0 * target; /* a hitch is not a trend */
    s_frameMs += (took - s_frameMs) / 32.0;
    if (s_nearOn) {
        if (s_nearDrawn && s_frameMs > target * 1.04 && !Dvar_GetInt("r_swapInterval")) {
            s_nearOn = 0;
            s_nearOffFill = s_fill;
        }
    } else if (s_fill < s_nearOffFill * 0.7) {
        s_nearOn = 1;
        s_frameMs = target;
    }
    s_nearDrawn = 0;
}

/* How the effects are drawn this frame: DIRECT, or the layer's level. Called once
 * before the emissive pass draws them. */
static int ChooseMode(void *device)
{
    int setting = Setting(), mode, cap;
    double budget;

    s_device = device;
    if (s_queryPending) {
        DWORD count = 0;
        if (CALL(GetDataFn, s_query, QUERY_GetData)(s_query, &count, sizeof(count), 0) == 0) {
            s_queryPending = 0;
            /* The query counts samples on the frame buffer and texels in a layer. */
            s_fill = s_queryMode == DIRECT ? (double)count / (s_samples > 1 ? s_samples : 1)
                                           : (double)count * (double)(1u << (2 * s_queryMode));
        }
    }
    s_nearFrame = Dvar_GetInt("r_fxLayerTwice") != 0;
    if (!setting || !s_sceneDepthTexture)
        return DIRECT;
    if (setting >= 2)
        return setting - 2 >= LEVELS ? LEVELS - 1 : setting - 2;

    /* As needed. The budget is what this was measured to draw in the time a frame has
     * at 250 a second, and grows as the frame rate asked for falls. Going back to a
     * costlier way of drawing waits until there is room to spare, so the choice does
     * not flicker. */
    cap = Dvar_GetInt("com_maxfps");
    if (cap <= 0 || cap > 250)
        cap = 250;
    if (cap < 60)
        cap = 60;
    NearGovern(cap);
    s_nearFrame = s_nearOn && Dvar_GetInt("r_fxLayerTwice");
    budget = (double)Dvar_GetInt("r_fxLayerBudget") * 1.0e6 * 250.0 / cap;
    for (mode = DIRECT; mode < LEVELS - 1; mode++) {
        if (mode == 0 && s_samples <= 1)
            continue; /* one sample a pixel: a layer of the picture's size saves nothing */
        if (Cost(mode) <= budget * (mode < s_mode ? 0.6 : 1.0))
            break;
    }
    return mode;
}

int RB_FxLayer_Wanted(void)
{
    return Setting() != 0;
}

int RB_FxLayer_Active(void)
{
    return s_active || s_depthOnly;
}

/* Can the layer hold a material with these state bits? */
int RB_FxLayer_Holds(unsigned stateBits0, unsigned stateBits1)
{
    unsigned source = stateBits0 & 0xf, dest = (stateBits0 >> 4) & 0xf, op = (stateBits0 >> 8) & 7;
    /* Blends into the frame buffer's alpha: held when alpha takes the colour's blend
     * (no blend of its own), which the second target can follow. */
    if ((stateBits0 & 0x10000000) && (stateBits0 & 0x07000000))
        return 0;
    if (stateBits1 & 0x41) /* writes depth, or uses the stencil buffer */
        return 0;
    return op == 1 && (source == BLEND_ZERO || source == BLEND_ONE || source == BLEND_SRCALPHA) &&
           (dest == BLEND_ZERO || dest == BLEND_ONE || dest == BLEND_INVSRCALPHA);
}

/* The alpha test the device should have now: the engine's, unless it only skips
 * pixels that would change nothing. */
static void ApplyAlphaTest(void)
{
    DWORD apply = s_alphaTest;
    if (s_active && !s_alphaPass && !s_farPass && s_alphaFunc == CMP_GREATER && s_alphaRef == 0 && s_sourceBlend == BLEND_SRCALPHA)
        apply = 0;
    if (apply != s_alphaTestApplied) {
        s_alphaTestApplied = apply;
        SetState(RS_ALPHATESTENABLE, apply);
    }
}

/* While the float-Z prepass draws models for the layer's sake: depth, no colour. */
void RB_FxLayer_DepthOnly(void *device, int on)
{
    if (!on == !s_depthOnly)
        return;
    s_device = device;
    if (on) {
        s_colorWriteWanted = GetState(RS_COLORWRITEENABLE);
        SetState(RS_COLORWRITEENABLE, 0);
    } else {
        SetState(RS_COLORWRITEENABLE, s_colorWriteWanted);
    }
    s_depthOnly = on;
}

/* The Direct3D 7 renderer has no float-Z prepass: its technique sets hold no depth
 * technique. For the layer the scene's depth is drawn there too, with the scene's own
 * techniques and colour off, into the same depth texture, and only in frames that
 * use the layer (RB_FxLayer_DepthPassWanted). The layer's full-size colour target
 * stands in as the target; nothing is written to it. */
/* Should this frame draw the scene's depth for the layer? Asked while the frame's
 * commands are made, so it goes by how the last frame's effects were drawn. */
int RB_FxLayer_DepthPassWanted(void)
{
    return Setting() != 0 && s_sceneDepthTexture != NULL && s_mode != DIRECT;
}

/* Bind the depth texture and start a pass that writes depth alone. Returns 0 when
 * that cannot be done; RB_FxLayer_DepthPassEnd follows a 1. */
int RB_FxLayer_DepthPassBegin(void *device)
{
    SurfaceDesc desc;
    Viewport viewport;
    Layer *layer;

    if (s_depthPass)
        return 1;
    if (s_active || s_depthOnly || !s_sceneDepthSurface)
        return 0;
    s_device = device;
    layer = GetLayer(0);
    if (!layer)
        return 0;
    memset(&desc, 0, sizeof(desc));
    s_depthPassColor = s_depthPassDepth = NULL;
    CALL(GetSurfaceFn, device, DEV_GetRenderTarget)(device, 0, &s_depthPassColor);
    CALL(GetDepthFn, device, DEV_GetDepthStencilSurface)(device, &s_depthPassDepth);
    if (!s_depthPassColor || CALL(GetDescFn, s_depthPassColor, SURF_GetDesc)(s_depthPassColor, &desc) < 0 ||
        desc.Width != s_sceneWidth || desc.Height != s_sceneHeight) {
        Release(&s_depthPassColor);
        Release(&s_depthPassDepth);
        return 0;
    }
    /* Setting a target resets the viewport to all of it; the engine's stays. */
    CALL(ViewportFn, device, DEV_GetViewport)(device, &viewport);
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, layer->colorSurface);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_sceneDepthSurface);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &viewport);
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 6, 0, 1.0f, 0); /* depth and stencil */
    s_colorWriteWanted = GetState(RS_COLORWRITEENABLE);
    SetState(RS_COLORWRITEENABLE, 0);
    s_depthOnly = s_depthPass = 1;
    return 1;
}

void RB_FxLayer_DepthPassEnd(void)
{
    void *device = s_device;
    Viewport viewport;

    if (!s_depthPass)
        return;
    s_depthOnly = s_depthPass = 0;
    SetState(RS_COLORWRITEENABLE, s_colorWriteWanted);
    CALL(ViewportFn, device, DEV_GetViewport)(device, &viewport);
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_depthPassColor);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_depthPassDepth);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &viewport);
    Release(&s_depthPassColor);
    Release(&s_depthPassDepth);
    s_sceneDepthDrawn = 1;
}

/* Called before the emissive pass draws its effects: decides how. */
void RB_FxLayer_Frame(void *device, int skyOnlyAlpha)
{
    s_skyOnlyAlpha = skyOnlyAlpha;
    s_mode = ChooseMode(device);
}

/* Start drawing a run of effects. `sceneDepthValid` says the float-Z prepass ran this
 * frame. Returns 1 when they go into the layer and 0 when they are to be drawn
 * straight onto the frame buffer; RB_FxLayer_End follows either way. */
int RB_FxLayer_Begin(void *device, int sceneDepthValid)
{
    Saved saved;
    SurfaceDesc desc;
    Viewport scaled;
    unsigned i;

    if (s_active)
        return 0;
    s_device = device;
    s_savedColor = s_savedDepth = NULL;
    s_layer = NULL;
    s_floatZReady = 0;
    memset(&desc, 0, sizeof(desc));
    CALL(GetSurfaceFn, device, DEV_GetRenderTarget)(device, 0, &s_savedColor);
    CALL(GetDepthFn, device, DEV_GetDepthStencilSurface)(device, &s_savedDepth);
    if (s_savedColor && CALL(GetDescFn, s_savedColor, SURF_GetDesc)(s_savedColor, &desc) >= 0 && !s_early)
        s_samples = desc.MultiSampleType;
    /* Drawn early, the layer may find the back buffer bound, which has no depth buffer
     * (last frame's 2D was the last thing drawn); it needs none of the target's own. */
    if (s_mode != DIRECT && sceneDepthValid && s_savedColor && (s_savedDepth || s_early) &&
        desc.Width == s_sceneWidth && desc.Height == s_sceneHeight)
        s_layer = GetLayer(s_mode);
    if (!s_layer) {
        Release(&s_savedColor);
        Release(&s_savedDepth);
        if (!s_early)
            CountBegin(DIRECT);
        return 0;
    }
    s_level = s_mode;
    CALL(ViewportFn, device, DEV_GetViewport)(device, &s_viewport);
    for (i = 0; i < COUNT(layerStates); i++)
        s_layerWanted[i] = GetState(layerStates[i]);
    s_alphaTest = s_alphaTestApplied = GetState(RS_ALPHATESTENABLE);
    s_alphaFunc = GetState(RS_ALPHAFUNC);
    s_alphaRef = GetState(RS_ALPHAREF);
    s_sourceBlend = GetState(RS_SRCBLEND);
    s_destBlend = GetState(RS_DESTBLEND);
    s_alphaPass = s_alphaDrawn = 0;

    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_layer->colorSurface);
    if (s_level) {
        PassBegin(&saved);
        s_floatZReady = CopyFloatZ();
        ReduceDepth(s_layer->depthSurface, s_layer->reduceShader);
        if (s_nearFrame)
            ReduceDepth(s_layer->nearSurface, s_layer->reduceNearShader);
        PassEnd(&saved);
    }
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_level ? s_layer->depthSurface : s_sceneDepthSurface);
    ScaleViewport(&s_viewport, &scaled);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &scaled);
    /* black, and all of the scene still showing through; the depth stays the scene's */
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 1, 0xff000000u, 1.0f, 0);

    SetState(RS_COLORWRITEENABLE, 0xf);
    SetState(RS_ZWRITEENABLE, 0);
    SetState(RS_SEPARATEALPHABLENDENABLE, 1);
    SetState(RS_BLENDOPALPHA, 1);
    SetState(RS_SRCBLENDALPHA, BLEND_ZERO);
    SetState(RS_DESTBLENDALPHA, AlphaDestFor(GetState(RS_DESTBLEND)));
    s_active = 1;
    ApplyAlphaTest();
    CountBegin(s_level);
    return 1;
}

/* The run is drawn into the layer. Some of it also blends into the frame buffer's
 * alpha: the caller now draws those sprites again, into the target that follows what
 * they do to it. Colour holds the product of their destination factors and alpha the
 * sum their source terms come to, under the blends the engine sets for each. */
void RB_FxLayer_AlphaBegin(void)
{
    void *device = s_device;
    Viewport scaled;

    if (!s_active || s_alphaPass)
        return;
    CountEnd();
    s_alphaPass = s_alphaDrawn = 1;
    s_nearPass = 0;
    ApplyAlphaTest();
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_layer->alphaSurface);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_level ? s_layer->depthSurface : s_sceneDepthSurface);
    ScaleViewport(&s_viewport, &scaled);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &scaled);
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 1, 0x00ffffffu, 1.0f, 0); /* product 1, sum 0 */
    SetState(RS_SRCBLEND, BLEND_ZERO);
    SetState(RS_SRCBLENDALPHA, s_sourceBlend);
    SetState(RS_DESTBLENDALPHA, s_destBlend);
}

/* A smaller layer's run is drawn. The caller now draws it a second time, into a
 * picture tested against the nearest depth of each block where the first was tested
 * against the farthest (BlendShader says what for). Returns 0 when there is nothing to
 * do. Called before the alpha pass, while the device is as the first drawing left it. */
int RB_FxLayer_NearBegin(void)
{
    void *device = s_device;
    Viewport scaled;

    if (!s_active || !s_level || !s_nearFrame || s_nearPass || s_alphaPass || s_farPass)
        return 0;
    CountEnd();
    s_nearPass = s_nearDrawn = 1;
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_layer->nearColorSurface);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_layer->nearSurface);
    ScaleViewport(&s_viewport, &scaled);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &scaled);
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 1, 0xff000000u, 1.0f, 0);
    return 1;
}

/* A smaller layer's run is drawn. The caller now draws it one more time, without
 * colour, keeping in a depth buffer of its own the farthest depth any of it reaches in
 * each texel (BlendShader says what for). Returns 0 when there is nothing to do. */
int RB_FxLayer_FarBegin(void)
{
    void *device = s_device;
    Viewport scaled;

    if (!s_active || !s_level || s_farPass)
        return 0;
    CountEnd();
    if (s_alphaPass) {
        s_alphaPass = 0;
        SetState(RS_SRCBLEND, s_sourceBlend);
    }
    s_farPass = 1;
    s_nearPass = 0;
    ApplyAlphaTest(); /* the engine's own: a sprite's empty corners reach nowhere */
    s_zFuncWanted = GetState(RS_ZFUNC);
    s_zEnableWanted = GetState(RS_ZENABLE);
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_layer->colorSurface);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_layer->farSurface);
    ScaleViewport(&s_viewport, &scaled);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &scaled);
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 2, 0, 0.0f, 0);
    SetState(RS_COLORWRITEENABLE, 0);
    SetState(RS_ZENABLE, 1);
    SetState(RS_ZWRITEENABLE, 1);
    SetState(RS_ZFUNC, CMP_GREATER);
    return 1;
}

/* Called for every render state the engine sets while the layer is in use.
 * Returns 1 when the engine's value must not reach the device now. */
int RB_FxLayer_RenderState(DWORD state, DWORD value)
{
    unsigned i;
    if (s_depthOnly) {
        if (state != RS_COLORWRITEENABLE)
            return 0;
        s_colorWriteWanted = value;
        return 1;
    }
    if (s_farPass && (state == RS_ZFUNC || state == RS_ZENABLE)) {
        *(state == RS_ZFUNC ? &s_zFuncWanted : &s_zEnableWanted) = value;
        return 1;
    }
    switch (state) {
    case RS_ALPHATESTENABLE: s_alphaTest = value; ApplyAlphaTest(); return 1;
    case RS_ALPHAFUNC: s_alphaFunc = value; ApplyAlphaTest(); return 0;
    case RS_ALPHAREF: s_alphaRef = value; ApplyAlphaTest(); return 0;
    case RS_SRCBLEND:
        s_sourceBlend = value;
        ApplyAlphaTest();
        if (!s_alphaPass)
            return 0;
        SetState(RS_SRCBLENDALPHA, value); /* colour keeps ZERO: it carries the product */
        return 1;
    case RS_DESTBLEND:
        s_destBlend = value;
        if (s_alphaPass) {
            SetState(RS_DESTBLENDALPHA, value);
            return 0;
        }
        break;
    default: break;
    }
    for (i = 0; i < COUNT(layerStates); i++)
        if (layerStates[i] == state) {
            s_layerWanted[i] = value;
            return 1;
        }
    if (state == RS_DESTBLEND)
        SetState(RS_DESTBLENDALPHA, AlphaDestFor(value));
    return 0;
}

/* Called for every viewport the engine sets while the layer is in use. */
void RB_FxLayer_Viewport(const void *requested, void *scaled)
{
    s_viewport = *(const Viewport *)requested;
    if (s_active)
        ScaleViewport(&s_viewport, (Viewport *)scaled);
    else
        *(Viewport *)scaled = s_viewport;
}

/* The run of effects is drawn into the layer: give the device back what it had. */
static void Leave(void)
{
    void *device = s_device;
    unsigned i;

    s_active = s_nearPass = 0;
    if (s_alphaPass) {
        s_alphaPass = 0;
        SetState(RS_SRCBLEND, s_sourceBlend);
    }
    if (s_farPass) {
        s_farPass = 0;
        SetState(RS_ZFUNC, s_zFuncWanted);
        SetState(RS_ZENABLE, s_zEnableWanted);
    }
    ApplyAlphaTest();
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_savedColor);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_savedDepth);
    Release(&s_savedColor);
    Release(&s_savedDepth);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &s_viewport);
    for (i = 0; i < COUNT(layerStates); i++)
        SetState(layerStates[i], s_layerWanted[i]);
}

/* The layer over the frame buffer, which is bound. */
static void Apply(void)
{
    Saved saved;

    PassBegin(&saved);
    Mark();
    Composite();
    if (s_alphaDrawn)
        CompositeAlpha();
    PassEnd(&saved);
}

/* The run of effects is drawn. If it went into the layer: back to the frame buffer,
 * and the layer over it. */
void RB_FxLayer_End(void)
{
    CountEnd();
    if (!s_active)
        return;
    Leave();
    Apply();
}

/* The layer needs nothing of the frame buffer until it is put over it: what hides the
 * effects is the float-Z prepass's depth. So the one run of effects a frame usually
 * has is drawn into the layer straight after that prepass, before the scene is begun,
 * and the scene's multisampled buffer is then drawn in one go. Left for its place in
 * the emissive pass, the layer would interrupt that buffer, and on this kind of
 * graphics chip every sample of it would be written out to memory and read back in.
 *
 * `samples` is what the frame buffer will have. Returns 1 when the effects go into the
 * layer now; RB_FxLayer_EndEarly follows, and RB_FxLayer_ApplyEarly where the run
 * stands in the emissive pass. */
int RB_FxLayer_BeginEarly(void *device, int samples)
{
    int held;

    s_earlyPending = 0;
    s_samples = samples;
    s_early = 1;
    held = RB_FxLayer_Begin(device, 1);
    s_early = 0;
    return held;
}

void RB_FxLayer_EndEarly(void)
{
    CountEnd();
    if (!s_active)
        return;
    Leave();
    s_earlyPending = 1;
}

/* Is a layer drawn early waiting? */
int RB_FxLayer_EarlyPending(void)
{
    return s_earlyPending;
}

/* Put it over the frame buffer, or (apply 0) let it go. */
void RB_FxLayer_ApplyEarly(void *device, int apply, int skyOnlyAlpha)
{
    if (!s_earlyPending)
        return;
    s_earlyPending = 0;
    if (!apply)
        return;
    s_device = device;
    s_skyOnlyAlpha = skyOnlyAlpha;
    CALL(ViewportFn, device, DEV_GetViewport)(device, &s_viewport);
    Apply();
}
#endif /* COD2_MAC_D3D9 */
