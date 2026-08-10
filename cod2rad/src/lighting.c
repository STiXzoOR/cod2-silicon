/*
 * lighting.c — Lightmap allocation, light sampling, gamma correction, final lightmap building
 */

#include "cod2rad64.h"
#include "crt_math_patches.h"
#define powf original_powf

extern void memcpy_fast(void *dst, const void *src, int size);
extern void BuildFinalLightmap_PerPixel(int lightmapIdx, int col, int row);

float          g_ambientR;
float          g_ambientG;
float          g_ambientB;
float          g_contrastScale = 2.0f;

/* g_contrastGamma is the same variable as g_contrastGain — alias so cmdline
 * or worldspawn override flows through to AdjustLightingContrast */
extern float g_contrastGain;
#define g_contrastGamma g_contrastGain

/* YUV luminance weights — bit-exact from binary .rdata */
float          g_lumWeightR = 0.2989999949932098388671875f;
float          g_lumWeightG = 0.5870000123977661132812500f;
float          g_lumWeightB = 0.1140000000596046447753906f;
unsigned char g_lightmapOutput[MAX_RAD_LIGHTMAP_BYTES];

void          *g_lightingSampleCallback;
void          *g_lightingPixelCallback;
int            g_lightmapSize = 0;
void          *g_lightingSamples;
int            g_usefulSampleCount = 0;
int            g_lightSourceCount;
float         *g_aoFactors;

/* g_degamma is the same variable as g_gamma — alias for readability */
extern float g_gamma;
#define g_degamma g_gamma

void          *g_sampleVarsPool;
int            g_totalSampleCount;
float          g_energyScale = 0.333333343f; /* 1/3 */
int            g_totalLightCount;
float          g_lightScale = 2.0f;
void          *g_sunDirGlobals;

/* g_numTraceDirections is the same variable as g_lightmapHeight */
extern int g_lightmapHeight;
#define g_numTraceDirections g_lightmapHeight

float         *g_traceDirections;
float         *g_lightDirArray;

/* 8 SH basis direction vectors (sky + 6 tetrahedral + sky-up) */
const float g_shBasis[24] = {
    0.0f, 0.0f, -1.0f,
    0.942809045f, 0.0f, 0.333333343f,
    -0.471404523f, 0.816496551f, 0.333333343f,
    0.471404523f, 0.816496551f, 0.333333343f,
    -0.471404523f, -0.816496551f, -0.333333343f,
    0.471404523f, -0.816496551f, -0.333333343f,
    -0.942809045f, 0.0f, -0.333333343f,
    0.0f, 0.0f, 1.0f
};

#define SAMPLE_VARS_SIZE 96

/*
================
Lighting_AllocLightmapData

Allocate the lightmap data buffer.
================
*/
void Lighting_AllocLightmapData(void)
{
    Assert("(lightingGlob.lmapCount >= 0)", ".\\lighting.cpp", 0x36, 0, 1);

    g_lightingSamples = malloc((unsigned long long)g_lightmapSize * LIGHTMAP_DATA_SIZE);
    if (!g_lightingSamples)
    {
        ErrorMsg("Couldn't allocate %g MB for lightmap data in %i lightmaps\n",
                    (double)((float)g_lightmapSize * (float)LIGHTMAP_DATA_SIZE / (1024.0f * 1024.0f)),
                    g_lightmapSize);
    }

    memset(g_lightingSamples, 0, (unsigned long long)g_lightmapSize << 23);

    g_lightSourceCount = GetPointLightCount() + 2;

    if (g_aoEnabled)
    {
        long long totalPixels = (long long)g_lightmapSize * 512 * 512;
        g_aoFactors = (float *)malloc(totalPixels * sizeof(float));
        if (!g_aoFactors)
            ErrorMsg("Couldn't allocate AO buffer (%lld pixels)\n", totalPixels);
        for (long long i = 0; i < totalPixels; i++)
            g_aoFactors[i] = 1.0f;
        Com_Printf("AO enabled: %d samples, %.0f unit distance, %lld pixels\n",
                    g_aoSamples, (double)g_aoDist, totalPixels);
    }
}

/*
================
Lighting_RegisterLightmap

Register a lightmap index, update max count.
================
*/
void Lighting_RegisterLightmap(int lmapIndex)
{
    int newCount;

    Assert("lmapIndex != LIGHTMAP_NONE", ".\\lighting.cpp", 0x0D, 0, 1);
    Assert("lightingGlob.lmapDefs == NULL", ".\\lighting.cpp", 0x2D, 0, 1);

    newCount = lmapIndex + 1;
    if (g_lightmapSize < newCount)
        g_lightmapSize = newCount;
}

