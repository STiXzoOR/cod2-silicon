#include "common_types.h"
#include "platform/cod2x_native.h"
#include "PC/qcommon/crash_handler.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static dvar_t freeze, home;
const dvar_t *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags)
{
    assert(!strcmp(name, "com_freezeWatch"));
    freeze.current.enabled = value;
    freeze.flags = flags;
    return &freeze;
}
const dvar_t *Dvar_FindVar(const char *name) { return !strcmp(name, "fs_homepath") ? &home : NULL; }
void Com_Printf(const char *format, ...) { (void)format; }
void Cod2xNativeURL_Frame(void) {}
void Cod2xNativeURL_Shutdown(void) {}
uint64_t MacSystem_Nanoseconds(void)
{
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}
static void delay(int seconds)
{
    struct timespec value = {seconds, 0};
    while (nanosleep(&value, &value) && errno == EINTR) {}
}
int main(int argc, char **argv)
{
    char path[256];
    home.current.string = argc > 1 ? argv[1] : "/tmp";
    Sys_InstallCrashHandler("CoD2x native freeze probe", "test", "native-arm64", "watchdog probe");
    Cod2xNative_Init();
    if (argc > 2 && !strcmp(argv[2], "--crash"))
        raise(SIGABRT);
    snprintf(path, sizeof(path), "%s/cod2_freeze_%d.txt", home.current.string, (int)getpid());
    for (int i = 0; i < 14; ++i) {
        Cod2xNative_Heartbeat();
        delay(1);
    }
    assert(access(path, F_OK) != 0); /* live/loading keep-alives do not freeze */
    freeze.current.enabled = 0;
    Cod2xNative_Frame();
    delay(14);
    assert(access(path, F_OK) != 0); /* disabled watch produces no report */
    freeze.current.enabled = 1;
    Cod2xNative_Frame();
    delay(14);
    assert(access(path, F_OK) == 0); /* real 12s stall created a diagnostic */
    Cod2xNative_Frame();
    Cod2xNative_Shutdown();
    puts("cod2x watchdog: heartbeat, disable, 12s stall and shutdown passed");
    return 0;
}
