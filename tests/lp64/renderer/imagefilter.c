#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "imagefilter_source.h"

_Alignas(DxGlobals) unsigned char dx[sizeof(DxGlobals)];
_Alignas(r_global_permanent_t) unsigned char rgp[sizeof(r_global_permanent_t)];
_Alignas(r_backEndGlobals_t) unsigned char backEnd[sizeof(r_backEndGlobals_t)];
_Alignas(vidConfig_t) unsigned char vidConfig[sizeof(vidConfig_t)];
Bool g_LastGlowFilter;

#define BACKEND (*(r_backEndGlobals_t *)backEnd)
#define TARGETS (((DxGlobals *)dx)->renderTargets)
#define VIDEO (*(vidConfig_t *)vidConfig)
#define MATERIALS (*(r_global_permanent_t *)rgp)

static GfxImage images[R_RENDERTARGET_COUNT];
static Material materials[9];
static GfxViewParms view;
static struct {
    GfxRenderTargetId target;
    GfxImage *source;
    const Material *material;
    float x, y, width, height, uv[4], taps[8][4];
} draws[32];
static int drawCount, endCount, targetCount;
static GfxRenderTargetId target;

void RB_SetRenderTarget(GfxRenderTargetId id)
{
    target = id;
    ++targetCount;
}

void RB_DrawStretchPic(const Material *material, float x, float y, float w, float h,
                      float s0, float t0, float s1, float t1,
                      unsigned int color, GfxPrimStatsTarget stats)
{
    assert(drawCount < 32 && color == 0xffffffff && stats == 10);
    assert(targetCount == drawCount + 1 && endCount == drawCount);
    draws[drawCount].target = target;
    draws[drawCount].source = BACKEND.currentFeedbackImage;
    draws[drawCount].material = material;
    draws[drawCount].x = x;
    draws[drawCount].y = y;
    draws[drawCount].width = w;
    draws[drawCount].height = h;
    float uv[] = { s0, t0, s1, t1 };
    memcpy(draws[drawCount].uv, uv, sizeof(uv));
    memcpy(draws[drawCount].taps, &BACKEND.codeConsts[35], sizeof(draws[drawCount].taps));
    ++drawCount;
}

void RB_EndSurface(void)
{
    ++endCount;
}

static void Init(void)
{
    memset(backEnd, 0, sizeof(backEnd));
    memset(draws, 0, sizeof(draws));
    drawCount = endCount = targetCount = 0;
    g_TotalFilterPasses = 0;
    g_LastGlowFilter = 0;
    for (int i = 0; i < R_RENDERTARGET_COUNT; ++i) {
        TARGETS[i].image = &images[i];
        TARGETS[i].width = TARGETS[i].height = 256;
    }
    for (int i = 0; i < 8; ++i)
        MATERIALS.symmetricFilterMaterial[i] = &materials[i];
    MATERIALS.glowSetupMaterial = &materials[8];
    BACKEND.viewParms = &view;
    BACKEND.resolvedSceneTarget = R_RENDERTARGET_RESOLVED_POST_SUN;
    VIDEO.width = VIDEO.height = 256;
    VIDEO.aspectRatioPixel = 1;
    view.viewport.Height = 480;
}

static void Near(float actual, float expected)
{
    if (fabsf(actual - expected) > 0.0000001f)
        fprintf(stderr, "got %.10g, expected %.10g\n", actual, expected);
    assert(fabsf(actual - expected) <= 0.0000001f);
}

static void CheckAspectPasses(int first)
{
    /* Independent discrete Gaussian sums at sigma 1.5 and 0.5, normalized
       across both signs; offsets are pair centroids divided by 256. */
    const float x[][2] = {
        { 0.002404697884f, 0.3459460971f },
        { 0.008779936723f, 0.1453340275f },
        { 0.01609063641f, 0.008625510013f }
    };
    const float y[][2] = {
        { 0.0008320857728f, 0.4997361229f },
        { 0.007812677335f, 0.0002638770559f }
    };
    assert(draws[first].material == &materials[2]);
    assert(draws[first + 1].material == &materials[1]);
    for (int i = 0; i < 3; ++i) {
        Near(draws[first].taps[i][0], x[i][0]);
        Near(draws[first].taps[i][1], 0);
        Near(draws[first].taps[i][2], 0);
        Near(draws[first].taps[i][3], x[i][1]);
    }
    for (int i = 0; i < 2; ++i) {
        Near(draws[first + 1].taps[i][0], 0);
        Near(draws[first + 1].taps[i][1], y[i][0]);
        Near(draws[first + 1].taps[i][2], 0);
        Near(draws[first + 1].taps[i][3], y[i][1]);
    }
}

static void CheckNoPass(void)
{
    assert(RB_ApplyGlowFilter(R_RENDERTARGET_RESOLVED_SCENE, R_RENDERTARGET_GLOW_0, 0)
           == R_RENDERTARGET_RESOLVED_SCENE);
    float radii[] = { 0.01f, 0 };
    RB_GlowFilterImage(radii);
    assert(!drawCount && !targetCount && !endCount && !g_TotalFilterPasses);
    assert(BACKEND.glowCount == 1 && g_LastGlowFilter);
    assert(BACKEND.glowImage[0] == &images[R_RENDERTARGET_RESOLVED_POST_SUN]);
}

