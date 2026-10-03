#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include "common_types.h"
#include "cod2x_native.h"
#include "macos_system.h"
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
        uint64_t last = atomic_load_explicit(&heartbeat, memory_order_relaxed);
        uint64_t now = MacSystem_Nanoseconds();
        if (atomic_load_explicit(&enabled, memory_order_relaxed) && now >= last &&
            now - last > 12000000000ULL && last != reportedHeartbeat) {
            reportedHeartbeat = last;
            fprintf(stderr, "CoD2x: main thread has stalled for more than 12 seconds; writing a native diagnostic.\n");
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
        Com_Printf("CoD2x: native freeze reporting could not install its signal handler.\n");
        return;
    }
    atomic_store(&stopping, 0);
    atomic_store(&enabled, freezeWatch->current.enabled);
    Cod2xNative_Heartbeat();
    if (pthread_create(&watchThread, NULL, WatchMain, NULL) != 0) {
        sigaction(SIGUSR2, &previousFreezeSignal, NULL);
        Com_Printf("CoD2x: native freeze watcher could not create its thread.\n");
        return;
    }
    watchStarted = 1;
}

void Cod2xNative_Heartbeat(void)
{
    atomic_store_explicit(&heartbeat, MacSystem_Nanoseconds(), memory_order_relaxed);
}

void Cod2xNative_Frame(void)
{
    Cod2xNative_Heartbeat();
    if (freezeWatch)
        atomic_store_explicit(&enabled, freezeWatch->current.enabled, memory_order_relaxed);
    Cod2xNativeURL_Frame();
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