/*
================
GetLightingSample

Look up a lighting sample by lightmap index and UV coords.
================
*/
void GetLightingSample(int lmapIndex, float sScaled, float tScaled, void **outSample)
{
    int s, t;

    Assert("(lmapIndex >= 0 && lmapIndex < ((124 * 512)))", ".\\lighting.cpp", 0x46, 0, 1);
    Assert("sample", ".\\lighting.cpp", 0x47, 0, 1);

    s = (int)floorf(sScaled);
    t = (int)floorf(tScaled);

    Assert("(s >= 0 && s < ((512 < 1024) ? 512 : 1024))", ".\\lighting.cpp", 0x4C, 0, 1);
    Assert("(t >= 0 && t < ((512 < 1024) ? 512 : 1024))", ".\\lighting.cpp", 0x4D, 0, 1);

    *outSample = (char *)g_lightingSamples
               + (((long long)lmapIndex * 512 + t) * 512 + s) * 32;
}

/*
================
GetLightingSubSample

Look up a subsample with different range checks.
================
*/
void GetLightingSubSample(int lmapIndex, float sScaled, float tScaled, SubSample_t *outSubSample)
{
    int s, t;

    Assert("(lmapIndex >= 0 && lmapIndex < ((124 * 512)))", ".\\lighting.cpp", 0x58, 0, 1);
    Assert("subSample", ".\\lighting.cpp", 0x59, 0, 1);

    s = (int)floorf(sScaled);
    t = (int)floorf(tScaled);

    Assert("(s >= 0 && s < ((512 < 1024) ? 512 : 1024))", ".\\lighting.cpp", 0x5E, 0, 1);
    Assert("(t >= 0 && t < ((512 < 1024) ? 512 : 1024))", ".\\lighting.cpp", 0x5F, 0, 1);

    outSubSample->s = s & 1;
    outSubSample->t = t & 1;

    outSubSample->sample = (Sample_t *)((char *)g_lightingSamples
                  + (((long long)lmapIndex * 512 + (t / 2)) * 512 + (s / 2)) * 32);
}

/*
================
DegammaColorChannel

Apply degamma (raise to g_gamma power).
================
*/
float DegammaColorChannel(float color)
{
    Assert("(color >= 0)", ".\\lighting.cpp", 0xBD, 0, 1);

    return powf(color, g_degamma);
}

/*
================
GammaCorrectColorChannel

Apply inverse gamma correction.
================
*/
float GammaCorrectColorChannel(float color)
{
    Assert("(color >= 0)", ".\\lighting.cpp", 0xCF, 0, 1);

    return powf(color, 1.0f / g_degamma);
}

/*
================
DegammaColor

Apply degamma to RGB color in place.
================
*/
void DegammaColor(float *color)
{
    Assert("(color >= 0)", ".\\lighting.cpp", 0xBD, 0, 1);
    color[0] = powf(color[0], g_degamma);

    Assert("(color >= 0)", ".\\lighting.cpp", 0xBD, 0, 1);
    color[1] = powf(color[1], g_degamma);

    Assert("(color >= 0)", ".\\lighting.cpp", 0xBD, 0, 1);
    color[2] = powf(color[2], g_degamma);
}

/*
================
Lighting_InitSamples

Count useful samples, allocate vars pool, assign vars.
================
*/
void Lighting_InitSamples(void)
{
    int allocSize;

    Assert("lightingGlob.totalSampleCount == 0", ".\\lighting.cpp", 0x9D, 0, 1);

    g_totalSampleCount = g_lightmapSize << 18;

    Assert("lightingGlob.usefulSampleCount == 0", ".\\lighting.cpp", 0xA0, 0, 1);

    /* pass 1: count useful samples */
    g_lightingSampleCallback = (void *)Lighting_IncrementUsefulSampleCount;
    ForEachLightmapPixel(g_lightmapSize << 18,
                          (void *)Lighting_SampleCallback_Trampoline1, 1);

    Assert("lightingGlob.usefulSampleCount <= lightingGlob.totalSampleCount",
           ".\\lighting.cpp", 0xA3, 0, 1);

    /* allocate vars pool */
    allocSize = g_usefulSampleCount * 3 * 32;
    g_sampleVarsPool = malloc((unsigned long long)allocSize);
    if (!g_sampleVarsPool)
        Com_Printf("Couldn't allocate %.2g MB for %i useful samples\n",
                    (double)((float)allocSize / (1024.0f * 1024.0f)),
                    g_usefulSampleCount);

    memset(g_sampleVarsPool, 0, allocSize);

    /* pass 2: assign vars to each useful sample */
    g_usefulSampleCount = 0;
    g_lightingSampleCallback = (void *)Lighting_AllocSampleVars;
    ForEachLightmapPixel(g_lightmapSize << 18,
                          (void *)Lighting_SampleCallback_Trampoline1, 1);
}

