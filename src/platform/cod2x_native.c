#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && !defined(DEDICATED)
#include "common_types.h"
#include "cod2x_native.h"
#include "macos_system.h"
#include "PC/qcommon/cod2x.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* <sys/sysctl.h> clashes with the engine's token_t. */
extern int sysctlbyname(const char *name, void *value, size_t *size, void *newValue, size_t newSize);
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Com_Printf(const char *, ...);
static const dvar_t *freezeWatch;
static pthread_t mainThread, watchThread;
static atomic_uint_fast64_t heartbeat;
static atomic_int stopping, enabled;
static atomic_uint userWaitDepth;
static int watchStarted;
static struct sigaction previousFreezeSignal;

static void FreezeSignal(int signal, siginfo_t *info, void *context)
{
    (void)signal;
    (void)info;
    Sys_CrashFreezeReport(context);
}

static void *WatchMain(void *argument)
{
    uint64_t reportedHeartbeat = 0;
    (void)argument;
    while (!atomic_load_explicit(&stopping, memory_order_relaxed)) {
        struct timespec delay = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&delay, NULL);
        if (atomic_load_explicit(&userWaitDepth, memory_order_acquire))
            continue;
        uint64_t last = atomic_load_explicit(&heartbeat, memory_order_relaxed);
        uint64_t now = MacSystem_Nanoseconds();
        if (!atomic_load_explicit(&userWaitDepth, memory_order_acquire) &&
            last == atomic_load_explicit(&heartbeat, memory_order_relaxed) &&
            atomic_load_explicit(&enabled, memory_order_relaxed) && now >= last &&
            now - last > 12000000000ULL && last != reportedHeartbeat) {
            reportedHeartbeat = last;
            fprintf(stderr, "CoD2 Silicon: main thread has stalled for more than 12 seconds; writing a native diagnostic.\n");
            pthread_kill(mainThread, SIGUSR2);
        }
    }
    return NULL;
}

void Cod2xNative_Init(void)
{
    const dvar_t *home;
    struct sigaction action = {0};
    if (watchStarted) return;
    freezeWatch = Dvar_RegisterBool("com_freezeWatch", 1, 0x1000);
    mainThread = pthread_self();
    home = Dvar_FindVar("fs_homepath");
    if (home && home->current.string)
        Sys_CrashSetDirectory(home->current.string);
    action.sa_sigaction = FreezeSignal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR2, &action, &previousFreezeSignal) != 0) {
        Com_Printf("CoD2 Silicon: native freeze reporting could not install its signal handler.\n");
        return;
    }
    atomic_store(&stopping, 0);
    atomic_store(&enabled, freezeWatch->current.enabled);
    Cod2xNative_Heartbeat();
    if (pthread_create(&watchThread, NULL, WatchMain, NULL) != 0) {
        sigaction(SIGUSR2, &previousFreezeSignal, NULL);
        Com_Printf("CoD2 Silicon: native freeze watcher could not create its thread.\n");
        return;
    }
    watchStarted = 1;
}

void Cod2xNative_Heartbeat(void)
{
    atomic_store_explicit(&heartbeat, MacSystem_Nanoseconds(), memory_order_relaxed);
}

/* A main-thread modal is expected to wait indefinitely for its user. */
void Cod2xNative_BeginUserWait(void)
{
    atomic_fetch_add_explicit(&userWaitDepth, 1, memory_order_acq_rel);
    Cod2xNative_Heartbeat();
}

void Cod2xNative_EndUserWait(void)
{
    /* Refresh before the last nested wait lets the watcher resume. */
    Cod2xNative_Heartbeat();
    atomic_fetch_sub_explicit(&userWaitDepth, 1, memory_order_acq_rel);
}

void Cod2xNative_Frame(void)
{
    Cod2xNative_Heartbeat();
    if (freezeWatch)
        atomic_store_explicit(&enabled, freezeWatch->current.enabled, memory_order_relaxed);
    Cod2xNativeURL_Frame();
}

/* Windows' crash report says whether a crash was inside Com_Frame; macOS's doesn't. */
void Cod2xNative_FrameDone(void)
{
}

/* What CoD2x's update request says about this machine. macOS keeps no history of
   identity changes; the system text takes CoD2x's Windows order: build, product,
   version, country, virtualisation, (no compatibility flags), admin or user. */
void Sys_Cod2xMachineHistory(Cod2xMachineHistory *out)
{
    char build[64] = "", version[64] = "", country[8] = "??";
    const char *language = getenv("LANG");
    size_t size;
    int virtualMachine = 0;
    memset(out, 0, sizeof(*out));
    size = sizeof(build);
    sysctlbyname("kern.osversion", build, &size, NULL, 0);
    size = sizeof(version);
    sysctlbyname("kern.osproductversion", version, &size, NULL, 0);
    size = sizeof(virtualMachine);
    sysctlbyname("kern.hv_vmm_present", &virtualMachine, &size, NULL, 0);
    if (language && strlen(language) >= 5 && language[2] == '_')
        snprintf(country, sizeof(country), "%.2s", language + 3);
    snprintf(out->system, sizeof(out->system), "%s\nmacOS\n%s\n%s\n%s\n\n%s", build, version, country,
             virtualMachine ? "VM" : "0", geteuid() == 0 ? "admin" : "user");
}

void Sys_Cod2xMachineHistoryClear(void)
{
}

void Cod2xNative_Shutdown(void)
{
    if (watchStarted) {
        atomic_store(&enabled, 0);
        atomic_store(&stopping, 1);
        pthread_join(watchThread, NULL);
        sigaction(SIGUSR2, &previousFreezeSignal, NULL);
        watchStarted = 0;
    }
    Cod2xNativeURL_Shutdown();
}
#endif
