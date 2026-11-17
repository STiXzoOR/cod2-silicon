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
 * - Where a sample of the frame buffer does not lie at the depth the layer
 *   was tested against (the edge of an object under anti-aliasing; for a
 *   smaller layer, any place its four nearest texels disagree), the layer has
 *   no right answer of its own. Those samples are marked in the stencil
 *   buffer and the layer is blended over the others only. A marked sample
 *   then takes the layer's value from the neighbouring texel that was tested
 *   against its own surface: the nearest one around it if the sample lies in
 *   front, the farthest if it lies behind. Smoke changes little from one
 *   pixel to the next, so an anti-aliased edge keeps its shape and what shows
 *   on each side of it. (Drawing the effects a second time into the marked
 *   samples alone would be exact, and was tried: the graphics chip takes as
 *   long to turn the unmarked samples away as it does to draw them.)
 *
 * - Only blends the target can hold go into it: source ZERO, ONE or SRCALPHA
 *   against destination ZERO, ONE or INVSRCALPHA. A material that writes depth
 *   or uses the stencil buffer is drawn as it always was. Without the float-Z
 *   prepass (r_zFeather 0) there is no scene depth and the layer is not used.
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
    RS_SCISSORTESTENABLE = 174, RS_TWOSIDEDSTENCILMODE = 185, RS_SEPARATEALPHABLENDENABLE = 206, RS_SRCBLENDALPHA = 207,
    RS_DESTBLENDALPHA = 208, RS_BLENDOPALPHA = 209,
};
enum { BLEND_ZERO = 1, BLEND_ONE = 2, BLEND_SRCALPHA = 5, BLEND_INVSRCALPHA = 6 };
enum { CMP_LESS = 2, CMP_EQUAL = 3, CMP_GREATER = 5, CMP_ALWAYS = 8, OP_KEEP = 1, OP_ZERO = 2, OP_REPLACE = 3 };
enum { FORMAT_A8R8G8B8 = 21, FORMAT_A16B16G16R16 = 36, FORMAT_INTZ = 0x5A544E49 };
enum { USAGE_RENDERTARGET = 1, USAGE_DEPTHSTENCIL = 2, QUERY_OCCLUSION = 9, ISSUE_END = 1, ISSUE_BEGIN = 2 };
/* Stencil bits of a sample the layer cannot stand for: it lies beyond, or in front of,
 * the depth the layer was tested against. */
enum { MARK_FAR = 0x40, MARK_NEAR = 0x80, MARK_ANY = 0xc0 };
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
    int failed;
} Layer;

static void *s_device;
static int s_active;    /* effects are being drawn into the layer */
static int s_depthOnly; /* the prepass is drawing models and sky: depth, no colour */
static void *s_sceneDepthTexture, *s_sceneDepthSurface; /* the prepass's depth, at the picture's size */
static unsigned s_sceneWidth, s_sceneHeight;
static Layer s_layers[LEVELS], *s_layer; /* s_layer: the one in use */
static Layer *GetLayer(int level);
static int s_level;
static unsigned s_samples;              /* samples per pixel of the frame buffer */
static void *s_savedColor, *s_savedDepth;
static Viewport s_viewport;             /* what the engine last asked for, at full size */
/* The count of pixels the effects cover. */
static void *s_opaqueShader; /* writes 1 to every channel it may */
static void *s_redShader;    /* a texture's red channel in every channel */
static int s_alphaPass;      /* the sprites that blend into alpha are being drawn a second time */
static int s_alphaDrawn;     /* ...and the frame buffer's alpha is owed the result */
static int s_skyOnlyAlpha;   /* this frame only sky samples need it */
static DWORD s_destBlend;    /* DESTBLEND as the engine wants it */
static void *s_query;
static int s_queryPending, s_queryMode, s_measuring;
static double s_fill;                   /* frame buffer pixels, as last counted */
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
    int i;
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
    Release(&s_redShader);
    s_alphaPass = s_alphaDrawn = 0;
    s_active = s_depthOnly = s_queryPending = s_measuring = 0;
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