/*
================
Lighting_InitAntiBleed

Allocate anti-bleed vars for empty samples.
================
*/
void Lighting_InitAntiBleed(void)
{
    int antiBleedCount;
    int allocSize;
    char *antiBleedPool;
    int antiBleedIndex;
    int lmap, row, col;
    long long sampleOffset = 0;

    antiBleedCount = g_totalSampleCount - g_usefulSampleCount;
    allocSize = antiBleedCount * 3 * 32;

    antiBleedPool = (char *)malloc((unsigned long long)antiBleedCount * SAMPLE_VARS_SIZE);
    if (!antiBleedPool)
        Com_Printf("Couldn't allocate %i bytes to fix lightmap bleeding\n", allocSize);

    memset(antiBleedPool, 0, allocSize);

    antiBleedIndex = 0;

    for (lmap = 0; lmap < g_lightmapSize; lmap++)
    {
        for (row = 0; row < 512; row++)
        {
            for (col = 0; col < 512; col++)
            {
                char *sampleSlot;

                sampleSlot = (char *)g_lightingSamples + sampleOffset;

                if (*(void **)sampleSlot == NULL)
                {
                    Assert("antiBleedIndex < lightingGlob.totalSampleCount - lightingGlob.usefulSampleCount",
                           ".\\lighting.cpp", 0x1AA, 0, 1);

                    *(void **)sampleSlot = antiBleedPool
                        + (long long)antiBleedIndex * SAMPLE_VARS_SIZE;
                    antiBleedIndex++;
                }

                sampleOffset += 32;
            }
        }
    }

    Assert("antiBleedIndex == lightingGlob.totalSampleCount - lightingGlob.usefulSampleCount",
           ".\\lighting.cpp", 0x1B1, 0, 1);
}

/*
================
InitBleeding

Init anti-bleed, then start bilinear bleeding pass.
================
*/
void InitBleeding(int threadCount)
{
    Lighting_InitAntiBleed();
    Lmap_InitBilinearBleeding(g_lightmapSize, threadCount);
}

/*
================
EncodeGammaCorrectedByte

Gamma correct a color channel, encode as byte [0..255].
================
*/
unsigned char EncodeGammaCorrectedByte(float value)
{
    float corrected;

    if (value <= 0.0f)
        return 0;
    if (value >= 1.0f)
        return 255;

    Assert("(color >= 0)", ".\\lighting.cpp", 0xCF, 0, 1);

    corrected = powf(value, 1.0f / g_degamma);

    if (corrected <= 0.0f)
        return 0;
    if (corrected >= 1.0f)
        return 255;

    return (unsigned char)(long long)floorf(corrected * 255.0f + 0.5f);
}

/*
================
BuildFinalLightmaps_TripleLoop

Iterate all lightmap pixels and build final data.
================
*/
void BuildFinalLightmaps_TripleLoop(void)
{
    int lmap, row, col;

    Com_Printf("Saving lightmaps...\n");

    for (lmap = 0; lmap < g_lightmapSize; lmap++)
    {
        for (row = 0; row < 512; row++)
        {
            for (col = 0; col < 512; col++)
            {
                BuildFinalLightmap_PerPixel(lmap, col, row);
            }
        }
    }

    { extern int numBSPLightBytes; numBSPLightBytes = g_lightmapSize << 22; }
}

/*
================
Lighting_GetGatheredLight

Read gathered light RGB from a sample's vars.
================
*/
void Lighting_GetGatheredLight(LightingSample_t *sample, float *outColor)
{
    SampleVars_t *vars;

    Assert("sample", ".\\lighting.cpp", 0xB2, 0, 1);
    Assert("sample->vars", ".\\lighting.cpp", 0xB3, 0, 1);

    vars = sample->vars;

    Assert("!IS_NAN((sample->vars->gathered)[0]) && !IS_NAN((sample->vars->gathered)[1]) && !IS_NAN((sample->vars->gathered)[2])",
           ".\\lighting.cpp", 0xB4, 0, 1);

    outColor[0] = vars->gathered[0];
    outColor[1] = vars->gathered[1];
    outColor[2] = vars->gathered[2];
}

