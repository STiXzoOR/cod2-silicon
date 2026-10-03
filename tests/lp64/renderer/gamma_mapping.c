#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_init.c"

vidConfig_t vidConfig;
static dvar_t gammaDvar, ignoreDvar;
static const dvar_t *gammaPtr = &gammaDvar, *ignorePtr = &ignoreDvar;
void *imp_r_gamma = &gammaPtr;
void *imp_r_ignoreHwGamma = &ignorePtr;
static unsigned short uploaded[256];
static int uploads;

void RB_SetGammaRamp(const GfxGammaRamp *table)
{
    memcpy(uploaded, table->entries, sizeof(uploaded));
    ++uploads;
}

int main(void)
{
    vidConfig.deviceSupportsGamma = 1;
    gammaDvar.current.value = 1.4f;
    R_SetColorMappings();
    assert(uploads == 1 && uploaded[0] == 0 && uploaded[255] == 65535);
    assert(uploaded[32] > 32 * 257 && uploaded[128] > 128 * 257);
    ignoreDvar.current.enabled = 1;
    R_SetColorMappings();
    assert(uploads == 2);
    for (int i = 0; i < 256; ++i)
        assert(uploaded[i] == i * 257);
    ignoreDvar.current.enabled = 0;
    R_SetColorMappings();
    assert(uploads == 3 && uploaded[128] > 128 * 257);
    gammaDvar.current.value = 1;
    R_SetColorMappings();
    for (int i = 0; i < 256; ++i)
        assert(uploaded[i] == i * 257);
    puts("native gamma mapping: gamma, ignore on/off and identity passed");
}
