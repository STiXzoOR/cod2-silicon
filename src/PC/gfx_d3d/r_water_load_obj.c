#include "common_types.h"
#include "imports.h"
#include <stdint.h>

extern void *Material_Alloc(int size);
extern void GaussianRandom(float *real, float *imag);
extern GfxImage * R_CreateWaterMap(char *name, int imageWidth, int imageHeight);
extern const char *va(const char *fmt, ...);
extern double exp(double x);
extern double log(double x);
extern float fabsf(float x);
extern float sqrtf(float x);

#if defined(COD2_X64)
/* Windows' renderer DLL owns its CRT random stream independently of the game.
 * Its per-thread seed starts at 1 (100fb56c), and no renderer code calls srand.
 * Cloud construction and water setup are the only reachable consumers. */
static unsigned int s_rendererRandomGeneration;

void R_ResetRendererRandom(void)
{
    /* CL_ShutdownRef unloads that DLL; GetRefAPI starts its next lifetime.
     * A generation also resets any surviving native worker's private stream. */
    ++s_rendererRandomGeneration;
}

int R_RendererRandom(void)
{
    static _Thread_local unsigned int generation;
    static _Thread_local uint32_t state = 1;
    if (generation != s_rendererRandomGeneration) {
        generation = s_rendererRandomGeneration;
        state = 1;
    }
    state = state * UINT32_C(214013) + UINT32_C(2531011);
    return (int)((state >> 16) & UINT32_C(32767));
}

static void R_WaterGaussianRandom(float *real, float *imag)
{
    float s1, s2, w;
    do {
        s1 = ((float)R_RendererRandom() / 32768.0f) * 2.0f - 1.0f;
        s2 = ((float)R_RendererRandom() / 32768.0f) * 2.0f - 1.0f;
        w = s1 * s1 + s2 * s2;
    } while (w > 1.0f);
    /* 100587f0 stores log(w), the quotient and sqrt back to float. */
    float logW = (float)log((double)w);
    float scale = sqrtf((-2.0f * logW) / w);
    *real = s1 * scale;
    *imag = s2 * scale;
}
#endif

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

qboolean R_WatersEquivalent(const water_t *w0, const water_t *w1)
{
    float dot, mag0_sq, mag1_sq, denom, cosAngle;

    if (w0->M != w1->M)
        return 0;
    if (w0->N != w1->N)
        return 0;

    if (w0->Lx != w1->Lx)
        return 0;
    if (w0->Lz != w1->Lz)
        return 0;

    if (fabsf(w0->amplitude - w1->amplitude) > 0.001f)
        return 0;

    if ((double)fabsf(w0->gravity - w1->gravity) > 0.1)
        return 0;

    if ((double)fabsf(w0->windvel - w1->windvel) > 0.1)
        return 0;

    dot = w0->winddir[0] * w1->winddir[0] + w0->winddir[1] * w1->winddir[1];

    mag0_sq = w0->winddir[0] * w0->winddir[0] + w0->winddir[1] * w0->winddir[1];
    mag1_sq = w1->winddir[0] * w1->winddir[0] + w1->winddir[1] * w1->winddir[1];

    denom = (float)((double)sqrtf(mag0_sq * mag1_sq) + 1e-10);
    cosAngle = dot / denom;

    if (fabsf(cosAngle - 1.0f) > 0.001f)
        return 0;

    return 1;
}

void R_CreateWaterSetup(const water_t *source, int waterMapSetupIndex, water_t *destination)
{
    int elementCount;
    int n, m, i;
    int N, M;
    float n_scale, m_scale;
    float kx, ky;
    float k_sq, k, omega_sq;
    float dot;
    float E_real, E_imag;
    float L;
    float coeff;
    double Ph;
    const char *name;

    elementCount = source->M * source->N;

    memcpy(destination, source, sizeof(water_t));

    destination->H0 = (complex_t *)Material_Alloc(elementCount * 8);

    destination->wTerm = (float *)Material_Alloc(elementCount * 4);

    N = destination->N;
    M = destination->M;

    L = destination->windvel * destination->windvel;
    L = L * L;
    L = L / destination->gravity;

    n_scale = (float)(2.0 * M_PI / ((double)((float)N * destination->Lx)));
    m_scale = (float)(2.0 * M_PI / ((double)((float)M * destination->Lz)));

    i = 0;
    for (n = -(N / 2); n < N / 2; n++) {
        kx = (float)n * n_scale;

        for (m = -(M / 2); m < M / 2; m++) {
            ky = (float)m * m_scale;

#if defined(COD2_X64)
            R_WaterGaussianRandom(&E_real, &E_imag);
#else
            GaussianRandom(&E_real, &E_imag);
#endif

            k_sq = kx * kx + ky * ky;
            k = sqrtf(k_sq);

            omega_sq = k * destination->gravity;

            dot = kx * destination->winddir[0] + ky * destination->winddir[1];

#if defined(COD2_X64)
            /* Mac's jb takes the spectrum branch for unordered dot products. */
            if (!(dot <= 0.0f)) {
#else
            if (dot > 0.0f) {
#endif

                Ph = (double)destination->amplitude * exp((double)(-1.0f / (k_sq * L)));
                Ph = Ph / (double)(k_sq * k_sq * k_sq);
                Ph = Ph * (double)(dot * dot);

                coeff = destination->amplitude * sqrtf((float)Ph * 0.5f);

                destination->H0[i].real = coeff * E_real;
                destination->H0[i].imag = coeff * E_imag;

                destination->wTerm[i] = sqrtf(omega_sq);
            } else {

                destination->H0[i].real = 0.0f;
                destination->H0[i].imag = 0.0f;
                destination->wTerm[i] = 0.0f;
            }

            i++;
        }
    }

    name = va("watersetup%i", waterMapSetupIndex);
    destination->image = R_CreateWaterMap( (char *)(name), (unsigned short)source->M, (unsigned short)source->N);
}