static void *CompileShader(const char *name, const char *source)
{
    void *blob = NULL, *messages = NULL, *shader = NULL;
    if (Win_D3D9_CompileShader(source, (int)strlen(source), name, "main", "ps_2_0", &blob, &messages) < 0 || !blob) {
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

/* Depth of a block of the scene's texels (2 by 2 for a half, 4 by 4 for a quarter):
 * the farthest, so an effect is drawn wherever any pixel of the block would show it.
 * Sizes are written into the source, so the shaders here need no constants of the
 * engine's. */
static void *ReduceShader(int level)
{
    char source[4096];
    int count = 1 << level, x, y, used;
    used = snprintf(source, sizeof source,
                    "sampler depthMap : register(s0);\n"
                    "struct Out { float4 color : COLOR0; float depth : DEPTH; };\n"
                    "Out main(float2 uv : TEXCOORD0)\n"
                    "{\n"
                    "    Out o;\n"
                    "    float z = 0.0;\n");
    for (y = 0; y < count; y++)
        for (x = 0; x < count; x++)
            used += snprintf(source + used, sizeof source - (size_t)used,
                             "    z = max(z, tex2D(depthMap, uv + float2(%.9g, %.9g)).r);\n",
                             (x + 0.5 - count / 2.0) / s_sceneWidth, (y + 0.5 - count / 2.0) / s_sceneHeight);
    snprintf(source + used, sizeof source - (size_t)used,
             "    o.depth = z;\n"
             "    o.color = 0;\n"
             "    return o;\n"
             "}\n");
    return CompileShader("fxlayer_reduce", source);
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
    }
    if (!layer->colorTexture || !layer->alphaSurface ||
        CALL(GetSurfaceFn, layer->colorTexture, TEX_GetSurfaceLevel)(layer->colorTexture, 0, &layer->colorSurface) < 0 ||
        (level && (!CreateDepth(device, layer->width, layer->height, &layer->depthTexture, &layer->depthSurface) ||
                   !(layer->reduceShader = ReduceShader(level)))) ||
        !(layer->markShader[0] = MarkShader(layer, 0, level)) || !(layer->markShader[1] = MarkShader(layer, 1, level)) ||
        !(layer->fixShader[0] = FixShader(layer, 0, level)) || !(layer->fixShader[1] = FixShader(layer, 1, level))) {
        layer->failed = 1;
        fprintf(stderr, "r_fxLayer: no layer of %ux%u; effects are drawn without it\n", layer->width, layer->height);
        return NULL;
    }
    return layer;
}

static void *LayerDepthTexture(void)
{
    return s_level ? s_layer->depthTexture : s_sceneDepthTexture;
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
    DWORD states[32], stages[6], samplers[2][5];
    void *textures[2], *decl, *vs, *ps;
} Saved;

static const DWORD passStates[] = { RS_ZENABLE, RS_ZWRITEENABLE, RS_ZFUNC, RS_ALPHATESTENABLE, RS_SRCBLEND, RS_DESTBLEND,
    RS_CULLMODE, RS_ALPHABLENDENABLE, RS_FOGENABLE, RS_LIGHTING, RS_COLORWRITEENABLE, RS_BLENDOP,
    RS_SEPARATEALPHABLENDENABLE, RS_SCISSORTESTENABLE, RS_STENCILENABLE, RS_STENCILFAIL, RS_STENCILZFAIL, RS_STENCILPASS,
    RS_STENCILFUNC, RS_STENCILREF, RS_STENCILMASK, RS_STENCILWRITEMASK, RS_TWOSIDEDSTENCILMODE };
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
    for (unit = 0; unit < 2; unit++) {
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

    for (unit = 0; unit < 2; unit++) {
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

/* Fill the layer's smaller depth buffer from the scene's. The layer's colour target
 * is bound only because a depth buffer cannot be drawn into alone. */
static void ReduceDepth(void)
{
    void *device = s_device;
    Viewport view = { 0, 0, s_layer->width, s_layer->height, 0.0f, 1.0f };

    SetState(RS_ZENABLE, 1);
    SetState(RS_ZWRITEENABLE, 1);
    SetState(RS_ZFUNC, CMP_ALWAYS);
    SetState(RS_ALPHABLENDENABLE, 0);
    SetState(RS_COLORWRITEENABLE, 0);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_layer->depthSurface);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &view);
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_layer->reduceShader);
    PassTexture(0, s_sceneDepthTexture, 1);
    /* A layer of odd size reaches past the scene's edge; read as far as it reaches. */
    PassDraw(0.0f, 0.0f, (float)view.Width, (float)view.Height, 0.0f, 0.0f,
             (float)(view.Width << s_level) / (float)s_sceneWidth, (float)(view.Height << s_level) / (float)s_sceneHeight);
    SetState(RS_ZWRITEENABLE, 0);
}

