/* Linux implementation of the native platform interface in platform/macos_system.h. */
#if defined(__linux__) && defined(COD2_X64)
#include "platform/macos_system.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_once_t homeOnce = PTHREAD_ONCE_INIT;
static char homePath[PATH_MAX];
static void (*observeFrame)(int);
static void (*observeWait)(uint64_t, uint64_t, uint64_t);
static pthread_once_t observerOnce = PTHREAD_ONCE_INIT;

static void MacSystem_InitObserver(void)
{
    /* No observer lookup or per-frame clocks unless explicitly requested. */
    if (getenv("COD2_FRAME_CSV")) {
        observeFrame = dlsym(RTLD_DEFAULT, "MacFrameProbe_Frame");
        observeWait = dlsym(RTLD_DEFAULT, "MacFrameProbe_Wait");
    }
}

void MacSystem_ObserveFrame(int engineTime)
{
    pthread_once(&observerOnce, MacSystem_InitObserver);
    if (observeFrame)
        observeFrame(engineTime);
}

uint64_t MacSystem_Nanoseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

void MacSystem_WaitUntil(uint64_t deadline)
{
    pthread_once(&observerOnce, MacSystem_InitObserver);
    /* Leave 80 us for the final spin; clock_nanosleep can wake late. */
    uint64_t now = MacSystem_Nanoseconds();
    if (observeWait)
        observeWait(now, deadline, 0);
    if (deadline > now && deadline - now > 80000) {
        uint64_t wake = deadline - 80000;
        struct timespec until = { (time_t)(wake / 1000000000u), (long)(wake % 1000000000u) };
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &until, NULL);
    }
    while (MacSystem_Nanoseconds() < deadline)
        ;
    if (observeWait)
        observeWait(now, deadline, MacSystem_Nanoseconds());
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
    /* The retail Linux server keeps its files here. */
    length = snprintf(homePath, sizeof(homePath), "%s/.callofduty2", home);
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
    long pages = sysconf(_SC_PHYS_PAGES);
    long size = sysconf(_SC_PAGESIZE);
    return pages > 0 && size > 0 ? (uint64_t)pages * (uint64_t)size : 0;
}

float MacSystem_CPUFrequencyGHz(void)
{
    char line[256];
    double khz = 0.0;
    FILE *file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (file) {
        if (fgets(line, sizeof(line), file))
            khz = atof(line);
        fclose(file);
    }
    if (khz <= 0.0 && (file = fopen("/proc/cpuinfo", "r"))) {
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                const char *value = strchr(line, ':');
                if (value)
                    khz = atof(value + 1) * 1000.0;
                break;
            }
        }
        fclose(file);
    }
    return (float)(khz / 1e6);
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