/*
================
AdjustLightingContrast

Adjust contrast of lighting samples.
If baseIndex == -1, uses midpoint of min/max luminance as base.
Otherwise uses luminance of sample[baseIndex].
================
*/
void AdjustLightingContrast(int sampleCount, int baseIndex, float *srcSamples, float *dstColors)
{
    int i;
    float luminances[16];
    float minLum, maxLum;
    float baseLum;
    float contrast;
    float contrastPow;

    Assert("(sampleCount > 0 && sampleCount <= (sizeof(luminances) / sizeof(luminances[0])))",
           ".\\lighting.cpp", 0x12B, 0, 1);
    Assert("((baseIndex >= 0 && baseIndex < sampleCount) || baseIndex == -1)",
           ".\\lighting.cpp", 0x12C, 0, 1);
    Assert("srcSamples", ".\\lighting.cpp", 0x12D, 0, 1);
    Assert("dstColors", ".\\lighting.cpp", 0x12E, 0, 1);

    minLum = 3.402823e+38f;
    maxLum = -3.402823e+38f;

    for (i = 0; i < sampleCount; i++)
    {
        float r, g, b, lum;

        r = g_ambientR + srcSamples[i * 3 + 0];
        g = g_ambientG + srcSamples[i * 3 + 1];
        b = g_ambientB + srcSamples[i * 3 + 2];
        dstColors[i * 3 + 0] = r;
        dstColors[i * 3 + 1] = g;
        dstColors[i * 3 + 2] = b;

        lum = r * g_lumWeightR + g * g_lumWeightG + b * g_lumWeightB;
        luminances[i] = lum;

        if (lum < minLum) minLum = lum;
        if (lum > maxLum) maxLum = lum;
    }

    if (baseIndex == -1)
        baseLum = (maxLum + minLum) * 0.5f;
    else
        baseLum = luminances[baseIndex];

    contrast = maxLum - minLum;

    Assert("contrast >= 0.0f", ".\\lighting.cpp", 0x141, 0, 1);

    if (contrast == 0.0f || contrast >= 0.5f)
        goto done;

    contrastPow = powf(contrast * g_contrastScale, -0.0f - g_contrastGamma);

    for (i = 0; i < sampleCount; i++)
    {
        float lum = luminances[i];
        float adjusted = (lum - baseLum) * contrastPow + baseLum;

        if (adjusted <= 0.0f)
        {
            dstColors[i * 3 + 0] = 0.0f;
            dstColors[i * 3 + 1] = 0.0f;
            dstColors[i * 3 + 2] = 0.0f;
        }
        else
        {
            float scale = adjusted / lum;
            dstColors[i * 3 + 0] *= scale;
            dstColors[i * 3 + 1] *= scale;
            dstColors[i * 3 + 2] *= scale;
        }
    }

done:
    return;
}

/*
================
BuildFinalLightmap_PerPixel

Build final lightmap data for a single pixel.
Adjusts contrast for 4 light sources, gamma corrects and encodes
each RGB channel as a byte, writes to lightmap output.
================
*/
void BuildFinalLightmap_PerPixel(int lmapIndex, int col, int row)
{
    LightmapSample_t *sample;
    SampleVars_t *vars;
    float adjustedColors[4 * 3];
    unsigned char encodedBytes[12];
    float gammaCorrectedVars[4];
    int i, j;
    unsigned char *dst;

    sample = (LightmapSample_t *)g_lightingSamples
           + ((long long)lmapIndex * 512 + row) * 512 + col;

    if (sample->weight <= 0.0f)
    {
        gammaCorrectedVars[0] = 0.0f;
        gammaCorrectedVars[1] = 0.0f;
        gammaCorrectedVars[2] = 0.0f;
        gammaCorrectedVars[3] = 0.0f;
        *(long long *)&encodedBytes[0] = 0;
        *(int *)&encodedBytes[8] = 0;
        goto write_output;
    }

    vars = sample->vars;
    Assert("sample->vars", ".\\lighting.cpp", 0x16A, 0, 1);

    AdjustLightingContrast(4, 0, vars->incident, adjustedColors);

    if (g_aoEnabled && g_aoFactors)
    {
        long long pixelIdx = ((long long)lmapIndex * 512 + row) * 512 + col;
        float ao = g_aoFactors[pixelIdx];
        for (i = 0; i < 12; i++)
            adjustedColors[i] *= ao;
    }

    /* encode each adjusted color channel as gamma-corrected byte */
    for (j = 0; j < 3; j++)
    {
        float *colorPtr = &adjustedColors[j];
        unsigned char *bytePtr = &encodedBytes[j * 4];

        for (i = 0; i < 4; i++)
        {
            bytePtr[i] = EncodeGammaCorrectedByte(*colorPtr);
            colorPtr += 3;
        }
    }

    /* gamma correct intensity values */
    {
        float *srcPtr = vars->intensity;
        float *dstPtr = gammaCorrectedVars;
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 2; j++)
            {
                float val = *srcPtr++;
                Assert("(color >= 0)", ".\\lighting.cpp", 0xCF, 0, 1);
                *dstPtr++ = powf(val, 1.0f / g_degamma);
            }
        }
    }