/* Mark the samples of the frame buffer that the layer cannot stand for. */
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
    PassTexture(0, LayerDepthTexture(), 1);
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
    CALL(SetObjectFn, device, DEV_SetPixelShader)(device, NULL);
    for (i = 0; i < COUNT(passStages); i++)
        CALL(SetStageFn, device, DEV_SetTextureStageState)(device, passStages[i][0], passStages[i][1], values[i]);
    PassTexture(0, s_layer->colorTexture, s_level ? 2 : 1);
    PassDrawViewport((float)(s_layer->width << s_level), (float)(s_layer->height << s_level));

    /* A sample marked both ways lies between two surfaces; it goes with the nearer. */
    SetState(RS_STENCILWRITEMASK, MARK_ANY);
    SetState(RS_STENCILPASS, OP_ZERO);
    PassTexture(0, LayerDepthTexture(), 1);
    PassTexture(1, s_layer->colorTexture, 1);
    for (pass = 1; pass >= 0; pass--) {
        SetState(RS_STENCILREF, pass ? MARK_NEAR : MARK_FAR);
        SetState(RS_STENCILMASK, pass ? MARK_NEAR : MARK_ANY);
        CALL(SetObjectFn, device, DEV_SetPixelShader)(device, s_layer->fixShader[pass]);
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
    if (s_active && !s_alphaPass && s_alphaFunc == CMP_GREATER && s_alphaRef == 0 && s_sourceBlend == BLEND_SRCALPHA)
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
    memset(&desc, 0, sizeof(desc));
    CALL(GetSurfaceFn, device, DEV_GetRenderTarget)(device, 0, &s_savedColor);
    CALL(GetDepthFn, device, DEV_GetDepthStencilSurface)(device, &s_savedDepth);
    if (s_savedColor && CALL(GetDescFn, s_savedColor, SURF_GetDesc)(s_savedColor, &desc) >= 0)
        s_samples = desc.MultiSampleType;
    if (s_mode != DIRECT && sceneDepthValid && s_savedColor && s_savedDepth && desc.Width == s_sceneWidth &&
        desc.Height == s_sceneHeight)
        s_layer = GetLayer(s_mode);
    if (!s_layer) {
        Release(&s_savedColor);
        Release(&s_savedDepth);
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
        ReduceDepth();
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
    ApplyAlphaTest();
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_layer->alphaSurface);
    ScaleViewport(&s_viewport, &scaled);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &scaled);
    CALL(ClearFn, device, DEV_Clear)(device, 0, NULL, 1, 0x00ffffffu, 1.0f, 0); /* product 1, sum 0 */
    SetState(RS_SRCBLEND, BLEND_ZERO);
    SetState(RS_SRCBLENDALPHA, s_sourceBlend);
    SetState(RS_DESTBLENDALPHA, s_destBlend);
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

/* The run of effects is drawn. If it went into the layer: back to the frame buffer,
 * and the layer over it. */
void RB_FxLayer_End(void)
{
    void *device = s_device;
    Saved saved;
    unsigned i;

    CountEnd();
    if (!s_active)
        return;
    s_active = 0;
    if (s_alphaPass) {
        s_alphaPass = 0;
        SetState(RS_SRCBLEND, s_sourceBlend);
    }
    ApplyAlphaTest();
    CALL(SetSurfaceFn, device, DEV_SetRenderTarget)(device, 0, s_savedColor);
    CALL(SetDepthFn, device, DEV_SetDepthStencilSurface)(device, s_savedDepth);
    Release(&s_savedColor);
    Release(&s_savedDepth);
    CALL(ViewportFn, device, DEV_SetViewport)(device, &s_viewport);
    for (i = 0; i < COUNT(layerStates); i++)
        SetState(layerStates[i], s_layerWanted[i]);

    PassBegin(&saved);
    Mark();
    Composite();
    if (s_alphaDrawn)
        CompositeAlpha();
    PassEnd(&saved);
}
#endif /* COD2_MAC_D3D9 */
