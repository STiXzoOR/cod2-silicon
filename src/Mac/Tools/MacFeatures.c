#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#if COD2_APPLE_SDK
#include "platform/macos_system.h"
#endif

SInt16 MacFeatures_GetSystemVersion(void)
{

    return 0x1040;
}

Boolean MacFeatures_HasGestaltAttribute(OSType inSelector, UInt32 inAttribute)
{

    if (inSelector == 0x78383666 && inAttribute == 0x19) {
        return 1;
    }
    return 0;
}

Boolean MacFeatures_IsAltiVecAvailable(UInt8 *outMajor, UInt8 *outMinor, UInt8 *outBug)
{

    return 0;
}

float MacFeatures_GetCPUSpeedInGHz(void)
{
#if COD2_APPLE_SDK
    return MacSystem_CPUFrequencyGHz();
#else
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[256];
    float mhz = 800.0f;

    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                char *p = strchr(line, ':');
                if (p) {
                    mhz = (float)atof(p + 1);
                }
                break;
            }
        }
        fclose(f);
    }
    return mhz / 1000.0f;
#endif
}

UInt32 MacFeatures_GetMemorySizeInMB(void)
{
#if COD2_APPLE_SDK
    return (UInt32)(MacSystem_MemoryBytes() >> 20);
#else
    FILE *f = fopen("/proc/meminfo", "r");
    char line[256];
    unsigned long kb = 128 * 1024;

    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                kb = strtoul(line + 9, NULL, 10);
                break;
            }
        }
        fclose(f);
    }
    return (UInt32)(kb / 1024);
#endif
}