write_output:
    /* write encoded RGB bytes to lightmap output (3 layers at 1MB stride) */
    {
        int baseOffset = lmapIndex * 2048 + row;
        int pixelAddr = (baseOffset * 512 + col) * 4;
        dst = g_lightmapOutput + pixelAddr;

        for (i = 0; i < 3; i++)
        {
            dst[0] = encodedBytes[i * 4 + 0];
            dst[1] = encodedBytes[i * 4 + 1];
            dst[2] = encodedBytes[i * 4 + 2];
            dst[3] = encodedBytes[i * 4 + 3];
            dst += 0x100000;
        }
    }

    /* write gamma-corrected intensity to lightgrid output */
    {
        int gridBase = (lmapIndex * 2048 + row + 0x600);
        int gridAddr = (gridBase * 1024 + col) * 2;
        dst = g_lightmapOutput + gridAddr;

        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 2; j++)
            {
                *dst = EncodeFloatInByte(gammaCorrectedVars[i * 2 + j]);
                dst++;
            }
            dst += 0x3FE;
        }
    }
}

/*
================
EncodeFloatInByte

Clamp float to [0,1], encode as byte [0..255].
================
*/
unsigned char EncodeFloatInByte(float value)
{
    if (value <= 0.0f)
        return 0;
    if (value >= 1.0f)
        return 255;

    return (unsigned char)(int)floorf(value * 255.0f + 0.5f);
}

/*
================
Lighting_AllocSampleVars

Assign vars from pool to a lighting sample.
================
*/
void Lighting_AllocSampleVars(LightingSample_t *sample)
{
    Assert("sample->vars == NULL", ".\\lighting.cpp", 0x93, 0, 1);

    sample->vars = (float *)((char *)g_sampleVarsPool
                   + (long long)g_usefulSampleCount * SAMPLE_VARS_SIZE);
    g_usefulSampleCount++;
}

/*
================
Lighting_IncrementUsefulSampleCount

Increment the useful sample counter.
================
*/
void Lighting_IncrementUsefulSampleCount(void)
{
    g_usefulSampleCount++;
}

/*
================
Lighting_SampleCallback_Trampoline1

Trampoline for per-sample callback.
================
*/
typedef void (*SampleCallbackFn)(void *sample);

void Lighting_SampleCallback_Trampoline1(int sampleIndex)
{
    LightmapSample_t *sample;

    sample = (LightmapSample_t *)g_lightingSamples + sampleIndex;
    if (sample->weight != 0.0f)
    {
        ((SampleCallbackFn)g_lightingSampleCallback)(sample);
    }
}

/*
================
Lighting_PixelCallback_Trampoline

Trampoline for per-pixel callback.
================
*/
typedef void (*PixelCallbackFn)(void *pixel, int index, int a2);

void Lighting_PixelCallback_Trampoline(int pixelIndex, int a2)
{
    void *pixel;

    pixel = (char *)g_lightingSamples + ((long long)pixelIndex << 23);
    ((PixelCallbackFn)g_lightingPixelCallback)(pixel, pixelIndex, a2);
}

/*
================
ForEachUsefulLightingSample

Iterate all useful lighting samples.
================
*/
void ForEachUsefulLightingSample(void *callback, int a2)
{
    g_lightingSampleCallback = callback;
    ForEachLightmapPixel(g_lightmapSize << 18, (void *)Lighting_SampleCallback_Trampoline1, a2);
}

/*
================
Lighting_ForEachPixel_Helper

Iterate all lightmap pixels.
================
*/
void Lighting_ForEachPixel_Helper(void *callback, int a2)
{
    g_lightingPixelCallback = callback;
    ForEachLightmapPixel(g_lightmapSize, (void *)Lighting_PixelCallback_Trampoline, a2);
}
