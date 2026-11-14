#include "com_random.h"

#if defined(COD2_X64) && COD2_X64
#include <stdint.h>

/* CoD2MP_s.exe 57baa0/57baad; TLS initializer 58099c starts at one.
 * Unsigned arithmetic reproduces the CRT's 32-bit wrap for every seed. */
static _Thread_local uint32_t executableSeed = 1;

static int CrtNext(uint32_t *seed)
{
    *seed = *seed * UINT32_C(214013) + UINT32_C(2531011);
    return (int)((*seed >> 16) & UINT32_C(32767));
}

int Com_CrtRand(void)
{
    return CrtNext(&executableSeed);
}

void Com_CrtSrand(unsigned int seed)
{
    executableSeed = seed;
}

#if defined(COD2_CODX) && COD2_CODX
int Cod2x_CrtRand(void)
{
    static _Thread_local uint32_t dllSeed = 1;
    return CrtNext(&dllSeed);
}

float Cod2x_CrtRandomf(void)
{
    /* shared/weapons.cpp divides by Windows RAND_MAX, including the top end.
     * The original executable's randomf instead divides by 32768. */
    return (float)Cod2x_CrtRand() / 32767.0f;
}
#endif
#endif
