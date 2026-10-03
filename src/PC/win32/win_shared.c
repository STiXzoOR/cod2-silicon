#include "common_types.h"
#include "imports.h"

#ifdef __cplusplus
extern "C" {
#endif
#if COD2_APPLE_SDK
DWORD timeGetTime(void);
#else
__declspec(dllimport) DWORD __stdcall timeGetTime(void);
#endif
#ifdef __cplusplus
}
#endif

#if COD2_APPLE_SDK
static uint32_t sys_timeBase;
static pthread_once_t clockOnce = PTHREAD_ONCE_INIT;
static void Sys_InitClock(void)
{
    sys_timeBase = (uint32_t)timeGetTime();
}
#else
extern int sys_timeBase;
static qboolean initialized;
#endif

int Sys_Milliseconds(void)
{
#if COD2_APPLE_SDK
    pthread_once(&clockOnce, Sys_InitClock);
    return (int)((uint32_t)timeGetTime() - sys_timeBase);
#else
    if (!initialized) {
        sys_timeBase = timeGetTime();
        initialized = 1;
    }
    return timeGetTime() - sys_timeBase;
#endif
}

int Sys_MillisecondsRaw(void)
{
#if COD2_APPLE_SDK
    return (int)(uint32_t)timeGetTime();
#else
    return timeGetTime();
#endif
}

void Sys_SnapVector(float *v)
{
    v[0] = (float)(int)v[0];
    v[1] = (float)(int)v[1];
    v[2] = (float)(int)v[2];
}
