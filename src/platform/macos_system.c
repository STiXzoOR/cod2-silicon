#if defined(__APPLE__) && defined(COD2_X64)
#include "macos_system.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <limits.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <IOKit/IOKitLib.h>

static pthread_once_t clockOnce = PTHREAD_ONCE_INIT;
static mach_timebase_info_data_t timebase;
static pthread_once_t homeOnce = PTHREAD_ONCE_INIT;
static char homePath[PATH_MAX];

static void MacSystem_InitClock(void)
{
    mach_timebase_info(&timebase);
}

uint64_t MacSystem_Nanoseconds(void)
{
    pthread_once(&clockOnce, MacSystem_InitClock);
    return (uint64_t)((__uint128_t)mach_absolute_time() * timebase.numer / timebase.denom);
}

static void MacSystem_InitHome(void)
{
    const char *home = getenv("HOME");
    int length;
    if (!home || !home[0]) {
        struct passwd *user = getpwuid(getuid());
        if (user)
            home = user->pw_dir;
    }
    if (!home || !home[0])
        return;
    length = snprintf(homePath, sizeof(homePath), "%s/Library/Application Support/CoD2-native", home);
    if (length < 0 || length >= (int)sizeof(homePath))
        homePath[0] = '\0';
}

char *MacSystem_HomePath(void)
{
    pthread_once(&homeOnce, MacSystem_InitHome);
    return homePath[0] ? homePath : NULL;
}

uint64_t MacSystem_MemoryBytes(void)
{
    uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    sysctlbyname("hw.memsize", &bytes, &size, NULL, 0);
    return bytes;
}

float MacSystem_CPUFrequencyGHz(void)
{
    uint64_t hz = 0;
    size_t size = sizeof(hz);
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, NULL, 0) == 0 && hz)
        return (float)((double)hz / 1e9);

    /* Apple Silicon exposes cluster frequencies in kHz/voltage pairs. */
    io_registry_entry_t entry = IORegistryEntryFromPath(kIOMainPortDefault, "IODeviceTree:/arm-io/pmgr-child");
    if (!entry)
        entry = IORegistryEntryFromPath(kIOMainPortDefault, "IODeviceTree:/arm-io/pmgr");
    if (entry) {
        const CFStringRef keys[] = { CFSTR("voltage-states1-sram"), CFSTR("voltage-states5-sram") };
        for (int key = 0; key < 2; ++key) {
            CFTypeRef data = IORegistryEntryCreateCFProperty(entry, keys[key], NULL, 0);
            if (data && CFGetTypeID(data) == CFDataGetTypeID()) {
                const UInt8 *bytes = CFDataGetBytePtr(data);
                for (CFIndex i = 0; i + 8 <= CFDataGetLength(data); i += 8) {
                    uint32_t khz;
                    memcpy(&khz, bytes + i, sizeof(khz));
                    uint64_t frequency = (uint64_t)khz * 1000;
                    if (frequency > hz)
                        hz = frequency;
                }
            }
            if (data)
                CFRelease(data);
        }
        IOObjectRelease(entry);
    }
    return (float)((double)hz / 1e9);
}

uintptr_t MacSystem_ImageOffset(const void *address)
{
    Dl_info info;
    if (address && dladdr(address, &info) && info.dli_fbase)
        return (uintptr_t)address - (uintptr_t)info.dli_fbase;
    return (uintptr_t)address;
}

int MacSystem_CaptureStack(void **frames, int count, int skip)
{
    void *stack[64];
    int captured, available;
    if (count <= 0)
        return 0;
    memset(frames, 0, (size_t)count * sizeof(*frames));
    captured = backtrace(stack, 64);
    skip++; /* This helper's frame. */
    available = captured - skip;
    if (available <= 0)
        return 0;
    if (available > count)
        available = count;
    memcpy(frames, stack + skip, (size_t)available * sizeof(*frames));
    return available;
}
#endif
