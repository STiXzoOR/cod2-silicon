#include "common_types.h"
#include "www_download.h"
#include "cod2_feature_config.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static clientConnection_t connection;
static clientConnection_t *clc = &connection;
static byte *connectionBytes = (byte *)&connection;
static byte **clc_ptr = &connectionBytes;
static clientStatic_t cls;
static LegacyHacks hacks;
static LegacyHacks *legacyHacks = &hacks;
static byte *uiBytes = (byte *)&hacks;
static byte **download_ui_ptr = &uiBytes;
static dvar_t developer;
static const dvar_t *com_developer = &developer;
static int reads, nextDownloads;
static dlStatus_t downloadStatus;
static char local[512], remote[512], renamed[512], commands[512];
static char homePath[256] = "/private", events[2048], errorText[1024], queued[256];
static int realFiles, renameFails, copies, removals, cancels, errors, beginFails, motd, redirectFlags;
static int directActive, directCancelled, directFinished;
static void event(const char *name)
{
    assert(strlen(events) + strlen(name) + 2 < sizeof(events));
    strcat(events, name); strcat(events, "|");
}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...)
{
    assert(code == 1); ++errors; event("error");
    va_list args; va_start(args, format); vsnprintf(errorText, sizeof(errorText), format, args); va_end(args);
}
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args; va_start(args, format); int n = vsnprintf(dest, size, format, args); va_end(args);
    return n;
}
char *va(const char *format, ...)
{
    static char text[1024];
    va_list args; va_start(args, format); vsnprintf(text, sizeof(text), format, args); va_end(args);
    return text;
}
void MSG_WriteReliableCommandToBuffer(const char *text, char *out, int size)
{ I_strncpyz(out, text, size); }
void CL_AddReliableCommand(const char *s) { strcat(commands, s); strcat(commands, "\n"); event(s); }
char *Dvar_GetString(const char *name) { (void)name; return homePath; }
char *MSG_ReadString(msg_t *msg) { (void)msg; return "https://example.test/main/map.iwd"; }
int MSG_ReadLong(msg_t *msg) { (void)msg; return reads++ ? redirectFlags : 100; }
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out)
{ snprintf(out, 256, "%s/%s/%s", base, game, qpath); }
int DL_BeginDownload(const char *to, const char *url, int debug)
{ (void)debug; strcpy(local, to); strcpy(remote, url); event("begin"); downloadStatus = DL_STATUS_IN_PROGRESS; return !beginFails; }
void DL_DownloadLoop(void) { event("poll"); }
dlStatus_t DL_GetStatus(void) { return downloadStatus; }
int DL_DLIsMotd(void) { return motd; }
void DL_CancelDownload(void) { ++cancels; event("cancel"); downloadStatus = DL_STATUS_CANCELLED; }
void Sys_OpenURL(const char *url, int active) { (void)url; (void)active; abort(); }
void Cbuf_ExecuteText(int when, const char *text) { assert(when == 2); snprintf(queued, sizeof(queued), "%s", text); event("reconnect"); }
int FS_ReadFile(const char *path, void **buf) { assert(!strcmp(path, "motd.txt")); *buf = "message of the day"; event("motd-read"); return 18; }
void FS_FreeFile(void *buf) { (void)buf; event("motd-free"); }
void Dvar_SetStringByName(const char *name, const char *value) { assert(!strcmp(name, "motd") && !strcmp(value, "message of the day")); event("motd-set"); }
static int Test_Rename(const char *old, const char *to)
{
    strcpy(renamed, to); event("rename");
    if (renameFails) { errno = EXDEV; return -1; }
    return realFiles ? rename(old, to) : 0;
}
static int Test_Remove(const char *path)
{
    ++removals; event("remove");
    return realFiles ? remove(path) : 0;
}
void FS_CopyFile(char *from, char *to)
{
    ++copies; event("copy");
    if (realFiles) {
        FILE *input = fopen(from, "rb"), *output = fopen(to, "wb");
        char buffer[256]; size_t size;
        assert(input && output);
        while ((size = fread(buffer, 1, sizeof(buffer), input)))
            assert(fwrite(buffer, 1, size, output) == size);
        assert(!ferror(input)); assert(!fclose(input) && !fclose(output));
    }
}
#define rename Test_Rename
#define remove Test_Remove
#ifdef TEST_WWW_LIFECYCLE
int CL_ClearStaticDownload(void);
#else
int CL_ClearStaticDownload(void) { return 0; }
#endif
void CL_NextDownload(void) { ++nextDownloads; event("next"); }
#if defined(COD2_CODX) && COD2_CODX
/* CL_BeginDownload asks the service boundary for a direct zPAM address first. These fixtures
 * cover ordinary server redirects, so the boundary declines and the lookup is counted. */
static dvar_t wwwDownload = { .current.enabled = 1 };
static const dvar_t *cl_wwwDownload = &wwwDownload;
static int directLookups;
int Cod2x_ZpamDirectURL(const char *name, int enabled, char *url, size_t size)
{
    assert(!strcmp(name, "main/map.iwd") && enabled);
    (void)url; (void)size; ++directLookups;
    return 0;
}
int Cod2x_ZpamDirectActive(void) { return directActive; }
const char *Cod2x_ZpamDirectRemote(void) { return "main/remote.iwd"; }
void Cod2x_ZpamDirectEnd(int cancelled)
{
    if (directActive) { event(cancelled ? "direct-cancel" : "direct-end"); directCancelled += cancelled; directFinished += !cancelled; }
    directActive = 0;
}
#endif
