#include "common_types.h"
#include "platform/cod2x_native.h"
#include "PC/qcommon/crash_handler.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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
int Cod2x_DvarText(const char *name, char *value, size_t size)
{
    (void)name, (void)value, (void)size;
    return 0;
}
extern int Cod2x_InLayer(const void *address);
extern int Cod2x_ConnectProtocol(int advertised);
extern void Cod2x_ReportError(const char *kind, const char *message);
extern const char *Cod2x_ServiceURL(const char *url, char *buffer, size_t size);
/* Reports that are not CoD2x's: "<path>|<reason>" lines in receiver.txt. */
static void receive(const char *path, const char *reason)
{
    char name[512], line[1024];
    int fd, length;
    snprintf(name, sizeof(name), "%s/receiver.txt", home.current.string);
    fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0600);
    length = snprintf(line, sizeof(line), "%s|%s\n", path ? path : "", reason);
    if (fd >= 0 && write(fd, line, (size_t)length) < 0) {}
    if (fd >= 0)
        close(fd);
}
void Cod2xNativeURL_Frame(void) {}
void Cod2xNativeURL_Shutdown(void) {}
uint64_t MacSystem_Nanoseconds(void)
{
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}
/* The freeze log of this process: cod2x_freeze.YYYYMMDD_HHMMSS.<pid>.log in the home folder. */
static int freeze_log_exists(void)
{
    char ending[32];
    struct dirent *entry;
    DIR *directory = opendir(home.current.string);
    int found = 0;
    snprintf(ending, sizeof(ending), ".%d.log", (int)getpid());
    while (directory && !found && (entry = readdir(directory)) != NULL) {
        size_t length = strlen(entry->d_name), endingLength = strlen(ending);
        found = !strncmp(entry->d_name, "cod2x_freeze.", 13) && length > endingLength &&
                !strcmp(entry->d_name + length - endingLength, ending);
    }
    if (directory)
        closedir(directory);
    return found;
}
static void delay(int seconds)
{
    struct timespec value = {seconds, 0};
    while (nanosleep(&value, &value) && errno == EINTR) {}
}
int main(int argc, char **argv)
{
    home.current.string = argc > 1 ? argv[1] : "/tmp";
    Sys_InstallCrashHandler("CoD2x native freeze probe", "test", "native-arm64", "watchdog probe");
    Sys_SetReportReceiver(receive);
    /* The CoD2x layer is the run of units linked between its two markers. */
    assert(Cod2x_InLayer((const void *)&Cod2x_ConnectProtocol));
    assert(!Cod2x_InLayer((const void *)&main) && !Cod2x_InLayer((const void *)&Sys_InstallCrashHandler));
    Cod2xNative_Init();
    if (argc > 2 && !strcmp(argv[2], "--crash"))
        raise(SIGABRT);
    /* For the consent probe in run_native.sh: an error report, or a fault in CoD2x-layer code
       (a NULL URL, with COD2X_TEST_SERVICE_URL set). */
    if (argc > 2 && !strcmp(argv[2], "--error")) {
        Cod2x_ReportError("zpam-update", "consent probe");
        delay(3);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "--layer-crash")) {
        char url[256];
        return Cod2x_ServiceURL(NULL, url, sizeof(url)) != NULL;
    }
    for (int i = 0; i < 14; ++i) {
        Cod2xNative_Heartbeat();
        delay(1);
    }
    assert(!freeze_log_exists()); /* live/loading keep-alives do not freeze */
    freeze.current.enabled = 0;
    Cod2xNative_Frame();
    delay(14);
    assert(!freeze_log_exists()); /* disabled watch produces no report */
    freeze.current.enabled = 1;
    Cod2xNative_Frame();
    delay(14);
    assert(freeze_log_exists()); /* real 12s stall created a diagnostic */
    Cod2xNative_Frame();
    Cod2xNative_Shutdown();
    puts("cod2x watchdog: heartbeat, disable, 12s stall and shutdown passed");
    return 0;
}
