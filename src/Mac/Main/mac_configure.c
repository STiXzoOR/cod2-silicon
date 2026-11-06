#if defined(COD2_X64) && defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <string.h>

extern float MacFeatures_GetCPUSpeedInGHz(void);
extern unsigned long MacFeatures_GetMemorySizeInMB(void);

double Sys_CpuGHz(void)
{
    return (double)MacFeatures_GetCPUSpeedInGHz();
}

int Sys_SystemMemoryMB(void)
{
    int mb = (int)MacFeatures_GetMemorySizeInMB();
    if (mb >= 0x8000001)
        mb = 0x8000000;
    return mb;
}

void Sys_DetectVideoCard(int descLimit, char *description)
{
    char renderer[512] = "Unknown GPU";
    io_service_t accelerator;

    if (descLimit <= 0)
        return;

    /* WinMain collects this before the native renderer creates a context. */
    accelerator = IOServiceGetMatchingService(kIOMainPortDefault,
                                              IOServiceMatching("IOAccelerator"));
    if (accelerator) {
        CFTypeRef model = IORegistryEntryCreateCFProperty(accelerator, CFSTR("model"), NULL, 0);
        IOObjectRelease(accelerator);
        if (model) {
            if (CFGetTypeID(model) == CFStringGetTypeID() &&
                !CFStringGetCString((CFStringRef)model, renderer, sizeof(renderer), kCFStringEncodingUTF8))
                strcpy(renderer, "Unknown GPU");
            CFRelease(model);
        }
    }

    strncpy(description, renderer, descLimit);
    /* sys_info feeds dvars and strcmp; retain its native C-string contract. */
    description[descLimit - 1] = '\0';
}

unsigned char Sys_SupportsSSE(void)
{
    /* The native SIMD implementation does not depend on an x86 Gestalt bit. */
    return 1;
}
#else
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"

extern float MacFeatures_GetCPUSpeedInGHz(void);
extern UInt32 MacFeatures_GetMemorySizeInMB(void);

double Sys_CpuGHz(void)
{
    return (double)MacFeatures_GetCPUSpeedInGHz();
}

int Sys_SystemMemoryMB(void)
{
    int mb = (int)MacFeatures_GetMemorySizeInMB();
    if (mb >= 0x8000001)
        mb = 0x8000000;
    return mb;
}

void Sys_DetectVideoCard(int descLimit, char *description)
{

    const char *renderer = "Linux OpenGL";
    strncpy(description, renderer, descLimit);
    description[descLimit - 1] = '\0';
    return 0;
}

bool Sys_SupportsSSE(void)
{
#ifdef BYTE_MATCH_DARWIN
    extern Boolean MacFeatures_HasGestaltAttribute(OSType inSelector, UInt32 inAttribute);
    return MacFeatures_HasGestaltAttribute(0x78383666, 25) != 0;
#else

    return 1;
#endif
}
#endif