static void CheckGaussianAspect(void)
{
    VIDEO.aspectRatioPixel = 3;
    g_TotalFilterPasses = 17;
    RB_GaussianFilterImage(0.5f, R_RENDERTARGET_GLOW_0);
    assert(drawCount == 2 && endCount == 2 && g_TotalFilterPasses == 17);
    CheckAspectPasses(0);
    assert(draws[0].target == R_RENDERTARGET_PINGPONG_0);
    assert(draws[0].source == &images[R_RENDERTARGET_RESOLVED_SCENE]);
    assert(draws[1].target == R_RENDERTARGET_GLOW_0);
    assert(draws[1].source == &images[R_RENDERTARGET_PINGPONG_0]);
    assert(draws[0].uv[1] == 0 && draws[0].uv[3] == 1);
    assert(draws[1].uv[1] == 1 && draws[1].uv[3] == 0);
}

static void CheckGlowAspect(void)
{
    VIDEO.aspectRatioPixel = 3;
    BACKEND.resolvedSceneTarget = R_RENDERTARGET_RESOLVED_SCENE;
    TARGETS[R_RENDERTARGET_RESOLVED_SCENE].width = 512;
    TARGETS[R_RENDERTARGET_RESOLVED_SCENE].height = 384;
    float radii[] = { 1, 0 };
    RB_GlowFilterImage(radii);
    assert(radii[0] == 0.5f && radii[1] == 0);
    assert(drawCount == 3 && endCount == 3 && g_TotalFilterPasses == 3);
    assert(BACKEND.glowCount == 1 && g_LastGlowFilter);
    assert(BACKEND.glowImage[0] == &images[R_RENDERTARGET_GLOW_0]);
    assert(draws[0].material == &materials[8]);
    /* Native render-target storage is exactly 512x384, with no POT padding. */
#if COD2_APPLE_SDK
    assert(draws[0].uv[2] == 1 && draws[0].uv[3] == 1);
#else
    assert(draws[0].uv[2] == 1 && draws[0].uv[3] == 0.75f);
#endif
    CheckAspectPasses(1);
    for (int i = 0; i < 3; ++i) {
        assert(draws[i].width == 256 && draws[i].height == 256);
        assert(draws[i].target == (i == 2 ? R_RENDERTARGET_GLOW_0 : R_RENDERTARGET_PINGPONG_0 + i));
        assert(draws[i].source == &images[i ? R_RENDERTARGET_PINGPONG_0 + i - 1 : R_RENDERTARGET_RESOLVED_SCENE]);
        assert(draws[i].uv[0] == 0 && draws[i].uv[1] == 0);
    }
}

static void CheckReversedRadii(void)
{
    float radii[] = { 2, 1 };
    RB_GlowFilterImage(radii);
    assert(isnan(radii[1]));
    assert(drawCount == 2 && endCount == 2 && g_TotalFilterPasses == 2);
    assert(g_LastGlowFilter && BACKEND.glowCount == 2);
    assert(BACKEND.glowImage[0] == &images[R_RENDERTARGET_GLOW_0]);
    assert(BACKEND.glowImage[1] == BACKEND.glowImage[0]);
    assert(draws[0].uv[1] == 0 && draws[1].uv[1] == 0);
}

static void CheckUnsignedHeight(int glow)
{
    view.viewport.Height = 0x80000000U;
    if (glow) {
        float radii[] = { 0x1.ep-24f, 0 };
        RB_GlowFilterImage(radii);
        assert(BACKEND.glowImage[0] == &images[R_RENDERTARGET_GLOW_0]);
    } else {
        RB_GaussianFilterImage(0x1.ep-24f, R_RENDERTARGET_GLOW_0);
    }
    /* Unsigned height * radius / 480 = 0.5, hence one 2D pass. */
    assert(drawCount == 1 && endCount == 1);
    assert(draws[0].material == &materials[7]);
}

static void CheckUnorderedDownsample(void)
{
    GfxImageFilterPass passes[16] = {0};
    int count = RB_GenerateGaussianFilterChain(0.5f, NAN, 512, 512, 256, 256, 16, passes);
    assert(count == 1);
    /* The Mac subtraction/select retains X when Y-X is unordered. */
    assert(isfinite(passes[0].tapOffsetsAndWeights[0][0]));
    assert(isfinite(passes[0].tapOffsetsAndWeights[0][3]));
}

static void CheckNativeExtent(void)
{
    VIDEO.width = 1280;
    VIDEO.height = view.viewport.Height = 720;
    for (int i = 0; i < R_RENDERTARGET_COUNT; ++i) {
        TARGETS[i].width = 320;
        TARGETS[i].height = 180;
    }
    RB_GaussianFilterImage(2, R_RENDERTARGET_BLURRED_SCREEN);
    assert(drawCount >= 2);
    for (int i = 0; i < drawCount; ++i) {
        assert(draws[i].x == -0.5f && draws[i].y == -0.5f);
        assert(draws[i].uv[0] == 0 && draws[i].uv[2] == 1);
        assert((draws[i].uv[1] == 0 && draws[i].uv[3] == 1) ||
               (draws[i].uv[1] == 1 && draws[i].uv[3] == 0));
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Init();
    if (!strcmp(argv[1], "no-pass")) CheckNoPass();
    else if (!strcmp(argv[1], "gaussian-aspect")) CheckGaussianAspect();
    else if (!strcmp(argv[1], "glow-aspect")) CheckGlowAspect();
    else if (!strcmp(argv[1], "reversed-radii")) CheckReversedRadii();
    else if (!strcmp(argv[1], "gaussian-height")) CheckUnsignedHeight(0);
    else if (!strcmp(argv[1], "glow-height")) CheckUnsignedHeight(1);
    else if (!strcmp(argv[1], "native-extent")) CheckNativeExtent();
    else if (!strcmp(argv[1], "unordered-downsample")) CheckUnorderedDownsample();
    else assert(0);
    printf("production imagefilter %s: passed\n", argv[1]);
}
